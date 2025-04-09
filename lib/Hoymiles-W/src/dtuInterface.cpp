#include "dtuInterface.h"
#include <Arduino.h>
#include <MessageOutput.h>
#include <memory>

DTUInterface::DTUInterface(HoymilesWConnectionStatistics & connectionStats,const char *server, uint16_t port) :
connectionStatistics(connectionStats),
serverIP(server),
serverPort(port),
client(nullptr),
_restartConnection(false){
    //default data count (cann be changed)
    unsigned int size = 10;
    dataHist.reserve(size);
    for(unsigned int i=0;i<size;i++){
        dataHist.push_back(std::unique_ptr<char[]>(new char[sizeof(BaseData)*5 + sizeof(uint16_t) + sizeof(int16_t)]));
        memset(dataHist[i].get(),0,sizeof(BaseData)*5 + sizeof(uint16_t) + sizeof(int16_t));
        resetConnectionInfo();
    }

}

DTUInterface::~DTUInterface()
{
    if (client)
    {
        delete client;
        client = nullptr;
    }
}

void DTUInterface::setup()
{
    // Serial.println(F("DTUinterface:\t setup ... check client ..."));
    if (!client)
    {
        inverterData.currentTimestamp = 0;
        // Serial.println(F("DTUinterface:\t no client - setup new client"));
        client = new AsyncClient();
        //This is not working because keep alive is not fine for rxTimeout
        //client->setRxTimeout(60);//sets timeout to 60 seconds (if no ack or data received in this time connection is handled as closed (timout))
        //std::function<void(void*, AsyncClient*)> f = std::bind(&dtuInterface,client);
        MessageOutput.println("DTUinterface:\t setup for DTU '"+String(serverIP)+":"+String(serverPort)+"'");
        client->onConnect([&](void * arg, AsyncClient * client){this->onConnect();});
        client->onDisconnect([&](void * arg, AsyncClient * client){this->onDisconnect();});
        client->onError([&](void * arg, AsyncClient * client,int8_t error){this->onError(error);});
        client->onData([&](void * arg, AsyncClient * client,void *data, size_t len){this->onDataReceived(data,len);});
        /*client->onTimeout([&](void * arg, AsyncClient * client,uint32_t time){
            MessageOutput.printf("Ack timeout: %d\n",time);
        });*/

        initializeCRC();
        loopTimer.attach(5, DTUInterface::dtuLoopStatic, this);
    }
}

void DTUInterface::connect()
{
    dtuConnection.dtuSerial = 0;//reset serial
    if (client && !client->connected())
    {
        MessageOutput.println("DTUinterface:\t client not connected with DTU! try to connect (server: " + String(serverIP) + " - port: " + String(serverPort) + ") ...");
        if (client->connect(serverIP.c_str(), serverPort))
        {
            // Serial.println(F("DTUinterface:\t connection attempt successfully started..."));
        }
        else
        {
            MessageOutput.println(F("DTUinterface:\t connection attempt failed..."));
            dtuConnection.dtuConnectState = DTU_STATE_OFFLINE;
        }
    }
}

void DTUInterface::disconnect(uint8_t tgtState)
{
    // Serial.println(F("DTUinterface:\t disconnect request - try to disconnect from DTU ..."));
    if (client && client->connected())
    {
        client->close(true);
        dtuConnection.dtuConnectState = tgtState;
        // inverterData.dtuRssi = 0;
        MessageOutput.println(F("DTUinterface:\t disconnect request - DTU connection closed"));
        if (tgtState == DTU_STATE_STOPPED)
        {
            delete client;
            MessageOutput.printlnDebug("DTUinterface:\t with freeing memory");
        }
    }
    else if (tgtState != DTU_STATE_STOPPED)
    {
        MessageOutput.printlnDebug("DTUinterface:\t disconnect request - no DTU connection to close");
    }
}

bool DTUInterface::requestDataUpdate()
{
    if(client && client->connected()){
        if(dtuConnection.dtuTxRxState == DTU_TXRX_STATE_IDLE){
            writeReqRealDataNew();
            return true;
        }
    }else{
        dtuConnection.uptodate = false;
        //Serial.println(F("DTUinterface:\t getDataUpdate - ERROR - not connected to DTU!"));
        //handleError(DTU_ERROR_NO_TIME);
    }
    return false;
}

bool DTUInterface::requestStatisticUpdate()
{
    if (client && client->connected()){
        if(dtuConnection.dtuTxRxState == DTU_TXRX_STATE_IDLE){
            writeReqAppGetHistPower();
            return true;
        }
    }else{
        dtuConnection.uptodate = false;
        //Serial.println(F("DTUinterface:\t getDataUpdate - ERROR - not connected to DTU!"));
        //handleError(DTU_ERROR_NO_TIME);
    }
    return false;
}


void DTUInterface::setServer(const String &server)
{
    serverIP = server;
    _restartConnection = true;
}

const String & DTUInterface::getServer()const{
    return serverIP;
}


uint16_t DTUInterface::getPort()const{
    return serverPort;
}


void DTUInterface::setPort(uint16_t port)
{
    serverPort = port;
    _restartConnection = true;
}

void DTUInterface::setServerAndPort(const String & server,uint16_t port){
    serverPort = port;
    serverIP = server;
    _restartConnection = true;
}

void DTUInterface::setPowerLimit(int limit)
{
    inverterData.powerLimitSet = limit;
    if (client->connected())
    {
        MessageOutput.println("DTUinterface:\t try to set setPowerLimit: " + String(limit) + " %");
        writeReqCommand(limit);
    }
    else
    {
        MessageOutput.println(F("DTUinterface:\t try to setPowerLimit - client not connected."));
    }
}

void DTUInterface::requestRestartDevice()
{
    if (client->connected())
    {
        MessageOutput.println(F("DTUinterface:\t requestRestartDevice - send command to DTU ..."));
        writeCommandRestartDevice();
    }
    else
    {
        MessageOutput.println(F("DTUinterface:\t requestRestartDevice - client not connected."));
    }
}

// internal control methods

void DTUInterface::dtuLoop()
{
    if(_restartConnection){
        _restartConnection = false;
        dtuConnection.dtuConnectRetriesShort = 0;
        dtuConnection.dtuConnectRetriesLong = 0;
        resetConnectionInfo();
    }

    txrxStateObserver();
    dtuConnectionObserver();

    // check for last data received
    checkingForLastDataReceived();

    // check if we are in a cloud pause period
    if (dtuConnection.dtuConnectState != DTU_STATE_STOPPED)
    {
        // Check if we are in a pause period and if 60 seconds have passed
        if (dtuConnection.dtuConnectRetriesLong > 0)
        {
            if (millis() - dtuConnection.pauseStartTime < 60000)
                return; // Still in pause period, exit the function early
            else
                dtuConnection.dtuConnectRetriesLong = 0; // Pause period has ended, reset long retry counter
        }

        // Proceed if not preventing cloud errors and if disconnected but WiFi is connected
        if ((!client || !client->connected())) // && WiFi.status() == WL_CONNECTED
        {
            // If not currently in phase for waiting to connect, attempt to connect
            if (dtuConnection.dtuConnectState != DTU_STATE_TRY_RECONNECT)
            {
                // Increment short retry counter and attempt to connect
                dtuConnection.dtuConnectRetriesShort += 1;
                if (dtuConnection.dtuConnectRetriesShort <= 5)
                {
                    MessageOutput.println("DTUinterface:\t dtuLoop - try to connect ... short: " + String(dtuConnection.dtuConnectRetriesShort) + " - long: " + String(dtuConnection.dtuConnectRetriesLong));
                    dtuConnection.dtuConnectState = DTU_STATE_TRY_RECONNECT;
                    connect(); // Attempt to connect
                }
                else
                {
                    MessageOutput.println("DTUinterface:\t dtuLoop - PAUSE ... short: " + String(dtuConnection.dtuConnectRetriesShort) + " - long: " + String(dtuConnection.dtuConnectRetriesLong));
                    dtuConnection.dtuConnectState = DTU_STATE_OFFLINE;
                    // Exceeded 5 attempts, initiate pause period
                    dtuConnection.dtuConnectRetriesShort = 0; // Reset short retry counter
                    dtuConnection.dtuConnectRetriesLong = 1;  // Indicate pause period has started
                    dtuConnection.pauseStartTime = millis();  // Record start time of pause
                }
            }
        }
    }
}

void DTUInterface::txrxStateObserver()
{
    // check current txrx state and set seen at time and check for timeout
    if (dtuConnection.dtuTxRxState != dtuConnection.dtuTxRxStateLast)
    {
        MessageOutput.printlnDebug("DTUinterface:\t stateObserver - change from " + String(dtuConnection.dtuTxRxStateLast) + " to " + String(dtuConnection.dtuTxRxState) + " - difference: " + String(millis() - dtuConnection.dtuTxRxStateLastChange) + " ms");
        dtuConnection.dtuTxRxStateLast = dtuConnection.dtuTxRxState;
        dtuConnection.dtuTxRxStateLastChange = millis();
    }
    else if (millis() - dtuConnection.dtuTxRxStateLastChange > 15000 && dtuConnection.dtuTxRxState != DTU_TXRX_STATE_IDLE)
    {
        dtuConnection.updateFailed = true;
        MessageOutput.printlnDebug("DTUinterface:\t stateObserver - timeout - reset txrx state to DTU_TXRX_STATE_IDLE");
        dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    }
}

void DTUInterface::dtuConnectionObserver()
{
    boolean currentOnlineOfflineState = false;
    if (dtuConnection.dtuConnectState == DTU_STATE_CONNECTED)
    {
        currentOnlineOfflineState = true;
    }
    else if (dtuConnection.dtuConnectState == DTU_STATE_OFFLINE || dtuConnection.dtuConnectState == DTU_STATE_TRY_RECONNECT || dtuConnection.dtuConnectState == DTU_STATE_STOPPED || dtuConnection.dtuConnectState == DTU_STATE_CONNECT_ERROR || dtuConnection.dtuConnectState == DTU_STATE_DTU_REBOOT)
    {
        currentOnlineOfflineState = false;
    }

    if (currentOnlineOfflineState != lastOnlineOfflineState)
    {
        MessageOutput.println("DTUinterface:\t setOverallOnlineOfflineState - change from " + String(lastOnlineOfflineState) + " to " + String(dtuConnection.dtuConnectionOnline));
        lastOnlineOfflineChange = millis();
        lastOnlineOfflineState = currentOnlineOfflineState;
    }

    // summary of connection state
    if (currentOnlineOfflineState)
    {
        dtuConnection.dtuConnectionOnline = true;
    }
    else if (millis() - lastOnlineOfflineChange > 60000 && currentOnlineOfflineState == false)
    {
        MessageOutput.println(F("DTUinterface:\t setOverallOnlineOfflineState - timeout - reset online offline state"));
        MessageOutput.printlnDebug(" - difference: " + String((millis() - lastOnlineOfflineChange)/1000,3) + " ms - current conn state: " + String(dtuConnection.dtuConnectState));
        dtuConnection.dtuConnectionOnline = false;
    }
}

void DTUInterface::dtuLoopStatic(DTUInterface *dtuInterface)
{
    if (dtuInterface)
    {
        dtuInterface->dtuLoop();
    }
}

void DTUInterface::keepAlive()
{
    if (client && client->connected())
    {
        const char *keepAliveMsg = "\0"; // minimal message
        client->write(keepAliveMsg, strlen(keepAliveMsg));
        MessageOutput.printfDebug("DTUinterface:\t keepAlive message sent.\n");
    }
    else
    {
        MessageOutput.println(F("DTUinterface:\t keepAlive - client not connected."));
    }
}

void DTUInterface::keepAliveStatic(DTUInterface *dtuInterface)
{
    if (dtuInterface)
    {
        dtuInterface->keepAlive();
    }
}

void DTUInterface::flushConnection()
{
    MessageOutput.printlnDebug("DTUinterface:\t Flushing connection and instance...");

    loopTimer.detach();
    MessageOutput.printlnDebug("DTUinterface:\t All timers stopped.");

    // Disconnect if connected
    if (client && client->connected())
    {
        client->close(true);
        MessageOutput.println("DTUinterface:\t Connection closed.");
    }

    // Free the client memory
    if (client)
    {
        delete client;
        client = nullptr;
        MessageOutput.printlnDebug("DTUinterface:\t Client memory freed.");
    }

    // Reset connection control and global data
    memset((void*)(&dtuConnection), 0, sizeof(ConnectionControl));
    memset((void*)(&inverterData), 0, sizeof(InverterData));
    MessageOutput.println(F("DTUinterface:\t Connection control and global data reset."));
}

// event driven methods
void DTUInterface::onConnect()
{
    client->setKeepAlive(10 * 1000, 5);
    // Connection established
    dtuConnection.dtuConnectState = DTU_STATE_CONNECTED;
    // DTUInterface *conn = static_cast<DTUInterface *>(arg);
    MessageOutput.println(F("DTUinterface:\t connected to DTU"));
    //MessageOutput.println(F("DTUinterface:\t starting keep-alive timer..."));
    //keepAliveTimer.attach(10, DTUInterface::keepAliveStatic, this);
    // initiate next data update immediately (at startup or re-connect)

    //TODO check what this here is fore
    //platformData.dtuNextUpdateCounterSeconds = inverterData.currentTimestamp - userConfig.dtuUpdateTime + 5;
}

void DTUInterface::onDisconnect()
{
    // Connection lost
    MessageOutput.println(F("DTUinterface:\t disconnected from DTU"));
    resetConnectionInfo();
}

void DTUInterface::onError(int8_t error)
{
    if(dtuConnection.dtuConnectState == DTU_STATE_CONNECTED){//check if not in reconcation state
        if(error == ERR_RST){
            connectionStatistics.Disconnects++;
        }else if(error == ERR_ABRT){//connection lost to timeout (keep alive message) or connection failed
            connectionStatistics.ConnectionTimeouts++;
        }
    }

    // Connection error
    String errorStr = client->errorToString(error);
    MessageOutput.println("DTUinterface:\t DTU Connection error: " + errorStr + " (" + String(error) + ")");
    dtuConnection.dtuConnectState = DTU_STATE_CONNECT_ERROR;
    inverterData.dtuRssi = 0;
}

void DTUInterface::handleError(uint8_t errorState)
{
    if (client->connected())
    {
        dtuConnection.dtuErrorState = errorState;
        dtuConnection.dtuConnectState = DTU_STATE_DTU_REBOOT;
        MessageOutput.print(F("DTUinterface:\t DTU Connection --- ERROR - try with reboot of DTU - error state: "));
        MessageOutput.printlnDebug(errorState);
        writeCommandRestartDevice();
        dtuConnection.dtuResetRequested = dtuConnection.dtuResetRequested + 1;
        // disconnect(dtuConnection.dtuConnectState);
    }
}

// Callback method to handle incoming dataf
void DTUInterface::onDataReceived(void *data, size_t len)
{
    txrxStateObserver();
    // first 10 bytes are header or similar and actual data starts from the 11th byte
    pb_istream_t istream = pb_istream_from_buffer(static_cast<uint8_t *>(data) + 10, len - 10);

    switch (dtuConnection.dtuTxRxState)
    {
    case DTU_TXRX_STATE_WAIT_REALDATANEW:
        readRespRealDataNew(istream);
        // if real data received, then request config for powerlimit
        writeReqGetConfig();
        break;
    case DTU_TXRX_STATE_WAIT_APPGETHISTPOWER:
        readRespAppGetHistPower(istream);
        break;
    case DTU_TXRX_STATE_WAIT_GETCONFIG:
        readRespGetConfig(istream);
        break;
    case DTU_TXRX_STATE_WAIT_COMMAND:
        readRespCommand(istream);
        // get updated power setting
        writeReqGetConfig();
        break;
    case DTU_TXRX_STATE_WAIT_RESTARTDEVICE:
        readRespCommandRestartDevice(istream);
        break;
    default:
        MessageOutput.println(F("DTUinterface:\t onDataReceived - no valid or known state"));
        break;
    }
}

// output data methods

void DTUInterface::printDataAsTextToSerial()
{
    std::lock_guard<std::mutex>lock (inverterDataMutex);
    MessageOutput.print("power limit (set): " + String(inverterData.powerLimit) + " % (" + String(inverterData.powerLimitSet) + " %) --- ");
    MessageOutput.print("inverter temp: " + String(inverterData.inverterTemp) + " °C \n");

    MessageOutput.print(F(" \t |_____current____|_____voltage___|_____power_____|________daily______|_____total_____|\n"));
    // 12341234 |1234 current  |1234 voltage  |1234 power1234|12341234daily 1234|12341234total 1234|
    // grid1234 |1234 123456 A |1234 123456 V |1234 123456 W |1234 12345678 kWh |1234 12345678 kWh |
    // pvO 1234 |1234 123456 A |1234 123456 V |1234 123456 W |1234 12345678 kWh |1234 12345678 kWh |
    // pvI 1234 |1234 123456 A |1234 123456 V |1234 123456 W |1234 12345678 kWh |1234 12345678 kWh |
    MessageOutput.print(F("grid\t"));
    MessageOutput.printf(" |\t %6.2f A", calcValue(inverterData.grid.current,100));
    MessageOutput.printf(" |\t %6.2f V", calcValue(inverterData.grid.voltage));
    MessageOutput.printf(" |\t %6.2f W", calcValue(inverterData.grid.power));
    MessageOutput.printf(" |\t %8.3f kWh", calcValue(inverterData.grid.dailyEnergy,1000));
    MessageOutput.printf(" |\t %8.3f kWh |\n", calcValue(inverterData.grid.totalEnergy,1000));

    for(int i=0;i<4;i++){
        MessageOutput.printf("pv%d\t",i);
        MessageOutput.printf(" |\t %6.2f A", calcValue(inverterData.pv[i].current,100));
        MessageOutput.printf(" |\t %6.2f V", calcValue(inverterData.pv[i].voltage));
        MessageOutput.printf(" |\t %6.2f W", calcValue(inverterData.pv[i].power));
        MessageOutput.printf(" |\t %8.3f kWh", calcValue(inverterData.pv[i].dailyEnergy,1000));
        MessageOutput.printf(" |\t %8.3f kWh |\n", calcValue(inverterData.pv[i].totalEnergy,1000));
    }
}

bool DTUInterface::isConnected()
{
    return client != nullptr && dtuConnection.dtuConnectState == DTU_STATE_CONNECTED;
}

//checks if connected and faitl for reconnect time (90 sec)
bool DTUInterface::isReachable()
{
    return client != nullptr && dtuConnection.dtuConnectionOnline;
}

// helper methods

float DTUInterface::calcValue(int32_t value, int32_t divider)
{
    float result = static_cast<float>(value) / divider;
    return result;
}

String getTimeStringByTimestamp(unsigned long timestamp)
{
    char s[20];
    memset(s,'\0',20);
    snprintf(s, 20, "%lu", timestamp);
    return String(s);
}

void DTUInterface::initializeCRC()
{
    // CRC
    crc.setInitial(CRC16_MODBUS_INITIAL);
    crc.setPolynome(CRC16_MODBUS_POLYNOME);
    crc.setReverseIn(CRC16_MODBUS_REV_IN);
    crc.setReverseOut(CRC16_MODBUS_REV_OUT);
    crc.setXorOut(CRC16_MODBUS_XOR_OUT);
    crc.restart();
    // Serial.println(F("DTUinterface:\t CRC initialized"));
}

void DTUInterface::checkingForLastDataReceived()//TODO (FOR ME) understand what that here is suppose to do
{
    //MessageOutput.printf("cur: %d last: %d dif: %d\n",inverterData.currentTimestamp,inverterData.lastRespTimestamp,(inverterData.currentTimestamp + 5) - inverterData.lastRespTimestamp);
    // check if last data received - currentTimestamp + 5 sec (to debounce async current timestamp) - lastRespTimestamp > 3 min
    if (((inverterData.currentTimestamp + 5) - inverterData.lastRespTimestamp) > (3 * 60) && inverterData.grid.voltage > 0 && dtuConnection.dtuErrorState != DTU_ERROR_LAST_SEND) // dtuGlobalData.grid.voltage > 0 indicates dtu/ inverter was working
    {
        inverterData.grid.power = 0;
        inverterData.grid.current = 0;
        inverterData.grid.voltage = 0;

        for(int i=0;i<4;i++){
            inverterData.pv[i].power = 0;
            inverterData.pv[i].current = 0;
            inverterData.pv[i].voltage = 0;
        }

        inverterData.dtuRssi = 0;

        dtuConnection.dtuErrorState = DTU_ERROR_LAST_SEND;
        dtuConnection.dtuConnectState = DTU_STATE_OFFLINE;
        //inverterData.updateReceived = true;
        MessageOutput.println("DTUinterface:\t checkingForLastDataReceived >>>>> TIMEOUT 5 min for DTU -> NIGHT - send zero values +++ currentTimestamp: " + String(inverterData.currentTimestamp) + " - lastRespTimestamp: " + String(inverterData.lastRespTimestamp));
    }
}

/**
 * @brief Checks for data updates and performs necessary actions.
 *
 * This function checks for hanging values on the DTU side and updates the grid voltage history.
 * It also checks if the response timestamp has changed and updates the local time if necessary.
 * If there is a response time error, it stops the connection to the DTU.
 */
void DTUInterface::checkingDataUpdate()
{
    // checking for hanging values on DTU side
    // fill grid voltage history

    MessageOutput.printfDebug("Current dataHitId %d of %d\n",dataHitsCount,dataHist.size());
    memcpy(dataHist[dataHitsCount].get(),&inverterData,sizeof(BaseData)*5 + sizeof(uint16_t) + sizeof(int16_t));
    dataHitsCount++;
    if(dataHitsCount >= dataHist.size()){
        dataHitsCount = 0;
    }

    bool gridVoltValueHanging = true;
    // Serial.println(F("DTUinterface:\t GridV check"));
    // compare all values in history with the first value - if all are equal, then the value is hanging
    if(inverterData.grid.voltage < 10 || dataHist.size() <= 1){//when grid voltage below zero inverter not conneced to grid so no restart needed
        gridVoltValueHanging = false;
        MessageOutput.printlnDebug("Data hang check not possible");
    }else{
        for (unsigned int i = 1; i < dataHist.size(); i++)
        {
            // Serial.println("DTUinterface:\t --> " + String(i) + " compare : " + String(gridVoltHist[i]) + " V - with: " + String(gridVoltHist[0]) + " V");
            int ret = memcmp(dataHist[i].get(),dataHist[0].get(),sizeof(BaseData)*5 + sizeof(uint16_t) + sizeof(int16_t));
            MessageOutput.printfDebug("Dif on memcompare is: %d\n",ret);
            if (ret != 0)
            {
                gridVoltValueHanging = false;
                break;
            }
        }
    }
    // Serial.println("DTUinterface:\t GridV check result: " + String(gridVoltValueHanging));
    if (gridVoltValueHanging)
    {
        MessageOutput.println(F("DTUinterface:\t checkingDataUpdate -> grid voltage observer found hanging value (DTU_ERROR_DATA_NO_CHANGE) - try to reboot DTU"));
        handleError(DTU_ERROR_DATA_NO_CHANGE);
        dtuConnection.uptodate = false;
    }

    // check for up-to-date - last response timestamp have to not equal the current response timestamp
    if ((inverterData.lastRespTimestamp != inverterData.respTimestamp) && (inverterData.respTimestamp != 0))
    {
        dtuConnection.uptodate = true;
        dtuConnection.dtuErrorState = DTU_ERROR_NO_ERROR;
        // sync local time (in seconds) to DTU time, only if abbrevation about 3 seconds
        if (abs((int(inverterData.respTimestamp) - int(inverterData.currentTimestamp))) > 3)
        {
            inverterData.currentTimestamp = inverterData.respTimestamp;
            MessageOutput.print(F("DTUinterface:\t checkingDataUpdate ---> synced local time with DTU time\n"));
        }
    }
    else
    {
        dtuConnection.uptodate = false;
        MessageOutput.println(F("DTUinterface:\t checkingDataUpdate -> (DTU_ERROR_NO_TIME) - try to reboot DTU"));
        // stopping connection to DTU when response time error - try with reconnec
        //removed by me
        handleError(DTU_ERROR_NO_TIME);
    }
    inverterData.lastRespTimestamp = inverterData.respTimestamp;
}

// protocol buffer methods

void DTUInterface::writeReqRealDataNew()
{
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    RealDataNewResDTO realdatanewresdto = RealDataNewResDTO_init_default;
    realdatanewresdto.offset = DTU_TIME_OFFSET;
    realdatanewresdto.time = int32_t(inverterData.currentTimestamp);
    bool status = pb_encode(&stream, RealDataNewResDTO_fields, &realdatanewresdto);

    if (!status)
    {
        MessageOutput.println(F("DTUinterface:\t writeReqRealDataNew - failed to encode"));
        return;
    }

    // Serial.println(F("DTUinterface:\t writeReqRealDataNew --- encoded: "));
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        // Serial.printf("%02X", buffer[i]);
        crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48;
    header[1] = 0x4d;
    header[2] = 0xa3;
    header[3] = 0x11; // RealDataNew = 0x11
    header[4] = 0x00;
    header[5] = 0x01;
    header[6] = (crc.calc() >> 8) & 0xFF;
    header[7] = (crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; // suggest parentheses around '+' inside '>>' [-Wparentheses]
    header[9] = (stream.bytes_written + 10) & 0xFF;        // warning: suggest parentheses around '+' in operand of '&' [-Wparentheses]
    crc.restart();

    uint8_t message[10 + stream.bytes_written];
    for (int i = 0; i < 10; i++)
    {
        message[i] = header[i];
    }
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        message[i + 10] = buffer[i];
    }

    // Serial.print(F("\nRequest: "));
    // for (int i = 0; i < 10 + stream.bytes_written; i++)
    // {
    //   Serial.print(message[i]);
    // }
    // Serial.println("");

    // Serial.println(F("DTUinterface:\t writeReqRealDataNew --- send request to DTU ..."));
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_WAIT_REALDATANEW;
    client->write((const char *)message, 10 + stream.bytes_written);

    // readRespRealDataNew(locTimeSec);
    connectionStatistics.SendRequests++;
}

std::unique_ptr<InverterData> DTUInterface::newDataAvailable()
{
    std::lock_guard<std::mutex>lock(inverterDataMutex);
    if(!dtuConnection.updateReceived){
        return nullptr;
    }
    auto ret = std::make_unique<InverterData>(inverterData);
    dtuConnection.updateReceived = false;
    return std::move(ret);
}

void DTUInterface::readRespRealDataNew(pb_istream_t istream)
{
    connectionStatistics.SuccessfulRequests++;
    std::lock_guard<std::mutex>lock (inverterDataMutex);
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    RealDataNewReqDTO realdatanewreqdto = RealDataNewReqDTO_init_default;

    PvMO pvData[4];
    for(int i = 0;i<4;i++){
        pvData[i] = PvMO_init_zero;
    }

    pb_decode(&istream, &RealDataNewReqDTO_msg, &realdatanewreqdto);
    MessageOutput.printlnDebug("DTUinterface:\t RealDataNew  - got remote (" + String(realdatanewreqdto.timestamp) + "):\t" + getTimeStringByTimestamp(realdatanewreqdto.timestamp));
    if (realdatanewreqdto.timestamp != 0)
    {
        inverterData.respTimestamp = uint32_t(realdatanewreqdto.timestamp);
        // dtuGlobalData.updateReceived = true; // not needed here - everytime both request (realData and getConfig) will be set
        dtuConnection.dtuErrorState = DTU_ERROR_NO_ERROR;

        for(int i=0;i<4;i++){
            pvData[i] = realdatanewreqdto.pv_data[i];
        }

        SGSMO gridData = SGSMO_init_zero;
        gridData = realdatanewreqdto.sgs_data[0];
        dtuConnection.dtuSerial = static_cast<uint64_t>(gridData.serial_number);

        inverterData.grid.current = static_cast<uint16_t>(gridData.current);
        inverterData.grid.voltage = static_cast<uint16_t>(gridData.voltage);
        inverterData.grid.power = static_cast<uint16_t>(gridData.active_power);

        inverterData.inverterTemp = static_cast<int16_t>(gridData.temperature);
        inverterData.gridFreq = static_cast<uint16_t>(gridData.frequency);
        inverterData.reactivePower = static_cast<uint16_t>(gridData.reactive_power);
        inverterData.powerFactor = static_cast<uint16_t>(gridData.power_factor);

        for(int i=0;i<4;i++){
            inverterData.pv[i].current = static_cast<uint16_t>(pvData[i].current);
            inverterData.pv[i].voltage = static_cast<uint16_t>(pvData[i].voltage);
            inverterData.pv[i].power = static_cast<uint16_t>(pvData[i].power);
            inverterData.pv[i].dailyEnergy = static_cast<uint32_t>(pvData[i].energy_daily);
            if (pvData[i].energy_total != 0){
                inverterData.pv[i].totalEnergy = static_cast<uint32_t>(pvData[i].energy_total);
            }

            //inverterData.grid.dailyEnergy += inverterData.pv[i].dailyEnergy/1000;
            //inverterData.grid.totalEnergy += inverterData.pv[i].totalEnergy/1000;
        }

        // checking for hanging values on DTU side and set control state
        checkingDataUpdate();
    }
    else
    {
        MessageOutput.println(F("DTUinterface:\t readRespRealDataNew -> got timestamp == 0 (DTU_ERROR_NO_TIME) - try to reboot DTU"));
        //removed by me
        handleError(DTU_ERROR_NO_TIME);
    }
}

void DTUInterface::writeReqAppGetHistPower()
{
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    AppGetHistPowerResDTO appgethistpowerres = AppGetHistPowerResDTO_init_default;
    appgethistpowerres.offset = DTU_TIME_OFFSET;
    appgethistpowerres.requested_time = int32_t(inverterData.currentTimestamp);
    bool status = pb_encode(&stream, AppGetHistPowerResDTO_fields, &appgethistpowerres);

    if (!status)
    {
        MessageOutput.println(F("DTUinterface:\t writeReqAppGetHistPower - failed to encode"));
        return;
    }

    // Serial.print(F("\nencoded: "));
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        // Serial.printf("%02X", buffer[i]);
        crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48;
    header[1] = 0x4d;
    header[2] = 0xa3;
    header[3] = 0x15; // AppGetHistPowerRes = 0x15
    header[4] = 0x00;
    header[5] = 0x01;
    header[6] = (crc.calc() >> 8) & 0xFF;
    header[7] = (crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; // suggest parentheses around '+' in operand of '&'
    header[9] = (stream.bytes_written + 10) & 0xFF;        // suggest parentheses around '+' in operand of '&'
    crc.restart();

    uint8_t message[10 + stream.bytes_written];
    for (int i = 0; i < 10; i++)
    {
        message[i] = header[i];
    }
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        message[i + 10] = buffer[i];
    }

    // Serial.print(F("\nRequest: "));
    // for (int i = 0; i < 10 + stream.bytes_written; i++)
    // {
    //   Serial.print(message[i]);
    // }
    // Serial.println("");

    //     dtuClient.write(message, 10 + stream.bytes_written);

    MessageOutput.printlnDebug("DTUinterface:\t writeReqAppGetHistPower --- send request to DTU ...");
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_WAIT_APPGETHISTPOWER;
    client->write((const char *)message, 10 + stream.bytes_written);
    //     readRespAppGetHistPower();
    connectionStatistics.SendRequests++;
}

void DTUInterface::readRespAppGetHistPower(pb_istream_t istream)
{
    connectionStatistics.SuccessfulRequests++;
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    AppGetHistPowerReqDTO appgethistpowerreqdto = AppGetHistPowerReqDTO_init_default;

    pb_decode(&istream, &AppGetHistPowerReqDTO_msg, &appgethistpowerreqdto);

    inverterData.grid.dailyEnergy = appgethistpowerreqdto.daily_energy;
    inverterData.grid.totalEnergy = appgethistpowerreqdto.total_energy;

    dtuConnection.statisticsInitialized = true;
    // Serial.printf("\n\n start_time: %i", appgethistpowerreqdto.start_time);
    // Serial.printf(" | step_time: %i", appgethistpowerreqdto.step_time);
    // Serial.printf(" | absolute_start: %i", appgethistpowerreqdto.absolute_start);
    // Serial.printf(" | long_term_start: %i", appgethistpowerreqdto.long_term_start);
    // Serial.printf(" | request_time: %i", appgethistpowerreqdto.request_time);
    // Serial.printf(" | offset: %i", appgethistpowerreqdto.offset);

    // Serial.printf("\naccess_point: %i", appgethistpowerreqdto.access_point);
    // Serial.printf(" | control_point: %i", appgethistpowerreqdto.control_point);
    // Serial.printf(" | daily_energy: %i", appgethistpowerreqdto.daily_energy);

    // Serial.printf(" | relative_power: %f", calcValue(appgethistpowerreqdto.relative_power));

    // Serial.printf(" | serial_number: %lld", appgethistpowerreqdto.serial_number);

    // Serial.printf(" | total_energy: %f kWh", calcValue(appgethistpowerreqdto.total_energy, 1000));
    // Serial.printf(" | warning_number: %i\n", appgethistpowerreqdto.warning_number);

    // Serial.printf("\n power data count: %i\n", appgethistpowerreqdto.power_array_count);
    // int starttimeApp = appgethistpowerreqdto.absolute_start;
    // for (unsigned int i = 0; i < appgethistpowerreqdto.power_array_count; i++)
    // {
    //   float histPowerValue = float(appgethistpowerreqdto.power_array[i]) / 10;
    //   Serial.printf("%i (%s) - power data: %f W (%i)\n", i, getTimeStringByTimestamp(starttimeApp), histPowerValue, appgethistpowerreqdto.power_array[i]);
    //   starttime = starttime + appgethistpowerreqdto.step_time;
    // }

    // Serial.printf("\nsn: %lld, relative_power: %i, total_energy: %i, daily_energy: %i, warning_number: %i\n", appgethistpowerreqdto.serial_number, appgethistpowerreqdto.relative_power, appgethistpowerreqdto.total_energy, appgethistpowerreqdto.daily_energy,appgethistpowerreqdto.warning_number);
}

void DTUInterface::writeReqGetConfig()
{
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    GetConfigResDTO getconfigresdto = GetConfigResDTO_init_default;
    getconfigresdto.offset = DTU_TIME_OFFSET;
    getconfigresdto.time = int32_t(inverterData.currentTimestamp);
    bool status = pb_encode(&stream, GetConfigResDTO_fields, &getconfigresdto);

    if (!status)
    {
        MessageOutput.println(F("DTUinterface:\t writeReqGetConfig - failed to encode"));
        return;
    }

    // Serial.print(F("\nencoded: "));
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        // Serial.printf("%02X", buffer[i]);
        crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48;
    header[1] = 0x4d;
    header[2] = 0xa3;
    header[3] = 0x09; // GetConfig = 0x09
    header[4] = 0x00;
    header[5] = 0x01;
    header[6] = (crc.calc() >> 8) & 0xFF;
    header[7] = (crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; // suggest parentheses around '+' inside '>>' [-Wparentheses]
    header[9] = (stream.bytes_written + 10) & 0xFF;        // warning: suggest parentheses around '+' in operand of '&' [-Wparentheses]
    crc.restart();

    uint8_t message[10 + stream.bytes_written];
    for (int i = 0; i < 10; i++)
    {
        message[i] = header[i];
    }
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        message[i + 10] = buffer[i];
    }

    // Serial.print(F("\nRequest: "));
    // for (int i = 0; i < 10 + stream.bytes_written; i++)
    // {
    //   Serial.print(message[i]);
    // }
    // Serial.println("");

    //     client->write(message, 10 + stream.bytes_written);
    // Serial.println(F("DTUinterface:\t writeReqGetConfig --- send request to DTU ..."));
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_WAIT_GETCONFIG;
    client->write((const char *)message, 10 + stream.bytes_written);
    //     readRespGetConfig();
    connectionStatistics.SendRequests++;
}

void DTUInterface::readRespGetConfig(pb_istream_t istream)
{
    connectionStatistics.SuccessfulRequests++;

    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    GetConfigReqDTO getconfigreqdto = GetConfigReqDTO_init_default;

    pb_decode(&istream, &GetConfigReqDTO_msg, &getconfigreqdto);
    // Serial.printf("\nsn: %lld, relative_power: %i, total_energy: %i, daily_energy: %i, warning_number: %i\n", appgethistpowerreqdto.serial_number, appgethistpowerreqdto.relative_power, appgethistpowerreqdto.total_energy, appgethistpowerreqdto.daily_energy,appgethistpowerreqdto.warning_number);
    //Serial.printf("\ndevice_serial_number: %lld", realdatanewreqdto.device_serial_number);
    // Serial.printf("\n\nwifi_rssi:\t %i %%", getconfigreqdto.wifi_rssi);
    // Serial.printf("\nserver_send_time:\t %i", getconfigreqdto.server_send_time);
    // Serial.printf("\nrequest_time (transl):\t %s", getTimeStringByTimestamp(getconfigreqdto.request_time));
    // Serial.printf("DTUinterface:\t limit_power_mypower:\t %f %%\n", calcValue(getconfigreqdto.limit_power_mypower));

    MessageOutput.printlnDebug("DTUinterface:\t GetConfig    - got remote (" + String(getconfigreqdto.request_time) + "):\t" + getTimeStringByTimestamp(getconfigreqdto.request_time));

    if (getconfigreqdto.request_time != 0 && dtuConnection.dtuErrorState == DTU_ERROR_NO_TIME)
    {
        dtuConnection.updateFailed = true;
        inverterData.respTimestamp = uint32_t(getconfigreqdto.request_time);
        MessageOutput.printlnDebug(" --> redundant remote time takeover to local");
    }

    int powerLimit = int(calcValue(getconfigreqdto.limit_power_mypower));

    inverterData.powerLimit = ((powerLimit != 0) ? powerLimit : inverterData.powerLimit);
    inverterData.dtuRssi = getconfigreqdto.wifi_rssi;
    dtuConnection.updateReceived = true;

    if(inverterData.powerLimit > 100){
        inverterData.powerLimit = 100;
    }
    /* There here sees not to work if this variable was never set so 254 = 100%
    if (inverterData.powerLimit != 254)
        inverterData.updateReceived = true;*/
}

boolean DTUInterface::writeReqCommand(uint8_t setPercent)
{
    if (!client->connected())
        return false;
    // prepare powerLimit
    // uint8_t setPercent = dtuGlobalData.powerLimitSet;
    uint16_t limitLevel = setPercent * 10;
    if (limitLevel > 1000)
    { // reducing to 2 % -> 100%
        limitLevel = 1000;
    }
    if (limitLevel < 20)
    {
        limitLevel = 20;
    }

    // request message
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    commandresdto.time = int32_t(inverterData.currentTimestamp);
    commandresdto.action = CMD_ACTION_LIMIT_POWER;
    commandresdto.package_nub = 1;
    commandresdto.tid = int32_t(inverterData.currentTimestamp);

    const int bufferSize = 61;
    char dataArray[bufferSize];
    String dataString = "A:" + String(limitLevel) + ",B:0,C:0\r";
    // Serial.print("\n+++ send limit: " + dataString);
    dataString.toCharArray(dataArray, bufferSize);
    strcpy(commandresdto.data, dataArray);

    bool status = pb_encode(&stream, CommandResDTO_fields, &commandresdto);

    if (!status)
    {
        MessageOutput.println(F("DTUinterface:\t writeReqCommand - failed to encode"));
        return false;
    }

    // Serial.print(F("\nencoded: "));
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        // Serial.printf("%02X", buffer[i]);
        crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48;
    header[1] = 0x4d;
    header[2] = 0xa3;
    header[3] = 0x05; // Command = 0x05
    header[4] = 0x00;
    header[5] = 0x01;
    header[6] = (crc.calc() >> 8) & 0xFF;
    header[7] = (crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; // suggest parentheses around '+' inside '>>' [-Wparentheses]
    header[9] = (stream.bytes_written + 10) & 0xFF;        // warning: suggest parentheses around '+' in operand of '&' [-Wparentheses]
    crc.restart();

    uint8_t message[10 + stream.bytes_written];
    for (int i = 0; i < 10; i++)
    {
        message[i] = header[i];
    }
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        message[i + 10] = buffer[i];
    }

    // Serial.print(F("\nRequest: "));
    // for (int i = 0; i < 10 + stream.bytes_written; i++)
    // {
    //   Serial.print(message[i]);
    // }
    // Serial.println("");

    //     dtuClient.write(message, 10 + stream.bytes_written);
    MessageOutput.printlnDebug("DTUinterface:\t writeReqCommand --- send request to DTU ...");
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_WAIT_COMMAND;
    client->write((const char *)message, 10 + stream.bytes_written);
    //     if (!readRespCommand())
    //         return false;
    return true;
}

boolean DTUInterface::readRespCommand(pb_istream_t istream)
{
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    // CommandReqDTO commandreqdto = CommandReqDTO_init_default;

    // pb_decode(&istream, &GetConfigReqDTO_msg, &commandreqdto);

    // Serial.print(" --> DTUInterface::readRespCommand - got remote: " + getTimeStringByTimestamp(commandreqdto.time));
    // Serial.printf("\ncommand req action: %i", commandreqdto.action);
    // Serial.printf("\ncommand req: %s", commandreqdto.dtu_sn);
    // Serial.printf("\ncommand req: %i", commandreqdto.err_code);
    // Serial.printf("\ncommand req: %i", commandreqdto.package_now);
    // Serial.printf("\ncommand req: %i", int(commandreqdto.tid));
    // Serial.printf("\ncommand req time: %i", commandreqdto.time);
    return true;
}

boolean DTUInterface::writeCommandRestartDevice()
{
    if (!client->connected())
    {
        MessageOutput.println(F("DTUinterface:\t writeCommandRestartDevice - not possible - currently not connect"));
        return false;
    }

    // request message
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    // commandresdto.time = int32_t(locTimeSec);

    commandresdto.action = CMD_ACTION_DTU_REBOOT;
    commandresdto.package_nub = 1;
    commandresdto.tid = int32_t(inverterData.currentTimestamp);

    bool status = pb_encode(&stream, CommandResDTO_fields, &commandresdto);

    if (!status)
    {
        MessageOutput.println(F("DTUinterface:\t writeCommandRestartDevice - failed to encode"));
        return false;
    }

    // Serial.print(F("\nencoded: "));
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        // Serial.printf("%02X", buffer[i]);
        crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48;
    header[1] = 0x4d;
    header[2] = 0x23; // Command = 0x03 - CMD_CLOUD_COMMAND_RES_DTO = b"\x23\x05"   => 0x23
    header[3] = 0x05; // Command = 0x05                                             => 0x05
    header[4] = 0x00;
    header[5] = 0x01;
    header[6] = (crc.calc() >> 8) & 0xFF;
    header[7] = (crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; // suggest parentheses around '+' inside '>>' [-Wparentheses]
    header[9] = (stream.bytes_written + 10) & 0xFF;        // warning: suggest parentheses around '+' in operand of '&' [-Wparentheses]
    crc.restart();

    uint8_t message[10 + stream.bytes_written];
    for (int i = 0; i < 10; i++)
    {
        message[i] = header[i];
    }
    for (unsigned int i = 0; i < stream.bytes_written; i++)
    {
        message[i + 10] = buffer[i];
    }

    // Serial.print(F("\nRequest: "));
    // for (int i = 0; i < 10 + stream.bytes_written; i++)
    // {
    //   Serial.print(message[i]);
    // }
    // Serial.println("");

    MessageOutput.printlnDebug("DTUinterface:\t writeCommandRestartDevice --- send request to DTU ...");
    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_WAIT_RESTARTDEVICE;
    client->write((const char *)message, 10 + stream.bytes_written);

    connectionStatistics.SendRequests++;

    return true;
}

boolean DTUInterface::readRespCommandRestartDevice(pb_istream_t istream)
{
    connectionStatistics.SuccessfulRequests++;

    dtuConnection.dtuTxRxState = DTU_TXRX_STATE_IDLE;
    CommandReqDTO commandreqdto = CommandReqDTO_init_default;

    MessageOutput.print("DTUinterface:\t -readRespCommandRestartDevice - got remote: " + getTimeStringByTimestamp(commandreqdto.time));

    pb_decode(&istream, &GetConfigReqDTO_msg, &commandreqdto);
    MessageOutput.printf("\ncommand req action: %i", commandreqdto.action);
    MessageOutput.printf("\ncommand req: %s", commandreqdto.dtu_sn);
    MessageOutput.printf("\ncommand req: %i", commandreqdto.err_code);
    MessageOutput.printf("\ncommand req: %i", commandreqdto.package_now);
    MessageOutput.printf("\ncommand req: %i", int(commandreqdto.tid));
    MessageOutput.printf("\ncommand req time: %i", commandreqdto.time);
    return true;
}

bool DTUInterface::isSerialValid(const uint64_t serial) const{
    return dtuConnection.dtuSerial == serial;
}

uint64_t DTUInterface::getRedSerial() const {
    return dtuConnection.dtuSerial;
}

void DTUInterface::resetConnectionInfo() {
    disconnect(DTU_STATE_OFFLINE);
    dtuConnection.dtuSerial = 0;//reset red serial
    dtuConnection.statisticsInitialized = false;
    dtuConnection.uptodate = false;
    dtuConnection.statisticsInitialized = false;
    dtuConnection.updateReceived = false;
    dtuConnection.updateFailed = false;
}

bool DTUInterface::statisticsReceived() {
    return dtuConnection.statisticsInitialized;
}

bool DTUInterface::lastRequestFailed() {
    bool failed = dtuConnection.updateFailed;
    dtuConnection.updateFailed = false;
    return failed;
}

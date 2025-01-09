#ifndef DTUINTERFACE_H
#define DTUINTERFACE_H

#include <mutex>

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <AsyncTCP.h>

#include <Ticker.h>

#include "pb_encode.h"
#include "pb_decode.h"
#include "AppGetHistPower.pb.h"
#include "RealtimeDataNew.pb.h"
#include "GetConfig.pb.h"
#include "CommandPB.pb.h"
#include "CRC16.h"
#include "dtuConst.h"
#include "parser/HoymilesWStatisticsParser.h"

#include <Config.h>

#define DTU_TIME_OFFSET 28800

#define DTU_STATE_OFFLINE 0
#define DTU_STATE_CONNECTED 1
#define DTU_STATE_TRY_RECONNECT 3
#define DTU_STATE_DTU_REBOOT 4
#define DTU_STATE_CONNECT_ERROR 5
#define DTU_STATE_STOPPED 6

#define DTU_ERROR_NO_ERROR 0
#define DTU_ERROR_NO_TIME 1
#define DTU_ERROR_TIME_DIFF 2
#define DTU_ERROR_DATA_NO_CHANGE 3
#define DTU_ERROR_LAST_SEND 4

#define DTU_TXRX_STATE_IDLE 0
#define DTU_TXRX_STATE_WAIT_APPGETHISTPOWER 1
#define DTU_TXRX_STATE_WAIT_REALDATANEW 2
#define DTU_TXRX_STATE_WAIT_GETCONFIG 3
#define DTU_TXRX_STATE_WAIT_COMMAND 4
#define DTU_TXRX_STATE_WAIT_RESTARTDEVICE 5
#define DTU_TXRX_STATE_ERROR 99


struct ConnectionControl
{
  boolean dtuConnectionOnline = true;          // true if connection is online as valued a summary
  uint8_t dtuConnectState = DTU_STATE_OFFLINE;
  uint8_t dtuErrorState = DTU_ERROR_NO_ERROR;
  uint8_t dtuTxRxState = DTU_TXRX_STATE_IDLE;
  uint8_t dtuTxRxStateLast = DTU_TXRX_STATE_IDLE;
  unsigned long dtuTxRxStateLastChange = 0;
  uint8_t dtuConnectRetriesShort = 0;
  uint8_t dtuConnectRetriesLong = 0;
  unsigned long pauseStartTime = 0;
};

typedef void (*DataRetrievalCallback)(const char* data, size_t dataSize, void* userContext);

class DTUInterface {
public:
    DTUInterface(const char* server="127.0.0.1", uint16_t port=10081);//TODO find better way to do this
    ~DTUInterface();
   
    void setup();
    void setServer(const char* server);
    void setPort(uint16_t serverPort);
   
    void connect();
    void disconnect(uint8_t tgtState);
    void flushConnection();    

    bool requestDataUpdate();
    bool requestStatisticUpdate();

    void setPowerLimit(int limit);
    void requestRestartDevice();

    void printDataAsTextToSerial();

    bool isSocketConnected();

    std::unique_ptr<InverterData> newDataAvailable();
private:

    ConnectionControl dtuConnection;
    InverterData inverterData;
    std::mutex inverterDataMutex;

    Ticker keepAliveTimer; // Timer to send keep-alive messages
    static void keepAliveStatic(DTUInterface* dtuInterface); // Static method for timer callback
    void keepAlive(); // Method to send keep-alive messages

    Ticker loopTimer; // local loop to handle 
    static void dtuLoopStatic(DTUInterface* instance);
    void dtuLoop();
       
    void onConnect();
    void onDisconnect();
    void onError(int8_t error);
    void onDataReceived(void* data, size_t len);

    void handleError(uint8_t errorState = DTU_ERROR_NO_ERROR);
    void initializeCRC();

    void txrxStateObserver();
    boolean lastOnlineOfflineState = false;
    unsigned long lastOnlineOfflineChange = 0;
    void dtuConnectionObserver();

    void checkingDataUpdate();
    void checkingForLastDataReceived();
        
    // Protobuf functions
    void writeReqAppGetHistPower();
    void readRespAppGetHistPower(pb_istream_t istream);

    void writeReqRealDataNew();
    void readRespRealDataNew(pb_istream_t istream);
    
    void writeReqGetConfig();
    void readRespGetConfig(pb_istream_t istream);
    
    boolean writeReqCommand(uint8_t setPercent);
    boolean readRespCommand(pb_istream_t istream);
    
    boolean writeCommandRestartDevice();
    boolean readRespCommandRestartDevice(pb_istream_t istream);
    
    const char* serverIP;
    uint16_t serverPort;
    AsyncClient* client;

    CRC16 crc;
    
    float gridVoltHist[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t gridVoltCnt = 0;
    unsigned long lastSwOff = 0;

    static float calcValue(int32_t value, int32_t divider = 10);
};

extern DTUInterface dtuInterface;

#endif // DTUINTERFACE_H
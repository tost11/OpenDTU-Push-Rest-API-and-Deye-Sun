/*

 This example connects to an unencrypted Wifi network.

 Then it prints the  MAC address of the Wifi shield,

 the IP address obtained, and other network details.

 Circuit:

 * WiFi shield attached

 created 13 July 2010

 by dlf (Metodo2 srl)

 modified 31 May 2012

 by Tom Igoe

 */
#include <SPI.h>
#include <WiFi.h>
#include <string>
#include <stdexcept>

//        SensorRegisterRange(group="micro", first_reg_address=0x3C, last_reg_address=0x74),

char ssid[] = "wifi_test";     //  your network SSID (name)
char pass[] = "87654321";  // your network password
WiFiClient TCP_client;
const char* TCP_SERVER_ADDR = "192.168.1.100";  // CHANGE TO TCP SERVER'S IP ADDRESS
const int TCP_SERVER_PORT = 8899;
char toSend[1000];
int lenToSend = 0;

std::string hex_to_bytes(const std::string_view &hex) {
    std::size_t i = 0, size = hex.size();
    if (size >= 2 && hex[0] == '0' && hex[1] == 'x')
        i += 2;

    std::string result;
    for (; i + 1 < size; i += 2) {
        char octet_chars[] = { hex[i], hex[i + 1], '\0' };

        char *end;
        unsigned long octet = std::strtoul(octet_chars, &end, 16);
        if (end != octet_chars + 2)
            throw std::runtime_error("error");

        result.push_back(static_cast<char>(octet));
    }

    if (i != size)
        throw std::runtime_error("error");

    return result;
}

unsigned hex_char_to_int( char c ) {
    unsigned result = -1;
    if( ('0' <= c) && (c <= '9') ) {
        result = c - '0';
    }
    else if( ('A' <= c) && (c <= 'F') ) {
        result = 10 + c - 'A';
    }
    else if( ('a' <= c) && (c <= 'f') ) {
        result = 10 + c - 'a';
    }
    else {
        return 0;
    }
    return result;
}

unsigned short modbusCRC16FromHex(const String & message)
{
    const unsigned short generator = 0xA001;
    unsigned short crc = 0xFFFF;
    for(int i = 0; i < message.length(); ++i)
    {
        crc ^= (unsigned short)message[i];
        for(int b = 0; b < 8; ++b)
        {
            if((crc & 1) != 0)
            {
                crc >>= 1;
                crc ^= generator;
            }
            else
                crc >>= 1;

        }
    }
    return crc;
}

static std::string lengthToHexString(int length, int fill)
{
    char res[fill+1];

    auto formater = String("%0") + String(fill).c_str() + "x";
    snprintf(res,fill+1,formater.c_str(),length);

    return std::string(res);
}

std::string modbusCRC16FromASCII(const std::string & input) {

    //Serial.print("Calculating crc for: ");
    //Serial.println(input);

    String hexString;

    for(int i=0;i<input.length();i=i+2){
        unsigned number = hex_char_to_int( input[ i ] ); // most signifcnt nibble
        unsigned lsn = hex_char_to_int( input[ i + 1 ] ); // least signt nibble
        number = (number << 4) + lsn;
        hexString += (char)number;
    }

    unsigned short res = modbusCRC16FromHex(hexString);
    std::string stringRes = lengthToHexString(res,4);

    return std::string()+stringRes[2]+stringRes[3]+stringRes[0]+stringRes[1];
}

#include <string>

std::string string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)
    {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

#include <stdexcept>

int hex_value(unsigned char hex_digit)
{
    static const signed char hex_values[256] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    int value = hex_values[hex_digit];
    if (value == -1) throw std::invalid_argument("invalid hex digit");
    return value;
}

std::string hex_to_string(const std::string& input)
{
    const auto len = input.length();
    if (len & 1) throw std::invalid_argument("odd length");

    std::string output;
    output.reserve(len / 2);
    for (auto it = input.begin(); it != input.end(); )
    {
        int hi = hex_value(*it++);
        int lo = hex_value(*it++);
        output.push_back(hi << 4 | lo);
    }
    return output;
}

int start_register = 60;
int end_register = 116;

void setup() {

  //Initialize serial and wait for port to open:

  Serial.begin(9600);

  while (!Serial) {

    ; // wait for serial port to connect. Needed for native USB port only

  }

  delay(5000);

  Serial.println("test whatever");

  auto str = hex_to_bytes("FF");
  str += hex_to_bytes("00");
  str += hex_to_bytes("01");
  Serial.print("hex Address: ");

  for(int i=0;i<str.length();i++){
      int ia = (int)str[i];
      Serial.print(ia);
      Serial.print(" ");
  }
  Serial.println("");

  std::string start = hex_to_bytes("A5");
  std::string length = hex_to_bytes("1700");
  std::string controlcode = hex_to_bytes("1045");
  std::string serial = hex_to_bytes("0000");
  std::string datafield = hex_to_bytes("020000000000000000000000000000");
  //std::string pos_ini = "003B";
  std::string pos_ini = lengthToHexString(start_register,4);
  std::string pos_fin = lengthToHexString(end_register - start_register + 1,4);
  std::string businessfield = "0103" + pos_ini + pos_fin;
  std::string crc = hex_to_bytes(modbusCRC16FromASCII(businessfield));
  std::string checksum = hex_to_bytes("00");
  std::string endCode = hex_to_bytes("15");
  // check for the presence of the shield:
  //std::string snHex = "F56E3BEA"
  std::string snHex = "EA3B6EF5";
  std::string inverter_sn2 = hex_to_bytes(snHex);

  std::string frame = start + length  + controlcode + serial + inverter_sn2 + datafield + hex_to_bytes(businessfield) + crc + checksum + endCode;
  //std::string frame = start + length + controlcode;

  lenToSend = frame.length();

  // copying the contents of the string to
  // char array
  memcpy(toSend, frame.c_str(),frame.size());
  int check = 0;
  for(int i=1;i<frame.length() - 2;i++){
      check += toSend[i] & 255;
  }
    toSend[frame.length() - 2] = int((check & 255));

  std::string test;
  for(int i=0;i<frame.length();i++){
      test += toSend[i];
  }

  for(int i=0;i<test.length();i++){
      Serial.println((int)test[i]);
  }

    /*Serial.print("Size is: ");
  Serial.println(hString.size());

  Serial.print("Hex string is: ");
  Serial.println(hString.c_str());*/

  if (WiFi.status() == WL_NO_SHIELD) {

    Serial.println("WiFi shield not present");

    // don't continue:

    while (true);

  }

  // attempt to connect to Wifi network:

  while (WiFi.status() != WL_CONNECTED) {


    Serial.print("Attempting to connect to WPA SSID: ");

    Serial.println(ssid);

    Serial.print("Status: ");

    Serial.println(WiFi.status());

    // Connect to WPA/WPA2 network:

    WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:

    delay(10000);

  }

  // you're connected now, so print out the data:

  Serial.print("You're connected to the network");

  printCurrentNet();

  printWifiData();
}

void printValue(std::string name,char * arr,int pos, float multiplier){
    int realPos = (pos - start_register + 1) * 2;
    //Serial.printf("Starting on real possition %d\n",realPos);
    char end[2];
    end[0] = arr[realPos + 1];
    end[1] = arr[realPos];
    int16_t res;
    memcpy(&res,end,2);
    Serial.printf((std::string() + name.c_str()+" is: %f\n").c_str(), ((float)res)*multiplier);
}

long lastChecked = 0;
long lastSend = 0;
char res[1000];

void loop() {

  // check the network connection once every 10 seconds:

  delay(100);

  if(millis() - lastChecked > 10000){
      printCurrentNet();
      lastChecked = millis();
  }

  // Read data from server and print them to Serial
  int i=0;

  if(TCP_client.available()) {
      delay(100);
  }

  while(TCP_client.available()) {
    if(i++ <= 0){
      Serial.print("New Data Available: ");
    }
    char c = TCP_client.read();
    if(i>=27){
        res[i-27]=c;
    }
    Serial.print(c);
  }
  if(i >= 27){
    i = i - 27;
    Serial.println("");
    for(int j=0;j<i;j++) {
        Serial.printf("read: %d bzw %d hex: %x\n",j, j/2 + start_register + 1,res[j]);
    }
    Serial.println("");
    /*for(int j=0;j<i;j = j+2){
        unsigned number = hex_char_to_int( res[j] ); // most signifcnt nibble
        unsigned lsn = hex_char_to_int( res[j + 1] ); // least signt nibble
        number = (number << 4) + lsn;

        Serial.printf("Index: %d hex: %x int: %d uint: %u\n", j,j+59+20,number,number);
    }*/

    // start is: 59

    for(int j=0;j<i;j++){
        Serial.printf("%x",res[j]);
    }
    Serial.println("");

    printValue("Daily Production",res,60,0.1f);
    printValue("Total Production 1",res,63,0.1f);
    printValue("Total Production 2",res,64,0.1f);
    printValue("Grid Voltage",res,73,0.1f);
    printValue("Grid Ampere",res,76,0.1f);
    printValue("Grid Frequenz",res,79,0.01f);
    printValue("Uptime Minutes",res,62,1.f);

    printValue("PV1 Voltage",res,109,0.1f);
    printValue("PV1 Ampere",res,110,0.1f);
    printValue("PV1 Daily Production",res,65,0.1f);
    printValue("PV1 Total Production 1",res,69,0.1f);
    printValue("PV1 Total Production 2",res,70,0.1f);

    printValue("PV2 Voltage",res,111,0.1f);
    printValue("PV2 Ampere",res,112,0.1f);
    printValue("PV2 Daily Production",res,66,0.1f);
    printValue("PV2 Total Production 1",res,71,0.1f);
    printValue("PV2 Total Production 2",res,72,0.1f);

    printValue("PV3 Voltage",res,113,0.1f);
    printValue("PV3 Ampere",res,114,0.1f);
    printValue("PV3 Daily Production",res,67,0.1f);
    printValue("PV3 Total Production 1",res,74,0.1f);
    printValue("PV3 Total Production 2",res,75,0.1f);

    printValue("PV4 Voltage",res,115,0.1f);
    printValue("PV4 Ampere",res,116,0.1f);
    printValue("PV4 Daily Production",res,68,0.1f);
    printValue("PV4 Total Production 1",res,77,0.1f);
    printValue("PV4 Total Production 2",res,78,0.1f);

    printValue("Operating Power",res,80,0.1f);
    printValue("AC Active Power 1",res,86,0.1f);
    printValue("AC Active Power 2",res,87,0.1f);
    printValue("Temperature",res,90,0.01f);
  }

  if (!TCP_client.connected()) {
    Serial.println("Connection is disconnected");
    TCP_client.stop();

    // reconnect to TCP server
    if (TCP_client.connect(TCP_SERVER_ADDR, TCP_SERVER_PORT)) {
      Serial.println("Reconnected to TCP server");
      TCP_client.write(toSend,lenToSend);  // send to TCP Server
      TCP_client.flush();
      lastSend = millis();
    } else {
      Serial.println("Failed to reconnect to TCP server");
      delay(1000);
    }
  }else{
    if(millis() - lastSend > 20000){
      Serial.println("Send new fetch data request");
      TCP_client.write(toSend,lenToSend);  // send to TCP Server
      TCP_client.flush();
      lastSend = millis();
    }
  }
}

void printWifiData() {

  // print your WiFi shield's IP address:

  IPAddress ip = WiFi.localIP();

  Serial.print("IP Address: ");

  Serial.println(ip);

  Serial.println(ip);

  // print your MAC address:

  byte mac[6];

  WiFi.macAddress(mac);

  Serial.print("MAC address: ");

  Serial.print(mac[5], HEX);

  Serial.print(":");

  Serial.print(mac[4], HEX);

  Serial.print(":");

  Serial.print(mac[3], HEX);

  Serial.print(":");

  Serial.print(mac[2], HEX);

  Serial.print(":");

  Serial.print(mac[1], HEX);

  Serial.print(":");

  Serial.println(mac[0], HEX);

}

void printCurrentNet() {

  // print the SSID of the network you're attached to:

  Serial.print("SSID: ");

  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:

  byte bssid[6];

  WiFi.BSSID(bssid);

  Serial.print("BSSID: ");

  Serial.print(bssid[5], HEX);

  Serial.print(":");

  Serial.print(bssid[4], HEX);

  Serial.print(":");

  Serial.print(bssid[3], HEX);

  Serial.print(":");

  Serial.print(bssid[2], HEX);

  Serial.print(":");

  Serial.print(bssid[1], HEX);

  Serial.print(":");

  Serial.println(bssid[0], HEX);

  // print the received signal strength:

  long rssi = WiFi.RSSI();

  Serial.print("signal strength (RSSI):");

  Serial.println(rssi);

  // print the encryption type:

  Serial.print("Encryption Type:");

  Serial.println(HEX);

  Serial.println();
}
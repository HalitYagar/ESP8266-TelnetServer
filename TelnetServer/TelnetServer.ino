/*
* Halit Yagar
* halityagar45@gmail.com
* Telnet server for ESP8266
* v1.0.1
*/
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <String.h>

#define MAX_STR 512
#define MAX_CLIENTS 4
#define PARAM 4 // message parameter number

//char ssid[MAX_STR] = "\0";
//char pass[MAX_STR] = "\0";

//char* ssid = "SUPERONLINE-WiFi_9956";
//char* pass = "J3CCTPAXAAV4";

char* ssid = "Aloha";
char* pass = "rZMhaXXm";

enum commands {
  scan = 1, connectWifi, password, dataSend, dataRecieve, clientConnect, restart, portNum
};

int connections = 0;
unsigned long startTime = millis();
boolean waitForPass = false;

// provide text for the WiFi status
const char *str_status[]= {
  "WL_IDLE_STATUS",
  "WL_NO_SSID_AVAIL",
  "WL_SCAN_COMPLETED",
  "WL_CONNECTED",
  "WL_CONNECT_FAILED",
  "WL_CONNECTION_LOST",
  "WL_DISCONNECTED"
};

// provide text for the WiFi mode
const char *str_mode[]= { "WIFI_OFF", "WIFI_STA", "WIFI_AP", "WIFI_AP_STA" };

WiFiClient client; // wifi client for getting public ip from ipfy.org

int portNumber = 23; // default port number
WiFiClient serverClients[MAX_CLIENTS];
boolean cntClients[MAX_CLIENTS]; // client connect control

void getMessage(WiFiServer &telnetServer); // get message from USER
void scanWifi(); // scan available networks
void connectToWifi(char *, char *, WiFiServer &telnetServer); // connect specific network
void checkWifiStatus(WiFiServer &telnetServer); // control Wifi Status and reconnect if necessary
void checkClients(WiFiServer &telnetServer); // check clients if still connected
void getClientData(); // get data from client
void clientDataSend(int, char*); // send data to client
void readStringUSR(char *); // read string from USER

int start = millis();
void setup() {
  delay(1000);
  Serial.begin(115200);
  EEPROM.begin(512);
  Serial.setDebugOutput(false);
  delay(1000);
  uint8_t byteHigh, byteLow, cntEEPROM;
  cntEEPROM = EEPROM.read(0);
  if(cntEEPROM == 1){
    byteHigh = EEPROM.read(1);
    byteLow = EEPROM.read(2);
    portNumber = byteHigh<<8 | byteLow;
  }
  Serial.printf("*1.0.1,%d,%d,0!\n",ESP.getChipId(), portNumber); // send opening mesage
  WiFi.mode(WIFI_STA); // set wifi mode as station
}

void loop() {
  static WiFiServer telnetServer(portNumber);
  getMessage(telnetServer);
  checkClients(telnetServer);
  if(millis() - start > 1000){ // check wifi status every 1 second
    checkWifiStatus(telnetServer);
    start = millis();
  }
  getClientData();
  delay(10); // to avoid strange characters left in buffer
}
  
void getMessage(WiFiServer &telnetServer){
  char message[MAX_STR];
  char parseMsg[PARAM][MAX_STR/PARAM];
  readStringUSR(message);
  int len = strlen(message)-1; // get length but ignore end character '\0'
  
  if(strlen(message) != 0 && (message[0] != '#' || message[len] != '!')){ 
    //Serial.printf("%s\n", message); // what is message
    Serial.printf("*0,0,0,0!\n"); // invalid message
  } else if(message[0] == '#' && message[len] == '!'){ // valid message
    message[len] = '\0';
    len = strlen(message);
    int count = 0;
    char* token;
    token = strtok(message+1, ","); // strtok parse strings with given parameter
    while(token != NULL){ // parse message as parameters
      strcpy(parseMsg[count++], token);
      token = strtok(NULL, ",");
    }
    int wifiNum = -1;
    pass[0] = '\0';
    int num = atoi(parseMsg[0]);
    switch(num){
      case scan:
        ssid[0] = '\0'; 
        scanWifi();
      break;
      case connectWifi:
        wifiNum = atoi(parseMsg[1])-1;
        if(wifiNum > connections-1){
           Serial.printf("*02,2,0,0!\n"); // out of index
        } else {
          strcpy(ssid, WiFi.SSID(wifiNum).c_str());
          if(WiFi.encryptionType(wifiNum) == ENC_TYPE_NONE){
            connectToWifi(ssid, "\0", telnetServer);
          } else {
            Serial.printf("*02,3,0,0!\n"); // need pasword
            waitForPass = true;
          }
        }
      break;
      case password:
        waitForPass = false;
        strcpy(pass, parseMsg[1]);
        connectToWifi(ssid, pass, telnetServer);
      break;
      case dataSend:
        clientDataSend(atoi(parseMsg[1]), parseMsg[2]);
      break;
      case restart:
      Serial.printf("*07,0,0,0!\n"); // restarting
      delay(100);
      ESP.restart();
      break;
      case portNum:
        uint16_t port;
        uint8_t byteHigh, byteLow;
        port = atoi(parseMsg[1]);
        byteHigh = port>>8;
        byteLow = port&0xFF;
        EEPROM.write(0,1);
        EEPROM.write(1,byteHigh);
        EEPROM.write(2,byteLow);
        bool cnt = EEPROM.commit();
        if(cnt == true){ // writing port number succesful
          Serial.printf("*08,0,%d,0!\n", byteHigh<<8 | byteLow);
        } else { // write failed
          Serial.printf("*08,1,0,0!\n");
        } 
      break;
    }
  }
}

void scanWifi(){
  connections = WiFi.scanNetworks(); // return the number of networks found
  if(connections != 0){
    for(int i = 0; i < connections; ++i){
      // Send response to the user
      char ssid[MAX_STR];
      char ch = (WiFi.encryptionType(i) == ENC_TYPE_NONE)?'0':'1';
      strcpy(ssid, WiFi.SSID(i).c_str());
      Serial.printf("*0%d,%d,%s,%c!\n", scan, i+1, ssid, ch);
      delay(10);
    }
  }
  Serial.printf("*0%d,0,0,0!\n", scan); // scan ends
}

void connectToWifi(char* ssid, char *password, WiFiServer &telnetServer){ // wifi handling
  if(waitForPass == true){
    return; // wait for pass when network is selected
  }
  WiFi.disconnect(); // disconnect previous connection
  delay(10);
  if(strlen(ssid) != 0 && strlen(password) != 0){
    WiFi.begin(ssid, password);
  } else if(strlen(ssid) != 0){
    WiFi.begin(ssid);
  } else {
    return ;
  }
  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startTime < 10000){
    delay(500);
  }
  if(WiFi.status() == WL_CONNECTED){ // Check connection
    char str[MAX_STR], publicIP[MAX_STR], ch;
    if (client.connect("api.ipify.org", 80)) {
      client.println("GET / HTTP/1.1");
      client.println("Host: api.ipify.org");
      client.println();
      delay(10);
      int len = 0;
      while(client.available() > 0){
        ch = client.read();
        str[len++] = ch;
      }
      str[len++] = '\0';
      int count = 0;
      char* token;
      token = strtok(str, "\n\r\0"); // strtok parse strings with given parameter
      while(token != NULL){ // parse message as parameters
        strcpy(publicIP, token);
        token = strtok(NULL, "\n\r\0");
      }
    } else {
      Serial.printf("*02,4,0,0!\n"); // there is no internet on this network
      return;
    }
    Serial.printf("*02,0,"); // wifi connected
    Serial.print(WiFi.localIP());
    Serial.printf(",");
    Serial.print(publicIP);
    Serial.printf("!\n");
    delay(10);
    telnetServer.begin();//start the telnet server
    telnetServer.setNoDelay(true); // it is about telnet protocol
    for(int i = 0 ; i < MAX_CLIENTS ; i++){ // set all clients as not connected
      cntClients[i] = false;
    }
  } else if(WiFi.status() == WL_CONNECT_FAILED){
    Serial.printf("*03,1,0,0!\n"); // re-enter password
  }
}

void checkWifiStatus(WiFiServer &telnetServer){ // check if Wifi still connected
  if(WiFi.status() != WL_CONNECTED && strlen(ssid) != 0 && waitForPass == false){
    Serial.printf("*02,1,0,0!\n"); // wifi disconnected
    connectToWifi(ssid, pass, telnetServer);
  }
}
void checkClients(WiFiServer &telnetServer){ // check clients connection
  if (telnetServer.hasClient()) { // check if there any new clients 
    bool tmp = false;
    for(int i = 0 ; i < MAX_CLIENTS ; i++){ // check for free/disconnected spot
      if(!serverClients[i] || !serverClients[i].connected()){
        //if (serverClients[i])serverClients[i].stop();
        serverClients[i] = telnetServer.available();
        cntClients[i] = true;
        tmp = true;
        serverClients[i].flush();  // clear input buffer, else you get strange characters
        Serial.printf("*06,%d,0,0!\n", i); // client connected
        break;
      }
    }
    if(tmp == false){ // there is no free/disconnected spots
      WiFiClient serverClient = telnetServer.available();
      serverClient.stop();
      Serial.printf("*06,%d,2,0!\n", MAX_CLIENTS); // server full
    }
  }
  for(int i = 0 ; i < MAX_CLIENTS ; i++){ // check for disconnected clients
    if(serverClients[i]){ // is clients connected before
      if(!serverClients[i].connected() && cntClients[i] == true){ // check clients is still connected
        cntClients[i] = false;
        serverClients[i].stop();
        Serial.printf("*06,%d,1,0!\n", i); // client gone
      }
    }
  }
}

void getClientData(){
  for(int i = 0 ; i < MAX_CLIENTS ; i++){ // check for free/disconnected spot
    if(serverClients[i].available()){
      delay(10);
      char str[MAX_STR], ch;
      int len = 0;
      while(serverClients[i].available() > 0){
        ch = serverClients[i].read();
        str[len] = ch;
        len++; 
      }
      str[len-1] = '\0'; // add a null character for strlen function
      Serial.printf("*05,%d,%s,0!\n", i, str); // get data from client
    }
  }
}
void clientDataSend(int clientNum, char* incomArr){ // post data to clients
  int len = strlen(incomArr);
  if(clientNum < 0 || clientNum >= MAX_CLIENTS){ 
    Serial.printf("*04,2,0,0!\n"); // there is no such client
  } else if(!serverClients[clientNum] || !serverClients[clientNum].connected()){
    Serial.printf("*04,1,0,0!\n"); // client is offline
  } else {
    for(int i = 0 ; i < len ; i++){// send data to client
      serverClients[clientNum].print(incomArr[i]);
    }
    serverClients[clientNum].println("");
    Serial.printf("*04,0,0,0!\n"); // data sent
  }
}

void readStringUSR(char* str){ // read string from user(serial)
  int len = 0;
  char ch;
  while(Serial.available() > 0){
    ch = Serial.read();
    str[len] = ch;
    len++; 
  }
  str[len] = '\0'; // add a null character for strlen function
}


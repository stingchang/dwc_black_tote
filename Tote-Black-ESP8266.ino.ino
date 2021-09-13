#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#ifndef STASSID
#define STASSID "***"
#define STAPSK  "***"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D5

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

const int coolerSwitch = D3;

bool isAutoOn = true;
bool isCoolerOn = false;
float waterTempC = 0;
float waterTempHigh = 23;
float waterTempLow = 17;

void handleRoot() {
  String msg = "";
  msg = msg + "Set temperature idea range between 14 to 30 \n example: localhost/?low=16&high=22&auto=on , \n example: localhost/?auto=off\n\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    String param = server.argName(i);
    String paramVal = server.arg(i);
    if( param == "auto") {
      msg = msg+"Set auto ";
      if(paramVal == "off")  {
        isAutoOn = false;
        msg = msg+"to OFF\n\n";
      } else if(paramVal == "on"){
        isAutoOn = true;
        msg = msg+"to ON\n\n";
      } else {
        msg = msg+"failed\n\n";
      }
      
    }
    
    int newHigh ;
    int newLow ;
    if( param == "high") {
       newHigh = paramVal.toInt();
      if(newHigh<=30 && newHigh>14 && newHigh>waterTempLow) {
        waterTempHigh = newHigh;
        msg = msg+"Set auto range high to "+String(waterTempHigh)+ " 'C\n\n";
      }else {
        msg = msg+"Set auto range high failed\n\n"; 
      } 
    }
    if( param == "low") {
       newLow = paramVal.toInt();
      if(newLow<waterTempHigh && newLow>=14){
        waterTempLow = newLow;
        msg = msg+"Set auto range low to "+String(waterTempLow)+ " 'C\n\n";
      }else {
        msg = msg+"Set auto range low failed\n\n";  
      } 
    } 

    
    updateTempControl();
  }
  
  msg = msg+"Temp auto on = " + String(isAutoOn? "ON":"OFF") +"\n";
  
  msg = msg+"Temperature = " + String(waterTempC) +" 'C\n";
  msg = msg+"Idea Range = " + String(waterTempLow) +" ~ " + String(waterTempHigh) +" 'C\n";
  msg = msg+"Cooler is  " + String(isCoolerOn? "ON":"OFF") +"\n";
  
//  Serial.println(msg);
  server.send(200, "text/plain", msg);
  
}

void handleNotFound() {
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
}
bool isTempTooHigh() {
  return waterTempC >= waterTempHigh;
}
void turnOnCooler() {
  digitalWrite(coolerSwitch, HIGH);
  isCoolerOn = true;
}
bool isTempTooLow() {
  return waterTempC <= waterTempLow;
}
void turnOffCooler() {
  digitalWrite(coolerSwitch, LOW);
  isCoolerOn = false;
}

void setup(void) {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  
  pinMode(coolerSwitch, OUTPUT);
  updateTempControl();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void updateTempControl(){
  sensors.requestTemperatures(); // Send the command to get temperatures
  waterTempC = sensors.getTempCByIndex(0);

  String msg;
  // Check if reading was successful
  if(waterTempC != DEVICE_DISCONNECTED_C) 
  {
    if(isAutoOn) {
      if(!isCoolerOn && isTempTooHigh()) {
        turnOnCooler();
        isCoolerOn = true;
        msg = msg+"Turn on cooler\n";
      }
      else if(isCoolerOn && isTempTooLow()) {
        turnOffCooler();
        isCoolerOn = false;
        msg = msg+"Turn off cooler\n";
      }      
    } else if(isCoolerOn) {
      turnOffCooler();
      isCoolerOn = false;
      msg = msg+"Turn off cooler\n";
    }
    
    msg = msg+"Temp auto on = " + String(isAutoOn) +"\n";
    
    msg = msg+"Temperature = " + String(waterTempC) +"'C\n";
    msg = msg+"Temp low = " + String(waterTempLow) +"\n";
    msg = msg+"Temp high = " + String(waterTempHigh) +"\n";
    msg = msg+"Cooler is " + String(isCoolerOn? "ON":"OFF") +"\n";
    Serial.println(msg);
  } 
  else
  {
    Serial.println("Error: Could not read temperature data");
  }
}

void loop(void) {
  server.handleClient();
  MDNS.update();


  static long watingPeriod = 10 * 60* 1000;
  static long lastCheckTime = millis() - watingPeriod + 10 * 1000;
  if(millis() > lastCheckTime + watingPeriod) {
    Serial.println("Requesting temperatures...");
    delay(2000);
    updateTempControl();
    Serial.println("DONE");
    lastCheckTime = millis();
  }
  
  
}

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>
#include <FS.h>
#include <Wire.h>

#define pin1 D1                     // Relay pin
#define LED_BUILTIN 2               // LED   pin

char ssid[] = "Shreyas#2";          //  your network SSID (name)
char pass[] = "prabhushrsha";       // your network password
int  status = WL_IDLE_STATUS;
WiFiClient    client;
int  loopCnt = 0;                   // loopCnt will give number of times ON/OFF
int  onTime  = 1;//60000;               // 1  minutes
int  offTime = 1;//3300000;             // 55 minutes
int  dripOn  = 1;                   // presently drip is ON

AsyncWebServer server(80);

const char* PARAM_ON_TIME  = "OnTime";
const char* PARAM_OFF_TIME = "OffTime";

String getCounter() {
  return String(loopCnt);
}
String getDripState() {
  String dripState;
  if(dripOn){
        dripState = "ON";
      }
      else{
        dripState = "OFF";
      }
      return  dripState;
}

// HTML web page to handle 3 input fields (inputString, inputInt, inputFloat)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Drip Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" type="text/css" href="style.css">
<head>  
<body>
  <h1>Drip Controller </h1>
  <p>
    <span class="sensor-labels">Drip State</span>
    <span id="dripState">%dripState%</span>
  <p>
    <span class="sensor-labels">Loop Counter</span>
    <span id="loopCnt">%loopCnt%</span>
  </p>
</body>
  <script>

    setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("dripState").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/dripState", true);
    xhttp.send();
  }, 10000 ) ;
 
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("loopCnt").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/loopCnt", true);
    xhttp.send();
  }, 10000 ) ;
 
  </script></head><body>
  <form action="/get" target="hidden-form">
    OnTime(min) (current value %inputOnTime%): <input type="number " name="OnTime">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    OffTime(min) (current value %inputOffTime%): <input type="number " name="OffTime">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}
// Replaces placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "inputOnTime"){
    return readFile(SPIFFS, "/inputOnTime.txt");
  }
  else if(var == "inputOffTime"){
    return readFile(SPIFFS, "/inputOffTime.txt");
  }
  else if (var == "loopCnt"){
    return getCounter();
  }
  else  if(var == "dripState"){
    return getDripState();
  }
  return String();
}

void setup() {
  pinMode(pin1, OUTPUT);            // set relay pin to output
  pinMode(LED_BUILTIN, OUTPUT);     // set LED   pin to output
  Serial.begin(9600);               // serial baud 9600

/******* Network Configuration *********/
  // Set your Static IP address
  IPAddress local_IP(192, 168, 0, 110);
  // Set your Gateway IP address
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);           // initialise wifi
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
/******** Network Configuration Ends *******/
// Initialise Flash Disk
    if(!SPIFFS.begin()){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
 
  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
    // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to set DripState
  server.on("/dripState", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getDripState().c_str());
  });

  server.on("/loopCnt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getCounter().c_str());
  });
 
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    if (request->hasParam(PARAM_ON_TIME)) {
      inputMessage = request->getParam(PARAM_ON_TIME)->value();
      writeFile(SPIFFS, "/inputOnTime.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_OFF_TIME)) {
      inputMessage = request->getParam(PARAM_OFF_TIME)->value();
      writeFile(SPIFFS, "/inputOffTime.txt", inputMessage.c_str());
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });
  server.onNotFound(notFound);
  server.begin();
}

void loop() {
  loopCnt++;

  // put your main code here, to run repeatedly:
   Serial.print("Switching ON :: Loopt count is ");
   Serial.print(loopCnt);
   Serial.print(" :: onTime is ");
   Serial.println(onTime);
   
   digitalWrite(pin1, HIGH);
   digitalWrite(LED_BUILTIN, LOW);
   dripOn = 1;
   delay(onTime*60000);

   Serial.println("Swithcing OFF");
   Serial.print(" :: offTime is ");
   Serial.println(offTime);
   digitalWrite(pin1, LOW);
   digitalWrite(LED_BUILTIN, HIGH);
   dripOn = 0;
   delay(offTime*60000);

  // To access your stored values of onTime and offTime
  onTime  = readFile(SPIFFS, "/inputOnTime.txt").toInt();
  offTime = readFile(SPIFFS, "/inputOffTime.txt").toInt();
}

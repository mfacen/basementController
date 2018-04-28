 #include <DHT.h>
#include <DHT_U.h>                ///esta es una prueba de GIT pruebo de nuevo !!!!!
#include <Servo.h>

#include <PID_v1.h>

#include <OneWire.h>
#include <DallasTemperature.h>                        //  !!!!!!!!!!!!!!!    HE LEIDO QUE EL PROBLEMA QUE NO FUNCIONA BIEN EN ALGUNOS BROWSERS....
#include <ESP8266WiFi.h>                               //   ES QUE EL ESP NO PUEDE SERVIRE VARIOS ARCHIVOS A LA VEZ, EL BROWSER PIDE TODOS LOS ARCHIVOS 
#include <ESP8266WiFiMulti.h>                          // Y EL ESP SE QUEDA PENSANDO. HAY QUE MANDAR TODO EL HTML DE UNA VEZ. tengo que probar con una pagina simple.
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>                 
#include <ESP8266mDNS.h>
#include <FS.h>
#define ONE_HOUR 3600000UL
#include <time.h>
//#include <PersWiFiManager.h>

#define TEMP_SENSOR_PIN D2
#define TEMP_EXT_SENSOR_PIN D2
const int pinServo = D7;
const int pinFan = D8;
//const char nl = char(13);
OneWire oneWire(TEMP_EXT_SENSOR_PIN);        // Set up a OneWire instance to communicate with OneWire devices

DallasTemperature tempSensors(&oneWire); // Create an instance of the temperature sensor class
//    DHT dht(TEMP_SENSOR_PIN, DHT11,11);

ESP8266WebServer server (80);       // create a web server on port 80

File fsUploadFile;                                    // a File variable to temporarily store the received file

ESP8266WiFiMulti wifiMulti;    // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const char *OTAName = "ESP8266";         // A name and a password for the OTA service
//const char *OTAPassword = "esp8266";

const char* mdnsName = "esp8266";        // Domain name for the mDNS responder

WiFiUDP UDP;                   // Create an instance of the WiFiUDP class to send and receive UDP messages

IPAddress timeServerIP;        // The time.nist.gov NTP server's IP address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message

const String nl = "/nl";
byte packetBuffer[NTP_PACKET_SIZE];      // A buffer to hold incoming and outgoing packets

float temperature = 0;
float humidity = 0;
float extTemperature = 0;
const int led = D0;
// variables para PID
double Setpoint, Input, Output;
int servoPos = 0; // temp to try out the servo
int fanCount = 0 ; // counts ticks to turn fan on/off like PWM
float fanSpeed;
float lastFanSpeed;             // Keeps track of when the fan speed changes to record it in the log
int minVentilation = 60;     // Minutes of cycle fan ventilation.
int minFanOn = 5;               // Minutes ventilation Fan is on.
int minLogging = 10;          // File logger of parameters
int ventilationLevel = 5;     // Speed of Fan on Ventilation
bool firstRun = true;
//Specify the links and initial tuning parameters
double Kp = 1, Ki = 3, Kd = 0.2;
String udpUrl = "192.168.1.255";
PID myPID (&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);
Servo servoIntake;
//##########################################################################################################################
/*__________________________________________________________SETUP__________________________________________________________*/
//##########################################################################################################################


void setup() {
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");
  //  dht.begin();
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 180); // 180 grados para el servo
  Setpoint = 27;
  servoIntake.attach(pinServo);  // attaches the servo on pin 2 to the servo object

  tempSensors.setWaitForConversion(false); // Don't block the program while the temperature sensor is reading
  tempSensors.begin();                     // Start the temperature sensor
  
  pinMode ( pinFan , OUTPUT );
  
  if (tempSensors.getDeviceCount() == 0) {
    Serial.printf("No DS18x20 temperature sensor found on pin %d. Rebooting.\r\n", TEMP_SENSOR_PIN);
    Serial.flush();
     ESP.reset();
  }
  //Wifi.mode(WIFI_STA);
  
  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();                  // Start the OTA service

  startSPIFFS();               // Start the SPIFFS and list all contents

  startMDNS();                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler and an upload handler

  startUDP();                  // Start listening for UDP messages to port 123

  WiFi.hostByName(ntpServerName, timeServerIP); // Get the IP address of the NTP server
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  delay(500);
}

//##########################################################################################################################
/*__________________________________________________________LOOP__________________________________________________________*/
//##########################################################################################################################


const unsigned long intervalNTP = ONE_HOUR; // Update the time every hour
unsigned long prevNTP = 0;
unsigned long prevLogging = millis();
unsigned long lastNTPResponse = millis();
unsigned long lastPIDCalculation = millis();
unsigned long lastFanOn = millis();
boolean ventilationOn = false;

const unsigned long intervalTemp = 60000;   // Do a temperature measurement every 10 minutes

const unsigned long intervalPID = 5000;   //Pid Interval
unsigned long prevTemp = 0;
unsigned long prevPwmMillis = 0;
bool tmpRequested = false;
const unsigned long DS_delay = 750;         // Reading the temperature from the DS18x20 can take up to 750ms

uint32_t timeUNIX = 0;                      // The most recent timestamp received from the time server

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // Request the time from the time server every hour
    prevNTP = currentMillis;
    sendNTPpacket(timeServerIP);
  }

  uint32_t time = getTime();                   // Check if the time server has responded, if so, get the UNIX time
  if (time) {
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = millis();
  } else if ((millis() - lastNTPResponse) > 24UL * ONE_HOUR) {
    Serial.println("More than 24 hours since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

    

    
  if (currentMillis - lastPIDCalculation > intervalPID ) {  // Every 5 seconds , compute PID , send UDP data
    lastPIDCalculation = currentMillis;
    Input = temperature;
    myPID.Compute();   //  result is in variable named "Output"
    
    ventilationOn ? servoIntake.write(0) : servoIntake.write(180 - Output);   // Esto es nuevo, si la ventilacion esta encendida abre el servo intake. 
    Serial.println( "PID computed, output = "+String(Output));
    UDP.beginPacket( udpUrl.c_str() , 9999);
    //UDP.write("Temperatura: ");
    UDP.write(temperature);
    UDP.write(extTemperature);
    UDP.write(fanSpeed);
    UDP.write((byte) Setpoint);
    UDP.write((byte) Output);
    UDP.write(digitalRead(pinFan));
    UDP.write(char(13));
    UDP.print(WiFi.localIP());
    UDP.write(char(13));
    UDP.endPacket();
    udpLog("Sent via UDP port 9998-- "+String(temperature));
    if (!ventilationOn){
    fanSpeed =  map  ( temperature , Setpoint + 1 ,  Setpoint + 3 , 0 ,  10 );  // mapea la diferencia de 1 grados sobre el setting hasta 3 grados sobre el setting en un numero entre 0 y 10;
    fanSpeed = constrain ( fanSpeed , 0 , 10 ) ; 
    if (fanSpeed<2) fanSpeed = 2; // en uno el venti no anda casi nada
    }
    
    if ( lastFanSpeed != fanSpeed ) {           // Si la velocidad del ventilador cambia entonces guardar datos en archivo.
      saveData();
      lastFanSpeed = fanSpeed;    
    }
  }



  
  if ( (currentMillis - prevTemp > intervalTemp) || ( prevTemp == 0   ) ) {  // Every  minute, request the temperature 
      tempSensors.requestTemperatures(); // Request the temperature from the sensor (it takes some time to read it)
      tmpRequested = true;
      prevTemp = currentMillis;
      Serial.println("Temperature requested");
      
         if ( timeUNIX == 0 )  {                                    // If we didn't receive an NTP response yet, send another request
          sendNTPpacket(timeServerIP);
            delay(500);
    
        }
    }


  if ( (currentMillis - prevTemp > DS_delay && tmpRequested)  || (firstRun == true && currentMillis > 750 ))  { // 750 ms after requesting the temperature
      firstRun = false;
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      // The actual time is the last NTP time plus the time that has elapsed since the last NTP response
      tmpRequested = false;
      //float temp = dht.readTemperature();// =
      float temp = tempSensors.getTempCByIndex(1); // Get the temperature from the sensor
      float tempExterior =  tempSensors.getTempCByIndex(0);
      float hum = 0;//float hum = dht.readHumidity();

      if ( temp > -10 && temp < 50 ) temperature = temp; // if the reading is not a number ignore it.
      if ( tempExterior > -10 && tempExterior < 50 ) extTemperature = tempExterior; // if the reading is not a number ignore it.
      if ( hum >= 0 && hum <= 100 ) humidity = hum; // if the reading is not a number ignore it.
      temperature = round(temperature * 100.0) / 100.0; // round temperature to 2 digits
      humidity = round(humidity * 100.0) / 100.0; // round humidity to 2 digits
    }

  if (  (currentMillis - prevLogging > (minLogging*60000)) && (timeUNIX != 0) )  {            //// Atencion ahora si no funciona el servidor del tiempo no ejecuta nada !!!!
        prevLogging= currentMillis;
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      Serial.printf("Appending temperature to file: %lu,", actualTime);
      Serial.println(temperature);
      File tempLog ;
      tempLog = SPIFFS.open("/dataLog.csv", "a"); // Write the time and the temperature to the csv file
      tempLog.print(actualTime);
      tempLog.print(',');
      tempLog.print(temperature);
      tempLog.print(',');
      tempLog.print(extTemperature);
      tempLog.print(',');
      tempLog.print(fanSpeed);
      tempLog.print(',');
      tempLog.println(Output / 18 + 20); // output goes from 20 ( 0 ) to 38 ( 180 )
      
      tempLog.close();
      

  }
    
 
  
  

  if ( currentMillis - ( 1500 ) > prevPwmMillis ) {  // cada segundo resetear registro ultimo prevPwmMillis
    prevPwmMillis = millis() ;
    }
   

  if ( ( ( currentMillis - ( fanSpeed * 150 ) > prevPwmMillis )  || fanSpeed == 0 ) && fanSpeed != 10 )  {   //  si el registro paso mas que 1/10 segundos * fanPWM desde el reset apagar fan
    digitalWrite (pinFan,LOW);
  }
  else{
        digitalWrite (pinFan,HIGH);   // si todavia no paso el tiempo fan ON.
        if ( !ventilationOn ) {lastFanOn = currentMillis;}      // solo updatear esto si no esta en modo ventilacion ( cada hora 5 minutos  ) 
  }

  if ( currentMillis  > lastFanOn + (60000 * minVentilation ) ) {     // si hace una hora desde la ultima vez que se encendio el venti
    fanSpeed = ventilationLevel;
    ventilationOn = true;
    lastFanOn = currentMillis;
  }

  if ( ventilationOn && (currentMillis  > lastFanOn  + (1000*60*minFanOn))  ) {  // Si esta la ventilacion encendida desde hace x minutos apagarla.
    fanSpeed = 0;
    ventilationOn = false; 
        lastFanOn = currentMillis;
  }
 
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
  
}     // end of LOOP

void saveData(){
      if (timeUNIX != 0 ) {
        uint32_t actualTime = timeUNIX + (millis() - lastNTPResponse) / 1000;

        File tempLog ;
      tempLog = SPIFFS.open("/dataLog.csv", "a"); // Write the time and the temperature to the csv file
      tempLog.print(actualTime);
      tempLog.print(',');
      tempLog.print(temperature);
      tempLog.print(',');
      tempLog.print(extTemperature);
      tempLog.print(',');
      tempLog.print(fanSpeed);
      tempLog.print(',');
      tempLog.println(Output / 18 + 20); // output goes from 20 ( 0 ) to 38 ( 180 )
      
      tempLog.close();

      }
}


//##########################################################################################################################
/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/
//##########################################################################################################################


void startWiFi() { // Try to connect to some given access points. Then wait for a connection
  wifiMulti.addAP("Primus-E74B", "3c90660be74b");   // add Wi-Fi networks you want to connect to
  //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());             // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages to port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  // ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // go to 'handleFileUpload'
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/delete", HTTP_GET, handleFileDelete);
  server.on("/info.html", handleInfo);
  server.on("/set", HTTP_GET , handleSet);
  server.on("/setUdp",HTTP_GET , handleSetUdp);
  server.on("/reset",HTTP_GET , handleReset );
  server.on("/",HTTP_GET , handleInfo );
  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

//##########################################################################################################################
/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/
//##########################################################################################################################


void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleSet() {
  String msg;
  if ( server.hasArg("setpoint")) {
    double SetpointDouble =   (server.arg("setpoint").toInt() );
    Setpoint = SetpointDouble;
    msg = "Setpoint: " + String(Setpoint)+"\n";
    //server.send(200, "text/plain", msg);
  }
  if ( server.hasArg("minVentilation")) {
    minVentilation = server.arg("minVentilation").toInt();
       msg += "Ventilation cycle lenght: " + String(minVentilation) + " minutes \n.";
    //server.send(200, "text/plain", msg);
  }
    if ( server.hasArg("minFanOn")) {
    minFanOn = server.arg("minFanOn").toInt();
      msg += "Ventilation on: " + String(minFanOn)+" minutes.\n";
    //server.send(200, "text/plain", msg);
  }
  if ( server.hasArg("ventilationLevel")) {
    ventilationLevel = server.arg("ventilationLevel").toInt();
       msg += "Ventilation Level: " + String(ventilationLevel)+"\n";
       if (ventilationOn) fanSpeed = ventilationLevel;
    //server.send(200, "text/plain", msg);
  }
  
    if ( server.hasArg("minLogging")) {
    minLogging = server.arg("minLogging").toInt();
       msg += "minLogging : " + String(minLogging);
    //server.send(200, "text/plain", msg);
  }
  if (msg != "") server.send(200,"text/plain",msg);
}


void handleSetUdp() {
    if ( server.hasArg("url")) {
    udpUrl =   (server.arg("url") );
        String msg = "New URL for UDP communication: " + udpUrl;
    server.send(200, "text/plain", msg);
}
}

void handleReset() {
  server.close();
  
  ESP.reset(); 
}
void handleFileDelete() {                     // Usage   192.168.1.13/delete?file=temp.csv
  if (!server.hasArg("file")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }
  String path = server.arg("file");
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
    
  SPIFFS.remove(path);
  server.send(200, "text/plain", "deleted" + path);
  path = String();
}

void handleInfo() {

    uint32_t actualTime = timeUNIX + (millis() - lastNTPResponse) / 1000;

    FSInfo fs_info;
  SPIFFS.info(fs_info);

  float fileTotalKB = (float)fs_info.totalBytes / 1024.0; 
  float fileUsedKB = (float)fs_info.usedBytes / 1024.0; 
  
  String msg;
msg = msg + "<!DOCTYPE html><head><meta content=\"text/html;charset=utf-8\" http-equiv=\"Content-Type\">"+
"<meta content=\"utf-8\" http-equiv=\"encoding\"> <script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script> <script src=\"tween-min.js\"></script> "+
  "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"> "+
  "<script>var temperature = "+String(temperature)+";\n"+
  "var extTemperature = "+String(extTemperature)+";\n"+
  "var humidity = "+String(humidity)+";\n"+
  "var setpoint = "+String(Setpoint)+";\n"+
  "var output = "+String(Output)+";\n"+
  "var fanSpeed = "+String(fanSpeed)+";\n"+
  "</script>"+
  "</head>"+


  
  "<body> <h1>ESP8266 Basement Controller</h1> " + 
      "<a href=\"dataLog.csv\">dataLog</a>" + 
      "   <a href=\"edit.html\">Upload File</a>   " + 
          "<a href=\"list?dir=/\">Directory</a>   "  +
          "<a href=\"style.css\">Style</a>"+
           "<div id=\"gauge_div\" class=\"gauge_div\"></div>   <div id=\"gauge_fanSpeed_div\"> </div>   <div> <div id=\"chart_div\" class=\"chart_div\">ChartDiv</div><div id=\"loading\">Loading ...</div>  <div id=\"dateselect\" style=\"visibility: hidden\">"+
           " <div id=\"date\"></div>  <button id=\"prev\">prev</button>  <button id=\"next\">next</button><button id=\"zoomout\">-</button>"+
           " <button id=\"zoomin\">+</button>  <button id=\"reset\" style=\"width: 5.4em;\">Reset</button> <button id=\"refresh\" style=\"width: 5.4em;\">Refresh</button> <br> </div></div>";


msg = msg+ "<div id=\"info\" class=\"info\"> <form> <ul> <li id=\"temperature\" > Temperature: " + String(temperature) + "</li><li id=\"extTemperature\"> Exterior Temperature: " + String( extTemperature)+"</li> "+
   "<li> Humidity: " + String(humidity) + "</li>"+
  "<li id=\"setpoint\"> Setpoint: " + String(Setpoint) +
        "  <input type=\"number\" min=\"20\" max=\"30\" step=\"1\" value=\""+ String(Setpoint) + "\" name=\"setpoint\"/>"+
         // "<input type=\"submit\" formaction=\"/set\"/></li>"+
  "<li id=\"output\"> Output: " + String(Output)+"</li></ul><br>"+ 
  
  "<ul><li> Last Fan On(min) : " + String( (millis() - lastFanOn ) / 60000 ) + "</li> <li> Ventilator " + ( (ventilationOn) ? "ON" : "OFF")  + "</li>"+
  "<li> Minutes of Vent Cycle:" + String(minVentilation) +
        "  <input type=\"number\" min=\"10\" max=\"60\" step=\"1\" value=\""+ String(minVentilation) + "\" name=\"minVentilation\"/>"+
          //"<input type=\"submit\" formaction=\"/set?minVentilation="+String(minVentilation)+"\"/>"+
          "</li>" +
  "<li> Minutes of Vent On:" + String(minFanOn)+
        "  <input type=\"number\" min=\"1\" max=\"60\" step=\"1\" value=\""+ String(minFanOn) + "\" name=\"minFanOn\"/>"+
         // "<input type=\"submit\" formaction=\"/set?minFanOn="+String(minFanOn)+"\"/>"+
         "</li>" +
  "<li> Ventilation Speed " + String(ventilationLevel)+
        " <input type=\"number\" min=\"1\" max=\"10\" step=\"1\" value=\""+ String(ventilationLevel) + "\" name=\"ventilationLevel\"/>"+
         // "<input type=\"submit\"formaction=\"/set\"/>"+
          "</li></ul><br>"+
          
   "<ul><li> Logging Period " + String(minLogging)+
        "  <input type=\"number\" min=\"1\" max=\"60\" step=\"1\" value=\""+ String(minLogging) + "\" name=\"minLogging\"/>"+
          "<input type=\"submit\"formaction=\"/set\"/>"+
          "</li>";
   
    msg = msg + "<li> Used Memory: " + String(fileUsedKB) +"/" + String (fileTotalKB) + "KB</li> <li>   fanSpeed: " + String (fanSpeed) +  "</li>" + 
    "<li>Minutes since last reset: " + String(millis()/60000)+"</li>"+
    "<li>Time: <script> var d=new Date(" + String(actualTime*1000) + "); document.write(d+\"--\"+"+String(actualTime)+");</script></li>"+
    "<li>Time from Server: " + String(timeUNIX) +"</li>"+
    "  </ul></form></div> ";
  

msg = msg + " <script src=\"humidityGraph.js\"></script></body></html>";

  server.send(200, "text/html", msg);
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();
  int memoryUsed = 0;
  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1) + "\"";
    output += "Size: " + String(entry.size());
    memoryUsed += entry.size();
    output += "\"}";
    output += "\n";
    entry.close();
  }

  output += "]";
  output += "    Total memory used: " + String (memoryUsed);

  server.send(200, "text/json", output);
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");               // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

unsigned long getTime() { // Check if the time server has responded, if so, get the UNIX time, otherwise, return 0
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}


void sendNTPpacket(IPAddress& address) {
  Serial.println("Sending NTP request");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode

  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(packetBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

long mapFloat ( float x , float in_min , float in_max  , float out_min , float out_max ) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void udpLog (String str){
 const char * msg = str.c_str();
      UDP.beginPacket( udpUrl.c_str() , 9998);
      UDP.write(msg);
      UDP.endPacket();

}


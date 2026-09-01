#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Wire.h>
#include <BH1750.h>
#include <RTClib.h>
#include <ESP32Ping.h> // ICMP connectivity check

// I2C bus: SDA=14, SCL=13
HTTPClient httpClient;
WiFiClientSecure secureClient;
struct tm networkTimeInfo;
DateTime now ;
BH1750 lightSensor;

RTC_DS1307 rtcModule;

//-------------------------------------------------------------*Variables*------------------------------------------------------



// Project configuration.
// Replace these placeholders with your own values before building.
const char* rootCaCertificate = "YOUR_ROOT_CA_CERTIFICATE";
const char* apiGetTimerUrl = "https://example.com/api/timer";
const char* apiGetWiFiUrl = "https://example.com/api/wifi";
const char* apiSendSensorUrl = "https://example.com/api/sensors";

#define SHT20_ADDRESS 0x40
#define SHT20_SDA_PIN 14
#define SHT20_SCL_PIN 13
#define ULTRASONIC_TRIG_PIN 33
#define ULTRASONIC_ECHO_PIN 32

const float SPEED_OF_SOUND_CM_PER_US = 0.034665f;
const unsigned long ECHO_START_TIMEOUT_US = 30000UL;
const unsigned long ECHO_HIGH_TIMEOUT_US = 30000UL;

unsigned long lastTimerUpdateMs; // Relay schedule update timestamp.
unsigned long lastSensorUpdateMs; // Sensor upload timestamp.
unsigned long lastModuleReadMs; // Sensor module read timestamp.



// SHT20 measurement commands.

int maxTemperature = 35; // Configured threshold.
int minTemperature = 25; // Configured threshold.
int temperatureCelsius = 0; // Latest sensor value.
int temperatureHistory[3]= {0};

int maxHumidity = 35; // Configured threshold.
int minHumidity = 25; // Configured threshold.
int humidityPercent = 0; // Latest sensor value.
int humidityHistory[3]= {0};
int environmentHistoryIndex = 0;
float measuredLux;
float measuredTemperature ;
float measuredHumidity ;

int maxLight = 35; // Configured threshold.
int minLight = 25; // Configured threshold.
int lightLevel = 0; // Latest sensor value.
int lightHistory[3] = {0};
int lightHistoryIndex = 0;


// Ultrasonic timing configuration.
int waterLevel = 0;
int maxWaterLevel = 35;
int minWaterLevel = 25;

bool error = false;
bool errorLampState = false;

int scheduleIndex = 0; // Each index represents a five-minute interval
int nextScheduleIndex = 0; 
String pumpScheduleBinary; // Pump schedule for five-minute intervals
String pumpScheduleHex;
bool pumpRelayState = false;
String lightingScheduleBinary; // Lighting schedule for five-minute intervals
String lightingScheduleHex;
bool lightingRelayState = false;

bool filtrationState = false; // Filtration state.

bool fiveMinuteTaskDone = false;

int hour = 0; 
int minute = 0; 
int second = 0;

int rtcHour = 0; 
int rtcMinute = 0; 
int rtcSecond = 0;

// Reserved for future filesystem support. 
StaticJsonDocument<128> sensorJsonDocument;
String sensorDataJson = "{\"sn\":\"\",\"h\":25,\"t\":25,\"wl\":354562,\"l\":56546}";
String serialNumber;

String lowThresholdsString;
String highThresholdsString;

int serialNumberAddress = 0;
int lowThresholdsAddress = 10;
int highThresholdsAddress = 50;
int timer1Address = 90;
int timer2Address = 170;
int timer3Address = 250;
int ssidAddress = 330;
int passwordAddress = 340;

#define PUMP_RELAY_PIN 18
#define LIGHTING_RELAY_PIN 23
#define FILTRATION_PIN 19
#define ERROR_LIGHT_PIN 22


float measuredDistance;  
int distanceHistory[3]= {0};
int distanceHistoryIndex = 0;


//-------------------------------------------------------------*Functions*------------------------------------------------------
String readStringFromEeprom(int addr);
void saveStringToEeprom(int addr, const String& value);
String hexToBin(String hexString);
void parseThresholds(String input, int& temperatureCelsius, int& humidityPercent, int& waterLevel, int& lightLevel);
void readRtcTime();
float readTemperature();
float readHumidity();
float readWaterLevelDistance();
void updateErrorIndicator();
void updateSensorHistory();
void updateScheduledOutputs(bool wifiConnected, bool internetAvailable);
void updateSensorData(bool wifiConnected, bool internetAvailable) ;
void updateRelaySchedules(bool wifiConnected, bool internetAvailable) ;
void updateTime(bool wifiConnected, bool internetAvailable);
//-------------------------------------------------------------*Setup*------------------------------------------------------


void setup() {
  // Initialize the controller.
  // Relay outputs.
  EEPROM.begin(1024);
  pinMode(2, OUTPUT);
  pinMode(PUMP_RELAY_PIN,OUTPUT);
  pinMode(LIGHTING_RELAY_PIN,OUTPUT);
  pinMode(FILTRATION_PIN,OUTPUT);
 
  digitalWrite(2, LOW);
  digitalWrite(PUMP_RELAY_PIN,LOW);
  digitalWrite(LIGHTING_RELAY_PIN,LOW);
  digitalWrite(FILTRATION_PIN,LOW);
  pinMode(ERROR_LIGHT_PIN,OUTPUT);
  digitalWrite(ERROR_LIGHT_PIN,LOW);
  

  // LDR input.

  // Ultrasonic water-level sensor.
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  pinMode(4, OUTPUT); // LED
  digitalWrite(4, LOW);


  Wire.begin( SHT20_SDA_PIN, SHT20_SCL_PIN);// I2C bus initialization.
  
  
  if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, 0x23, &Wire)){
  } else {
  }

  if (!rtcModule.begin(&Wire)) {
    
  }
  now = rtcModule.now();

  delay(100);
 

  deserializeJson(sensorJsonDocument,sensorDataJson);
  serialNumber = readStringFromEeprom(serialNumberAddress);
  String wifiSsid = readStringFromEeprom(ssidAddress);
  String wifiPassword = readStringFromEeprom(passwordAddress + wifiSsid.length());
  pumpScheduleHex = readStringFromEeprom(timer1Address); 
  lightingScheduleHex = readStringFromEeprom(timer2Address); 
  lowThresholdsString = readStringFromEeprom(lowThresholdsAddress);
  highThresholdsString = readStringFromEeprom(highThresholdsAddress);
  parseThresholds(highThresholdsString, maxTemperature, maxHumidity, maxWaterLevel, maxLight);
  parseThresholds(lowThresholdsString, minTemperature, minHumidity, minWaterLevel, minLight);

  sensorJsonDocument["sn"] = serialNumber;
  
  WiFi.begin(wifiSsid, wifiPassword);
  uint8_t counter = 0;
  while(WiFi.status() != WL_CONNECTED && counter++ < 100){
    delay(600);
  }
  // secureClient.setInsecure();
  secureClient.setCACert(rootCaCertificate);
  httpClient.setTimeout(5000);

  configTime(3.5 * 3600, 0, "pool.ntp.org");                // test
  delay(500);
  if(WiFi.status() == WL_CONNECTED){
        counter = 0;
    while (!getLocalTime(&networkTimeInfo) && counter++ < 100) {
      delay(600);
    }
    
    // Check for updated Wi-Fi credentials.
    httpClient.begin(secureClient, urlGetWiFi);
    httpClient.addHeader("Content-Type", "application/json");
    int httpCode = httpClient.POST("{\"sn\":\""+ serialNumber +"\"}");
    if(httpCode == 200){
      
      String payload = httpClient.getString();
      StaticJsonDocument<512> wifiJsonDocument;
      deserializeJson(wifiJsonDocument, payload);
      if((wifiJsonDocument["ssid"].as<String>() != wifiSsid) || (wifiJsonDocument["password"].as<String>() != wifiPassword)){
        wifiSsid = wifiJsonDocument["ssid"].as<String>();
        wifiPassword = wifiJsonDocument["password"].as<String>();
        saveStringToEeprom(ssidAddress, wifiSsid);
        saveStringToEeprom(passwordAddress + wifiSsid.length(), wifiPassword);
        WiFi.disconnect();
        delay(500);
        WiFi.begin(wifiSsid, wifiPassword);
        while(WiFi.status() != WL_CONNECTED){
          delay(500);
        }
      }

    }else{

    }
    httpClient.end();
  }
  delay(10);



  pumpScheduleHex = readStringFromEeprom(timer1Address);
  pumpScheduleBinary = hexToBin(pumpScheduleHex);
  lightingScheduleHex = readStringFromEeprom(timer2Address);
  lightingScheduleBinary = hexToBin(lightingScheduleHex);

  lastTimerUpdateMs = millis();
  lastSensorUpdateMs = millis();
  lastModuleReadMs = millis();
}



//----------------------------------------  loop  ------------------------------------------ 


void loop() {
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool internetAvailable = Ping.ping("8.8.8.8", 2);

  updateTime(wifiConnected, internetAvailable);
  updateRelaySchedules(wifiConnected, internetAvailable);
  updateSensorData(wifiConnected, internetAvailable);
  updateScheduledOutputs(wifiConnected, internetAvailable);
  updateSensorHistory();
  updateErrorIndicator();
}


//-------------------------------------------------------------*Functions*------------------------------------------------------


void updateTime(bool wifiConnected, bool internetAvailable) {
  // Read current time.
  if(wifiConnected && internetAvailable){
    if (getLocalTime(&networkTimeInfo)) {
      hour = networkTimeInfo.tm_hour;
      minute = networkTimeInfo.tm_min;
      second = networkTimeInfo.tm_sec;
    }
  }else{
    readRtcTime();
    hour = rtcHour;
    minute = rtcMinute;
    second = rtcSecond;
    //continue;
  }


}

void updateRelaySchedules(bool wifiConnected, bool internetAvailable) {
  // Get relay schedules from the server.
  if(millis() - lastTimerUpdateMs >= 60000 && wifiConnected && internetAvailable){
    httpClient.begin(secureClient, urlGetTimerData);
    httpClient.addHeader("Content-Type", "application/json");
    int httpCode = httpClient.POST("{\"sn\":\""+ serialNumber +"\"}");
    if(httpCode == 200){
      
      String payload = httpClient.getString();
      StaticJsonDocument<512> timerJsonDocument;
      deserializeJson(timerJsonDocument, payload); 

      if(pumpScheduleHex != timerJsonDocument["timer1"].as<String>() ){
        pumpScheduleHex = timerJsonDocument["timer1"].as<String>();
        saveStringToEeprom(timer1Address,pumpScheduleHex);
        pumpScheduleBinary = hexToBin(pumpScheduleHex);

      }
      if(lightingScheduleHex != timerJsonDocument["timer2"].as<String>()){
        lightingScheduleHex = timerJsonDocument["timer2"].as<String>();
        saveStringToEeprom(timer2Address, lightingScheduleHex);
        lightingScheduleBinary = hexToBin(lightingScheduleHex);
      }
    }else{
    }
    httpClient.end();

    lastTimerUpdateMs = millis();
  }else if(lastTimerUpdateMs > millis()){
    lastTimerUpdateMs = millis();
  }
  delay(200);
}

void updateSensorData(bool wifiConnected, bool internetAvailable) {
  // Send sensor values and receive updated thresholds. 
  if(millis() - lastSensorUpdateMs > 5000){
    // humidity
    // light
    // temperature
    // waterLevel
    int distanceSum = 0;
    int lightSum = 0;
    int temperatureSum = 0;
    int humiditySum = 0;
    //String 
    for(int i = 0 ; i < 3 ; i++){
      distanceSum += distanceHistory[i];
      lightSum += lightHistory[i];
      humiditySum += humidityHistory[i];
      temperatureSum += temperatureHistory[i];
      delay(1);
    }
    waterLevel = distanceSum / 3;
    lightLevel = lightSum / 3;
    humidityPercent = humiditySum / 3;
    temperatureCelsius = temperatureSum / 3;
    sensorJsonDocument["h"] = humidityPercent;
    sensorJsonDocument["t"] = temperatureCelsius;
    sensorJsonDocument["l"] = lightLevel;
    sensorJsonDocument["wl"] = waterLevel;
    serializeJson(sensorJsonDocument,sensorDataJson);
    if(wifiConnected && internetAvailable){
      httpClient.begin(secureClient, urlSendSensorValue);
      httpClient.addHeader("Content-Type", "application/json");
      int httpCode = httpClient.POST(sensorDataJson);
      if(httpCode == 200){
        String payload = httpClient.getString();
        StaticJsonDocument<512> thresholdJsonDocument;
        deserializeJson(thresholdJsonDocument, payload); 
        if(thresholdJsonDocument["h"].as<String>() != highThresholdsString){
          parseThresholds(thresholdJsonDocument["h"].as<String>(), maxTemperature, maxHumidity, maxWaterLevel, maxLight);
          saveStringToEeprom(highThresholdsAddress, thresholdJsonDocument["h"].as<String>());
        }if(thresholdJsonDocument["l"].as<String>() != lowThresholdsString){
          parseThresholds(thresholdJsonDocument["l"].as<String>(), minTemperature, minHumidity, minWaterLevel, minLight);
          saveStringToEeprom(lowThresholdsAddress, thresholdJsonDocument["l"].as<String>());
        }
      }else{
      }
      httpClient.end();
    }
    error = false ;
    if(humidityPercent < minHumidity){ // If the humidity drops below the specified value
      error = true;

    }else if(humidityPercent > maxHumidity){ //If the humidity rises above the specified value
      error = true;
    }

    if(temperatureCelsius < minTemperature){ // If the temperature drops below the specified value
      error = true;
    }else if(temperatureCelsius > maxTemperature){ // If the temperature rises above the specified value
      error = true;
      if(pumpScheduleBinary[scheduleIndex] == '0'){
        // Turn on the pump according to the schedule.
      }

    }

    if(lightLevel < minLight){// If the light drops below the specified value
      error = true;

    }else if(lightLevel > maxLight){ // If the light rises above the specified value
      error = true;

    }

    if(waterLevel > maxWaterLevel){// If the waterLevel drops below the specified value
   
      error = true;

    }else if(waterLevel < minWaterLevel){ // If the waterLevel rises above the specified value
      error = true;
    }

    lastSensorUpdateMs = millis();
  }else if(lastSensorUpdateMs > millis()){
    lastSensorUpdateMs = millis();
  }

}

void updateScheduledOutputs(bool wifiConnected, bool internetAvailable) {
  if(minute % 5 == 0 && fiveMinuteTaskDone == false){
    fiveMinuteTaskDone = true;
    scheduleIndex = hour * 12 + minute / 5;
    nextScheduleIndex = (scheduleIndex + 1) % 277;
    if(pumpScheduleBinary[scheduleIndex] == '1' && pumpRelayState == false){
      digitalWrite(PUMP_RELAY_PIN, HIGH);
      pumpRelayState = true;
    }else if(pumpScheduleBinary[scheduleIndex] == '0' && pumpRelayState == true){
      digitalWrite(PUMP_RELAY_PIN, LOW);
      pumpRelayState = false;
    }

    if(lightingScheduleBinary[scheduleIndex] == '1' && lightingRelayState == false){
      digitalWrite(LIGHTING_RELAY_PIN, HIGH);
      lightingRelayState = true;
    }else if(lightingScheduleBinary[scheduleIndex] == '0' && lightingRelayState == true){
      digitalWrite(LIGHTING_RELAY_PIN, LOW);
      lightingRelayState = false;
    }

    // Calibrate the DS1307 RTC from network time.
    if(wifiConnected && internetAvailable){
      
      if (getLocalTime(&networkTimeInfo)) {
        hour = networkTimeInfo.tm_hour;
        minute = networkTimeInfo.tm_min;
        second = networkTimeInfo.tm_sec;
      }
      readRtcTime();
      if((hour != rtcHour) || (minute != rtcMinute)){
        rtcModule.adjust(DateTime(networkTimeInfo.tm_year,networkTimeInfo.tm_mon,networkTimeInfo.tm_mday,hour,minute,second));
      }

    }

  }else if(minute % 5 == 1 && fiveMinuteTaskDone == true){
    fiveMinuteTaskDone = false;
    if(filtrationState == false && pumpRelayState == true){ // Turn on filtration.
      digitalWrite(FILTRATION_PIN, HIGH);
      filtrationState = true;
    }
  
  }else if (minute % 5 == 4 && filtrationState == true && pumpScheduleBinary[ nextScheduleIndex ] == '0') {
    // Turn off filtration.
    digitalWrite(FILTRATION_PIN, LOW);
    filtrationState = false;
  }



  

}

void updateSensorHistory() {
  if(millis() - lastModuleReadMs > 5000){
    // Read water level.

    measuredDistance = readWaterLevelDistance();

    distanceHistory[distanceHistoryIndex] = measuredDistance;
    distanceHistoryIndex = (distanceHistoryIndex + 1) % 3;

    // Read ambient light.
    measuredLux = lightSensor.readLightLevel();
    lightHistory[lightHistoryIndex] = measuredLux;
    lightHistoryIndex = (lightHistoryIndex + 1) % 3;
  

    // Read temperature and humidity.
    measuredTemperature = readTemperature();
    measuredHumidity = readHumidity();
    humidityHistory[environmentHistoryIndex] = measuredHumidity;
    temperatureHistory[environmentHistoryIndex] = measuredTemperature;
    environmentHistoryIndex = (environmentHistoryIndex + 1) % 3; 
    lastModuleReadMs = millis();
  
  }else if(lastModuleReadMs > millis()){
    lastModuleReadMs = millis();
  }
 
}

void updateErrorIndicator() {
  delay(1000);
  if(error == true){
    if(errorLampState == false){
      digitalWrite(ERROR_LIGHT_PIN, HIGH);
      errorLampState = true;
    }else{
      digitalWrite(ERROR_LIGHT_PIN, LOW);
      errorLampState = false;
    }
  }else{
    if(errorLampState == true){
      digitalWrite(ERROR_LIGHT_PIN, LOW);
      errorLampState = false;
    }
  }

}

void readRtcTime(){
  now = rtcModule.now();
  rtcHour = now.hour();
  rtcMinute = now.minute();
  rtcSecond = now.second();
}

// -------------------------------------------------------------
// Sensor functions
// -------------------------------------------------------------

float readTemperature() {
  Wire.beginTransmission(SHT20_ADDRESS);
  Wire.write(0xF3); // Trigger temperature measurement.
  Wire.endTransmission();
  delay(100); // Sensor conversion time.

  Wire.requestFrom(SHT20_ADDRESS, 2);
  if (Wire.available() < 2) {
    return NAN;
  }

  uint16_t rawValue = Wire.read() << 8 | Wire.read();
  float temperatureCelsius = -46.85f + 175.72f * rawValue / 65536.0f;
  return temperatureCelsius;
}

float readHumidity() {
  Wire.beginTransmission(SHT20_ADDRESS);
  Wire.write(0xF5); // Trigger humidity measurement.
  Wire.endTransmission();
  delay(50); // Sensor conversion time.

  Wire.requestFrom(SHT20_ADDRESS, 2);
  if (Wire.available() < 2) {
    return NAN;
  }

  uint16_t rawValue = Wire.read() << 8 | Wire.read();
  float humidityPercent = -6.0f + 125.0f * rawValue / 65536.0f;
  return humidityPercent;
}

float readWaterLevelDistance() {
  // Send the standard trigger pulse.
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Wait for the echo pulse to start.
  unsigned long startWait = micros();
  while (digitalRead(ULTRASONIC_ECHO_PIN) == LOW) {
    if (micros() - startWait > ECHO_START_TIMEOUT_US) {
      return -1.0f;
    }
  }

  unsigned long pulseStart = micros();

  // Wait for the echo pulse to end.
  while (digitalRead(ULTRASONIC_ECHO_PIN) == HIGH) {
    if (micros() - pulseStart > ECHO_HIGH_TIMEOUT_US) {
      return -1.0f;
    }
  }

  unsigned long pulseEnd = micros();
  unsigned long echoDurationUs = pulseEnd - pulseStart;

  // Convert round-trip time to distance in centimeters.
  float distanceCm = (echoDurationUs * SPEED_OF_SOUND_CM_PER_US) / 2.0f;
  return distanceCm;
}

// -------------------------------------------------------------
// EEPROM and data-processing functions
// -------------------------------------------------------------

String readStringFromEeprom(int address) {
  int length = EEPROM.read(address);
  char data[length + 1];

  for (int i = 0; i < length; i++) {
    data[i] = EEPROM.read(address + 1 + i);
  }

  data[length] = '\\0';
  return String(data);
}

void saveStringToEeprom(int address, const String& value) {
  int length = value.length();
  EEPROM.write(address, length);

  for (int i = 0; i < length; i++) {
    EEPROM.write(address + 1 + i, value[i]);
  }

  EEPROM.commit();
}

String hexToBin(String hexString) {
  String binaryString = "";
  int startIndex = (hexString.startsWith("0x") || hexString.startsWith("0X")) ? 2 : 0;

  for (int i = startIndex; i < hexString.length(); i++) {
    char hexChar = hexString.charAt(i);

    switch (hexChar) {
      case '0': binaryString += "0000"; break;
      case '1': binaryString += "0001"; break;
      case '2': binaryString += "0010"; break;
      case '3': binaryString += "0011"; break;
      case '4': binaryString += "0100"; break;
      case '5': binaryString += "0101"; break;
      case '6': binaryString += "0110"; break;
      case '7': binaryString += "0111"; break;
      case '8': binaryString += "1000"; break;
      case '9': binaryString += "1001"; break;
      case 'a': case 'A': binaryString += "1010"; break;
      case 'b': case 'B': binaryString += "1011"; break;
      case 'c': case 'C': binaryString += "1100"; break;
      case 'd': case 'D': binaryString += "1101"; break;
      case 'e': case 'E': binaryString += "1110"; break;
      case 'f': case 'F': binaryString += "1111"; break;
      default:
        return "";
    }
  }

  return binaryString;
}

void parseThresholds(String input, int& temperature, int& humidity, int& waterLevel, int& light) {
  int index = 0;

  while (input.length() > 0) {
    int delimiterIndex = input.indexOf('-');
    String numberPart;

    if (delimiterIndex == -1) {
      numberPart = input;
      input = "";
    } else {
      numberPart = input.substring(0, delimiterIndex);
      input.remove(0, delimiterIndex + 1);
    }

    switch (index) {
      case 0:
        temperature = numberPart.toInt();
        break;
      case 1:
        humidity = numberPart.toInt();
        break;
      case 2:
        waterLevel = numberPart.toInt();
        break;
      case 3:
        light = numberPart.toInt();
        break;
    }

    index++;
  }
}

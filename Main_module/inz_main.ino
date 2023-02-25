#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "time.h"

#define SERIAL_DEBUG 1
#define DEBUG_TIME_INJECTION 0

#define WIFI_SSID "xyz"
#define WIFI_PASSWORD #include "wifi_pass.txt"
#define API_KEY #include "api_key.txt"
#define DATABASE_URL "xyz"
#define USER_EMAIL "xyz"
#define USER_PASSWORD "xyz"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

typedef struct {
  uint8_t mode_status;
  uint32_t current_time;
  uint8_t heater_status;
  uint32_t setTemp;
  uint8_t enable;
} debugStruct_t;

#if SERIAL_DEBUG
debugStruct_t serialDebug;
uint8_t debugFrame[13];
#endif
const int heater = 32;
unsigned long sendDataPrevMillis = 0;
const int oneWireBus = 33;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 0;  //Replace with your daylight offset (seconds)



typedef struct {
int setStartTime;
int setStopTime;
} setTime_t;

typedef struct {
bool enable;
float setTemp;
} setTemp_t;

uint32_t currentTime = 0;
uint32_t prevLocalSync = 0;
uint32_t prevGlobalSync = 0;

uint32_t syncTime(uint32_t currentTime, uint32_t* prevLocalSync, uint32_t* prevGlobalSync);
setTime_t timeSum (int setStartHour, int setStartMinute, int setStopHour, int setStopMinute);
bool timeComp (setTime_t result, uint32_t currentTime);
bool tempComp(float setTemp);
bool enabledTime (bool setStatus, bool timeCompResult);
setTemp_t ScheduleStatusParse(uint32_t currentTime);
setTemp_t AutoStatusParse();
bool ManualStatusParse();

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  delay(2000);

  //---------------------------
  // Current time init
  //---------------------------
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  currentTime = getSyncLocalTime();

  prevLocalSync = millis();
  prevGlobalSync = millis();

  //---------------------------
  // pin mode init
  //---------------------------
  pinMode(heater, OUTPUT);
   
}




void loop() {
  
  
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 150 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    currentTime = syncTime(currentTime, &prevLocalSync, &prevGlobalSync);
    
    if (Firebase.getString(fbdo, F("/firebase_test_1/Mode")) == true)
    {
      bool manualResult;
      setTemp_t autoResult;
      setTemp_t scheduleResult;
      
      switch (fbdo.to<String>().toInt())
      {
        case 0:
        digitalWrite(heater, LOW);

        #if SERIAL_DEBUG == 0
        Serial.println("OFF");
        #else
        serialDebug.mode_status = 0;
        serialDebug.heater_status = 0;
        serialDebug.setTemp = 0;
        serialDebug.enable = 0;
        #endif
        
        break;        

        //---------------------------
        // Manual 
        //---------------------------
        case 1:
        manualResult = ManualStatusParse();
        if(manualResult)
        {
          digitalWrite(heater, HIGH);
          #if SERIAL_DEBUG == 0
          Serial.println("ON");
          #else
          serialDebug.mode_status = 1;
          serialDebug.heater_status = 1;
          serialDebug.setTemp = 0;
          serialDebug.enable = 1;
          #endif
        }
        else
        {
          digitalWrite(heater, LOW);
          #if SERIAL_DEBUG == 0
          Serial.println("OFF");
          #else
          serialDebug.mode_status = 1;
          serialDebug.heater_status = 0;
          serialDebug.setTemp = 0;
          serialDebug.enable = 0;
          #endif
        }
        break;

        //---------------------------
        // Auto
        //---------------------------
        case 2:
        autoResult = AutoStatusParse();
        if(autoResult.enable)
        {
          Firebase.setFloat(fbdo, F("/firebase_test_1/setTemp"), autoResult.setTemp);
          digitalWrite(heater, HIGH);
          
          #if SERIAL_DEBUG == 0
          Serial.println("ON");
          #else
          serialDebug.mode_status = 2;
          serialDebug.heater_status = 1;
          serialDebug.setTemp = *(uint32_t*)&autoResult.setTemp;
          serialDebug.enable = 1;
          #endif
        }
        else
        {
          Firebase.setFloat(fbdo, F("/firebase_test_1/setTemp"), autoResult.setTemp);
          digitalWrite(heater, LOW);
          
          #if SERIAL_DEBUG == 0
          Serial.println("OFF");
          #else
          serialDebug.mode_status = 2;
          serialDebug.heater_status = 0;
          serialDebug.setTemp = *(uint32_t*)&autoResult.setTemp;
          serialDebug.enable = 0;
          #endif
        }
        break;

        //---------------------------
        // Schedule
        //---------------------------
        case 3:
        scheduleResult = ScheduleStatusParse(currentTime);
        if(scheduleResult.enable)
        {
          Firebase.setFloat(fbdo, F("/firebase_test_1/setTemp"), scheduleResult.setTemp);
          digitalWrite(heater, HIGH);
          
          #if SERIAL_DEBUG == 0
          Serial.println("ON");
          #else
          serialDebug.mode_status = 3;
          serialDebug.heater_status = 1;
          serialDebug.setTemp = *((uint32_t*)&autoResult.setTemp);
          serialDebug.enable = 1;
          #endif
        }
        else
        {
          Firebase.setFloat(fbdo, F("/firebase_test_1/setTemp"), scheduleResult.setTemp);
          digitalWrite(heater, LOW);
          
          #if SERIAL_DEBUG == 0
          Serial.println("OFF");
          #else
          serialDebug.mode_status = 3;
          serialDebug.heater_status = 0;
          serialDebug.setTemp = *((uint32_t*)&autoResult.setTemp);
          serialDebug.enable = 0;
          #endif
        }
        break;
      }
    }
    #if SERIAL_DEBUG
    serialDebug.current_time = currentTime;
    debugFrame[0] = 0x55;
    debugFrame[1] = 0x5A;
    debugFrame[2] = 0xAA;
    debugFrame[3] = serialDebug.mode_status;
    debugFrame[4] = (serialDebug.current_time & 0xFF000000) >> 24;
    debugFrame[5] = (serialDebug.current_time & 0x00FF0000) >> 16;
    debugFrame[6] = (serialDebug.current_time & 0x0000FF00) >> 8;
    debugFrame[7] = (serialDebug.current_time & 0x000000FF);
    debugFrame[8] = serialDebug.heater_status;
    debugFrame[9] = (serialDebug.setTemp & 0xFF000000) >> 24;
    debugFrame[10] = (serialDebug.setTemp & 0x00FF0000) >> 16;
    debugFrame[11] = (serialDebug.setTemp & 0x0000FF00) >> 8;
    debugFrame[12] = (serialDebug.setTemp & 0x000000FF);
    debugFrame[13] = serialDebug.enable;
    Serial.write(debugFrame, 13);
    #endif
  }
  else
  {
    #if SERIAL_DEBUG == 1
    Serial.println("firebase ready: ");
    Serial.println(Firebase.ready());
    Serial.println(millis() - sendDataPrevMillis);
    #endif
  }
}

int getSyncLocalTime()
{
  #if SERIAL_DEBUG == 0
  Serial.println("im in getSyncLocalTime");
  #endif
  struct tm timeinfo;
  int c_hour;
  int c_min;
  int c_sec;
  if(!getLocalTime(&timeinfo)){
    while(1)
    {
      #if SERIAL_DEBUG == 1
      Serial.println("Cannot obtain localtime");
      #endif
      delay(2000);
    }
  }
  c_hour = timeinfo.tm_hour;
  c_min = timeinfo.tm_min;
  c_sec = timeinfo.tm_sec;
  #if SERIAL_DEBUG == 0
  Serial.print("Time: ");
  Serial.print(c_hour);
  Serial.print(":");
  Serial.print(c_min);
  Serial.print(":");
  Serial.println(c_sec);
  #endif
  return (c_hour*3600+c_min*60+c_sec)*1000;
}



uint32_t syncTime(uint32_t currentTime, uint32_t* prevLocalSync, uint32_t* prevGlobalSync)
{
  //Serial.println("im in syncTime");
  int tempTime = 0;
  if((millis()-(*prevGlobalSync)) >= 360*1000)
  {
    tempTime = getSyncLocalTime();
    *prevGlobalSync = millis();
    //Serial.println("Global time sync");
  }
  else
  {
    tempTime =  currentTime + (millis()-(*prevLocalSync));
    //Serial.println("Local time sync");
  }
  *prevLocalSync = millis();
  return tempTime;
}

setTime_t timeSum (int setStartHour, int setStartMinute, int setStopHour, int setStopMinute)
{
  //Serial.println("im in timeSum");
  //Serial.println(setStartHour);
  //Serial.println(setStartMinute);
  //Serial.println(setStopHour);
  //Serial.println(setStopMinute);
  
  setTime_t result;
  result.setStartTime = setStartHour*3600*1000 + setStartMinute*60*1000;
  result.setStopTime = setStopHour*3600*1000 + setStopMinute*60*1000;
  //Serial.println(result.setStartTime);
  //Serial.println(result.setStopTime);
  return result;
}

bool timeComp (setTime_t result, uint32_t currentTime)
{
  #if SERIAL_DEBUG == 0
  Serial.print("aktualny czas w milisekundach: ");
  Serial.println(currentTime);
  #endif
  //Serial.println("im in timeComp");
  if (result.setStartTime < result.setStopTime)
  {
    if (currentTime >= result.setStartTime && currentTime < result.setStopTime)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else if (result.setStartTime >= result.setStopTime)
  {
    if (currentTime >= result.setStopTime && currentTime < result.setStartTime)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  else
  {
    return false;
  }
}

bool tempComp(float setTemp)
{
  //Serial.println("im in tempComp");
  //Serial.println(setTemp);
  float currentTemp;
    if (Firebase.getString(fbdo, F("/firebase_test_1/currentTemp")) == true)
  {
    currentTemp = (fbdo.to<String>().toFloat());
  }
  
  if (setTemp > currentTemp)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool enabledTime (bool setStatus, bool timeCompResult)
{
  #if SERIAL_DEBUG == 0
  Serial.println("im in enabledTime");
  Serial.println(setStatus);
  Serial.println(timeCompResult);
  #endif
  if (setStatus && timeCompResult)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void getSchedule (bool* setStatus, float* setTemp, int* setStartHour, int* setStartMinute, int* setStopHour, int* setStopMinute)
{
  //Serial.println("im in getSchedule");
  char string[60];
  int i;
  for (i = 0; i < 3; ++i)
  {
    sprintf(string,"/firebase_test_1/Schedule_setStatus_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      if(fbdo.to<String>() == "1")
      {
        setStatus[i] = true;
      }
      else
      {
        setStatus[i] = false;
        continue;
      }
    }
    sprintf(string,"/firebase_test_1/Schedule_setTemp_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      setTemp[i] = (fbdo.to<String>().toFloat());
      Serial.print(setTemp[i]);
    }
    sprintf(string,"/firebase_test_1/Schedule_setStartHour_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      setStartHour[i] = (fbdo.to<String>().toInt());
    }
    sprintf(string,"/firebase_test_1/Schedule_setStartMinute_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      setStartMinute[i] = (fbdo.to<String>().toInt());
    }
    sprintf(string,"/firebase_test_1/Schedule_setStopHour_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      setStopHour[i] = (fbdo.to<String>().toInt());
    }
    sprintf(string,"/firebase_test_1/Schedule_setStopMinute_%d", i+1); 
    if (Firebase.getString(fbdo, F(string)) == true)
    {
      setStopMinute[i] = (fbdo.to<String>().toInt());
    }
  }
}

//---------------------------
// Schedule 
//---------------------------
setTemp_t ScheduleStatusParse(uint32_t currentTime)
{
  //Serial.println("im in schedule status parse");
  setTemp_t result;
  bool setStatus[3];
  float setTemp[3];
  int setStartHour[3];
  int setStartMinute[3];
  int setStopHour[3];
  int setStopMinute[3];

  getSchedule(setStatus, setTemp, setStartHour, setStartMinute, setStopHour, setStopMinute);
  
  setTime_t timeSumResult[3];
  bool timeCompResult[3];
  bool enabledTimeResult[3];
  bool tempCompResult[3];
  int i;
  for (i = 0; i < 3; ++i)
  {
    timeSumResult[i] = timeSum(setStartHour[i], setStartMinute[i], setStopHour[i], setStopMinute[i]);
    timeCompResult[i] = timeComp(timeSumResult[i], currentTime);
    enabledTimeResult[i] = enabledTime(setStatus[i], timeCompResult[i]);
    tempCompResult[i] = tempComp(setTemp[i]);

    if(enabledTimeResult[i] && tempCompResult[i])
    {
      result.enable = true;
      result.setTemp = setTemp[i];
      return result;
    }
    else if(enabledTimeResult[i])
    {
      result.enable = false;
      result.setTemp = setTemp[i];
      return result;
    }
  }
  result.enable = false;
  result.setTemp = 0.0;
  return result;
  


  
}

//---------------------------
// Auto
//---------------------------
setTemp_t AutoStatusParse()
{
  setTemp_t result;
  float tempCompResult;
  float setTemp;
  if (Firebase.getString(fbdo, F("/firebase_test_1/Auto_setTemp")) == true)
  {
    setTemp = (fbdo.to<String>().toFloat());
  }
  tempCompResult = tempComp(setTemp);
  if (tempCompResult)
  {
    result.enable = true;
    result.setTemp = setTemp;
    return result;
  }
  else
  {
    result.enable = false;
    result.setTemp = setTemp;
    return result;
  }
}


//---------------------------
// Manual 
//---------------------------
bool ManualStatusParse()
{
  if(Firebase.getString(fbdo, F("/firebase_test_1/Manual_setStatus")) == true)
  {
    if(fbdo.to<String>() == "1")
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

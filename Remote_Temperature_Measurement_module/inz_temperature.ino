#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "xyz"
#define WIFI_PASSWORD #include "wifi_pass.txt"
#define API_KEY #include "api_key.txt"
#define DATABASE_URL "xyz"
#define USER_EMAIL "xyz"
#define USER_PASSWORD "xyz"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const int oneWireBus = 32;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);


void setup(){
    
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  delay(2000);
 
  Firebase.ready();
  Firebase.setFloat(fbdo, F("/firebase_test_1/currentTemp"), temperatureC);

  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();

}

void loop(){
  //This is not going to be called
}

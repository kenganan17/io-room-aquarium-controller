#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Time.h>
#include <DS3232RTC.h>


#define ONE_WIRE_BUS 2
#define LIGHT_PIN 16
#define CO2_PIN 14
#define LIGHT_BUTTON_PIN 12
#define CO2_BUTTON_PIN 13
#define CO2_LED_PIN 15


const String serverDomain = ""; // backend server domain
const char* routerSSID = ""; // access point name
const char* routerPassword = ""; // access point password
const char* userId = ""; // user id on user_record (backend server)
const char* userPassword = ""; // // user password on user_record (backend server)


OneWire ourWire(ONE_WIRE_BUS);
DallasTemperature sensors(&ourWire);

float celcius;

int lightOpen = true;
int co2Open = true;
int restartStatus = false;
int sentTemp = true;
int updateSet = false;

int lastSecDiv = 0;
int currentSecDiv;

String settingCode;
String stringCache;
String settingString = "";
String lastSettingString = "'20`30`03`30`1`1`'";

int openingHour = 20;
int openingMinute = 30;
int closingHour = 4;
int closingMinute = 30;

String URL;
int httpCode;
String payload;
float sensorValue;
int currentHour;
int currentMin;
int currentSec;
int lastMin;


int httpCheck;

int lightButtonState = LOW;             // the current reading from the input pin
int lastLightButtonState = HIGH;   // the previous reading from the input pin
int lightButtonReading;

int co2ButtonState = LOW;             // the current reading from the input pin
int lastCo2ButtonState = HIGH;   // the previous reading from the input pin
int co2ButtonReading;

long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;
int strCount = 0;



void updateTemp(float celciusTemp) {
  HTTPClient http;
  URL = serverDomain + "aquarium_update_temperature?temperature=" + String(celciusTemp) + "&username=" + userId + "&password=" + userPassword;
  http.begin(URL);
  httpCode = http.GET();
  delay(1000);
  if (httpCode) {
//    Serial.printf("[HTTP] Update temperature to server... code: %d\n", httpCode);

    // if(httpCode == 200) {
    //    payload = http.getString();
    //    return payload;
    // }
  } else {
//    Serial.print("[HTTP] GET... failed, no connection or no HTTP server\n");

  }
  // return lastStatusString;
}

String updateSettings() {
  HTTPClient http;
  URL = serverDomain + "aquarium_update_setting";
  http.begin(URL);
  httpCode = http.GET();
  delay(1000);
  if (httpCode) {
//    Serial.printf("[HTTP] Update to server... code: %d\n", httpCode);

    if (httpCode == 200) {
      payload = http.getString();
      return payload;
    }
  } else {
//    Serial.print("[HTTP] GET... failed, no connection or no HTTP server\n");

  }
  return lastSettingString;
}

int openRelay(int lightStr, int co2Str) {

  lastSettingString.setCharAt(13, char(lightStr));
  lastSettingString.setCharAt(15, char(co2Str));

  HTTPClient http;
  URL = serverDomain + "aquarium_relay?light_status=" + String(lightStr) + "&co2_status=" + String(co2Str) + "&username=" + userId + "&password=" + userPassword;
  http.begin(URL);
  httpCode = http.GET();
  delay(1000);
  if (httpCode) {
//    Serial.printf("[HTTP] Send open relay request to server... code: %d\n", httpCode);
    return httpCode;
  } else {
//    Serial.print("[HTTP] GET... failed, no connection or no HTTP server\n");
    return 0;
  }
}

int restartPing() {
  HTTPClient http;
  URL = serverDomain + "restart_ping";
  http.begin(URL);
  httpCode = http.GET();
  delay(1000);
  if (httpCode) {
//    Serial.printf("[HTTP] Send restart ping to server... code: %d\n", httpCode);
    return httpCode;
  } else {
//    Serial.print("[HTTP] GET... failed, no connection or no HTTP server\n");
    return 0;
  }
}

void inputButtonChecks() {
  lightButtonReading = digitalRead(LIGHT_BUTTON_PIN);
  if (lightButtonReading != lastLightButtonState) {
    lastDebounceTime = millis();
    lastLightButtonState = lightButtonReading;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lightButtonState != lastLightButtonState) {
      lightButtonState = lastLightButtonState;
      if (lightButtonState == HIGH) {
        lightOpen = !lightOpen;
        openRelay(lightOpen, co2Open);
        //        Serial.println("light button!");
      }
    }
  }

  co2ButtonReading = digitalRead(CO2_BUTTON_PIN);

  if (co2ButtonReading != lastCo2ButtonState) {
    lastDebounceTime = millis();
    lastCo2ButtonState = co2ButtonReading;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (co2ButtonState != lastCo2ButtonState) {
      co2ButtonState = lastCo2ButtonState;
      if (co2ButtonState == HIGH) {
        co2Open = !co2Open;
        openRelay(lightOpen, co2Open);
        //          Serial.println("co2 button!");
      }
    }
  }
}

float getSampleTemperature() {
  sensors.requestTemperatures();
  sensorValue = sensors.getTempCByIndex(0);
  return sensorValue;
}


void setup(void) {
  Serial.begin(115200);

  //  Serial.print("Configuring access point...");
  WiFi.mode(WIFI_STA);

  IPAddress myIP = WiFi.softAPIP();
  //  Serial.print("AP IP address: ");
  //  Serial.println(myIP);

  // Connect to WiFi network
  WiFi.begin(routerSSID, routerPassword);
  //  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //    Serial.print(".");
  }
  //  Serial.println("");
  //  Serial.print("Connected to ");
  //  Serial.println(routerSSID);
  //  Serial.print("IP address: ");
  //  Serial.println(WiFi.localIP());

  setSyncProvider(RTC.get);

  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(CO2_PIN, OUTPUT);
  pinMode(LIGHT_BUTTON_PIN, INPUT);
  pinMode(CO2_BUTTON_PIN, INPUT);
  pinMode(CO2_LED_PIN, OUTPUT);


  if (restartPing() == 200) {
    restartStatus = true;
  }



}

void loop(void) {
  if (!restartStatus) {
    if (restartPing() == 200) {
      restartStatus = true;
    }
  }


  currentHour = hour();
  currentMin = minute();
  currentSec = second();
  //  Serial.print(currentHour);
  //  Serial.print(":");
  //  Serial.print(currentMin);
  //  Serial.print(":");
  //  Serial.print(currentSec);
  currentSecDiv = (currentSec + 10) / 10;



  if (currentSecDiv - lastSecDiv != 0) {
    updateSet = false;
    lastSecDiv = currentSecDiv;
  }

  if (currentMin % 2 == 0) {
    if (currentMin != lastMin) {
      sentTemp = false;
      lastMin = currentMin;
    }

  }

  if (!updateSet) {
    settingCode = updateSettings();
    lastSettingString = settingCode;

    stringCache = "";
    stringCache += settingCode.charAt(1);
    stringCache += settingCode.charAt(2);
    openingHour = stringCache.toInt();

    stringCache = "";
    stringCache += settingCode.charAt(4);
    stringCache += settingCode.charAt(5);
    openingMinute = stringCache.toInt();

    stringCache = "";
    stringCache += settingCode.charAt(7);
    stringCache += settingCode.charAt(8);
    closingHour = stringCache.toInt();

    stringCache = "";
    stringCache += settingCode.charAt(10);
    stringCache += settingCode.charAt(11);
    closingMinute = stringCache.toInt();

    stringCache = "";
    stringCache += settingCode.charAt(13);
    lightOpen = stringCache.toInt();

    stringCache = "";
    stringCache += settingCode.charAt(15);
    co2Open = stringCache.toInt();
  }

  updateSet = true;

  if (!sentTemp) {
    celcius = getSampleTemperature();
    delay(50);
//    Serial.println(celcius);
    updateTemp(celcius);
  }

  sentTemp = true;



  if (currentHour == openingHour && currentMin == openingMinute && (lightOpen || co2Open)) {
    lightOpen = false;
    co2Open = false;
    openRelay(lightOpen, co2Open);
  }

  if (currentHour == closingHour && currentMin == closingMinute && (!lightOpen || !co2Open)) {
    lightOpen = true;
    co2Open = true;
    openRelay(lightOpen, co2Open);
  }



  inputButtonChecks();


  digitalWrite(LIGHT_PIN, lightOpen);
  digitalWrite(CO2_PIN, co2Open);
  digitalWrite(CO2_LED_PIN, !co2Open);



  delay(10);

}




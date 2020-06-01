
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "Fonts/Exo2SemiBold24pt7b.h"
#include "Fonts/Exo2Light8pt.h"

#include "bitmaps.h"

/*
   Configuration
*/
#define DEBUG true

#define DATA_URL "http://10.120.120.10:8888/display.php"
#define V_BAT_PIN 35
#define FULL_UPDATE_CYCLES 8    // Defines how often a full update of the display is done (also forecast data is read then)
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 900       //Time ESP32 will go to sleep (in seconds)
#define WIFI_SSID "ItenIOT"
#define WIFI_PASS "das pferd"

/*
   Global Variables
*/
GxEPD2_BW<GxEPD2_213, GxEPD2_213::HEIGHT> display(GxEPD2_213(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

RTC_DATA_ATTR int bootCount = 0;


RTC_DATA_ATTR float poolTempS, airTempS;
RTC_DATA_ATTR int min1S, min2S, min3S, max1S, max2S, max3S, icon1S, icon2S, icon3S;

float poolTemp, airTemp;
int min1, min2, min3, max1, max2, max3, icon1, icon2, icon3;

/*
   Prototypes
*/
void getData(bool full, int rawBatteryValue);
esp_sleep_wakeup_cause_t getWakeupReason();




/*
   SETUP
   We only use setup functions, as we go to sleep after our job
*/
void setup() {
  char buffer[128];
  bool full = false;
  int16_t x, y, tbx, tby;
  uint16_t tbw, tbh;

  // Only set serial if we are debugging
#if DEBUG
  Serial.begin(115200);
  Serial.println("Booting...");
#endif

  //Increment boot number
  ++bootCount;

  /*
    First we configure the wake up source
    We set our ESP32 to wake up every x seconds
  */
  // Get the wakeup reason for ESP32
  getWakeupReason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
#if DEBUG
  Serial.println("Boot number: " + String(bootCount));
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");
#endif

  //Start WiFi
#if DEBUG
  Serial.println("Connecting to WiFi");
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  /*
     Check if we are going to do a full update. If so,
     we can use the time the WiFi needs to connect to
     draw the background and fully refresh the screen.
  */

  //  if (bootCount % FULL_UPDATE_CYCLES == 1) {
  if (true) {
    full = true;
#if DEBUG
    Serial.println("We are doing a full update/refresh");
#endif
    // clear display
    display.init();
    display.setRotation(3);
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);

    // draw black squares for labels
    display.fillRect(0, 0, 16, 44, GxEPD_BLACK);    // Air
    display.fillRect(133, 0, 16, 44, GxEPD_BLACK);  // Pool
    display.fillRect(0, 45, 16, 78, GxEPD_BLACK);   // Today
    display.fillRect(83, 45, 16, 78, GxEPD_BLACK);  // Tomorrow
    display.fillRect(166, 45, 16, 78, GxEPD_BLACK); // The day after tomorrow

    // draw borders
    display.drawRect(0, 0, 250, 45, GxEPD_BLACK);
    display.drawRect(0, 44, 250, 78, GxEPD_BLACK);

    // set font for labels and draw labels
    display.setFont(&Exo2_Light8pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setRotation(2);
    display.setCursor(92, 13);
    display.print("Luft");
    display.setCursor(91, 145);
    display.print("Pool");
    sprintf(buffer, "Heute");
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    x = 43 - (tbw + tbx) / 2;
    display.setCursor(x, 13);
    display.print(buffer);
    sprintf(buffer, "Morgen");
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    x = 43 - (tbw + tbx) / 2;
    display.setCursor(x, 95);
    display.print(buffer);

    sprintf(buffer, "Ueberm.");
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    x = 43 - (tbw + tbx) / 2;
    display.setCursor(x, 178);
    display.print(buffer);
    display.display(false);
  }

#if DEBUG
  Serial.print("Waiting for WiFi... ");
#endif

  while (WiFi.status() != WL_CONNECTED) {
#if DEBUG
    Serial.print(".");
    delay(500);
#endif
  }

#if DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  // Get the data and send raw battery voltage
  int RAW = analogRead(V_BAT_PIN);
  getData(true, RAW);

  // Shut down radio to save power
#if DEBUG
  Serial.println("Switching of radio...");
#endif
  WiFi.mode(WIFI_OFF);
  btStop();

  //Drawing the results
  display.setRotation(3);
  display.setFont(&Exo2_SemiBold24pt7b);
  display.setTextColor(GxEPD_BLACK);
  sprintf(buffer, "%.1f", airTemp);
  display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  x = 125 - tbw - tbx;
  y = 38;
  display.setCursor(x, y);
  display.print(buffer);
  airTempS = airTemp;
  sprintf(buffer, "%.1f", poolTemp);
  display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  x = display.width() - tbw - tbx - 7;
  display.setCursor(x, y);
  display.print(buffer);

  display.setFont(&Exo2_Light8pt7b);
  display.drawBitmap(16, 45, weather_icons[icon1], 67, 63, GxEPD_BLACK);
  display.drawBitmap(99, 45, weather_icons[icon2], 67, 63, GxEPD_BLACK);
  display.drawBitmap(182, 45, weather_icons[icon3], 67, 63, GxEPD_BLACK);

  sprintf(buffer, "%i/%i", min1, max1);
  display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  x = 16 + (84 - 16 - tbw - tbx) / 2;
  display.setCursor(x, 118);
  display.print(buffer);
  sprintf(buffer, "%i/%i", min2, max2);
  display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  x = 97 + (84 - 16 - tbw - tbx) / 2;
  display.setCursor(x, 118);
  display.print(buffer);
  sprintf(buffer, "%i/%i", min3, max3);
  display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  x = 182 + (84 - 16 - tbw - tbx) / 2;
  display.setCursor(x, 118);
  display.print(buffer);

  display.display(true);

#if DEBUG
  Serial.println("Going to sleep now");
  Serial.flush();
#endif
  esp_deep_sleep_start();
}

void loop() {
  //  int RAW = analogRead(V_BAT_PIN);
  //  float VBAT = (float)RAW / 4095 * 2 * 3.3 * 1.1;
}

/*
   Functions
*/
void getData(bool full, int rawBattery) {
  HTTPClient http;
  DynamicJsonDocument doc(1024);
  String jsonstring;
  String url = String(String(DATA_URL) + String("?rawBattery=") + String(rawBattery));
  if (full) {
    url += "&full=true";
  }

#if DEBUG
  Serial.println("Fetching URL: " + url);
#endif

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode > 0) { //Check for the returning code
    jsonstring = http.getString();
#if DEBUG
    Serial.print("HTTP-Code:\t"); Serial.println(httpCode);
    Serial.print("Payload:\t"); Serial.println(jsonstring);
#endif
  }

  else {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources
  deserializeJson(doc, jsonstring);
  JsonObject obj = doc.as<JsonObject>();
  poolTemp = obj["poolTemp"];
  airTemp = obj["airTemp"];
  if (full) {
    max1 = obj["max1"];
    max2 = obj["max2"];
    max3 = obj["max3"];
    min1 = obj["min1"];
    min2 = obj["min2"];
    min3 = obj["min3"];
    icon1 = obj["icon1"];
    icon2 = obj["icon2"];
    icon3 = obj["icon3"];
  }
#if DEBUG
  Serial.print("Pool:\t"); Serial.println(poolTemp);
  Serial.print("Luft:\t"); Serial.println(airTemp);
  if (full) {
    Serial.print("min1:\t"); Serial.println(min1);
    Serial.print("max1:\t"); Serial.println(max1);
    Serial.print("icon1:\t"); Serial.println(icon1);
    Serial.print("min2:\t"); Serial.println(min2);
    Serial.print("max2:\t"); Serial.println(max2);
    Serial.print("icon2:\t"); Serial.println(icon2);
    Serial.print("min3:\t"); Serial.println(min3);
    Serial.print("max3:\t"); Serial.println(max3);
    Serial.print("icon3:\t"); Serial.println(icon3);
  }
#endif
}

/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
esp_sleep_wakeup_cause_t getWakeupReason() {
  esp_sleep_wakeup_cause_t wakeupReason;
  wakeupReason = esp_sleep_get_wakeup_cause();
#ifdef DEBUG
  switch (wakeupReason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeupReason); break;
  }
#endif
  return wakeupReason;
}

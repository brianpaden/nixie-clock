#include <Arduino.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>

// #define DEBUG_NTPClient

int timeZoneOffsetHours = -9;

WiFiUDP udp;
NTPClient timeClient(udp);
WiFiManager wifiMgr;

RTC_DS3231 rtc;

uint32_t lastClockPrintAt = 0;
uint32_t lastWifiWaitStatusAt = 0;

// The Fatal Error Handler
void panic(const char *errorMessage)
{
  // 1. Output the error details for debugging
  Serial.println(F("\n===================================="));
  Serial.println(F("           FATAL ERROR              "));
  Serial.println(F("===================================="));
  Serial.println(errorMessage);
  Serial.flush(); // Force the serial buffer to empty completely

  // 2. Lock the processor and flash the built-in LED endlessly
  while (true)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void checkWiFiHardware()
{
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Please upgrade the firmware");
  }
}

void serviceWifiFor(uint32_t durationMs)
{
  const uint32_t startedAt = millis();
  while (millis() - startedAt < durationMs)
  {
    wifiMgr.loop();
    delay(10);
  }
}

void printWifiWaitStatus()
{
  Serial.print(F("WiFi wait status="));
  Serial.print(WiFi.status());
  Serial.print(F(" localIP="));
  Serial.print(WiFi.localIP());
  Serial.print(F(" softAPIP="));
  Serial.print(WiFi.softAPIP());
  Serial.print(F(" apSSID="));
  Serial.println(WiFi.softAPSSID());
}

//****************************************************************************
void printFourDigits(int number)
{
  char buffer[5];
  sprintf(buffer, "%04d", number);
  Serial.print(buffer);
}

void printTwoDigits(int number)
{
  char buffer[3];
  sprintf(buffer, "%02d", number);
  Serial.print(buffer);
}

//****************************************************************************
void setup()
{
  //-----[ Setup Serial ]-----------------------------------------------------
  Serial.begin(115200);

  // Wait up to 3000ms for the Serial Monitor to connect
  while (!Serial && millis() < 3000)
  {
    ; // Do nothing, just wait
  }
  if (!Serial)
  {
    panic("Unable to initialize serial");
  }
  Serial.println("System Initialized.");

  //-----[ Setup Wifi ]-----------------------------------------------------
  checkWiFiHardware();

  wifiMgr.settings().registerInt("tz_offset", "Timezone offset (hours)", -9, -12, 14);
  wifiMgr.settings().registerBool("hour24", "24-hour mode", true);
  wifiMgr.settings().registerInt("brightness", "Nixie brightness", 128, 0, 255);
  Serial.println(F("Starting WiFi manager."));
  wifiMgr.begin();
  Serial.println(F("WiFi manager started."));
  printWifiWaitStatus();

  Serial.println("Waiting for WiFi connection. Use the setup AP if needed.");
  while (!wifiMgr.isConnected())
  {
    wifiMgr.loop();
    if (millis() - lastWifiWaitStatusAt >= 5000)
    {
      lastWifiWaitStatusAt = millis();
      printWifiWaitStatus();
    }
    delay(10);
  }

  timeZoneOffsetHours = wifiMgr.settings().getInt("tz_offset");
  Serial.println("Connected to WiFi");
  printWifiStatus();

  if (!rtc.begin())
  {
    panic("Error: unable to initialize RTC module");
  }

  Serial.println("Connecting to ntp");
  int attempts = 0;
  while (!timeClient.update())
  {
    attempts += 1;
    Serial.println("Forcing ntp update");
    timeClient.forceUpdate();
    if (attempts >= 10)
    {
      panic("Unable to connect to NTP server");
    }
    serviceWifiFor(500);
  }
  auto unixTime = timeClient.getEpochTime();
  if (0 == unixTime || -1 == unixTime)
  {
    panic("Failed to real NTP data");
  }
  else
  {
    auto adjustedTime = unixTime + (timeZoneOffsetHours * 3600);

    Serial.print("Unix time = ");
    Serial.println(adjustedTime);
    DateTime dt(adjustedTime);
    Serial.println("Updating time on rtc clock");
    rtc.adjust(dt);
  }
}

//****************************************************************************
void loop()
{
  wifiMgr.loop();

  if (millis() - lastClockPrintAt < 10000)
  {
    return;
  }
  lastClockPrintAt = millis();

  // Fetch the current date and time from the RTC
  DateTime now = rtc.now();

  // Print Date: YYYY-MM-DD
  printFourDigits(now.year());
  Serial.print('-');
  printTwoDigits(now.month());
  Serial.print('-');
  printTwoDigits(now.day());
  Serial.print(" ");

  // Print Time: HH:MM:SS
  printTwoDigits(now.hour());
  Serial.print(':');
  printTwoDigits(now.minute());
  Serial.print(':');
  printTwoDigits(now.second());
  // Serial.println(); // Move to the next line

  Serial.print(" - Temp: ");
  Serial.print(rtc.getTemperature());
  Serial.println();
}

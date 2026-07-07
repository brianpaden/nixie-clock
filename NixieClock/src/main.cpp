#include <Arduino.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "secrets.hpp"

// #define DEBUG_NTPClient

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int timeZoneOffsetHours = -9;

int wifiStatus = WL_IDLE_STATUS;
WiFiUDP udp;
NTPClient timeClient(udp);

RTC_DS3231 rtc;

Preferences preferences;
const char *prefName = "nixie";

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

void connectToWiFi()
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

  // attempt to connect to WiFi network:
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi");
  printWifiStatus();
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

  //-----[ Load Preferences ]-----------------------------------------------
  Serial.print("Reading preferences: ");
  Serial.print(prefName);
  Serial.println();

  preferences.begin(prefName, false);
  size_t entriesFree = preferences.freeEntries();
  Serial.print("Free entries: ");
  Serial.print(entriesFree);
  Serial.println();

  //-----[ Setup Wifi ]-----------------------------------------------------
  connectToWiFi();

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
    delay(500);
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

  delay(1000); // Wait 1 second before reading the time again
}

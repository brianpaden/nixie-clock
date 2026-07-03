#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiManager.h>

void setup()
{
  Serial.begin(115200);

  WifiManager wm;

  bool res;
  res = wm.autoConnect();

  if (!res)
  {
    Serial.println("Failed to connect")
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
}

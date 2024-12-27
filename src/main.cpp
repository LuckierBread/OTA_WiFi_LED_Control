//----OTA Stuff----
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//----Basic Needs----
#include <FastLED.h>
#include "credentials.h"

//----Web Server----
#include <Arduino.h>
#include <WebServer.h>
#include <SPIFFS.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>

#define NUM_LEDS 1
#define DATA_PIN 48

CRGB leds[NUM_LEDS];          // Define LED for FastLED. I don't know how to define a single one without the array.
CRGB lastColor = CRGB::Black; // Store the last color to avoid errors in GetColorFromString when invalid is recieved

const char *ssid = MY_WIFI_SSID;
const char *password = MY_WIFI_PASS;

WebServer server(80); // Defines the Web server on port 80

String GetValueFromQuery(String query, String key)
{
  query.toLowerCase();               // convert the query to lowercase
  int keyStart = query.indexOf(key); // find the key in the query
  if (keyStart == -1)
    return "";                                   // if the key is not found return an empty string
  int valueStart = keyStart + key.length() + 1;  // the plus 1 is to account for the = sign
  int valueEnd = query.indexOf('&', valueStart); // find the next & after the value
  if (valueEnd == -1)
    return query.substring(valueStart); // if there is no & at the end of the value
  else
    return query.substring(valueStart, valueEnd); // if there is a & at the end of the value tell substring where to end
}

CRGB GetCRGBFromString(String color)
{
  if (color == "red")
    return CRGB::Red;
  if (color == "green")
    return CRGB::Green;
  if (color == "blue")
    return CRGB::Blue;
  if (color == "yellow")
    return CRGB::Yellow;
  if (color == "purple")
    return CRGB::Purple;
  if (color == "cyan")
    return CRGB::Cyan;
  if (color == "orange")
    return CRGB::Orange;
  if (color == "pink")
    return CRGB::Pink;
  if (color == "white")
    return CRGB::White;
  if (color == "black")
    return CRGB::Black;
  return lastColor;
}

String GetStringFromCRGB(CRGB color)
{
  if (color == CRGB::Red)
    return "red";
  if (color == CRGB::Green)
    return "green";
  if (color == CRGB::Blue)
    return "blue";
  if (color == CRGB::Yellow)
    return "yellow";
  if (color == CRGB::Purple)
    return "purple";
  if (color == CRGB::Cyan)
    return "cyan";
  if (color == CRGB::Orange)
    return "orange";
  if (color == CRGB::Pink)
    return "pink";
  if (color == CRGB::White)
    return "white";
  if (color == CRGB::Black)
    return "black";
  return "invalid";
}

void handleRoot()
{
  Serial.println("Root page requested");
  File file = SPIFFS.open("/index.html", "r");
  if (!file)
  {
    Serial.println("/index.html not found");
    server.send(404, "text/plain", "404: Page Not Found");
    return;
  }
  Serial.println("Sending root page");
  server.streamFile(file, "text/html");
  file.close();
}

void handleStyles()
{
  Serial.println("Style page requested");
  File file = SPIFFS.open("/style.css", "r");
  if (!file)
  {
    Serial.println("/style.css not found");
    server.send(404, "text/plain", "404: Styles Not Found");
    return;
  }
  Serial.println("Sending style page");
  server.streamFile(file, "text/css");
  file.close();
}

void handleUpdate()
{
  bool updated = false;

  Serial.println("Update page requested");
  // Retrieve query parameters
  if (server.hasArg("brightness"))
  {
    int brightness = constrain(server.arg("brightness").toInt(), 0, 255);
    FastLED.setBrightness(brightness);
    updated = true;
  }

  if (server.hasArg("color"))
  {
    lastColor = GetCRGBFromString(server.arg("color"));
    leds[0] = lastColor; // leave last color here. it's needed to handle invalid colors
    updated = true;
  }

  if (updated)
  {
    FastLED.show();
  }
  // Redirect back to the root page
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleValues()
{
  String json = "{\"brightness\":" + String(FastLED.getBrightness()) + ",\"color\":\"" + GetStringFromCRGB(lastColor) + "\"}";
  server.send(200, "application/json", json);
}

void listSPIFFS()
{
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    Serial.print("File: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
}

void setup()
{
  SPIFFS.begin(true); // Start the SPIFFS service
  //--------FastLED and OTA setup--------
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(4);
  leds[0] = CRGB::Yellow; // Shows Yellow while booting to check for hanging
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    leds[0] = CRGB::Red; // Helps show connection problems when not on serial monitor
    FastLED.show();
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  ArduinoOTA.setPassword("MikesTestPass");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]()
                     {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      } });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/style.css", handleStyles);
  server.on("/update", handleUpdate);
  server.on("/values", handleValues);
  server.begin();
  Serial.println("HTTP server started");

  leds[0] = CRGB::Green; // Shows Green when ready
  FastLED.show();
  delay(1000);
  FastLED.clear();
  FastLED.show();
  //---------Start any new setup code here--------
  listSPIFFS();
}

void loop()
{
  ArduinoOTA.handle();   // leave this in loop to listen for update commands. Be carful not to block this line with long running code
  server.handleClient(); // Handle the web server
}
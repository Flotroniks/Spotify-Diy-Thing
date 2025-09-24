/*******************************************************************
    Displays Album Art on a 320 x 240 ESP32.

    Parts:
    ESP32 With Built in 320x240 LCD with Touch Screen (ESP32-2432S028R)
    https://github.com/witnessmenow/Spotify-Diy-Thing#hardware-required

 *******************************************************************/

// ----------------------------
// Display type
// ---------------------------

// This project currently supports the following displays
// (Uncomment the required #define)

// 1. Cheap yellow display (Using TFT-eSPI library)
// #define YELLOW_DISPLAY

// 2. Matrix Displays (Like the ESP32 Trinity)
// #define MATRIX_DISPLAY

// If no defines are set, it will default to CYD
#if !defined(YELLOW_DISPLAY) && !defined(MATRIX_DISPLAY)
#define YELLOW_DISPLAY // Default to Yellow Display for display type
#endif

#define NFC_ENABLED 1

// This causes issues in certain circumstances e.g. Play an album and let it auto play to related songs
bool writeContextToNfc = true;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;

// ----------------------------
// Library Defines - Need to be defined before library import
// ----------------------------

#define ESP_DRD_USE_SPIFFS true

// ----------------------------
// Standard Libraries
// ----------------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <FS.h>
#include "SPIFFS.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <WiFiManager.h>
// Captive portal for configuring the WiFi

// If installing from the library manager (Search for "WifiManager")
// https://github.com/tzapu/WiFiManager

#include <ESP_DoubleResetDetector.h>
// A library for checking if the reset button has been pressed twice
// Can be used to enable config mode
// Can be installed from the library manager (Search for "ESP_DoubleResetDetector")
// https://github.com/khoih-prog/ESP_DoubleResetDetector

#include <SpotifyArduino.h>

// including a "spotify_server_cert" variable
// header is included as part of the SpotifyArduino libary
#include <SpotifyArduinoCert.h>

#include <ArduinoJson.h>

WiFiClientSecure client;

//------- Replace the following! ------

// Country code, including this is advisable
#define SPOTIFY_MARKET "IE"
//------- ---------------------- ------

// ----------------------------
// Internal includes
// ----------------------------
#include "refreshToken.h"

#include "spotifyDisplay.h"

#include "spotifyLogic.h"

#include "configFile.h"

#include "serialPrint.h"

#include "WifiManagerHandler.h"

// ----------------------------
// Display Handling Code
// ----------------------------

#if defined YELLOW_DISPLAY

#include "cheapYellowLCD.h"
CheapYellowDisplay cyd;
SpotifyDisplay *spotifyDisplay = &cyd;

#elif defined MATRIX_DISPLAY
#include "matrixDisplay.h"
MatrixDisplay matrixDisplay;
SpotifyDisplay *spotifyDisplay = &matrixDisplay;

#endif
// ----------------------------

#ifdef NFC_ENABLED
#include "nfc.h"
#endif

void drawWifiManagerMessage(WiFiManager *myWiFiManager)
{
  spotifyDisplay->drawWifiManagerMessage(myWiFiManager);
}

bool connectToStoredWifi()
{
  if (wifiSsid[0] == '\0')
  {
    Serial.println(F("No WiFi credentials found in config"));
    return false;
  }

  Serial.print(F("Connecting to stored WiFi SSID: "));
  Serial.println(wifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.begin(wifiSsid, wifiPassword);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    WiFi.setAutoReconnect(true);
    Serial.println(F("Connected to WiFi using stored credentials"));
    return true;
  }

  Serial.println(F("Failed to connect with stored WiFi credentials"));
  return false;
}

void setup()
{
  Serial.begin(115200);

  bool forceConfig = false;

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset())
  {
    Serial.println(F("Forcing config mode as there was a Double reset detected"));
    forceConfig = true;
  }

  spotifyDisplay->displaySetup(&spotify);

#ifdef NFC_ENABLED
  if (nfcSetup(&spotify, spotifyDisplay))
  {
    Serial.println("NFC Good");
  }
  else
  {
    Serial.println("NFC Bad");
  }
#endif

  // Initialise SPIFFS, if this fails try .begin(true)
  // NOTE: I believe this formats it though it will erase everything on
  // spiffs already! In this example that is not a problem.
  // I have found once I used the true flag once, I could use it
  // without the true flag after that.
  bool spiffsInitSuccess = SPIFFS.begin(false) || SPIFFS.begin(true);
  if (!spiffsInitSuccess)
  {
    Serial.println("SPIFFS initialisation failed!");
    while (1)
      yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nInitialisation done.");

  refreshToken[0] = '\0';
  wifiSsid[0] = '\0';
  wifiPassword[0] = '\0';
  bool haveStoredConfig = fetchConfigFile(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
  if (!haveStoredConfig)
  {
    // Failed to fetch config file, need to launch Wifi Manager
    forceConfig = true;
  }

  bool wifiConnected = false;
  if (!forceConfig)
  {
    wifiConnected = connectToStoredWifi();
  }

  if (!wifiConnected)
  {
    setupWiFiManager(forceConfig, refreshToken, &saveConfigFile, &drawWifiManagerMessage);
    wifiConnected = WiFi.isConnected();
  }

  if (!wifiConnected)
  {
    Serial.println(F("Failed to connect to WiFi"));
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // If we are here we should be connected to the Wifi
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  spotifySetup(spotifyDisplay, clientId, clientSecret);

#if defined YELLOW_DISPLAY

  pinMode(0, INPUT); // has an internal pullup
  bool forceRefreshToken = digitalRead(0) == LOW;
  if (forceRefreshToken)
  {
    Serial.println("GPIO 0 is low, forcing refreshToken");
  }

#else
  bool forceRefreshToken = false;

#endif

  // Check if we have a refresh Token
  if (forceRefreshToken || refreshToken[0] == '\0')
  {

    spotifyDisplay->drawRefreshTokenMessage();
    Serial.println("Launching refresh token flow");
    if (launchRefreshTokenFlow(&spotify, clientId))
    {
      Serial.print("Refresh Token: ");
      Serial.println(refreshToken);
      saveConfigFile(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
    }
  }

  spotifyRefreshToken(refreshToken);

  spotifyDisplay->showDefaultScreen();
}

void loop()
{
  drd->loop();

  spotifyDisplay->checkForInput();

  bool forceUpdate = false;

#ifdef NFC_ENABLED
  if (writeContextToNfc)
  {
    forceUpdate = nfcLoop(lastTrackUri, lastTrackContextUri);
  }
  else
  {
    forceUpdate = nfcLoop(lastTrackUri);
  }

#endif

  updateCurrentlyPlaying(forceUpdate);

  updateProgressBar();
}

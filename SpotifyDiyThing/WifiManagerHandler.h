#include <cstdio>
#include <cstring>
// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define WM_CLIENT_ID_LABEL "clientID"
#define WM_CLIENT_SECRET_LABEL "clientSecret"
#define WM_REFRESH_TOKEN_LABEL "refreshToken"

#ifndef WIFI_SSID_FIELD_LENGTH
#define WIFI_SSID_FIELD_LENGTH 33
#endif

#ifndef WIFI_PASSWORD_FIELD_LENGTH
#define WIFI_PASSWORD_FIELD_LENGTH 65
#endif

char wifiSsid[WIFI_SSID_FIELD_LENGTH];
char wifiPassword[WIFI_PASSWORD_FIELD_LENGTH];

DoubleResetDetector* drd;

char clientId[50];
char clientSecret[50];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWiFiManager(bool forceConfig, char *refreshToken, void (*saveConfig)(char *, char *, char *, const char *, const char *), void (*configModeCallback)(WiFiManager *myWiFiManager)){
  WiFiManager wm;
  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);  

  WiFiManagerParameter clientIdParam(WM_CLIENT_ID_LABEL, "Client ID", clientId, 40);
  WiFiManagerParameter clientSecretParam(WM_CLIENT_SECRET_LABEL, "Client Secret", clientSecret, 40);
  WiFiManagerParameter clientRefreshToken(WM_REFRESH_TOKEN_LABEL, "Refresh Token (optional)", refreshToken, 399);

  wm.addParameter(&clientIdParam);
  wm.addParameter(&clientSecretParam);
  wm.addParameter(&clientRefreshToken);

  if (forceConfig) {
    // IF we forced config this time, lets stop the double reset so it doesn't get stuck in a loop
    drd->stop();
    if (!wm.startConfigPortal("SpotifyDIY", "thing123")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  } else {
    if (!wm.autoConnect("SpotifyDIY", "thing123")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {

    strncpy(clientId, clientIdParam.getValue(), 40);
    strncpy(clientSecret, clientSecretParam.getValue(), 40);
    strncpy(refreshToken, clientRefreshToken.getValue(), 399);

    String currentSsid = wm.getWiFiSSID(true);
    String currentPass = wm.getWiFiPass(true);

    snprintf(wifiSsid, WIFI_SSID_FIELD_LENGTH, "%s", currentSsid.c_str());
    snprintf(wifiPassword, WIFI_PASSWORD_FIELD_LENGTH, "%s", currentPass.c_str());

    saveConfig(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
    drd->stop();
    ESP.restart();
    delay(5000);
  }
  else
  {
    if (wifiSsid[0] == '\0')
    {
      String currentSsid = wm.getWiFiSSID(true);
      snprintf(wifiSsid, WIFI_SSID_FIELD_LENGTH, "%s", currentSsid.c_str());
    }

    if (wifiPassword[0] == '\0')
    {
      String currentPass = wm.getWiFiPass(true);
      snprintf(wifiPassword, WIFI_PASSWORD_FIELD_LENGTH, "%s", currentPass.c_str());
    }
  }
  
}

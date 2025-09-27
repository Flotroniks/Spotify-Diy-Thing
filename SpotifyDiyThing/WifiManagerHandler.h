
// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#include <cstring>

#define WM_CLIENT_ID_LABEL "clientID"
#define WM_CLIENT_SECRET_LABEL "clientSecret"
#define WM_REFRESH_TOKEN_LABEL "refreshToken"

#define CLIENT_ID_MAX_LEN 50
#define CLIENT_SECRET_MAX_LEN 50
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWORD_MAX_LEN 63

DoubleResetDetector *drd;

char clientId[CLIENT_ID_MAX_LEN];
char clientSecret[CLIENT_SECRET_MAX_LEN];
char wifiSsid[WIFI_SSID_MAX_LEN + 1];
char wifiPassword[WIFI_PASSWORD_MAX_LEN + 1];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWiFiManager(bool forceConfig, char *refreshToken, void (*saveConfig)(char *, char *, char *, char *, char *), void (*configModeCallback)(WiFiManager *myWiFiManager)){
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

  if (WiFi.status() == WL_CONNECTED)
  {
    String connectedSsid = WiFi.SSID();
    strncpy(wifiSsid, connectedSsid.c_str(), WIFI_SSID_MAX_LEN);
    wifiSsid[WIFI_SSID_MAX_LEN] = '\0';

    String connectedPassword = WiFi.psk();
    strncpy(wifiPassword, connectedPassword.c_str(), WIFI_PASSWORD_MAX_LEN);
    wifiPassword[WIFI_PASSWORD_MAX_LEN] = '\0';
  }

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {

    strncpy(clientId, clientIdParam.getValue(), 40);
    clientId[40 - 1] = '\0';
    strncpy(clientSecret, clientSecretParam.getValue(), 40);
    clientSecret[40 - 1] = '\0';
    strncpy(refreshToken, clientRefreshToken.getValue(), 399);
    refreshToken[399] = '\0';

    if (WiFi.status() == WL_CONNECTED)
    {
      String connectedSsid = WiFi.SSID();
      strncpy(wifiSsid, connectedSsid.c_str(), WIFI_SSID_MAX_LEN);
      wifiSsid[WIFI_SSID_MAX_LEN] = '\0';

      String connectedPassword = WiFi.psk();
      strncpy(wifiPassword, connectedPassword.c_str(), WIFI_PASSWORD_MAX_LEN);
      wifiPassword[WIFI_PASSWORD_MAX_LEN] = '\0';
    }

    saveConfig(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
    drd->stop();
    ESP.restart();
    delay(5000);
  }

}

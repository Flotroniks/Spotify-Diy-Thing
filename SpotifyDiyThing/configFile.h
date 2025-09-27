#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>

#include <cstring>

#define SPOTIFY_CONFIG_FILE "/spotify_diy_config.yaml"
#define LEGACY_SPOTIFY_CONFIG_JSON "/spotify_diy_config.json"

#define REFRESH_TOKEN_LABEL "refreshToken"
#define CLIENT_ID_LABEL "clientId"
#define CLIENT_SECRET_LABEL "clientSecret"
#define WIFI_SSID_LABEL "wifiSsid"
#define WIFI_PASSWORD_LABEL "wifiPassword"

extern bool g_sdCardAvailable;

inline void safeStringCopy(char *destination, size_t destinationLength, const char *source)
{
  if (destination == nullptr || destinationLength == 0)
  {
    return;
  }

  if (source == nullptr)
  {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, destinationLength - 1);
  destination[destinationLength - 1] = '\0';
}

inline String decodeDoubleQuotedValue(const String &value)
{
  String inner = value.substring(1, value.length() - 1);
  String result;
  result.reserve(inner.length());

  for (size_t i = 0; i < inner.length(); ++i)
  {
    char c = inner.charAt(i);
    if (c == '\\' && i + 1 < inner.length())
    {
      char next = inner.charAt(i + 1);
      switch (next)
      {
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case '\\':
        result += '\\';
        break;
      case '"':
        result += '"';
        break;
      default:
        result += next;
        break;
      }
      ++i;
    }
    else
    {
      result += c;
    }
  }

  return result;
}

inline String decodeSingleQuotedValue(const String &value)
{
  String inner = value.substring(1, value.length() - 1);
  inner.replace("''", "'");
  return inner;
}

inline String normalizeYamlValue(String value)
{
  value.trim();
  if (value.length() == 0)
  {
    return "";
  }

  if (value.startsWith("\"") && value.endsWith("\"") && value.length() >= 2)
  {
    return decodeDoubleQuotedValue(value);
  }

  if (value.startsWith("'") && value.endsWith("'") && value.length() >= 2)
  {
    return decodeSingleQuotedValue(value);
  }

  int commentIndex = value.indexOf('#');
  if (commentIndex >= 0)
  {
    String beforeComment = value.substring(0, commentIndex);
    beforeComment.trim();
    value = beforeComment;
  }

  value.trim();
  return value;
}

inline bool parseYamlConfig(File &configFile, const char *fsLabel,
                            char *refreshToken, size_t refreshTokenLen,
                            char *clientId, size_t clientIdLen,
                            char *clientSecret, size_t clientSecretLen,
                            char *wifiSsid, size_t wifiSsidLen,
                            char *wifiPassword, size_t wifiPasswordLen)
{
  bool clientIdValid = false;
  bool clientSecretValid = false;
  bool refreshTokenSeen = false;
  bool wifiSsidSeen = false;
  bool wifiPasswordSeen = false;

  while (configFile.available())
  {
    String rawLine = configFile.readStringUntil('\n');
    rawLine.trim();

    if (rawLine.length() == 0)
    {
      continue;
    }

    if (rawLine.startsWith("\xEF\xBB\xBF"))
    {
      rawLine.remove(0, 3);
      rawLine.trim();
      if (rawLine.length() == 0)
      {
        continue;
      }
    }

    if (rawLine.startsWith("#"))
    {
      continue;
    }

    int colonIndex = rawLine.indexOf(':');
    if (colonIndex < 0)
    {
      continue;
    }

    String key = rawLine.substring(0, colonIndex);
    key.trim();

    if (key.length() == 0)
    {
      continue;
    }

    String value = rawLine.substring(colonIndex + 1);
    value = normalizeYamlValue(value);

    if (key.equals(REFRESH_TOKEN_LABEL))
    {
      if (refreshToken != nullptr && refreshTokenLen > 0)
      {
        safeStringCopy(refreshToken, refreshTokenLen, value.c_str());
      }
      refreshTokenSeen = true;
    }
    else if (key.equals(CLIENT_ID_LABEL))
    {
      if (clientId != nullptr && clientIdLen > 0)
      {
        safeStringCopy(clientId, clientIdLen, value.c_str());
      }
      clientIdValid = true;
    }
    else if (key.equals(CLIENT_SECRET_LABEL))
    {
      if (clientSecret != nullptr && clientSecretLen > 0)
      {
        safeStringCopy(clientSecret, clientSecretLen, value.c_str());
      }
      clientSecretValid = true;
    }
    else if (key.equals(WIFI_SSID_LABEL))
    {
      if (wifiSsid != nullptr && wifiSsidLen > 0)
      {
        safeStringCopy(wifiSsid, wifiSsidLen, value.c_str());
      }
      wifiSsidSeen = true;
    }
    else if (key.equals(WIFI_PASSWORD_LABEL))
    {
      if (wifiPassword != nullptr && wifiPasswordLen > 0)
      {
        safeStringCopy(wifiPassword, wifiPasswordLen, value.c_str());
      }
      wifiPasswordSeen = true;
    }
  }

  if (!refreshTokenSeen && refreshToken != nullptr && refreshTokenLen > 0)
  {
    refreshToken[0] = '\0';
  }

  if (!wifiSsidSeen && wifiSsid != nullptr && wifiSsidLen > 0)
  {
    wifiSsid[0] = '\0';
  }

  if (!wifiPasswordSeen && wifiPassword != nullptr && wifiPasswordLen > 0)
  {
    wifiPassword[0] = '\0';
  }

  if (!clientIdValid || !clientSecretValid)
  {
    Serial.println("Config missing client ID or Secret");
    return false;
  }

  Serial.print("Parsed YAML config from ");
  Serial.println(fsLabel);
  return true;
}

inline bool parseJsonConfig(File &configFile, const char *fsLabel,
                            char *refreshToken, size_t refreshTokenLen,
                            char *clientId, size_t clientIdLen,
                            char *clientSecret, size_t clientSecretLen,
                            char *wifiSsid, size_t wifiSsidLen,
                            char *wifiPassword, size_t wifiPasswordLen)
{
  StaticJsonDocument<512> json;
  DeserializationError error = deserializeJson(json, configFile);

  if (error)
  {
    Serial.print("Failed to load json config from ");
    Serial.println(fsLabel);
    Serial.println(error.c_str());
    return false;
  }

  if (refreshToken != nullptr && refreshTokenLen > 0)
  {
    if (json.containsKey(REFRESH_TOKEN_LABEL))
    {
      safeStringCopy(refreshToken, refreshTokenLen, json[REFRESH_TOKEN_LABEL]);
    }
    else
    {
      refreshToken[0] = '\0';
    }
  }

  if (json.containsKey(CLIENT_ID_LABEL) && json.containsKey(CLIENT_SECRET_LABEL))
  {
    safeStringCopy(clientId, clientIdLen, json[CLIENT_ID_LABEL]);
    safeStringCopy(clientSecret, clientSecretLen, json[CLIENT_SECRET_LABEL]);
  }
  else
  {
    Serial.println("Config missing client ID or Secret");
    return false;
  }

  if (wifiSsid != nullptr && wifiSsidLen > 0)
  {
    if (json.containsKey(WIFI_SSID_LABEL))
    {
      safeStringCopy(wifiSsid, wifiSsidLen, json[WIFI_SSID_LABEL]);
    }
    else
    {
      wifiSsid[0] = '\0';
    }
  }

  if (wifiPassword != nullptr && wifiPasswordLen > 0)
  {
    if (json.containsKey(WIFI_PASSWORD_LABEL))
    {
      safeStringCopy(wifiPassword, wifiPasswordLen, json[WIFI_PASSWORD_LABEL]);
    }
    else
    {
      wifiPassword[0] = '\0';
    }
  }

  Serial.print("Parsed JSON config from ");
  Serial.println(fsLabel);
  return true;
}

inline bool fetchConfigFromFs(fs::FS &fs, const char *path, const char *fsLabel,
                              char *refreshToken, size_t refreshTokenLen,
                              char *clientId, size_t clientIdLen,
                              char *clientSecret, size_t clientSecretLen,
                              char *wifiSsid, size_t wifiSsidLen,
                              char *wifiPassword, size_t wifiPasswordLen,
                              bool *configWasJson = nullptr)
{
  if (!fs.exists(path))
  {
    Serial.print(fsLabel);
    Serial.print(" config file does not exist at ");
    Serial.println(path);
    return false;
  }

  Serial.print("Reading config file from ");
  Serial.print(fsLabel);
  Serial.print(": ");
  Serial.println(path);

  File formatProbe = fs.open(path, "r");
  if (!formatProbe)
  {
    Serial.print("Failed to open config file from ");
    Serial.println(fsLabel);
    return false;
  }

  int firstChar = -1;
  while (formatProbe.available())
  {
    int peeked = formatProbe.peek();
    if (peeked < 0)
    {
      break;
    }

    char c = static_cast<char>(peeked);
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
    {
      formatProbe.read();
      continue;
    }

    if (static_cast<unsigned char>(c) == 0xEF && formatProbe.available() >= 3)
    {
      char bom[3];
      size_t readCount = formatProbe.readBytes(bom, sizeof(bom));
      if (readCount == sizeof(bom) && static_cast<unsigned char>(bom[0]) == 0xEF && static_cast<unsigned char>(bom[1]) == 0xBB && static_cast<unsigned char>(bom[2]) == 0xBF)
      {
        continue;
      }
      else
      {
        firstChar = static_cast<unsigned char>(bom[0]);
        break;
      }
    }

    firstChar = static_cast<unsigned char>(formatProbe.read());
    break;
  }
  formatProbe.close();

  if (firstChar == -1)
  {
    Serial.print("Config file on ");
    Serial.print(fsLabel);
    Serial.println(" is empty.");
    return false;
  }

  bool fileIsJson = (firstChar == '{' || firstChar == '[');
  if (configWasJson != nullptr)
  {
    *configWasJson = fileIsJson;
  }

  File configFile = fs.open(path, "r");
  if (!configFile)
  {
    Serial.print("Failed to open config file from ");
    Serial.println(fsLabel);
    return false;
  }

  bool parsed = false;
  if (fileIsJson)
  {
    parsed = parseJsonConfig(configFile, fsLabel,
                             refreshToken, refreshTokenLen,
                             clientId, clientIdLen,
                             clientSecret, clientSecretLen,
                             wifiSsid, wifiSsidLen,
                             wifiPassword, wifiPasswordLen);
  }
  else
  {
    parsed = parseYamlConfig(configFile, fsLabel,
                             refreshToken, refreshTokenLen,
                             clientId, clientIdLen,
                             clientSecret, clientSecretLen,
                             wifiSsid, wifiSsidLen,
                             wifiPassword, wifiPasswordLen);
  }

  configFile.close();
  return parsed;
}

inline void writeYamlEscapedString(File &configFile, const char *value)
{
  configFile.print('"');

  if (value != nullptr)
  {
    while (*value != '\0')
    {
      char c = *value;
      switch (c)
      {
      case '\\':
        configFile.print("\\\\");
        break;
      case '"':
        configFile.print("\\\"");
        break;
      case '\n':
        configFile.print("\\n");
        break;
      case '\r':
        configFile.print("\\r");
        break;
      case '\t':
        configFile.print("\\t");
        break;
      default:
        configFile.print(c);
        break;
      }
      ++value;
    }
  }

  configFile.print('"');
}

inline void writeYamlField(File &configFile, const char *key, const char *value)
{
  configFile.print(key);
  configFile.print(": ");
  writeYamlEscapedString(configFile, value != nullptr ? value : "");
  configFile.print('\n');
}

inline bool saveConfigToFs(fs::FS &fs, const char *path, const char *fsLabel,
                           const char *refreshToken, const char *clientId,
                           const char *clientSecret, const char *wifiSsid,
                           const char *wifiPassword)
{
  File configFile = fs.open(path, "w");
  if (!configFile)
  {
    Serial.print("Failed to open config file for writing on ");
    Serial.println(fsLabel);
    return false;
  }

  configFile.println("# Spotify DIY Thing credentials");
  writeYamlField(configFile, CLIENT_ID_LABEL, clientId);
  writeYamlField(configFile, CLIENT_SECRET_LABEL, clientSecret);
  writeYamlField(configFile, REFRESH_TOKEN_LABEL, refreshToken);
  writeYamlField(configFile, WIFI_SSID_LABEL, wifiSsid);
  writeYamlField(configFile, WIFI_PASSWORD_LABEL, wifiPassword);
  configFile.flush();
  configFile.close();

  Serial.print("Saved config to ");
  Serial.print(fsLabel);
  Serial.print(" (");
  Serial.print(path);
  Serial.println(")");
  return true;
}

inline void removeLegacyConfig(fs::FS &fs, const char *fsLabel)
{
  if (fs.exists(LEGACY_SPOTIFY_CONFIG_JSON))
  {
    if (fs.remove(LEGACY_SPOTIFY_CONFIG_JSON))
    {
      Serial.print("Removed legacy JSON config from ");
      Serial.println(fsLabel);
    }
    else
    {
      Serial.print("Failed to remove legacy JSON config from ");
      Serial.println(fsLabel);
    }
  }
}

inline void saveConfigFile(char *refreshToken, char *clientId, char *clientSecret, char *wifiSsid, char *wifiPassword)
{
  Serial.println(F("Saving configuration to YAML"));

  bool spiffsSaved = saveConfigToFs(SPIFFS, SPOTIFY_CONFIG_FILE, "SPIFFS",
                                    refreshToken, clientId, clientSecret,
                                    wifiSsid, wifiPassword);
  if (spiffsSaved)
  {
    removeLegacyConfig(SPIFFS, "SPIFFS");
  }

  if (g_sdCardAvailable)
  {
    bool sdSaved = saveConfigToFs(SD, SPOTIFY_CONFIG_FILE, "SD",
                                  refreshToken, clientId, clientSecret,
                                  wifiSsid, wifiPassword);
    if (sdSaved)
    {
      removeLegacyConfig(SD, "SD");
    }
  }
}

inline bool fetchConfigFile(char *refreshToken, size_t refreshTokenLen,
                            char *clientId, size_t clientIdLen,
                            char *clientSecret, size_t clientSecretLen,
                            char *wifiSsid, size_t wifiSsidLen,
                            char *wifiPassword, size_t wifiPasswordLen)
{
  bool configWasJson = false;
  bool configLoaded = fetchConfigFromFs(SPIFFS, SPOTIFY_CONFIG_FILE, "SPIFFS",
                                        refreshToken, refreshTokenLen,
                                        clientId, clientIdLen,
                                        clientSecret, clientSecretLen,
                                        wifiSsid, wifiSsidLen,
                                        wifiPassword, wifiPasswordLen,
                                        &configWasJson);
  if (configLoaded && configWasJson)
  {
    Serial.println(F("SPIFFS config was JSON formatted; rewriting as YAML."));
    saveConfigFile(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
  }

  if (!configLoaded)
  {
    configWasJson = false;
    bool loadedJson = fetchConfigFromFs(SPIFFS, LEGACY_SPOTIFY_CONFIG_JSON, "SPIFFS",
                                        refreshToken, refreshTokenLen,
                                        clientId, clientIdLen,
                                        clientSecret, clientSecretLen,
                                        wifiSsid, wifiSsidLen,
                                        wifiPassword, wifiPasswordLen,
                                        &configWasJson);
    if (loadedJson)
    {
      Serial.println(F("Migrating SPIFFS config from JSON to YAML."));
      saveConfigFile(refreshToken, clientId, clientSecret, wifiSsid, wifiPassword);
      configLoaded = true;
    }
  }

  return configLoaded;
}

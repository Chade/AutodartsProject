#include <WiFiManager.h> 
WiFiManager wifiManager;
WiFiManagerParameter autodartsUsername("username", "Autodarts Username", "", 40);
WiFiManagerParameter autodartsPassword("password", "Autodarts Password", "", 20);

#include "AutodartsClient.h"
autodarts::Client client;

#include <SPIFFS.h>

#define LED_RED    32
#define LED_GREEN  33
#define LED_BLUE   14
#define LED_WHITE  12

bool paramsSave  = false;
bool paramsApply = false;



// Increase stack size
SET_LOOP_TASK_STACK_SIZE(16*1024); // 16KB

void onBoardConnectionCallback(const String& boardName, const String& boardId, bool connected) {
  if (connected) {
    LOG_INFO("Autodarts", F("Board '") << boardName << F("' connected"));
    digitalWrite(LED_RED, LOW);
  }
  else {
    LOG_INFO("Autodarts", F("Board '") << boardName << F("' disconnected"));
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_WHITE, LOW);
  }
}

void onCameraSystemStateCallback(const String& boardName, const String& boardId, autodarts::State opened, autodarts::State running) {
  if (running == autodarts::State::TRUE || running == autodarts::State::TURNED_TRUE) {
    LOG_INFO("Autodarts", F("Cameras started"));
    digitalWrite(LED_WHITE, HIGH);
  }
  else if(running == autodarts::State::FALSE || running == autodarts::State::TURNED_FALSE) {
    LOG_INFO("Autodarts", F("Cameras stopped"));
    digitalWrite(LED_WHITE, LOW);
  }
}

void onSaveWifiParams () {
  if (strlen(autodartsUsername.getValue()) != 0 && strlen(autodartsPassword.getValue()) != 0) {
    paramsSave = true;
    paramsApply = true;
  }
}

bool connect(uint8_t numRetries = 0) {
  uint8_t retries = 0;
  while (retries <= numRetries) {
    if (client.autoDetectBoards(autodartsUsername.getValue(), autodartsPassword.getValue()) == HTTP_CODE_OK) {
      client.openBoards();
      return true;
    }
    retries++;
  }
  return false;
}

bool saveParams() {
  bool ret = false;

  DynamicJsonDocument json(1024);
  json["autodarts_username"] = autodartsUsername.getValue();
  json["autodarts_password"] = autodartsPassword.getValue();

  File configFile = SPIFFS.open("/autodarts_config.json", "w");
  if (configFile) {
    serializeJsonPretty(json, Serial);
    serializeJson(json, configFile);
    ret = true;
  }
  else {
    LOG_ERROR("Autodarts", F("Failed to open config file for writing!"));
  }

  configFile.close();
  return ret;
}

bool loadParams() {
  bool ret = false;

  if (SPIFFS.exists("/autodarts_config.json")) {
    File configFile = SPIFFS.open("/autodarts_config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      char buffer[size];
      configFile.readBytes(buffer, size);

      DynamicJsonDocument json(1024);
      DeserializationError err = deserializeJson(json, buffer);
      if ( !err ) {
        serializeJsonPretty(json, Serial);
        autodartsUsername.setValue(json["autodarts_username"], 40);
        autodartsPassword.setValue(json["autodarts_password"], 20);
        ret = true;
      } else {
        LOG_ERROR("Autodarts", F("Failed to read json config!"));
      }
    }
    configFile.close();
  } else {
    LOG_ERROR("Autodarts", F("Could not open config file for reading!"));
  }
  return ret;
}

void setup() {
  Serial.begin(115200);

  // Init pins
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, HIGH);

  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);

  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, LOW);

  pinMode(LED_WHITE, OUTPUT);
  digitalWrite(LED_WHITE, LOW);

  // Try to load config from SPIFFS
  if (SPIFFS.begin()) {
    paramsApply = loadParams();
  }
  else {
    LOG_ERROR("Autodarts", F("Could not mount file system!"));
  }

  // Set wifi mode
  WiFi.mode(WIFI_STA);  

  // Add params for config portal
  const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  wifiManager.setMenu(menu,6);
  wifiManager.addParameter(&autodartsUsername);
  wifiManager.addParameter(&autodartsPassword);
  wifiManager.setSaveParamsCallback(onSaveWifiParams);
  
  // 
  wifiManager.setConfigPortalBlocking(false);

  // Automatically connect using saved credentials if they exist
  // If connection fails it starts an access point with the specified name
  if(wifiManager.autoConnect("AutodartsAP")){
    LOG_INFO("Autodarts", F("Wifi connected"));
  }
  else {
    LOG_INFO("Autodarts", F("Wifi portal running"));
  }

  // Start webportal
  wifiManager.startWebPortal();

  // Register callbacks
  client.onBoardConnection(onBoardConnectionCallback);
  client.onCameraSystemState(onCameraSystemStateCallback);
}

void loop() {
  wifiManager.process();

  if (paramsSave) {
    paramsSave = !saveParams();
  }

  if (paramsApply) {
    paramsApply = !connect(1);
  }

  client.updateBoards();
  delay(1);
}
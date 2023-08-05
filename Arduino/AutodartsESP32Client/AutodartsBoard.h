#ifndef AutodartsBoard_h_
#define AutodartsBoard_h_

#include <ArduinoJson.h>

#define ALTERNATE_WEBSOCKET
#ifdef ALTERNATE_WEBSOCKET
#include <WebSocketsClient.h>
#else
#include <ArduinoWebsockets.h>
#endif

#include "AutodartsDefines.h"
#include "AutodartsDetector.h"

namespace autodarts {

  class Board {
  public:
    Board() = delete;

    Board(const JsonObjectConst& json) :
      _detector(_name, _id) {
      fromJson(json);
    };

    Board(const String& name, const String& id, const String& version, const String& url) : 
      _name(name), _id(id), _version(version), _url(url), _detector(_name, _id) {

    };

    Board(const String& name, const String& id, const String& version, const IPAddress& address, uint16_t port = 3180) : 
      _name(name), _id(id), _version(version), _detector(_name, _id) {
      _url = address.toString() + ':' + String(port);
    };
    
    String getName() const {
      return _name;
    }

    void setName(const String& name) {
      _name = name;
    }

    String getId() const {
      return _id;
    }

    void setId(const String& id) {
      _id = id;
    }

    String getVersion() const {
      return _version;
    }

    void setVersion(const String& version) {
      _version = version;
    }
    
    String getUrl() const {
      return _url;
    }
    
    void setUrl(const String& url) {
      _url = url;
    }

    bool isOpen() const {
      return _isOpen;
    }

#ifdef ALTERNATE_WEBSOCKET
    bool open(bool force = false) {
      // Check if already open
      if (!force && isOpen()) {
        return true;
      }

      // Check if url is valid
      if (_url.isEmpty()) {
        return false;
      }

      // Split url
      int index = _url.indexOf(':');
      String address = _url.substring(0, index);
      int port = _url.substring(index+1).toInt();

      // Open websocket
      LOG_DEBUG(_name.c_str(), F("Opening connection"));
      _websocket.begin(address, port, "/api/events");

      // Register event callback
      _websocket.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
        switch(type) {
          case WStype_CONNECTED: {
            _isOpen = true;
            _onBoardConnectionCallback(_name, _id, _isOpen);
            break;
          }
          case WStype_DISCONNECTED: {
            _isOpen = false;
            _onBoardConnectionCallback(_name, _id, _isOpen);
            break;
          }
          case WStype_TEXT: {
            LOG_DEBUG(_name.c_str(), F("Received data"));
            DynamicJsonDocument json(2048);
            deserializeJson(json, payload);
            _detector.fromJson(json.as<JsonObjectConst>());
            break;
          }
          case WStype_BIN:
          case WStype_ERROR:		
          case WStype_FRAGMENT_TEXT_START:
          case WStype_FRAGMENT_BIN_START:
          case WStype_FRAGMENT:
          case WStype_FRAGMENT_FIN:
            break;
        }
        resetAlive();
      });

      // try ever 5000 again if connection has failed
      _websocket.setReconnectInterval(5000);

      return true;
    }
#else
    bool open(bool force = false) {
      // Check if already open
      if (!force && isOpen()) {
        return true;
      }

      // Check if url is valid
      if (_url.isEmpty()) {
        return false;
      }

      // Register message callback
      _websocket.onMessage([this](websockets::WebsocketsMessage message) {
        LOG_DEBUG(_name.c_str(), F("Received data"));
        DynamicJsonDocument json(2048);
        deserializeJson(json, message.data());
        _detector.fromJson(json.as<JsonObjectConst>());
        resetAlive();
      });

      // Register event callback
      _websocket.onEvent([this](websockets::WebsocketsEvent event, String data) {
        if(event == websockets::WebsocketsEvent::ConnectionOpened) {
            LOG_DEBUG(_name.c_str(), F("Connection opened"));
            _isOpen = true;
            _onConnectionCallback(_name, _id, _isOpen);
        } else if(event == websockets::WebsocketsEvent::ConnectionClosed) {
            LOG_DEBUG(_name.c_str(), F("Connection closed"));
            _isOpen = false;
            _onConnectionCallback(_name, _id, _isOpen);
        }
        resetAlive();
      });

      // Open websocket
      LOG_DEBUG(_name.c_str(), F("Opening connection"));
      char websocketUrl[48];
      sprintf(websocketUrl, AUTODARTS_WS_LOCAL_URL, _url.c_str());
      return _websocket.connect(websocketUrl);
    }
#endif



    void close() {
      
    LOG_DEBUG(_name.c_str(), F("Closing connection"));
#ifdef ALTERNATE_WEBSOCKET
      _websocket.disconnect();
#else
      _websocket.close();
#endif
      _isOpen = false;
    }

    bool update() {
#ifdef ALTERNATE_WEBSOCKET
      _websocket.loop();
#else
      if(_websocket.available() && _websocket.poll()) {
        return true;
      }
#endif

      if (isOpen() && !isAlive()) {
        LOG_ERROR(_name.c_str(), F("Connection timeout!"));
        close();
      }

      return false;
    }

    bool isAlive() const {
      return (millis() - _lastAlive) < 10000;
    }

    void resetAlive() {
      _lastAlive = millis();
    }

    void fromJson(const JsonObjectConst& root) {
      _id      = String(root["id"].as<const char*>());
      _name    = String(root["name"].as<const char*>());
      _url     = String(root["ip"].as<const char*>());
      _version = String(root["version"].as<const char*>());
    }

    void toJson(JsonObject& root) const {
      root["id"]      = _id.c_str();
      root["name"]    = _name.c_str();
      root["ip"]      = _url.c_str();
      root["version"] = _version.c_str();
    }

    void onBoardConnection(BoardConnectionCallback callback) {
      _onBoardConnectionCallback = callback;
    }

    void onCameraStats(CameraStatsCallback callback) {
      _onCameraStatsCallback = callback;
      _detector.onCameraStats(_onCameraStatsCallback);
    }

    void onCameraSystemState(CameraSystemStateCallback callback) {
      _onCameraSystemStateCallback = callback;
      _detector.onCameraSystemState(_onCameraSystemStateCallback);
    }

    void onDetectionStats(DetectionStatsCallback callback) {
      _onDetectionStatsCallback = callback;
      _detector.onDetectionStats(_onDetectionStatsCallback);
    }

    void onDetectionState(DetectionStateCallback callback) {
      _onDetectionStateCallback = callback;
      _detector.onDetectionState(_onDetectionStateCallback);
    }

    void onDetectionEvent(DetectionEventCallback callback) {
      _onDetectionEventCallback = callback;
      _detector.onDetectionEvent(_onDetectionEventCallback);
    }

  private:
    String _name = "";
    String _id = "";
    String _url = "";
    String _version = "";
    bool _isOpen = false;
    uint64_t _lastAlive = 0;
    Detector _detector;

#ifdef ALTERNATE_WEBSOCKET
    WebSocketsClient _websocket;
#else
    websockets::WebsocketsClient _websocket;
#endif

    BoardConnectionCallback   _onBoardConnectionCallback   = [](const String&, const String&, bool){};
    CameraStatsCallback       _onCameraStatsCallback       = [](const String&, const String&, int8_t, int8_t, int16_t, int16_t){};
    CameraSystemStateCallback _onCameraSystemStateCallback = [](const String&, const String&, State, State){};
    DetectionStatsCallback    _onDetectionStatsCallback    = [](const String&, const String&, int8_t, int16_t, int16_t){};
    DetectionStateCallback    _onDetectionStateCallback    = [](const String&, const String&, State, State,  int16_t){};
    DetectionEventCallback    _onDetectionEventCallback    = [](const String&, const String&, Status::Code, Event::Code){};
  };
} // autodarts


#endif // AutodartsBoard_h_

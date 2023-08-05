#include "WString.h"
#ifndef AutodartsDetector_h_
#define AutodartsDetector_h_

#include <ArduinoJson.h>

#include "AutodartsDefines.h"
#include "AutodartsCameras.h"

namespace autodarts {

  class Detector {
  public:
    Detector() = delete;

    Detector(const String& boardName, const String& boardId) :
      _boardName(boardName), _boardId(boardId), _cameraSystem(boardName, boardId) {

    }

    String getBoardId() const {
      return _boardId;
    }

    String getBoardName() const {
      return _boardName;
    }

    bool isConnected() const {
      return _isConnected;
    }

    bool isRunning() const {
      return _isRunning;
    }

    int16_t getNumThrows() const {
      return _numThrows;
    }

    int8_t getFPS() const {
      return _fps;
    }

    int16_t getWidth() const {
      return _width;
    }

    int16_t getHeight() const {
      return _height;
    }

    Status getStatus() const {
      return _status;  
    }

    Event getEvent() const {
      return _event;
    }

    CameraSystem& getCameraSystem() {
      return _cameraSystem;
    }

    void fromJson(const JsonObjectConst& root) {
      if (root["type"] == "state") {
        _wasConnected = _isConnected;
        _wasRunning   = _isRunning;

        _isConnected = root["data"]["connected"];
        _isRunning   = root["data"]["running"];
        _numThrows   = root["data"]["numThrows"];
        _status.fromString(root["data"]["status"]);
        _event.fromString(root["data"]["event"]);

        State connected = static_cast<State>(2*_isConnected - _wasConnected);
        State running   = static_cast<State>(2*_isRunning   - _wasRunning);
        _onDetectionStateCallback(_boardName, _boardId, connected, running, _numThrows);
        _onDetectionEventCallback(_boardName, _boardId, _status.value(), _event.value());
      }
      else if (root["type"] == "stats") {
        _fps    = root["data"]["fps"];
        _width  = root["data"]["resolution"]["width"];
        _height = root["data"]["resolution"]["height"];
        _onDetectionStatsCallback(_boardName, _boardId, _fps, _width, _height);
      }
      else if (root["type"] == "motion_state") {
        LOG_WARNING(__FUNCTION__, F("Deserialization of motion state not implemented yet!"));
        serializeJsonPretty(root, Serial);
      }
      else {
        _cameraSystem.fromJson(root);
      }
    }

    void toJson(JsonObject& root) const {
      JsonObject data = root.createNestedObject("data");
      data["connected"] = _isConnected;
      data["running"]   = _isRunning;
      data["status"]    = _status.toString();
      data["event"]     = _event.toString();
      data["numThrows"] = _numThrows;
      root["type"]      = "state";
    }

    void onCameraStats(CameraStatsCallback callback) {
      _onCameraStatsCallback = callback;
      _cameraSystem.onCameraStats(_onCameraStatsCallback);
    }

    void onCameraSystemState(CameraSystemStateCallback callback) {
      _onCameraSystemStateCallback = callback;
      _cameraSystem.onCameraSystemState(_onCameraSystemStateCallback);
    }

    void onDetectionStats(DetectionStatsCallback callback) {
      _onDetectionStatsCallback = callback;
    }

    void onDetectionState(DetectionStateCallback callback) {
      _onDetectionStateCallback = callback;
    }

    void onDetectionEvent(DetectionEventCallback callback) {
      _onDetectionEventCallback = callback;
    }

  private:
    CameraSystem _cameraSystem;
    
    const String& _boardName, _boardId;
    
    bool _isConnected = false;
    bool _isRunning = false;
    bool _wasConnected = false;
    bool _wasRunning = false;
    int16_t _numThrows = -1;
    int8_t  _fps = -1;
    int16_t _width = -1;
    int16_t _height = -1;

    Status _status = Status::Code::UNKNOWN;
    Event _event = Event::Code::UNKNOWN;

    CameraStatsCallback       _onCameraStatsCallback       = [](const String&, const String&, int8_t, int8_t, int16_t, int16_t){};
    CameraSystemStateCallback _onCameraSystemStateCallback = [](const String&, const String&, State, State){};
    DetectionStatsCallback    _onDetectionStatsCallback    = [](const String&, const String&, int8_t, int16_t, int16_t){};
    DetectionStateCallback    _onDetectionStateCallback    = [](const String&, const String&, State, State,  int16_t){};
    DetectionEventCallback    _onDetectionEventCallback    = [](const String&, const String&, Status::Code, Event::Code){};
  };

} // autodarts


#endif // AutodartsDetector_h_

#ifndef AWSConfig_h
#define AWSConfig_h

#include "ZentserSensor.hpp"

#if defined(ESP8266)

#include <ESP8266WiFi.h>

#elif defined(ESP32)

#include <WiFi.h>
#include <WiFiClientSecure.h>

#else
#error "This ain't a ESP8266 or ESP32!"
#endif

#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <PubSubClient.h>

class AWSConfig {
  ///////////
  private:
  ///////////
  const char *awsEndpoint = "a3k0qrpvxv7791-ats.iot.us-west-1.amazonaws.com";
  const uint16_t MQTT_BUFFER_SIZE = 1024 * 2;
  // PUB SUB CLIENT TOPICS
  String OUT_TOPIC_PREFIX = "zentser/device/";
  String OUT_TOPIC_SUFFIX = "/message";
  String IN_TOPIC_PREFIX = "zentser/device/";
  String IN_TOPIC_SUFFIX = "/event";
  // state variables
  String deviceId;
  String sensorId;
  Sensor *sensorsArray;
  size_t sensorsArraySize;
  float sensorMinLimit = NAN;
  float sensorMaxLimit = NAN;
  unsigned long lastPublish;

#if defined(ESP8266)

  BearSSL::WiFiClientSecure wiFiClient = BearSSL::WiFiClientSecure();

#elif defined(ESP32)

  WiFiClientSecure wiFiClient = WiFiClientSecure();

#else
#error "This ain't a ESP8266 or ESP32!"
#endif

  PubSubClient pubSubClient;
  //sensors params
  unsigned int sendDataInterval = 60000 * 5;
  unsigned int readSensorInterval = 10000;
  unsigned short attemptCount = 3;

  // sent telemetry immediately if it needs
  bool sendNow = true;

  // methods
  void _setCurrentTime() {
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
  }

  void _pubSubCheckConnect() {
    if (!pubSubClient.connected()) {
      pubSubClient.setServer(awsEndpoint, 8883);
      pubSubClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
        this->_msgReceived(topic, payload, length);
      });
      pubSubClient.setClient(wiFiClient);
      pubSubClient.setKeepAlive((sendDataInterval / 1000) + 5);
      pubSubClient.setBufferSize(MQTT_BUFFER_SIZE);

      Serial.printf("PubSubClient connecting to: %s", awsEndpoint);
      while (!pubSubClient.connected()) {
        Serial.print(".");
        bool isConnected = pubSubClient.connect(deviceId.c_str());
        if (!isConnected) {
          Serial.printf("\nNot connected. Wait. State is: %d\n", pubSubClient.state());
          if (pubSubClient.state() == MQTT_CONNECTION_TIMEOUT) {
            Serial.println("It is possible a ploblem with certificates!");
          }

          char errBuf[200];

#if defined(ESP8266)
          int errorCode = wiFiClient.getLastSSLError(errBuf, 200);
#elif defined(ESP32)
          int errorCode = wiFiClient.lastError(errBuf, 200);
#else
#error "This ain't a ESP8266 or ESP32!"
#endif
          if (errorCode != 0) {
            Serial.printf("SSL error code: %d\n", errorCode);
            Serial.println(errBuf);
          } else {
            Serial.println("Connection error");
          }
          delay(1000);
        }
        yield();
      }
      Serial.println("Connected!");
      String topic = IN_TOPIC_PREFIX + deviceId + IN_TOPIC_SUFFIX;
      Serial.print("Subscribe topic: ");
      Serial.println(topic);
      pubSubClient.subscribe(topic.c_str());
    }
  }

  void _msgReceived(char *topic, byte *payload, unsigned int length) {
    Serial.printf("Message received on %s\n", topic);

    DynamicJsonDocument doc(MQTT_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("Parsing settings from AWS failed: ");
      Serial.println(error.f_str());
      return;
    }

    JsonVariant sendInterval = doc["send_data_interval"];
    if (!sendInterval.isNull()) {
      sendDataInterval = sendInterval;
      Serial.printf("Received sendDataInterval: %u\n", sendDataInterval);
    }

    JsonVariant readInterval = doc["check_sensor_interval"];
    if (!readInterval.isNull()) {
      readSensorInterval = readInterval;
      Serial.printf("Received readSensorInterval: %u\n", readSensorInterval);
    }

    JsonVariant count = doc["attempt_count"];
    if (!count.isNull()) {
      attemptCount = count;
      Serial.printf("Received attemptCount: %d\n", attemptCount);
    }

    JsonArray arr = doc["sensors"].as<JsonArray>();

    char *pch;
    for (JsonObject sensor : arr) {
      const char *type = sensor["default_type"];

      Serial.print("Sensor type: ");
      Serial.println(type);

      pch = strstr(type, "custom");
      if (pch != NULL) {
        String min = sensor["min_value"];
        sensorMinLimit = _parseStringToFloat(min);
        String max = sensor["max_value"];
        sensorMaxLimit = _parseStringToFloat(max);
        Serial.printf("Received min: %0.1f\n", sensorMinLimit);
        Serial.printf("Received max: %0.1f\n", sensorMaxLimit);

        for (size_t i = 0; i < sensorsArraySize; ++i) {
          if (sensorsArray[i].id == sensor["id"].as<String>()) {
            sensorsArray[i].alarmMinLimit = sensorMinLimit;
            sensorsArray[i].alarmMaxLimit = sensorMaxLimit;
          }
        }
      }
    }

    // show errors if they are
    JsonArray errors = doc["errors"].as<JsonArray>();
    for (JsonObject error : errors) {
      Serial.printf("Error: %s\n", error["kind"].as<const char *>());
      Serial.println(error["message"].as<const char *>());
    }

    delay(500);
  }

  String _createJsonForPublishing(String sensorId, float value) {
    DynamicJsonDocument doc(256);

    doc[sensorId] = value;

    String jsonString;
    serializeJson(doc, jsonString);

    return jsonString;
  }

  String _createJsonForPublishing() {
    DynamicJsonDocument doc(256);

    for (size_t i = 0; i < sensorsArraySize; ++i) {
      doc[sensorsArray[i].id] = sensorsArray[i].value;
    }

    String jsonString;
    serializeJson(doc, jsonString);

    return jsonString;
  }

  bool _publishFloat(float value) {
    _pubSubCheckConnect();
    String msgData = _createJsonForPublishing(sensorId, value);
    String topic = OUT_TOPIC_PREFIX + deviceId + OUT_TOPIC_SUFFIX;

    bool published = pubSubClient.publish(topic.c_str(), msgData.c_str());
    if (wiFiClient.available()) {
      wiFiClient.flush();
      Serial.println("wiFiClient flushed!");
    }

    if (published) {
      Serial.print("Published: ");
      Serial.println(msgData);
    } else {
      Serial.printf("Can't publish value %f", value);
    }
    lastPublish = millis();

    return published;
  }

  bool _publishSensorsData() {

    _pubSubCheckConnect();
    String msgData = _createJsonForPublishing();
    String topic = OUT_TOPIC_PREFIX + deviceId + OUT_TOPIC_SUFFIX;

    bool published = pubSubClient.publish(topic.c_str(), msgData.c_str());
    if (wiFiClient.available()) {
      wiFiClient.flush();
      Serial.println("wiFiClient flushed!");
    }

    if (published) {
      Serial.print("Published: ");
      Serial.println(msgData);
    } else {
      Serial.printf("Can't publish value %s", msgData.c_str());
    }
    lastPublish = millis();

    return published;
  }

  inline float _parseStringToFloat(String value) {
    if (value == "null") {
      return NAN;
    } else {
      return value.toFloat();
    }
  }

  bool _isOutOfLimits(float value) {
    if (isnan(sensorMinLimit) && isnan(sensorMaxLimit)) {
      return false;
    }

    if ((value < sensorMinLimit) || (value > sensorMaxLimit)) {
      return true;
    }

    return false;
  }

  ///////////
  public:
  ///////////
  void setupAWSCertificates(const char *certificate, const char *privateKey, const char *caCertificate) {

    _setCurrentTime();

#if defined(ESP8266)

    wiFiClient.setTrustAnchors(new BearSSL::X509List(caCertificate));
    wiFiClient.setClientRSACert(new BearSSL::X509List(certificate), new BearSSL::PrivateKey(privateKey));

#elif defined(ESP32)

    wiFiClient.setCACert(caCertificate);
    wiFiClient.setCertificate(certificate);
    wiFiClient.setPrivateKey(privateKey);

#else
#error "This ain't a ESP8266 or ESP32!"
#endif
  }

  bool isTimeToSend() {
    if ((millis() - lastPublish > sendDataInterval) || (lastPublish == 0)) {
      return true;
    }
    return false;
  }

  bool sendTelemetryFloat(float value) {

    if (isnan(value)) {
      Serial.printf("Can't send data. Wrong value %f. Please check, that your sensor is connected properly.\n", value);
      return false;
    }

    if (_isOutOfLimits(value)) {
      if (sendNow) {
        Serial.println("Send data NOW!");
        _publishFloat(value);
        loop();
        sendNow = false;
        return true;
      }
    }

    if (isTimeToSend()) {
      if (!_isOutOfLimits(value)) {
        sendNow = true;
      }
      Serial.println("Send data by schedule.");
      _publishFloat(value);
      loop();
      return true;
    }

    return false;
  }

  /*!
  *  @brief  Send telemetry of all sensors
  */
  void sendSensorsTelemetry() {
    for (size_t i = 0; i < sensorsArraySize; ++i) {
      if (sensorsArray[i].isOutOfLimits() && sendNow) {
        Serial.println("Send data NOW!");
        _publishSensorsData();
        sendNow = false;
        loop();

        return;
      }
    }

    if (isTimeToSend()) {
      for (size_t i = 0; i < sensorsArraySize; ++i) {
        if (!sensorsArray[i].isOutOfLimits()) {
          sendNow = true;
          break;
        }
      }
      Serial.println("Send data by schedule.");
      _publishSensorsData();
      loop();
    }
  }

  inline void loop() {
    pubSubClient.loop();
  }

  void delayNextRead() {
    delay(readSensorInterval);
  }

  AWSConfig(String deviceId, String sensorId) {
    this->deviceId = deviceId;
    this->sensorId = sensorId;
  }

  template <size_t N>
  AWSConfig(String deviceId, Sensor (&array)[N]) {
    this->deviceId = deviceId;
    this->sensorsArray = array;
    this->sensorsArraySize = N;
  }

  ~AWSConfig() {}
};

#endif

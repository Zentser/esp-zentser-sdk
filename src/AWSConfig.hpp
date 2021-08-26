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
  String deviceId;
  String sensorId;
  // PUB SUB CLIENT TOPICS
  String OUT_TOPIC_PREFIX = "zentser/device/";
  String OUT_TOPIC_SUFFIX = "/message";
  String IN_TOPIC_PREFIX = "zentser/device/";
  String IN_TOPIC_SUFFIX = "/event";
  // state variables
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
  unsigned int sendDataInterval = 60000;
  unsigned int readSensorInterval = 10000;
  unsigned short attemptCount = 3;

  // sent telemetry immediately if it needs
  bool sendNow = true;

  // methods
  void setCurrentTime() {
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

  void pubSubCheckConnect() {
    if (!pubSubClient.connected()) {
      pubSubClient.setServer(awsEndpoint, 8883);
      pubSubClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
        this->msgReceived(topic, payload, length);
      });
      pubSubClient.setClient(wiFiClient);
      pubSubClient.setKeepAlive((sendDataInterval / 1000) + 5);
      pubSubClient.setBufferSize(1024);

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

  void msgReceived(char *topic, byte *payload, unsigned int length) {
    Serial.printf("Message received on %s\n", topic);

    DynamicJsonDocument doc(1024);
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
        sensorMinLimit = parseStringToFloat(min);
        String max = sensor["max_value"];
        sensorMaxLimit = parseStringToFloat(max);
        Serial.printf("Received min: %0.1f\n", sensorMinLimit);
        Serial.printf("Received max: %0.1f\n", sensorMaxLimit);
      }
    }

    delay(500);
  }

  String createJsonForPublishing(String sensorId, float value) {
    DynamicJsonDocument doc(256);

    doc[sensorId] = value;

    String jsonString;
    serializeJson(doc, jsonString);

    return jsonString;
  }

  bool publishFloat(float value) {
    pubSubCheckConnect();
    String msgData = createJsonForPublishing(sensorId, value);
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

  template <size_t N>
  bool publishSensorsData(Sensor (&array)[N]) {
    for (Sensor sensor : array) {
      Serial.printf("Sent %s id: %s value = %6.2f\n", sensor.name.c_str(), sensor.id.c_str(), sensor.value);
    }

    lastPublish = millis();

    return true;
  }

  inline float parseStringToFloat(String value) {
    if (value == "null") {
      return NAN;
    } else {
      return value.toFloat();
    }
  }

  bool isOutOfLimits(float value) {
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

    setCurrentTime();

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

    if (isOutOfLimits(value)) {
      if (sendNow) {
        Serial.println("Send data NOW!");
        publishFloat(value);
        loop();
        sendNow = false;
        return true;
      }
    }

    if (isTimeToSend()) {
      if (!isOutOfLimits(value)) {
        sendNow = true;
      }
      Serial.println("Send data by schedule.");
      publishFloat(value);
      loop();
      return true;
    }

    return false;
  }

  /*!
  *  @brief  Send telemetry of all sensors
  */
  template <size_t N>
  void sendSensorsTelemetry(Sensor (&array)[N]) {
    for (Sensor sensor : array) {
      if (sensor.isOutOfLimits() && sendNow) {
        Serial.println("Send data NOW!");
        publishSensorsData(array);
        sendNow = false;
        loop();

        return;
      }
    }

    if (isTimeToSend()) {
      for (Sensor sensor : array) {
        if (!sensor.isOutOfLimits()) {
          sendNow = true;
          break;
        }
      }
      Serial.println("Send data by schedule.");
      publishSensorsData(array);
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

  AWSConfig(String deviceId) {
    this->deviceId = deviceId;
  }

  ~AWSConfig() {}
};

#endif

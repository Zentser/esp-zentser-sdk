#ifndef AWSConfig_h
#define AWSConfig_h

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
  void setCurrentTime();

  void pubSubCheckConnect();

  void msgReceived(char *topic, byte *payload, unsigned int length);

  String createJsonForPublishing(String sensorId, float value);

  bool publishFloat(float value);

  inline float parseStringToFloat(String value) {
    if (value == "null") {
      return NAN;
    } else {
      return value.toFloat();
    }
  }

  bool isOutOfLimits(float value);

  ///////////
  public:
  ///////////
  void setupAWSCertificates(const char *certificate, const char *privateKey, const char *caCertificate);

  bool isTimeToSend();

  bool sendTelemetryFloat(float value);

  inline void loop() {
    pubSubClient.loop();
  }

  void delayNextRead();

  AWSConfig(String deviceId, String sensorId);
  ~AWSConfig();
};

#endif

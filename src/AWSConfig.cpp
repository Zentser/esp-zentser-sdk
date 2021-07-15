#include "AWSConfig.h"

void AWSConfig::setCurrentTime() {
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

void AWSConfig::setupAWSCertificates(const char *certificate, const char *privateKey, const char *caCertificate) {

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

void AWSConfig::msgReceived(char *topic, byte *payload, unsigned int length) {
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


bool AWSConfig::isOutOfLimits(float value) {
  if ((sensorMinLimit == NAN) && (sensorMaxLimit == NAN)) {
    return false;
  }

  if ((value < sensorMinLimit) || (value > sensorMaxLimit)) {
    return true;
  }

  return false;
}

void AWSConfig::pubSubCheckConnect() {
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

String AWSConfig::createJsonForPublishing(String sensorId, float value) {
  DynamicJsonDocument doc(256);

  doc[sensorId] = value;

  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}

bool AWSConfig::isTimeToSend() {
  if ((millis() - lastPublish > sendDataInterval) || (lastPublish == 0)) {
    return true;
  }
  return false;
}

bool AWSConfig::publishFloat(float value) {
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


bool AWSConfig::sendTelemetryFloat(float value) {
  
  if(isOutOfLimits(value)) {
    if (sendNow) {
      Serial.println("Send data NOW!");
      publishFloat(value);
      loop();
      sendNow = false;
      return true;
    }
  }

  if (isTimeToSend()) {
    if(!isOutOfLimits(value)) {
      sendNow = true;
    }
    Serial.println("Send data by schedule.");
    publishFloat(value);
    loop();
    return true;
  }
  
  return false;
}

void AWSConfig::delayNextRead() {
  delay(readSensorInterval);
}

AWSConfig::AWSConfig(String deviceId, String sensorId) {
  this->deviceId = deviceId;
  this->sensorId = sensorId;
}

AWSConfig::~AWSConfig() {}

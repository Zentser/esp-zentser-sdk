#include <Zentser_ESP_SDK.h>
/*#include <YOUR_SENSORS_LIBRARY> */

const int analogInPin = A0; 

int sensorValue = 0;
int sensorPercentValue = 0;

char const *ssid = "YOUR_WIFI";
char const *wiFiPassword = "YOUR_WIFI_PASSWORD";

// Replace on generated code from Zentser Application
// --- START ---
static const char caCert[] PROGMEM = R"EOF(
/* CA CERTIFICATE */
)EOF";
 
static const char cert[] PROGMEM = R"KEY(
/* CERTIFICATE */
)KEY";
 
static const char privateKey[] PROGMEM = R"KEY(
/* PRIVATE_KEY */
)KEY";
 
String deviceId = "YOUR_DEVICE_ID"; // Zentser Device ID
// Zentser Sensor ID
Sensor sensors[] {
  Sensor("YOUR_SENSOR_ID_0", "YOUR_SENSOR_NAME"),
};

// --- END ---

AWSConfig aws = AWSConfig(deviceId, sensors); // init AWS function

// Function to connect to WiFi
void connectToWiFi() {
  Serial.print("\nConnecting to AP ...");

  WiFi.begin(ssid, wiFiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to AP");
}

// initial function at starting your microcontroller (ESP, Arduino, etc)
void setup() {
  Serial.begin(115200); // init serial for logging

  connectToWiFi(); // call function to start WiFi connection

  //get AWS certificates to authenticate sending sensor data
  aws.setupAWSCertificates(cert, privateKey, caCert);

  delay(500);
}

// main loop for microcontroller where you will collect and send sensor data
void loop() {

  // Check Wifi connection is still active
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // MODIFY to get sensor readings from hardware connected to your microcontroller
  sensorValue = analogRead(analogInPin); 
  sensorPercentValue = map(sensorValue, 0, 1023, 0, 100);
  Serial.printf("sensorValue = %i\n", sensorValue);
  Serial.printf("sensorPercentValue = %i\n", sensorPercentValue);

  sensors[0].value = sensorPercentValue;

  aws.sendSensorsTelemetry();
  
  // check that we're not bombarding Zentser with too much data
  // don't want to bring the system down in unintended DDoS attack
  aws.delayNextRead();
}

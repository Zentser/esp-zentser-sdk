#include <Zentser_ESP_SDK.h>
/*#include <YOUR_SENSORS_LIBRARY> */
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>

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
String sensorId = "YOUR_SENSOR_ID"; // Zentser Sensor ID

// --- END ---

AWSConfig aws = AWSConfig(deviceId, sensorId); // init AWS function

// Initialize Your sensor
Adafruit_BMP280 bmp; // use I2C interface
Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();

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

  // connect and start your sensor
  if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
  // if previous doesn't work, try default value for address
  //if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    while (1) delay(2000);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  delay(500);
}

// main loop for microcontroller where you will collect and send sensor data
void loop() {

  // Check Wifi connection is still active
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // MODIFY to get sensor readings from hardware connected to your microcontroller
  sensors_event_t pressure_event;
  bmp_pressure->getEvent(&pressure_event);
  
  // if You prefer to see values in a different unit, just comment hPa line and uncomment another one
  Serial.printf("Pressure =  %8.2f hPa\n", pressure_event.pressure);  // in hPa
  //Serial.printf("Pressure =  %8.2f mmHg\n", pressure_event.pressure * 0.75);  // in mmHg
  //Serial.printf("Pressure =  %8.2f inHg\n", pressure_event.pressure * 0.03);  // in inHg

  aws.sendTelemetryFloat(pressure_event.pressure);  //in hPa
  //aws.sendTelemetryFloat(pressure_event.pressure * 0.75);  //in mmHg
  //aws.sendTelemetryFloat(pressure_event.pressure * 0.03);  //in inHg
  
  // check that we're not bombarding Zentser with too much data
  // don't want to bring the system down in unintended DDoS attack 
  aws.delayNextRead();
}

#include <Zentser_ESP_SDK.h>
/*#include <YOUR_SENSORS_LIBRARY> */
#include <DHT.h>

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
Sensor sensors[] {
  Sensor("YOUR_SENSOR_ID_0", "Temperature (F)"),
  Sensor("YOUR_SENSOR_ID_1", "Humidity (%)"),
  Sensor("YOUR_SENSOR_ID_2", "Heat Index (F)"),
}; 

// --- END ---

AWSConfig aws = AWSConfig(deviceId, sensors); // init AWS function

// Initialize Your sensor
// Digital pin connected to the DHT sensor
uint8_t DHTPIN = 14; //D5

// Uncomment whatever DHT sensor type you're using
//#define DHTTYPE DHT11 // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

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
  pinMode(DHTPIN, INPUT);
  dht.begin();

  delay(500);
}

// main loop for microcontroller where you will collect and send sensor data
void loop() {

  // Check Wifi connection is still active
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // MODIFY to get sensor readings from hardware connected to your microcontroller
  float t = dht.readTemperature(true);
  float h = dht.readHumidity();
  float hIdx = dht.computeHeatIndex(t, h);
  Serial.printf("t = %6.2f; h = %6.2f; heatIndex = %6.2f\n", t, h, hIdx);

  sensors[0].value = t;
  sensors[1].value = h;
  sensors[2].value = hIdx;

  aws.sendSensorsTelemetry();
  
  // check that we're not bombarding Zentser with too much data
  // don't want to bring the system down in unintended DDoS attack
  aws.delayNextRead();
}

#include <pgmspace.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include "DHT.h"
#include "Seeed_BMP280.h"
#include "arduino_secrets.h"

#define DHTPIN 2
#define DHTTYPE DHT22
#define DEBUG true

const unsigned long MAX_UNSIGNED_LONG = 4294967295;
const int BACKOFF_IN_MILLIS = 5000;
const long MILLIS_IN_SECOND = 1000;
const long SECONDS_IN_MINUTE = 60;
const double DATA_INTERVAL_IN_MIN = 5;
const double TIME_SYNC_INTERVAL_IN_MIN = 60;
const double DATA_INTERVAL_IN_MILLIS = MILLIS_IN_SECOND * SECONDS_IN_MINUTE * DATA_INTERVAL_IN_MIN;
const double TIME_SYNC_INTERVAL_IN_MILLIS = MILLIS_IN_SECOND * SECONDS_IN_MINUTE * TIME_SYNC_INTERVAL_IN_MIN;
const int TIME_FIELD_LENGTH = 25; // 24 characters plus a null terminator

const char ssid[]         = WIFI_SSID;
const char network_pass[] = WIFI_PASSWORD;
const char broker[]       = AWS_IOT_ENDPOINT;
const int  port           = 8883;
const char topic[]        = TOPIC;
const char client_id[]    = THINGNAME;
const char location[]     = LOCATION;
const char ntpServer1[]   = "pool.ntp.org";
const char ntpServer2[]   = "time.nist.gov";

// Class initialize
BMP280 bmp280;
DHT dht(DHTPIN, DHTTYPE);

WiFiClientSecure wifiClient = WiFiClientSecure();
MQTTClient mqttClient = MQTTClient(256);

// To send weather data at the start
unsigned long timeOfLastMessage = MAX_UNSIGNED_LONG - DATA_INTERVAL_IN_MILLIS;
unsigned long timeOfLastClockSync = MAX_UNSIGNED_LONG - TIME_SYNC_INTERVAL_IN_MILLIS;
JsonDocument sensorMetadata;

struct WeatherData {
  char time[TIME_FIELD_LENGTH];
  float temperature;
  float pressure;
  float humidity;
  float windSpeed;
  char windDirection[2];
  float luminosity;
  float uvIndex;
};

void setup() {
  // Start and wait for serial to begin
  Serial.begin(9600);
  while(!Serial) {
    delay(1000);
  }
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.println("Starting setup");

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, network_pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to the network");

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  wifiClient.setCACert(AWS_CERT_CA);
  wifiClient.setCertificate(AWS_CERT_CRT);
  wifiClient.setPrivateKey(AWS_CERT_PRIVATE);

  // Setup sensor metadata
  sensorMetadata["sensorId"] = client_id;
  sensorMetadata["location"] = location;
  sensorMetadata["tags"][0] = "test";

  // // Setup sensors
  if (!bmp280.init()) {
    Serial.println("Device error!");
  }
  dht.begin(); // Initialize the DHT sensor

  Serial.println("Done with setup!");
  Serial.println();
};

void loop() {
  unsigned long currentTime = millis();
  syncClock(currentTime);
  sendData(currentTime);
  
  delay(BACKOFF_IN_MILLIS);
}

/*
  Syncs the clock if there has been sufficient time since last clock sync
*/
void syncClock(unsigned long currentTime) {
  unsigned long timeSinceLastClockSync;
  if (currentTime < timeOfLastClockSync) {
    // Millis have looped back around
    timeSinceLastClockSync = (MAX_UNSIGNED_LONG - timeOfLastClockSync) + currentTime;
  } else {
    timeSinceLastClockSync = currentTime - timeOfLastClockSync;
  }

  if (timeSinceLastClockSync > TIME_SYNC_INTERVAL_IN_MILLIS) {
    Serial.println("Syncing time");
    configTime(0, 0, ntpServer1, ntpServer2);
    timeOfLastClockSync = currentTime;
    delay(BACKOFF_IN_MILLIS);
  } else if (DEBUG) {
    Serial.print("Skipping clock sync because time since last sync is ");
    Serial.print(timeSinceLastClockSync / MILLIS_IN_SECOND);
    Serial.println(" seconds.");
  }
}

/*
  Sends data using MQTT client if there has been sufficient time since the last time a message was sent
*/
void sendData(unsigned long currentTime) {
  unsigned long timeSinceLastMessage;
  if (currentTime < timeOfLastMessage) {
    // Millis have looped back around
    timeSinceLastMessage = (MAX_UNSIGNED_LONG - timeOfLastMessage) + currentTime;
  } else {
    timeSinceLastMessage = currentTime - timeOfLastMessage;
  }

  if (timeSinceLastMessage > DATA_INTERVAL_IN_MILLIS) {
    bool connected = connectToAwsIot();
    if (connected) {
      WeatherData * weatherData = (WeatherData *) malloc (sizeof(struct WeatherData));
      getWeatherData(weatherData);
      publishMessage(weatherData);
      free(weatherData);
      timeOfLastMessage = currentTime;
      mqttClient.disconnect();
      if (DEBUG) {
        Serial.print("Sent message at ");
        Serial.println(timeOfLastMessage);
      }
    } else {
      Serial.print("Failed to connect to broker, will retry");
    }
  } else if (DEBUG) {
    Serial.print("Skipping message because time since last message is ");
    Serial.print(timeSinceLastMessage / MILLIS_IN_SECOND);
    Serial.println(" seconds.");
  }
}

/*
  Connects the MQTT client to AWS IOT
*/
bool connectToAwsIot() {
  mqttClient.begin(broker, port, wifiClient);

  Serial.print("Connecting to AWS IOT");
  while (!mqttClient.connect(client_id)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqttClient.connected()) {
    Serial.println("AWS IoT Timeout!");
    return false;
  }
  Serial.println("AWS IoT Connected!");
  return true;
}

/*
  Fills out all the data required in the provided WeatherData struct
*/
void getWeatherData(WeatherData * data) {
  // Get time
  struct tm timeInfo;
  if (getLocalTime(&timeInfo)) { // No offset, use UTC
    memset(data->time, '\0', TIME_FIELD_LENGTH); // Zero out time string
    // 2024-07-09T17:48:47.043Z
    int year = timeInfo.tm_year + 1900; // The year in timeInfo is time since 1900
    int month = timeInfo.tm_mon + 1; // The month in timeInfo is months since January (0 indexed)
    sprintf(data->time, "%d-%02d-%02dT%02d:%02d:%02d.000Z", year, month, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    Serial.println(data->time);
    Serial.println(&timeInfo, "%A, %B %d %Y %H:%M:%S");
  } else {
    Serial.println("Failed to obtain time");
  }

  // Fill out sensor data
  float humidity  = dht.readHumidity();           // In percent
  float temperature = dht.readTemperature();      // In celsius
  // check if any reads failed 
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    data->temperature = temperature;
    data->humidity = humidity;
  }
  
  float pressure = bmp280.getPressure();          // In pascals
  // check if any reads failed
  if (isnan(pressure)) {
    Serial.println("Failed to read from BMP sensor!");
  } else {
    data->pressure = pressure * 0.01;             // Convert pascals to millibars
  }

  return;
}

/*
  Publishes a WeatherData struct to the MQTT client
*/
void publishMessage(WeatherData * data) {
  JsonDocument weatherData;
  weatherData["sensorMetadata"] = sensorMetadata;
  weatherData["time"] = data->time;
  weatherData["humidity"] = data->humidity;
  weatherData["pressure"] = data->pressure;
  weatherData["temperature"] = data->temperature;

  if (DEBUG) {
    Serial.print("Sending message to topic: ");
    Serial.println(topic);
    serializeJsonPretty(weatherData, Serial);
    Serial.println();
  }
  const unsigned long messageLength = measureJson(weatherData) + 1; // Add 1 for null terminator
  char jsonBuffer[messageLength];
  memset(jsonBuffer, '\0', messageLength); // Zero out buffer
  serializeJson(weatherData, jsonBuffer, messageLength);

  mqttClient.publish(topic, jsonBuffer);
}

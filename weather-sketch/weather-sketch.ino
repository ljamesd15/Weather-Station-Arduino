#include "DHT.h"
#include "Seeed_BMP280.h"
#include <Wire.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"

#define DHTPIN 2
#define DHTTYPE DHT22
#define DEBUG true

BMP280 bmp280;
DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const int BACKOFF_IN_MILLIS = 5000;
const long MILLIS_IN_SECONDS = 1000;
const long SECONDS_IN_MINUTE = 60;
const long DATA_INTERVAL_IN_MIN = 5;
const int TIME_LEN = 24;

const char ssid[] = NETWORK_SSID;
const char network_pass[] = NETWORK_PASSWORD;
const char mqtt_username[] = MQTT_USERNAME;
const char mqtt_password[] = MQTT_PASSWORD;
const char broker[] = "192.168.86.15";
const int  port     = 1883;
const char topic[]  = "weather";
const char client_id[] = "arduino1";

int count = 0;
bool sentMessage = false;
JsonDocument sensorMetadata;

struct WeatherData {
  char time[TIME_LEN];
  float temperature;
  float pressure;
  float humidity;
  float windSpeed;
  char windDirection[2];
  float luminosity;
  float uvIndex;
};

time_t getTime() {
  time_t timeInSeconds = WiFi.getTime();
  while (timeInSeconds == 0) {
    Serial.println("Date time not yet initalized, sleeping.");
    delay(BACKOFF_IN_MILLIS);
    timeInSeconds = WiFi.getTime();
  }
};

void setup() {
  Serial.begin(9600);
  if(!bmp280.init()){
    Serial.println("Device error!");
  }
  dht.begin(); // Initialize the DHT sensor

  // Attempt to connect to Wifi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, network_pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(BACKOFF_IN_MILLIS);
  }
  Serial.println("You're connected to the network");

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.print(broker);
  Serial.print(":");
  Serial.println(port);

  mqttClient.setId(client_id);
  mqttClient.setUsernamePassword(mqtt_username, mqtt_password);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    int connect_error = mqttClient.connectError();
    Serial.println(connect_error);
    Serial.println(connect_error < 0);

    while (1);
  }
  Serial.println("You're connected to the MQTT broker!");

  sensorMetadata["sensorId"] = client_id;
  sensorMetadata["location"] = "greenhouse";
  sensorMetadata["tags"][0] = "test";

  setSyncProvider(getTime);
  setSyncInterval(300);
  Serial.println("Done with setup!");
  Serial.println();
};

void loop() {
  if (!sentMessage) {
    WeatherData * weatherData = (WeatherData *) malloc (sizeof(struct WeatherData));
    getWeatherData(weatherData);
    setTime(weatherData);
    sendData(weatherData);
    free(weatherData);
    //sentMessage = true;
  }

  // Wait some time between measurements.
  if (DEBUG) {
    Serial.print("Waiting ");
    Serial.print(MILLIS_IN_SECONDS * SECONDS_IN_MINUTE * DATA_INTERVAL_IN_MIN);
    Serial.println(" milliseconds before sending more data");
  }
  delay(MILLIS_IN_SECONDS * SECONDS_IN_MINUTE * DATA_INTERVAL_IN_MIN);
}

void getWeatherData(WeatherData * data) {
  float humidity  = dht.readHumidity();           // In percent
  float temperature = dht.readTemperature();      // In celsius
  // check if any reads failed 
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  data->temperature = temperature;
  data->humidity = humidity;
  
  float pressure = bmp280.getPressure();          // In pascals
  float bmpTemperature = bmp280.getTemperature(); // In celsius
  // check if any reads failed
  if (isnan(pressure) || isnan(bmpTemperature)) {
    Serial.println("Failed to read from BMP sensor!");
  }
  data->pressure = pressure * 0.01;                // Convert pascals to millibars

  return data;
}

void setTime(WeatherData * data) {
  // 2024-07-09T17:48:47.043Z
  sprintf(data->time, "%d-%02d-%02dT%02d:%02d:%02d.000Z", year(), month(), day(), hour(), minute(), second());
  Serial.println(data->time);
}

void sendData(WeatherData * data) {
  // call poll() regularly to allow the library to send MQTT keep alive which
  // avoids being disconnected by the broker
  mqttClient.poll();

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

  mqttClient.beginMessage(topic, (unsigned long)measureJson(weatherData));
  serializeJson(weatherData, mqttClient);
  mqttClient.endMessage();
}

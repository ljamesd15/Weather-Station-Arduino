#include "DHT.h"
#include "Seeed_BMP280.h"
#include <Wire.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"
#define DHTPIN 2
#define DHTTYPE DHT22

BMP280 bmp280;
DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char ssid[] = NETWORK_SSID;
const char network_pass[] = NETWORK_PASSWORD;
const char mqtt_username[] = MQTT_USERNAME;
const char mqtt_password[] = MQTT_PASSWORD;
const char broker[] = "192.168.86.15";
const int  port     = 1883;
const char topic[]  = "weather";
const char client_id[] = "arduino1";

int count = 0;

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
    delay(5000);
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
  Serial.println();
}

bool sentMessage = false;

void loop() {
  if (!sentMessage) {
    tempHumidity();
    tempPressure();
    mqqt();
    sentMessage = true;
  }

  // Wait some time between measurements.
  delay(10000);
}


void mqqt() {
  // call poll() regularly to allow the library to send MQTT keep alive which
  // avoids being disconnected by the broker
  mqttClient.poll();

  int Rvalue = analogRead(A2);

  Serial.print("Sending message to topic: ");
  Serial.println(topic);
  Serial.println(Rvalue);

  // send message, the Print interface can be used to set the message contents
  mqttClient.beginMessage(topic);
  mqttClient.print(Rvalue);
  mqttClient.endMessage();

  Serial.println();
}

void tempPressure() {
  float pressure;

  //get and print temperatures
  Serial.print("GROVE BMP280:: Temp: ");
  Serial.print(bmp280.getTemperature());
  Serial.print("C"); // The unit for  Celsius because original arduino don't support speical symbols

  Serial.print("  |  "); 

  //get and print atmospheric pressure data
  Serial.print("Pressure: ");
  Serial.print(pressure = bmp280.getPressure());
  Serial.print("Pa");

  Serial.print("  |  "); 

  //get and print altitude data
  Serial.print("Altitude: ");
  Serial.print(bmp280.calcAltitude(pressure));
  Serial.println("m");

  Serial.println("\n");//add a line between output of different times.
}

void tempHumidity() {
  // read humidity
  float humi  = dht.readHumidity();
  // read temperature as Celsius
  float tempC = dht.readTemperature();
  // read temperature as Fahrenheit
  float tempF = dht.readTemperature(true);

  // check if any reads failed
  if (isnan(humi) || isnan(tempC) || isnan(tempF)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("DHT 22:: Humidity: ");
    Serial.print(humi);
    Serial.print("%");

    Serial.print("  |  "); 

    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.print("°C ~ ");
    Serial.print(tempF);
    Serial.println("°F");
  }
}

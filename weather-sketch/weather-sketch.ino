#include "DHT.h"
#include "Seeed_BMP280.h"
#include <Wire.h>
#define DHTPIN 2
#define DHTTYPE DHT22

BMP280 bmp280;
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  if(!bmp280.init()){
    Serial.println("Device error!");
  }
  dht.begin(); // initialize the sensor
}

void loop() {
  // put your main code here, to run repeatedly:
  tempHumidity();
  tempPressure();

  // wait a few seconds between measurements.
  delay(10000);
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

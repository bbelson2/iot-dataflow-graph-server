/*
   IoT Dataflow Graph Server
   Copyright (c) 2015, Adam Rehn, Jason Holdsworth

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

// necessary 3rd party library: https://github.com/adafruit/DHT-sensor-library
// make sure you have the lastest ESP8266 Arduino stable release to match!
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <DHT.h> 
#include <ESP8266MulticastUDP.h>

//The identifier for this node
const String& NODE_IDENTIFIER = "temperature2-source";

//The interval (in milliseconds) at which input is read
#define READ_INTERVAL 2000

// Setup the ESP8266 Multicast UDP object as a source
ESP8266MulticastUDP multicast("iot-dataflow", "it-at-jcu",
  IPAddress(224, 0, 0, 114), 7070);


#define TEMP_PIN 12
#define DHTTYPE DHT11

#define ERROR_PIN 15

DHT dht(TEMP_PIN, DHTTYPE);


void setup()
{
  //Initialise serial communications
  Serial.begin(115200);

  dht.begin();

  multicast.begin();
  Serial.print(NODE_IDENTIFIER);
  if (multicast.isConnected()) {
    Serial.println(" connected to Wifi network");
  } else {
    Serial.println(" error: failed to connect to WiFi network!");
    pinMode(ERROR_PIN, OUTPUT);
    digitalWrite(ERROR_PIN, HIGH);
  }
}

int readSource() {
  return dht.readTemperature();
}

void loop()
{
  if (multicast.isConnected())
  {
    int value = readSource();
    if (value > 100) return;
    String message = String(NODE_IDENTIFIER) + "\n" + value;
    multicast.write(message);
    Serial.print("Temp: ");
    Serial.println(value);
  }
  delay(READ_INTERVAL);
}

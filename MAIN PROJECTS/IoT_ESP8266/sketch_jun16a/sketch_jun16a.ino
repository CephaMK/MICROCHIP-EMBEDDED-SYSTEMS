#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "dekut";
const char* password = "dekut@ict2023";

// ================= THINGSPEAK =================
unsigned long channelID = 3409436;
const char* writeAPIKey = "1JFOFNPUSFHSAO1Z";

WiFiClient client;

// ================= DHT11 =================
#define DHTPIN D1
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ================= LDR =================
const int ldrPin = A0;

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println("Smart Home IoT Starting...");

  dht.begin();

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(client);

  Serial.println("ThingSpeak Ready");
}

void loop()
{
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int light = analogRead(ldrPin);

  if (isnan(temp) || isnan(hum))
  {
    Serial.println("DHT11 Read Failed");
    delay(2000);
    return;
  }

  // Display readings
  Serial.println("---------------");

  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.println(" %");

  Serial.print("Light Level: ");
  Serial.println(light);

  // Send to ThingSpeak
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, hum);
  ThingSpeak.setField(3, light);

  int response = ThingSpeak.writeFields(channelID, writeAPIKey);

  if(response == 200)
  {
    Serial.println("Data Uploaded Successfully");
  }
  else
  {
    Serial.print("Upload Failed. Error Code: ");
    Serial.println(response);
  }

  Serial.println("---------------");

  // ThingSpeak recommends ≥15 seconds
  delay(20000);
}

#include <ESP8266WiFi.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "dekut";
const char* password = "dekut@ict2023";

WiFiServer server(80);

// ================= LED =================
const int ledPin = D2;

// ================= DHT11 =================
#define DHTPIN D1
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ================= LDR =================
const int ldrPin = A0;

// ================= AUTO LIGHT CONTROL =================
const int LIGHT_ON_THRESHOLD  = 1000;   // LED turns ON below this
const int LIGHT_OFF_THRESHOLD = 1110;  // LED turns OFF above this

bool autoMode = true;     // automatic lighting enabled
bool ledState = false;    // current LED state

// ==================================================
void setup()
{
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP8266 SMART LIGHTING SYSTEM");
    Serial.println("================================");

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    Serial.println("Initializing DHT...");
    dht.begin();

    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int timeout = 0;

    while (WiFi.status() != WL_CONNECTED && timeout < 30)
    {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        server.begin();
        Serial.println("Web Server Started");
    }
    else
    {
        Serial.println("WiFi Connection Failed");
    }

    Serial.println("Setup Complete");
}

// ==================================================
void loop()
{
    static unsigned long lastSensorRead = 0;

    float temp = NAN;
    float hum = NAN;
    int light = 0;

    // ================= SENSOR READING =================
    if (millis() - lastSensorRead > 2000)
    {
        lastSensorRead = millis();

        temp = dht.readTemperature();
        hum = dht.readHumidity();
        light = analogRead(ldrPin);

        Serial.println("------ SENSOR DATA ------");

        if (isnan(temp) || isnan(hum))
        {
            Serial.println("DHT11 Read Failed");
        }
        else
        {
            Serial.print("Temp: ");
            Serial.print(temp);
            Serial.println(" C");

            Serial.print("Humidity: ");
            Serial.print(hum);
            Serial.println(" %");
        }

        Serial.print("LDR: ");
        Serial.println(light);

        // ================= AUTO LIGHT CONTROL =================
        if (autoMode)
        {
            if (!ledState && light < LIGHT_ON_THRESHOLD)
            {
                ledState = true;
                digitalWrite(ledPin, HIGH);
                Serial.println("AUTO: LED ON (Night detected)");
            }

            if (ledState && light > LIGHT_OFF_THRESHOLD)
            {
                ledState = false;
                digitalWrite(ledPin, LOW);
                Serial.println("AUTO: LED OFF (Daylight detected)");
            }
        }

        Serial.println("-------------------------");
    }

    // ================= WEB CLIENT =================
    WiFiClient client = server.available();

    if (client)
    {
        Serial.println("Client Connected");

        String request = client.readStringUntil('\r');
        Serial.println(request);
        client.flush();

        // ================= MANUAL OVERRIDE =================
        if (request.indexOf("/LEDON") >= 0)
        {
            autoMode = false;
            ledState = true;
            digitalWrite(ledPin, HIGH);
            Serial.println("MANUAL: LED ON");
        }

        if (request.indexOf("/LEDOFF") >= 0)
        {
            autoMode = false;
            ledState = false;
            digitalWrite(ledPin, LOW);
            Serial.println("MANUAL: LED OFF");
        }

        if (request.indexOf("/AUTO") >= 0)
        {
            autoMode = true;
            Serial.println("AUTO MODE ENABLED");
        }

        // ================= RESPONSE =================
        temp = dht.readTemperature();
        hum = dht.readHumidity();
        light = analogRead(ldrPin);

        if (isnan(temp)) temp = 0;
        if (isnan(hum)) hum = 0;

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();

        client.print("{");

        client.print("\"temp\":");
        client.print(temp);
        client.print(",");

        client.print("\"hum\":");
        client.print(hum);
        client.print(",");

        client.print("\"light\":");
        client.print(light);
        client.print(",");

        client.print("\"led\":");
        client.print(ledState);
        client.print(",");

        client.print("\"auto\":");
        client.print(autoMode);
        client.print(",");

        client.print("\"wifi\":");
        client.print(WiFi.RSSI());

        client.print("}");

        client.stop();

        Serial.println("Client Disconnected");
    }
}

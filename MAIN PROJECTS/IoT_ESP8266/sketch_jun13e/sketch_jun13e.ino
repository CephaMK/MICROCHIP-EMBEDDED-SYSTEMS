#include <ESP8266WiFi.h>
#include <DHT.h>

// ======================================================
// WIFI SETTINGS
// ======================================================

const char* ssid = "Cephasbrown";
const char* password = "Cadrian19";

WiFiServer server(80);

// ======================================================
// PINS
// ======================================================

#define DHTPIN D1
#define DHTTYPE DHT11

const int ledPin = D2;
const int ldrPin = A0;

// ======================================================
// OBJECTS
// ======================================================

DHT dht(DHTPIN, DHTTYPE);

// ======================================================
// LIGHTING SETTINGS
// ======================================================

const int LIGHT_ON_THRESHOLD = 1000;
const int LIGHT_OFF_THRESHOLD = 1110;

bool autoMode = true;
bool ledState = false;

// ======================================================
// SETUP
// ======================================================

void setup()
{
Serial.begin(115200);


Serial.println();
Serial.println("====================================");
Serial.println("SMART HOME IoT DASHBOARD STARTING");
Serial.println("====================================");

pinMode(ledPin, OUTPUT);
digitalWrite(ledPin, LOW);

dht.begin();

// WiFi Connection

WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);

Serial.print("Connecting to WiFi");

while (WiFi.status() != WL_CONNECTED)
{
    delay(500);
    Serial.print(".");
}

Serial.println();
Serial.println("WiFi Connected!");

Serial.print("IP Address: ");
Serial.println(WiFi.localIP());

Serial.print("Open Browser: http://");
Serial.println(WiFi.localIP());

server.begin();

Serial.println("Web Server Started");


}

// ======================================================
// LOOP
// ======================================================

void loop()
{
// ================= SENSOR READINGS =================


float temp = dht.readTemperature();
float hum = dht.readHumidity();

int light = analogRead(ldrPin);

// ================= AUTO LIGHT CONTROL =================

if(autoMode)
{
    if(!ledState && light < LIGHT_ON_THRESHOLD)
    {
        ledState = true;
        digitalWrite(ledPin, HIGH);
    }

    if(ledState && light > LIGHT_OFF_THRESHOLD)
    {
        ledState = false;
        digitalWrite(ledPin, LOW);
    }
}

// ================= WEB CLIENT =================

WiFiClient client = server.available();

if(!client)
{
    return;
}

Serial.println("Client Connected");

String request = client.readStringUntil('\r');

client.flush();

// ==================================================
// BUTTON ACTIONS
// ==================================================

if(request.indexOf("/LEDON") >= 0)
{
    autoMode = false;

    ledState = true;

    digitalWrite(ledPin, HIGH);

    Serial.println("LED ON");
}

if(request.indexOf("/LEDOFF") >= 0)
{
    autoMode = false;

    ledState = false;

    digitalWrite(ledPin, LOW);

    Serial.println("LED OFF");
}

if(request.indexOf("/AUTO") >= 0)
{
    autoMode = true;

    Serial.println("AUTO MODE ENABLED");
}

// ==================================================
// SENSOR CHECK
// ==================================================

if(isnan(temp))
{
    temp = 0;
}

if(isnan(hum))
{
    hum = 0;
}

String lightCondition;

if(light < LIGHT_ON_THRESHOLD)
{
    lightCondition = "Dark";
}
else
{
    lightCondition = "Bright";
}

String ledStatus;

if(ledState)
{
    ledStatus = "ON";
}
else
{
    ledStatus = "OFF";
}

String modeStatus;

if(autoMode)
{
    modeStatus = "AUTO";
}
else
{
    modeStatus = "MANUAL";
}

// ==================================================
// HTML PAGE
// ==================================================

client.println("HTTP/1.1 200 OK");
client.println("Content-type:text/html");
client.println();

client.println("<!DOCTYPE html>");
client.println("<html>");

client.println("<head>");
client.println("<title>Smart Home Dashboard</title>");

client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");

client.println("<style>");

client.println("body{font-family:Arial;background:#f2f2f2;text-align:center;}");

client.println(".card{background:white;width:350px;margin:auto;padding:20px;border-radius:12px;box-shadow:0px 0px 10px gray;}");

client.println("button{padding:12px 20px;font-size:18px;margin:5px;border:none;border-radius:8px;cursor:pointer;}");

client.println("h1{color:#333;}");

client.println("</style>");

client.println("</head>");

client.println("<body>");

client.println("<div class='card'>");

client.println("<h1>🏠 Smart Home Dashboard</h1>");

client.print("<h3>🌡 Temperature: ");
client.print(temp);
client.println(" °C</h3>");

client.print("<h3>💧 Humidity: ");
client.print(hum);
client.println(" %</h3>");

client.print("<h3>☀ Light Level: ");
client.print(light);
client.println("</h3>");

client.print("<h3>Environment: ");
client.print(lightCondition);
client.println("</h3>");

client.print("<h3>💡 LED Status: ");
client.print(ledStatus);
client.println("</h3>");

client.print("<h3>Mode: ");
client.print(modeStatus);
client.println("</h3>");

client.println("<br>");

client.println("<a href='/LEDON'><button>LED ON</button></a>");

client.println("<a href='/LEDOFF'><button>LED OFF</button></a>");

client.println("<a href='/AUTO'><button>AUTO MODE</button></a>");

client.println("</div>");

client.println("</body>");
client.println("</html>");

client.stop();

delay(100);

}

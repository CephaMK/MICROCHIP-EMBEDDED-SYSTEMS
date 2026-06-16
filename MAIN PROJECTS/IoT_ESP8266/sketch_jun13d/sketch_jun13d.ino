#include <ESP8266WiFi.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "Cephasbrown";
const char* password = "Cadrian19";

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
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        
        client.println("<!DOCTYPE html>");
        client.println("<html>");
        
        client.println("<head>");
        client.println("<title>Smart Home Dashboard</title>");
        
        client.println("<style>");
        client.println("body{font-family:Arial;text-align:center;background:#f4f4f4;}");
        client.println(".card{background:white;padding:20px;margin:20px auto;width:350px;border-radius:10px;box-shadow:0 0 10px gray;}");
        client.println("button{padding:10px 20px;font-size:18px;margin:5px;}");
        client.println("</style>");
        
        client.println("</head>");
        
        client.println("<body>");
        
        client.println("<div class='card'>");
        
        client.println("<h1>Smart Home Dashboard</h1>");
        
        client.print("<h3>Temperature: ");
        client.print(temp);
        client.println(" °C</h3>");
        
        client.print("<h3>Humidity: ");
        client.print(hum);
        client.println(" %</h3>");
        
        client.print("<h3>Light Level: ");
        client.print(light);
        client.println("</h3>");
        
        client.print("<h3>Environment: ");
        client.print(lightCondition);
        client.println("</h3>");
        
        client.print("<h3>LED Status: ");
        client.print(ledStatus);
        client.println("</h3>");
        
        client.println("<p>");
        client.println("<a href='/LEDON'><button>LED ON</button></a>");
        client.println("<a href='/LEDOFF'><button>LED OFF</button></a>");
        client.println("<a href='/AUTO'><button>AUTO MODE</button></a>");
        client.println("</p>");
        
        client.println("</div>");
        
        client.println("</body>");
        client.println("</html>");
        
        client.stop();  
    }
}

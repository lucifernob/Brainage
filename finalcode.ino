#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Servo.h>
Servo servo;

/*=====================================================================================*/
#define PULSE_PIN D2  //gpio4
#define turbid_pin A0  //gpio4

volatile long pulseCount=0;
float calibrationFactor = 4.5;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
float totalLitres;
float volt;
unsigned long oldTime;

void ICACHE_RAM_ATTR pulseCounter()
{
  pulseCount++;
}
void flowMeter(void);
void Turbidity(void);

const char* ssid = "abc";
const char* password = "1234567890";

/*=====================================================================================*/
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char* AWS_endpoint = ""; //MQTT broker ip

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
/*-------------------------------------------------------------------------------------*/
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient); //set MQTT port number to 8883 as per //standard
long lastMsg = 0;
char msg[50];
int value = 0;
/*-------------------------------------------------------------------------------------*/
void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  espClient.setX509Time(timeClient.getEpochTime());
}
/*-------------------------------------------------------------------------------------*/
void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPthing"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("Turbidity", "FlowMeter");
      client.publish("Turbidity", "Turbidity");
      // ... and resubscribe
      client.subscribe("inTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
    delay(5000);
    }
  }
}
/*-------------------------------------------------------------------------------------*/
void setup()
{
  Serial.begin(9600);
  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0; 
  Serial.setDebugOutput(true);
  setup_wifi();
  delay(1000);
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
  return;
  }
  /*-------------------------------------------------------------------------------------*/
  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert)
  {
    Serial.println("Failed to open cert file");
  }
  else
  Serial.println("Success to open cert file");
  delay(1000);

  if (espClient.loadCertificate(cert))
  Serial.println("cert loaded");
  else
  Serial.println("cert not loaded");

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key)
  {
    Serial.println("Failed to open private cert file");
  }
  else
  Serial.println("Success to open private cert file");
  delay(1000);

  if (espClient.loadPrivateKey(private_key))
  Serial.println("private key loaded");
  else
  Serial.println("private key not loaded");

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
  if (!ca)
  {
    Serial.println("Failed to open ca ");
  }
  else
  Serial.println("Success to open ca");

  delay(1000);

  if(espClient.loadCACert(ca))
  Serial.println("ca loaded");
  else
  Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}
/*-------------------------------------------------------------------------------------*/
void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  flowMeter();
  ++value;
  snprintf (msg, 75, "{\"Total Litres\" : \"%f\"}", totalLitres);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("FlowMeter", msg);

  Turbidity();
  snprintf (msg, 75, "{\"Turbid level\" : \"%f\"}\n", volt);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("Turbidity", msg);
  delay(1000);
}

/*==============================================================================*/
void flowMeter(void)
{
  detachInterrupt(PULSE_PIN);
  flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
  oldTime = millis();
  flowMilliLitres = (flowRate / 60) * 1000;
  totalMilliLitres += flowMilliLitres;     
  totalLitres = totalMilliLitres * 0.001;  
  pulseCount = 0;
  attachInterrupt(PULSE_PIN, pulseCounter, FALLING);
  delay (1000);
}

/*==============================================================================*/
void Turbidity(void)
{
  servo.attach(D4); 
  detachInterrupt(turbid_pin);
  delay(1000);
  
  int turbidity=analogRead(A0);
  volt=turbidity*1;
  
  if (volt < 9500 && volt > 8600)
  {
    Serial.println("Clean water");
    servo.write(180);
    delay(150);               
  }
  else if (volt < 8600 && volt > 8200)
  {
    Serial.println("Turbid water");
    servo.write(90);
    delay(150);               
  }
  else
  {
    Serial.println("Segregate water");
    servo.write(0);
    delay(150);     
  }
  delay(500);  
}

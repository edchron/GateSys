#include <WiFiClientSecure.h>
#include "certs.h"
#include <Servo.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// The name of the device. This MUST match up with the name defined in the AWS console
#define DEVICE_NAME "GateVisitorSys"

// The MQTTT endpoint for the device (unique for each AWS account but shared amongst devices within the account)
#define AWS_IOT_ENDPOINT "a38fyy88ks1o8p-ats.iot.us-east-1.amazonaws.com"

// The MQTT topic that this device should publish to
#define AWS_IOT_TOPIC "$aws/things/" DEVICE_NAME

#define AWS_IOT_SUB AWS_IOT_TOPIC "/button"

#define AWS_IOT_PUB AWS_IOT_TOPIC "/message"

// How many times we should attempt to connect to AWS
#define AWS_MAX_RECONNECT_TRIES 50

#define WIFI_SSID "dd-wrt" // SSID of your WIFI
#define WIFI_PASSWD "" //your wifi password

const int pir1 = 5; // pin PIR 1 // Pin D5 on ESP32
const int pir2 = 4; // pin PIR 2 // Pin D4 in ESP32
const int servoPin = 18; //Servo pin

int i = 1;
boolean state1 = true, state2 = true;
char* gate = "Closed";
int pos = 90;
int pirOut, pirIn;
unsigned long lastPublish, previousTime;
int current = 0;
int total = 0;
//pir luar (pirOut) = untuk orang masuk (letaknya di luar pintu)
//pir dalam (pirIn) = untuk orang keluar (letaknya di dalam pintu)

Servo servo;
WiFiClientSecure wifiClient = WiFiClientSecure();
void msgReceived(char* topic, byte* payload, unsigned int len);
PubSubClient pubSubClient(AWS_IOT_ENDPOINT, 8883, msgReceived, wifiClient);

void setup_wifi() {
  unsigned long previousTime = 0;
  if (millis() - previousTime >= 10) {
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.waitForConnectResult();
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void aws_connect() {
  // Configure WiFiClientSecure to use the AWS certificates we generated
  wifiClient.setCACert(AWS_CERT_CA);
  wifiClient.setCertificate(AWS_CERT_CRT);
  wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
}

void msgReceived(char* topic, byte * payload, unsigned int length) {
  Serial.print("Message received on ");
  Serial.print(topic);
  Serial.print(": ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "$aws/things/GateVisitorSys/button") {
    Serial.println("Resetting current and total visitor");
    if (messageTemp == "reset") {
      current = 0;
      total = 0;
      Serial.print("Curret visitor: ");
      Serial.print(current);
      Serial.print("  Total visitor:");
      Serial.println(total);
    }
  }
}

void reconnect() {
  while ( ! pubSubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (pubSubClient.connect(DEVICE_NAME)) {
      Serial.println("connected");
      // Subscribe
      pubSubClient.subscribe(AWS_IOT_SUB);
      Serial.println("AWS connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void getData() {
  pirOut = digitalRead(pir1); //untuk orang masuk
  pirIn = digitalRead(pir2); //untuk orang keluar

  if (pirOut && i == 1 && state1) {
    for (pos; pos <= 180; pos++) {
      servo.write(pos);
      delay(5);
    }
    i = 2;
    state1 = false;
    gate = "Open";
  }

  if (pirIn && i == 2 && state2) {
    //    Serial.println("Entering into room");
    i = 1 ;
    current++;
    total++;
    //    Serial.print("No of persons inside the room: ");
    //    Serial.println(current);
    state2 = false;
    delay(1500);
    for (pos; pos >= 90; pos--) {
      servo.write(pos);
      delay(5);
    }
    gate = "Closed";
  }

  if (pirIn && i == 1 && state2 ) {
    for (pos; pos <= 180; pos++) {
      servo.write(pos);
      delay(5);
    }
    i = 2 ;
    state2 = false;
    gate = "Open";
  }

  if (pirOut && i == 2 && state1 ) {
    //    Serial.println("Exiting from room");
    current--;
    if (current < 0) {
      current = 0;
    }
    //    Serial.print("No of persons inside the room: ");
    //    Serial.println(current);
    i = 1;
    state1 = false;
    delay(1500);
    for (pos; pos >= 90; pos--) {
      servo.write(pos);
      delay(5);
    }
    gate = "Closed";
  }

  if (!pirOut) {
    state1 = true;
  }

  if (!pirIn) {
    state2 = true;
  }
}

void publishData() {
  StaticJsonDocument<512> doc;
  JsonObject object = doc.to<JsonObject>();
  JsonObject visitors = object.createNestedObject("visitors");
  visitors["total"] = total;
  visitors["current"] = current;
  object["gate"] = gate;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); //print to client

  if (millis() - lastPublish > 1000) {
    boolean rc = pubSubClient.publish(AWS_IOT_PUB, jsonBuffer);
    Serial.print("Published, rc=");
    Serial.print( (rc ? "OK: " : "FAILED: ") );
    Serial.println(jsonBuffer);
    lastPublish = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing");
  pinMode(pir1, INPUT);
  pinMode(pir2, INPUT);
  servo.attach(servoPin);
  servo.write(pos);
  Serial.println("Initializing WiFi connection....");
  setup_wifi();
  Serial.println("Initializing AWS connection....");
  aws_connect();
}

void loop() {
  if (!pubSubClient.connected()) {
    reconnect();
  }
  pubSubClient.loop();
  getData();
  publishData();
}

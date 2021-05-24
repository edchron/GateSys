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

// How many times we should attempt to connect to AWS
#define AWS_MAX_RECONNECT_TRIES 50

#define WIFI_SSID "dd-wrt" // SSID of your WIFI
#define WIFI_PASSWD "" //your wifi password

const int echoPin = 5; // Echo Pin of Ultrasonic Sensor // Pin D5 on ESP32
const int trigPin = 4; // Trigger Pin of Ultrasonic Sensor // Pin D4 in ESP32
const int servoPin = 18; //Servo pin

int visitor;
char* state = "close";
int pos = 90;
unsigned long lastPublish, previousTime;
int msgCount;
long duration;
int distance;

Servo servo;
WiFiClientSecure wifiClient = WiFiClientSecure();
//MQTTClient client = MQTTClient(512);
void msgReceived(char* topic, byte* payload, unsigned int len);
PubSubClient pubSubClient(AWS_IOT_ENDPOINT, 8883, msgReceived, wifiClient);

void setup_wifi() {
  delay(10);
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
}

void pubSubCheckConnect() {
  if ( ! pubSubClient.connected()) {
    Serial.print("PubSubClient connecting to: ");
    Serial.print(AWS_IOT_ENDPOINT);
    while ( ! pubSubClient.connected()) {
      Serial.print(".");
      pubSubClient.connect(DEVICE_NAME);
      delay(1000);
    }
    Serial.println(" connected");
    pubSubClient.subscribe("hello");
  }
  pubSubClient.loop();
}

void getDataPub() {
  int tempVisitor = visitor;
  if (millis() - previousTime > 500) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH); // using pulsein function to determine total time
    distance = duration * 0.034 / 2;
    previousTime = millis();
  }

  if (isnan(distance)) { // NAN means no available data
    Serial.println("Reading failed.");
  }
  else {
    if (distance < 16) {
      for (pos; pos <= 180; pos++) {
        servo.write(pos);
        delay(5);
      }
      if (distance < 8 && state == "close" ) {
        servo.write(pos); //just in case
        visitor = visitor + 1;
        state = "open";
        //        Serial.print("Dist: ");
        //        Serial.print(distance);
        //        Serial.print(" Visitors: ");
        //        Serial.print(visitor);
        //        Serial.print(" Gate: ");
        //        Serial.println(state);
      }
    }
    else {
      for (pos; pos >= 90; pos--) {
        servo.write(pos);
        delay(5);
      }
      servo.write(pos); //just in case
      state = "close";
      //      Serial.print("Dist: ");
      //      Serial.print(distance);
      //      Serial.print(" Visitors: ");
      //      Serial.print(visitor);
      //      Serial.print(" Gate: ");
      //      Serial.println(state);
    }
  }

  StaticJsonDocument<200> doc;
  doc["distance"] = distance;
  doc["visitors"] = visitor;
  doc["gate"] = state;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); //print to client
  if (millis() - lastPublish > 1000) {
    boolean rc = pubSubClient.publish(AWS_IOT_TOPIC+"/message", jsonBuffer);
    Serial.print("Published, rc="); Serial.print( (rc ? "OK: " : "FAILED: ") );
    Serial.println(jsonBuffer);
    lastPublish = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing");
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servo.attach(servoPin);
  servo.write(pos);
  Serial.println("Initializing WiFi connection....");
  setup_wifi();
  Serial.println("Initializing connection to AWS....");
  aws_connect();
}

void loop() {
  pubSubCheckConnect();
  getDataPub();
}

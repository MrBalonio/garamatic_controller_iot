/*
Author: MrBalonio
  ESP8266 MQTT Garage Controller

 This sketch is use to create a garage door controller.
 it can use switches on door to determine state
 it can actuate a garage door remote and some transistors
 currently it supports two doors 
 
 Licensed Under GPL V3

*/
// Must Add this libraries to your Arduino IDE
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"


// Pin Assiments 
#define DHTPIN D7  // Pin for Temperature Sensor
#define doorRemote1 D5
#define doorRemote2 D2
#define doorRemote3 D0
#define door1Pin D6
#define door2Pin D12

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Add your Wifi info
const char* ssid = "";
const char* password = "";

// MQTT Broker info
const char* mqtt_server = "";
const int mqtt_server_port = 1883;

char myClientID[20];
char tmpChar[15];
char* clientID;

// MQTT Topics
#define house_door_state_topic "house/garage/doors/state"
#define house_garage_remote_topic "house/garage/doors/remote"
#define house_garage_env_topic "house/garage/weather"
#define pingTopic "devices/ping"



// Set up temperature sensor object
DHT dht(DHTPIN, DHTTYPE);
// Wifi Client
WiFiClient espClient;

// MQTT Client 
PubSubClient client(espClient);

int REMOTE_SKIP_COUNT = 1;
int TEMP_INTERVAL  = 300;
String oldMsg;
long lastMsg = 0;
long longerCount = 0;
char msg[50];
char __doorsStatus[80];
int value = 0;

String doorPinsState(){
  // Read the status of the door pins
  int door1PinRead = digitalRead(door1Pin);
  int door2PinRead = digitalRead(door2Pin);
  // Build jsonstring
  String doorsStatus = "{\"door1\":";
  doorsStatus.concat(door1PinRead);
  doorsStatus.concat( ",\"door2\":" );
  doorsStatus.concat( door2PinRead);
  doorsStatus.concat("}");
  // Return the status as a json string
  return doorsStatus;
}

char* generateClientID(){
  // Create dynamic client name
  String mac = WiFi.macAddress();
  mac.toCharArray(tmpChar,50);
  strcat(myClientID, "RTI");
  strcat(myClientID, tmpChar);
  return myClientID;
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void sendDoorStatus(){
  // Get the status of the doors
  String newMsg = doorPinsState();
  // Check if the status has changed
  if ( oldMsg != newMsg){
    // if it has changed then lets send an updated state
    oldMsg = newMsg;
    newMsg.toCharArray(msg, sizeof(msg));
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(house_door_state_topic, msg);
  }
}

void remoteClick(int remotePin){
  if ( REMOTE_SKIP_COUNT > 0 ){
    Serial.println("Skipping on reconnect");
    REMOTE_SKIP_COUNT--;
    return;
  }
  Serial.println("Activating remote clicK");
  digitalWrite(remotePin, HIGH);
  delay(1000);
  digitalWrite(remotePin, LOW);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String myCommand;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    myCommand.concat((char)payload[i]);
  }
  Serial.println();
  if (myCommand == "click1"){
    remoteClick(doorRemote1);
  }
  if (myCommand == "click2"){
    remoteClick(doorRemote2);
  }
  if (myCommand == "light"){
    remoteClick(doorRemote3);
  }
  if (myCommand == "status"){
    sendDoorStatus();
  }
  if (myCommand == "ping"){
    client.publish("devices/pong", clientID);
  }
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void setup() {
  clientID = generateClientID();
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  // remote pin initialization
  pinMode(doorRemote1, OUTPUT);
  pinMode(doorRemote2, OUTPUT);
  pinMode(doorRemote3, OUTPUT);
  // Set door sensor pins as input
  pinMode(door1Pin,INPUT);
  pinMode(door2Pin,INPUT);
  digitalWrite(doorRemote1, LOW);
  digitalWrite(doorRemote2, LOW);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_server_port);
  client.setCallback(callback);
  dht.begin();

}


String getTempHumidity(){
  float h = dht.readHumidity();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return "";
  }else{
    String message = "";
    message.concat("{\"humidity\": ");
    message.concat(h);
    message.concat(",\"temperature\": ");
    message.concat(f);
    message.concat("}");
    return message;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("devices/online", clientID);
      // ... and resubscribe
      client.subscribe("devices/ping");

client.subscribe(house_garage_remote_topic);
} else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
 digitalWrite(BUILTIN_LED, HIGH);
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis();
// Uncomment for temp sendsor reading and  reporting 
// at TEMP_INTERVAL
//  if (now - longerCount > (TEMP_INTERVAL * 1000)) {
//    longerCount = now;
//    String tempMsg =   getTempHumidity();
//    Serial.println(tempMsg);
//    tempMsg.toCharArray(msg, sizeof(msg));
//    client.publish(house_garage_env_topic, msg);
//  }

// Uncomment if you like the door status sent at an interval
//  if (now - lastMsg > 2000) {
//    lastMsg = now;
//    ++value;
//    sendDoorStatus();
//  }
}

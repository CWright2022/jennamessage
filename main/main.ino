#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"
#include <Adafruit_NeoPixel.h>

#define RX_PIN 5  // RX for printer
#define TX_PIN 4  // TX for printer
#define LED_PIN 14 //ws2812 LED pin
#define BUTTON_PIN 10 //pin for pushbutton


//i hate embedded code - this system of multiple queues isn't ideal
#define QUEUE_SIZE 10

//setup arrays, global vars and tracking vars
String messageQueue[QUEUE_SIZE];
bool invertQueue[QUEUE_SIZE];
bool underlineQueue[QUEUE_SIZE];
bool boldQueue[QUEUE_SIZE];
char justifyQueue[QUEUE_SIZE];
char sizeQueue[QUEUE_SIZE];

bool INVERT = false;
bool UNDERLINE = false;
bool BOLD = false;
char JUSTIFY = 'C';
char SIZE = 'L';

//NEED TO TEST THIS
bool canPrintTimeout = 1;
unsigned long previousMillis = 0;

int queueStart = 0;
int queueEnd = 0;
int queueCount = 0;

char mqtt_id[24];
char mqtt_topic[50];

uint32_t lastTimeItHappened = millis() + papercheck_milliseconds;

WiFiClient client;
PubSubClient mqtt(client);

SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Thermal printer(&mySerial);
Adafruit_NeoPixel mainLed(1, LED_PIN, NEO_GRB + NEO_KHZ800);

bool printMessage = false;




// Function to add a message to the queue
void queueMessage(const String& message) {
  if (queueCount < QUEUE_SIZE) {  // Only add if there's space
    messageQueue[queueEnd] = message;
    invertQueue[queueEnd]=INVERT;
    boldQueue[queueEnd]=BOLD;
    justifyQueue[queueEnd]=JUSTIFY;
    sizeQueue[queueEnd]=SIZE;
    Serial.print("queued message at index:");
    Serial.println(queueEnd);
    queueEnd = (queueEnd + 1) % QUEUE_SIZE;
    queueCount++;
    //breathe purple LED
    mainLed.setPixelColor(0, mainLed.Color(255,0,255));
    mainLed.show();
  }
}

//actually prints the message via the printer
void printQueuedMessage(){
    invertQueue[queueStart] ? printer.inverseOn() : printer.inverseOff();
    underlineQueue[queueStart] ? printer.underlineOn() : printer.underlineOff();
    boldQueue[queueStart] ? printer.boldOn() : printer.boldOff();
    printer.justify(justifyQueue[queueStart]);
    printer.setSize(sizeQueue[queueStart]);
    if(messageQueue[queueStart].length()>140){
      messageQueue[queueStart] = messageQueue[queueStart].substring(0,140);
    }
    printer.print(messageQueue[queueStart]);
    Serial.print("printed message: ");
    Serial.println(messageQueue[queueStart]);
    // Serial.print("Inverted: ");
    // Serial.println(invertQueue[queueStart]);
    // Serial.print("Underline: ");
    // Serial.println(underlineQueue[queueStart]);
    // Serial.print("Bold: ");
    // Serial.println(boldQueue[queueStart]);
    // Serial.print("Justify: ");
    // Serial.println(justifyQueue[queueStart]);
    // Serial.print("Size: ");
    // Serial.println(sizeQueue[queueStart]);
    printer.feed(4);
    queueStart = (queueStart + 1) % QUEUE_SIZE;
    queueCount--;
    //if no more messages, stop breathing
    if(queueCount == 0){
      mainLed.clear();
      mainLed.show();
    }
    Serial.print("QueueCount:");
    Serial.println(queueCount);
    //reset flag and timeout
    printMessage = false;
    //publish number of messages in queue
    char queueCountStr[3]; // Temporary buffer to hold the string representation
    snprintf(queueCountStr, sizeof(queueCountStr), "%d", queueCount); // Convert to string
    mqtt.publish(mqtt_topic_messages_in_queue, queueCountStr);
}

//MQTT callback - needs major refactoring to support format changes
void callback(char* topic, byte* payload, unsigned int length) {
  // set textlineheight
  if (strcmp(topic, mqtt_listen_topic_textlineheight) == 0) {
    //this topic expects integer!
    int payload_int = mqtt_row_spacing;
    for (int i = 0; i < length; i++) {
      char c = payload[i];
      if (c >= '0' && c <= '9')
        payload_int = payload_int * 10 + c - '0';  //encode to interger
    }
    printer.setLineHeight(payload_int);
  }
  // set text size (S | M | L)
  if (strcmp(topic, mqtt_listen_topic_textsize) == 0) {
    char c = 'S';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    SIZE = c;
  }
  // topic to inverse the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textinverse) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      INVERT = true;
    } else {
      INVERT = false;
    }
  }
  // topic to justify the text (L | C | R)
  if (strcmp(topic, mqtt_listen_topic_textjustify) == 0) {
    char c = 'L';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    JUSTIFY = c;
  }
  // topic to bold the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textbold) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      BOLD = true;
    } else {
      BOLD = false;
    }
  }
  // topic to underline the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textunderline) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      UNDERLINE = true;
    } else {
      UNDERLINE = false;
    }
  }

  // topic to print text
  if (strcmp(topic, mqtt_listen_topic_text2print) == 0) {
    // printer.print(F("Message arrived:\n"));
    String output = "";
    for (int i = 0; i < length; i++) {
      // printer.print((char)payload[i]);
      output.concat((char)payload[i]);
    }
    queueMessage(output);
  }

  if (strcmp(topic, mqtt_topic_get_messages_in_queue) == 0) {
    char payloadStr[4]; // Adjust size as needed
    strncpy(payloadStr, reinterpret_cast<const char*>(payload), sizeof(payloadStr) - 1);
    payloadStr[sizeof(payloadStr) - 1] = '\0'; // Ensure null termination
    if(strcmp(payloadStr, "get") == 0){
      //publish number of messages in queue
      char queueCountStr[3]; // Temporary buffer to hold the string representation
      snprintf(queueCountStr, sizeof(queueCountStr), "%d", queueCount); // Convert to string
      mqtt.publish(mqtt_topic_messages_in_queue, queueCountStr);
    }
  }
}

void setup() {
  //basic setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  mainLed.begin();
  mainLed.clear();
  mainLed.show();
  mainLed.setPixelColor(0, mainLed.Color(255,255,255));
  mainLed.show();
  Serial.begin(115200);
  Serial.println("JENNA'S SECRET CHRISTMAS PRESENT 2024 - by Cayden Wright");
  Serial.println("booting...");

  //initialize printer
  mySerial.begin(baud);
  printer.begin();
  printer.setDefault();
  printer.setSize(mqtt_text_size);
  printer.setLineHeight(mqtt_row_spacing);
  Serial.println("printer initializzed");

  //initialize wifi
  wifi_station_set_hostname(my_id);
  Serial.print(F("Hostname: "));
  Serial.println(my_id);
  WiFi.mode(WIFI_STA);

  //setup MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
  mainLed.clear();
}

void loop() {
  if(digitalRead(BUTTON_PIN) == LOW && queueCount > 0 && canPrintTimeout){
    canPrintTimeout = 0;
    previousMillis=millis();
    printQueuedMessage();
  }

  //NEED TO TEST THIS
  if(!canPrintTimeout &&millis()-previousMillis >= 500){
    canPrintTimeout=1;
    previousMillis = millis();
  }

  //connect to Wifi - RED LED
  if (WiFi.status() != WL_CONNECTED) {
    mainLed.setPixelColor(0, mainLed.Color(255,0,0));
    mainLed.show();

    Serial.println(F("Connecting to WiFi..."));
    WiFi.begin(ssid, password);

    unsigned long begin_started = millis();
    //if wifi isn't working, reboot
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      if (millis() - begin_started > 60000) {
        Serial.println("Wifi not connected in 1 minute. Restarting...");
        ESP.restart();
      }
    }
    Serial.println("WiFi connected!");
    mainLed.clear();
    mainLed.show();
  }


  //connect to broker - YELLOW LED
  if (!mqtt.connected()) {
    mainLed.setPixelColor(0, mainLed.Color(255,50,0));
    mainLed.show();
    Serial.print("connecting to broker at: ");
    Serial.println(mqtt_server);

    if (mqtt.connect(mqtt_id, mqtt_user, mqtt_pass)) {
      Serial.print("Connected to broker at: ");
      Serial.println(mqtt_server);
      mainLed.setPixelColor(0, mainLed.Color(0,255,0));
      mainLed.show();
      //subscribe to all topics
      mqtt.subscribe(mqtt_listen_topic_text2print);
      mqtt.subscribe(mqtt_listen_topic_textsize);
      mqtt.subscribe(mqtt_listen_topic_textlineheight);
      mqtt.subscribe(mqtt_listen_topic_textinverse);
      mqtt.subscribe(mqtt_listen_topic_textjustify);
      mqtt.subscribe(mqtt_listen_topic_textbold);
      mqtt.subscribe(mqtt_listen_topic_textunderline);
      mqtt.subscribe(mqtt_topic_get_messages_in_queue);
      //reset LED
      mainLed.clear();
      mainLed.show();
    } else {
      Serial.print("connection to broker:");
      Serial.print(mqtt_server);
      Serial.println(" failed.");
      delay(2000);
      return;
    }
  }

  //check the paperload
  if ((millis() - lastTimeItHappened >= papercheck_milliseconds)) {
    bool bPaperCheck = printer.hasPaper();
    delay(100);
    if (bPaperCheck) {
      mqtt.publish(mqtt_listen_topic_papercheck, "yes");
      Serial.println("paper OK");
    } else {
      mqtt.publish(mqtt_listen_topic_papercheck, "no");
      Serial.println("out of paper");
    }
    lastTimeItHappened = millis();
  }

  mqtt.loop();
}

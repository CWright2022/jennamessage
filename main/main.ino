#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"
#include <jled.h>

#define RX_PIN 5  // RX for printer
#define TX_PIN 4  // TX for printer
#define RED_PIN 14
#define GREEN_PIN 12
#define BLUE_PIN 13
#define BUTTON_PIN 10
#define DEBUG_PIN 15

#define MAX_TOPIC_LENGTH 50
#define MAX_PAYLOAD_LENGTH 10

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

auto redLed = JLed(RED_PIN);
auto greenLed = JLed(GREEN_PIN);
auto blueLed = JLed(BLUE_PIN);

bool printMessage = false;

//interrupt function to set a flag for later (above) since we can't spend too long on an interrupt
ICACHE_RAM_ATTR void isr() {
  if(queueCount > 0){
    printMessage = true;
  }
}


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
    redLed.Breathe(2000).DelayAfter(1000).Forever();
    blueLed.Breathe(2000).DelayAfter(1000).Forever();
  }
}

//actually prints the message via the printer
void printQueuedMessage(){
    invertQueue[queueStart] ? printer.inverseOn() : printer.inverseOff();
    underlineQueue[queueStart] ? printer.underlineOn() : printer.underlineOff();
    boldQueue[queueStart] ? printer.boldOn() : printer.boldOff();
    printer.justify(justifyQueue[queueStart]);
    printer.setSize(sizeQueue[queueStart]);
    printer.print(messageQueue[queueStart]);
    Serial.print("printed message: ");
    Serial.println(messageQueue[queueStart]);
    Serial.print("Inverted: ");
    Serial.println(invertQueue[queueStart]);
    Serial.print("Underline: ");
    Serial.println(underlineQueue[queueStart]);
    Serial.print("Bold: ");
    Serial.println(boldQueue[queueStart]);
    Serial.print("Justify: ");
    Serial.println(justifyQueue[queueStart]);
    Serial.print("Size: ");
    Serial.println(sizeQueue[queueStart]);
    queueStart = (queueStart + 1) % QUEUE_SIZE;
    queueCount--;
    //if no more messages, stop breathing
    if(queueCount == 0){
      redLed.Stop();
      blueLed.Stop();
    }
    //reset flag
    printMessage = false;
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
}

//manually setting LED color without JLED
void setLed(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void setup() {
  //basic setup
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(DEBUG_PIN, INPUT);
  setLed(255, 255, 255);
  Serial.begin(115200);
  attachInterrupt(BUTTON_PIN, isr, FALLING);

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
}

void loop() {
  //print message if we were told to by the interrupt
  if(printMessage){
    printQueuedMessage();
  }

  //JLED update functions
  redLed.Update();
  greenLed.Update();
  blueLed.Update();

  //connect to Wifi - RED LED
  if (WiFi.status() != WL_CONNECTED) {
    setLed(255, 0, 0);

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
  }


  //connect to broker - YELLOW LED
  if (!mqtt.connected()) {
    setLed(255, 50, 0);
    Serial.print("connecting to broker at: ");
    Serial.println(mqtt_server);

    if (mqtt.connect(mqtt_id)) {
      Serial.print("Connected to broker at: ");
      Serial.println(mqtt_server);
      //green LED for a tiny bit 
      setLed(0, 255, 0);
      //subscribe to all topics
      mqtt.subscribe(mqtt_listen_topic_text2print);
      mqtt.subscribe(mqtt_listen_topic_textsize);
      mqtt.subscribe(mqtt_listen_topic_textlineheight);
      mqtt.subscribe(mqtt_listen_topic_textinverse);
      mqtt.subscribe(mqtt_listen_topic_textjustify);
      mqtt.subscribe(mqtt_listen_topic_textbold);
      mqtt.subscribe(mqtt_listen_topic_textunderline);
      //reset LED so JLED can take over
      setLed(0, 0, 0);
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

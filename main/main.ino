#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"
#include <jled.h>
// #include "Queue.h"

#define RX_PIN 5  // RX GPIO-Pin of your Microcontroller
#define TX_PIN 4  // TX GPIO-Pin of your Microcontroller
#define RED_PIN 14
#define GREEN_PIN 12
#define BLUE_PIN 13
#define BUTTON_PIN 10
#define DEBUG_PIN 15

#define MAX_TOPIC_LENGTH 50
#define MAX_PAYLOAD_LENGTH 10

//i hate embedded code - I spent hours trying to et a queue to work
#define QUEUE_SIZE 10
String messageQueue[QUEUE_SIZE];
int queueStart = 0;
int queueEnd = 0;
int queueCount = 0;

char mqtt_id[24];
char mqtt_topic[50];

bool printMessage = false;

uint32_t lastTimeItHappened = millis() + papercheck_milliseconds;

WiFiClient client;
PubSubClient mqtt(client);

SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Thermal printer(&mySerial);

auto redLed = JLed(RED_PIN);
auto greenLed = JLed(GREEN_PIN);
auto blueLed = JLed(BLUE_PIN);

//interrupt function to set a flag for later since we can't spend too long on an interrupt
ICACHE_RAM_ATTR void isr() {
  if(queueCount > 0){
    printMessage = true;
  }
}

// Function to add a message to the queue
void queueMessage(const String& message) {
  if (queueCount < QUEUE_SIZE) {  // Only add if there's space
    messageQueue[queueEnd] = message;
    Serial.print("queued message at index:");
    Serial.println(queueEnd);
    queueEnd = (queueEnd + 1) % QUEUE_SIZE;
    queueCount++;
    Serial.print("queueStart: ");
    Serial.print(queueStart);
    Serial.print(" queueEnd: ");
    Serial.print(queueEnd);
    Serial.print(" queueCount: ");
    Serial.println(queueCount);
    redLed.Breathe(2000).DelayAfter(1000).Forever();
    blueLed.Breathe(2000).DelayAfter(1000).Forever();
  }
}

//actually prints the message
void printQueuedMessage(){
    printer.print(messageQueue[queueStart]);
    Serial.print("printed message: ");
    Serial.println(messageQueue[queueStart]);
    queueStart = (queueStart + 1) % QUEUE_SIZE;
    queueCount--;
    if(queueCount == 0){
      redLed.Stop();
      blueLed.Stop();
    }
    printMessage = false;
}

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
    printer.setSize(c);
  }
  // topic to inverse the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textinverse) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      printer.inverseOn();
    } else {
      printer.inverseOff();
    }
  }
  // topic to justify the text (L | C | R)
  if (strcmp(topic, mqtt_listen_topic_textjustify) == 0) {
    char c = 'L';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    printer.justify(c);
  }
  // topic to bold the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textbold) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      printer.boldOn();
    } else {
      printer.boldOff();
    }
  }
  // topic to underline the text (0 | 1)
  if (strcmp(topic, mqtt_listen_topic_textunderline) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) {
      c = payload[i];
    }
    if (c == '1') {
      printer.underlineOn();
    } else {
      printer.underlineOff();
    }
  }

  // topic to print barcode
  if (strcmp(topic, mqtt_listen_topic_barcode) == 0) {
    uint8_t barcode_type = 0;
    char barcode_value[255] = "";  // some borcodes allows only 255 digits(!) but not for our 58mm printer ;)
    int y = 0;
    bool barcodeseperatorfound = false;
    for (int i = 0; i < length; i++) {
      if (!barcodeseperatorfound) {
        if (payload[i] == '|') {
          barcodeseperatorfound = true;
          continue;
        }
        char c = payload[i];
        if (c >= '0' && c <= '9') {
          barcode_type = barcode_type * 10 + c - '0';  //encode to interger
        }
      } else {
        barcode_value[y++] = (char)payload[i];
      }
    }

    printer.printBarcode(barcode_value, (uint8_t)barcode_type);
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

void setLed(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void setup() {

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(DEBUG_PIN, INPUT);
  setLed(255, 255, 255);
  Serial.begin(115200);
  attachInterrupt(BUTTON_PIN, isr, FALLING);

  mySerial.begin(baud);
  printer.begin();
  printer.setDefault();

  printer.setSize(mqtt_text_size);
  printer.setLineHeight(mqtt_row_spacing);
  Serial.println("printer initializzed");

  wifi_station_set_hostname(my_id);
  Serial.print(F("Hostname: "));
  Serial.println(my_id);
  WiFi.mode(WIFI_STA);

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
}

void loop() {
  if(printMessage){
    printQueuedMessage();
  }

  redLed.Update();
  greenLed.Update();
  blueLed.Update();

  // if (digitalRead(BUTTON_PIN) == LOW) {
  //   Serial.println("button press detected");
  //   printQueuedMessage();
  //   delay(100);
  // }

  if (WiFi.status() != WL_CONNECTED) {
    setLed(255, 0, 0);

    Serial.println(F("Connecting to WiFi..."));
    WiFi.begin(ssid, password);

    unsigned long begin_started = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      if (millis() - begin_started > 60000) {
        Serial.println("Wifi not connected in 1 minute. Restarting...");
        ESP.restart();
      }
    }
    Serial.println("WiFi connected!");
  }

  if (!mqtt.connected()) {
    setLed(255, 50, 0);
    Serial.print("connecting to broker at:");
    Serial.println(mqtt_server);

    if (mqtt.connect(mqtt_id)) {
      Serial.println("Connected to broker at: ");
      Serial.println(mqtt_server);
      setLed(0, 255, 0);
      // printer.feed(1);
      mqtt.subscribe(mqtt_listen_topic_text2print);
      mqtt.subscribe(mqtt_listen_topic_textsize);
      mqtt.subscribe(mqtt_listen_topic_textlineheight);
      mqtt.subscribe(mqtt_listen_topic_textinverse);
      mqtt.subscribe(mqtt_listen_topic_textjustify);
      mqtt.subscribe(mqtt_listen_topic_textbold);
      mqtt.subscribe(mqtt_listen_topic_textunderline);
      mqtt.subscribe(mqtt_listen_topic_barcode);
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

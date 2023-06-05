#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BH1750.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <Wire.h>

int flexs = 32;
int buzzerPin = 12;
int badan = 0;
String durasi = "";
int count = 0;
boolean avgstatus = true;
boolean waitstatus = false;
boolean buzzerstatus = false;
boolean buzzerpermission = true;
unsigned long UpdateInsStamp = 0;
unsigned long BuzzerTimeStamp = 0;
unsigned long BuzzerMaxTime = 20000;
unsigned long WaitingInsStamp = 0;
unsigned long messageTimestamp = 0;
unsigned long instanceTimestamp = 0;
unsigned long instanceMaxTime = 30000;
long TotalLux = 0;
long AvgLux = 0;
long JmlLux = 0;
long TotalCm = 0;
long AvgCm = 0;
long JmlCm = 0;
int postur = 0;
int addr = 0;
const int trigPin = 5;
const int echoPin = 18;
#define SOUND_SPEED 0.034
long duration;
float distanceCm;
String BASE_URL = "http://192.168.159.251:8000";
String data;
String readFromEEPROM(int addrOffset);
void writeToEEPROM(int addrOffset, const String& strToWrite);

WiFiClient mqttClient;
PubSubClient subClient(mqttClient);
boolean message = false;

const char* mqttServer = "192.168.159.251";
const char* mqttTopicStart = "body/monitor/start/";
const char* mqttTopicStop = "body/monitor/stop/";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived in topic: " + String(topic));

  // Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Serial.println("-----------------------");
  String top1 = mqttTopicStart + data;
  const char* top1Char = top1.c_str();

  String top2 = mqttTopicStop + data;
  const char* top2Char = top2.c_str();
  if (String(topic) == top1) {
    Serial.println("Sesi mulai");
    message = true;
    count = 0;
    avgstatus = true;
    waitstatus = false;
    UpdateInsStamp = 0;
    WaitingInsStamp = 0;
    messageTimestamp = 0;
    instanceTimestamp = 0;
  }
  if (String(topic) == top2) {
    message = false;
    Serial.println("Sesi selesai");
  }
}

void connectMqtt() {
  subClient.setServer(mqttServer, mqttPort);
  subClient.setCallback(callback);
  while (!subClient.connected()) {
    Serial.println("Connecting to PubSubClient MQTT...");

    if (subClient.connect("body/monitoring/ESP32", mqttUser, mqttPassword)) {
      Serial.println("Connected to PubSubClient MQTT broker");

      String top1 = mqttTopicStart + data;
      const char* top1Char = top1.c_str();

      String top2 = mqttTopicStop + data;
      const char* top2Char = top2.c_str();

      // Subscribe to the desired topic
      subClient.subscribe(top1Char);
      subClient.subscribe(top2Char);
    } else {
      Serial.print("Failed to connect to MQTT broker, state: ");
      Serial.println(subClient.state());
      delay(2000);
    }
  }
}

int SendDataServer(String endpoint, String payload) {
  String url = BASE_URL + endpoint;
  Serial.print("URL: ");
  Serial.println(url);
  HTTPClient client;
  client.begin(url);
  client.addHeader("Content-Type", "application/json");
  int httpCode = client.POST(payload);
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  client.end();
  return httpCode;
}

int DeviceId(String endpoint, String payload) {
  String url = BASE_URL + endpoint;
  Serial.print("URL: ");
  Serial.println(url);
  HTTPClient client;
  client.begin(url);
  client.addHeader("Content-Type", "application/json");
  int httpCode = client.POST(payload);
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  Serial.print("Data: ");

  String message = client.getString();
  Serial.println(message);
  StaticJsonDocument<500> doc;  // Memory pool
  DeserializationError error = deserializeJson(doc, message);
  String id = doc["data"]["data"]["shortid"];
  Serial.println(id);
  Serial.print("err: ");
  Serial.println(error.f_str());
  writeToEEPROM(0, id);
  data = readFromEEPROM(0);
  Serial.print("data yang diinput = ");
  Serial.println(data);

  client.end();
  return httpCode;
}

String readFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void writeToEEPROM(int addrOffset, const String& strToWrite) {
  int len = strToWrite.length();
  Serial.println("Store new data to EEPROM");
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    EEPROM.commit();
  }
}

void BuzzerMethod() {
  digitalWrite(buzzerPin, HIGH);
  delay(300);
  digitalWrite(buzzerPin, LOW);
}

BH1750 lightMeter;

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                             "Thursday", "Friday", "Saturday"};

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // put your setup code here, to run once:
  EEPROM.begin(512);
  int eeAddress = 0;
  Serial.begin(9600);
  Serial.println("READY");
  pinMode(flexs, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  Wire.begin();
  lightMeter.begin();
  pinMode(trigPin, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  rtc.adjust(DateTime(__DATE__, __TIME__));

  display.display();
  delay(2);
  display.clearDisplay();

  display.clearDisplay();
  display.setTextColor(WHITE);
  // display.startscrollright(0x00, 0x0F);
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print(" Sistem Monitoring ");
  display.setCursor(0, 20);
  display.print(" Pengguna Komputer ");
  display.setCursor(0, 40);
  display.print(" By Rayhan ");
  display.display();
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFiManager wm;

  bool res;
  res = wm.autoConnect("AutoConnectAP", "password");

  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yeey :)");
  }
  // DeviceId("/api/v1/device/generate-id", "{}");
  Serial.print("Status EEPROM: ");
  // float TestAddress;
  // EEPROM.get(0, TestAddress);
  data = readFromEEPROM(0);

  if (data == "") {
    Serial.print("EEPROM Kosong");
    DeviceId("/api/v1/device/generate-id", "{}");
  }

  else {
    Serial.println("EEPROM isi");
    data = readFromEEPROM(0);
    Serial.print("Isi EEPROM Adalah = ");
    Serial.println(data);
  }
  instanceTimestamp = millis();
  connectMqtt();
}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();
  badan = analogRead(flexs);
  String postur = "";
  String bacaeprom = readFromEEPROM(0);
  float lux = lightMeter.readLightLevel();
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * SOUND_SPEED / 2;
  if (millis() < instanceTimestamp + instanceMaxTime) {
    TotalLux += lux;
    JmlLux += 1;
    TotalCm += distanceCm;
    JmlCm += 1;
  }

  if (avgstatus == true && message == true &&
      millis() > instanceTimestamp + instanceMaxTime) {
    AvgLux = TotalLux / JmlLux;
    AvgCm = TotalCm / JmlCm;
    if (AvgLux >= 90 && AvgLux <= 110) {
      if (AvgCm >= 51 && AvgCm <= 75) {
        Serial.println("Rentang lama = 45 Menit");
        durasi = "45";
      } else if (AvgCm >= 40 && AvgCm <= 50) {
        Serial.println("Rentang lama = 30 Menit");
        durasi = "30";
      } else if (AvgCm > 75) {
        Serial.println("Jarak layar terlalu jauh");
        durasi = "45";
      } else {
        Serial.println("Jarak anda terlalu dekat");
        durasi = "20";
      }
    } else if (AvgLux >= 120 && AvgLux <= 150) {
      if (AvgCm >= 51 && AvgCm <= 75) {
        Serial.println("Rentang lama = 25 Menit");
        durasi = "25";
      } else if (AvgCm >= 40 && AvgCm <= 50) {
        Serial.println("Rentang lama = 20 Menit");
        durasi = "20";
      } else if (AvgCm > 75) {
        Serial.println("Jarak layar terlalu jauh");
        durasi = "20";
      } else {
        Serial.println("Jarak anda terlalu dekat");
        durasi = "25";
      }
    }

    else if (AvgLux > 150) {
      Serial.println("Pencahayaan layar terlalu terang");
      durasi = "20";
    }

    else {
      Serial.println("Pencahayaan layar terlalu redup");
      durasi = "45";
    }

    int DurasiInt = durasi.toInt();
    avgstatus = false;
    waitstatus = true;
    UpdateInsStamp = millis();
    WaitingInsStamp = DurasiInt * 1000;  // detik /menit * 60000;
    Serial.printf("waktu istirahat = %s\n", durasi);
  }

  if (waitstatus == true && message == true &&
      millis() > UpdateInsStamp + WaitingInsStamp) {
    Serial.println("cari average lagi");
    String strcount;
    count += 1;
    strcount = String(count);
    avgstatus = true;
    instanceTimestamp = millis();
    waitstatus = false;
    TotalLux = 0;
    JmlLux = 0;
    TotalCm = 0;
    JmlCm = 0;
    String dataToSend = "{\"count\" : \"" + strcount + "\"}";
    SendDataServer("/api/v1/session/restupdate", dataToSend);
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(bacaeprom);
  if (lux >= 90 && lux <= 110) {
    if (distanceCm >= 51 && distanceCm <= 75) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println(bacaeprom);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Device Berjalan");
      display.setCursor(0, 40);
      display.println("Skripsi TMJ PNJ");
    } else if (distanceCm >= 40 && distanceCm <= 50) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println(bacaeprom);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Device Berjalan");
      display.setCursor(0, 40);
      display.println("Skripsi TMJ PNJ");
    } else if (distanceCm > 75) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Jarak Anda Dengan ");
      display.setCursor(0, 30);
      display.println("Layar Terlalu Jauh");
      buzzerstatus = true;
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Jarak Anda Dengan ");
      display.setCursor(0, 30);
      display.println("Layar Terlalu Dekat");
      buzzerstatus = true;
    }
  } else if (lux >= 120 && lux <= 150) {
    if (distanceCm >= 51 && distanceCm <= 75) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println(bacaeprom);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Device Berjalan");
      display.setCursor(0, 40);
      display.println("Skripsi TMJ PNJ");
    } else if (distanceCm >= 40 && distanceCm <= 50) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println(bacaeprom);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Device Berjalan");
      display.setCursor(0, 40);
      display.println("Skripsi TMJ PNJ");
    } else if (distanceCm > 75) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Jarak Anda Dengan ");
      display.setCursor(0, 30);
      display.println("Layar Terlalu Jauh");
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Jarak Anda Dengan ");
      display.setCursor(0, 30);
      display.println("Layar Terlalu Dekat");
    }

  } else if (lux > 150) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Pencahayaan Laptop");
    display.setCursor(0, 30);
    display.println("Terlalu Terang");
    buzzerstatus = true;

  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Pencahayaan Laptop");
    display.setCursor(0, 30);
    display.println("Terlalu Redup");
    buzzerstatus = true;
  }
  if (buzzerstatus == true && buzzerpermission == true) {
    BuzzerMethod();
    BuzzerTimeStamp = millis();
    buzzerstatus = false;
    buzzerpermission = false;
  }
  if (millis() - BuzzerTimeStamp > BuzzerMaxTime) {
    buzzerpermission = true;
  }
  if (badan < 720) {
    Serial.println("Postur Bungkuk");
    postur = "Bungkuk";
  } else if (badan > 800) {
    Serial.println("Postur Tegang");
    postur = "Tegang";
  } else if (badan >= 720 && badan <= 800) {
    Serial.println("Optimal");
    postur = "Optimal";
  } else {
    Serial.println("Error");
    postur = "Error";
  }
  display.display();
  uint64_t skrg = millis();
  if (skrg - messageTimestamp > 1000 && message == true) {
    messageTimestamp = millis();
    String strpostur = String(postur);
    String strlux = String(lux);
    String strjarak = String(distanceCm);
    String strdurasi = durasi;
    String dataToSend = "{\"cahaya\" : \"" + strlux + "\",\"jarak\" : \"" +
                        strjarak + "\",\"flex\":\"" + strpostur +
                        "\",\"rest\": \"" + strdurasi + "\",\"deviceId\" : \"" +
                        bacaeprom + "\"}";
    SendDataServer("/api/v1/session/stream", dataToSend);
  }
  if (!subClient.connected()) {
    connectMqtt();
  }
  subClient.loop();
}
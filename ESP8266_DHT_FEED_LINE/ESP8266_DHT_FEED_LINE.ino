#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <MicroGear.h>
#include "DHT.h"                              // library สำหรับอ่านค่า DHT Sensor ต้องติดตั้ง DHT sensor library by Adafruit v1.2.3 ก่อน

// ----- แก้ค่า config 8 ค่าข้างล่างนี้ --------------------------------------------------------
const char* ssid     = "YOUR_WIFI_SSID";      // ชื่อ ssid
const char* password = "YOUR_WIFI_PASSWORD";  // รหัสผ่าน wifi

#define APPID   "GROUP_APPID"
#define KEY     "GROUP_KEY"
#define SECRET  "GROUP_SECRET"

#define ALIAS   "YOUR_UNIQUE_ALIAS"           // แทนที่ด้วยหมายเลขของท่าน เช่น "A01"
#define NEIGHBOR "NEIGHBOR_ALIAS"             // ชื่ออุปกรณ์ของเพื่อน เช่น "A02"

#define LINE_TOKEN "YOUR LINE TOKEN"

// --------------------------------------------------------------------------------------

#define LEDSTATETOPIC "/ledstate/" ALIAS      // topic ที่ต้องการ publish ส่งสถานะ led ในที่นี้จะเป็น /ledstate/{ชื่อ alias ตัวเอง}
#define DHTDATATOPIC "/dht/" ALIAS            // topic ที่ต้องการ publish ส่งข้อมูล dht ในที่นี่จะเป็น /dht/{ชื่อ alias ตัวเอง}

#define BUTTONPIN  D7                         // pin ที่ต่อกับปุ่ม Flash บนบอร์ด NodeMCU
#define LEDPIN     D0                        // pin ที่ต่อกับไฟ LED บนบอร์ด NodeMCU

#define FEEDID   "FEEDID"           // ให้แทนที่ด้วย FeedID
#define FEEDAPI  "FEEDAPI"          // ให้แทนที่ด้วย FeedAPI

int currentLEDState = 0;      // ให้เริ่มต้นเป็น OFF
int lastLEDState = 1;
int currentButtonState = 1;   // หมายเหตุ ปุ่ม flash ต่อเข้ากับ GPIO0 แบบ pull-up
int lastButtonState = 1;

#define DHTPIN    D4          // GPIO2 ขาที่ต่อเข้ากับขา DATA (บางโมดูลใช้คำว่า OUT) ของ DHT
#define DHTTYPE   DHT22       // e.g. DHT11, DHT21, DHT22
DHT dht(DHTPIN, DHTTYPE);

float humid = 0;     // ค่าความชื้น
float temp  = 0;     // ค่าอุณหภูมิ

long lastDHTRead = 0;
long lastDHTPublish = 0;

long lastTimeWriteFeed = 0;

WiFiClient client;
MicroGear microgear(client);

void Line_Notify(String LINE_Token, String message) {

  String msg = String("message=") + message;

  WiFiClientSecure client;
  if (!client.connect("notify-api.line.me", 443)) {
    Serial.println("connection failed");
    return;
  }

  String req = "";
  req += "POST /api/notify HTTP/1.1\r\n";
  req += "Host: notify-api.line.me\r\n";
  req += "Content-Type: application/x-www-form-urlencoded\r\n";
  req += "Authorization: Bearer " + String(LINE_Token) + "\r\n";
  req += "Content-Length: " + String(msg.length()) + "\r\n";
  req += "\r\n";
  req +=  msg;

  client.print(req);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
}

void updateLED(int state) {
  // ไฟ LED บน NodeMCU จะติดก็ต่อเมื่อส่งค่า HIGH ไปให้ LEDPIN
  if (state == 1 && currentLEDState == 0) {
    currentLEDState = 1;
    digitalWrite(LEDPIN, HIGH); // LED ON
  }
  else if (state == 0 && currentLEDState == 1) {
    currentLEDState = 0;
    digitalWrite(LEDPIN, LOW); // LED OFF
  }
}

void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
  Serial.print("Incoming message --> ");
  msg[msglen] = '\0';
  Serial.println((char *)msg);

  if (*(char *)msg == '0') updateLED(0);
  else if (*(char *)msg == '1') updateLED(1);
}

void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.println("Connected to NETPIE...");
  microgear.setAlias(ALIAS);
}

void setup() {
  microgear.on(MESSAGE, onMsghandler);
  microgear.on(CONNECTED, onConnected);

  Serial.begin(115200);
  Serial.println("Starting...");
  dht.begin(); // initialize โมดูล DHT

  // กำหนดชนิดของ PIN (ขาI/O) เช่น INPUT, OUTPUT เป็นต้น
  pinMode(LEDPIN, OUTPUT);          // LED pin mode กำหนดค่า
  pinMode(BUTTONPIN, INPUT);        // Button pin mode รับค่า
  updateLED(currentLEDState);

  if (WiFi.begin(ssid, password)) {
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  microgear.init(KEY, SECRET, ALIAS); // กำหนดค่าตันแปรเริ่มต้นให้กับ microgear
  microgear.connect(APPID);           // ฟังก์ชั่นสำหรับเชื่อมต่อ NETPIE
}

void loop() {
  if (microgear.connected()) {
    microgear.loop();

    if (currentLEDState != lastLEDState) {
      microgear.publish(LEDSTATETOPIC, currentLEDState);  // LEDSTATETOPIC ถูก define ไว้ข้างบน
      lastLEDState = currentLEDState;
    }

    currentButtonState = digitalRead(BUTTONPIN);
    if (currentButtonState == 0) {
      delay(300);
      if (lastLEDState == 0) {
        updateLED(1);
      } else {
        updateLED(0);
      }
    }

    // เซนเซอร์​ DHT อ่านถี่เกินไปไม่ได้ จะให้ค่า error เลยต้องเช็คเวลาครั้งสุดท้ายที่อ่านค่า
    // ว่าทิ้งช่วงนานพอหรือยัง ในที่นี้ตั้งไว้ 2 วินาที ก
    if (millis() - lastDHTRead > 2000) {
      humid = dht.readHumidity();     // อ่านค่าความชื้น
      temp  = dht.readTemperature();  // อ่านค่าอุณหภูมิ
      lastDHTRead = millis();

      Serial.print("Humid: "); Serial.print(humid); Serial.print(" %, ");
      Serial.print("Temp: "); Serial.print(temp); Serial.println(" C ");

      // ตรวจสอบค่า humid และ temp เป็นตัวเลขหรือไม่
      if (isnan(humid) || isnan(temp)) {
        Serial.println("Failed to read from DHT sensor!");
      }
      else {
        // เตรียมสตริงในรูปแบบ "{humid},{temp}"
        String datastring = (String)humid + "," + (String)temp;
        Serial.print("Sending --> ");
        Serial.println(datastring);
        microgear.publish(DHTDATATOPIC, datastring);  // DHTDATATOPIC ถูก define ไว้ข้างบน
        if (temp > 35) Line_Notify(LINE_TOKEN, "Temp Over 35");
      }
    }

    if (millis() - lastTimeWriteFeed > 15000) {
      lastTimeWriteFeed = millis();
      if (humid != 0 && temp != 0) {
        String data = "{\"humid\":";
        data += humid ;
        data += ", \"temp\":";
        data += temp ;
        data += "}";
        Serial.print("Write Feed --> ");
        Serial.println(data);
        microgear.writeFeed(FEEDID, data);
        
        //microgear.writeFeed(FEEDID,data,FEEDAPI);
      }
    }
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
}

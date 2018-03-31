#include <ESP8266WiFi.h>
#include <MicroGear.h>

// ----- แก้ค่า config 7 ค่าข้างล่างนี้ --------------------------------------------------------
const char* ssid     = "SSID";      // ชื่อ ssid
const char* password = "PASSWORD";  // รหัสผ่าน wifi

#define APPID   "GROUP_APPID"
#define KEY     "GROUP_KEY"
#define SECRET  "GROUP_SECERT"

#define ALIAS   "ALIAS"           // แทนที่ด้วยหมายเลขของท่าน เช่น "A01"
#define NEIGHBOR "NEIGHBOR"             // ชื่ออุปกรณ์ของเพื่อน เช่น "A02"
// --------------------------------------------------------------------------------------

#define BUTTONPIN  D7                         // pin ที่ต่อกับปุ่มบนบอร์ด NodeMCU
#define LEDPIN     D0                // pin ที่ต่อกับไฟ LED บนบอร์ด NodeMCU

int currentLEDState = 0;      // ให้เริ่มต้นเป็น OFF
int currentButtonState = 1;   
int lastButtonState = 1;

WiFiClient client;
MicroGear microgear(client);

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
    currentButtonState = digitalRead(BUTTONPIN);
    if (currentButtonState != lastButtonState) {
      microgear.chat(NEIGHBOR, !currentButtonState);
      lastButtonState = currentButtonState;
    }
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
}


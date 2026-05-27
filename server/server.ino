#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "function.h"
#include "Arduino_LED_Matrix.h" 

// --- 小節コントロール用変数 ---
uint32_t LastBarTime = 0;
uint16_t Interval = 2000;
uint8_t BarCount = 39;
IPAddress BCaddress(255, 255, 255, 255);
uint16_t Port = 3000;

WiFiUDP udp;
ArduinoLEDMatrix matrix; 

// --- BPMコントロール用変数 ---
uint8_t BPM = 120; // BPMの初期値
uint32_t LastPressTime = 0; // スイッチが最後に押された時間
bool Flag = false; // BPM変更が発生したかどうかのフラグ

char ssid[] = "hackathon003-WPA2";
char pass[] = "hackathon003";

void setup() {
  Serial.begin(115200);
  matrix.begin(); 

  Interval = (60000 / BPM) * 4;
  
  pinMode(2, INPUT); 
  pinMode(3, INPUT);

  Serial.println("Wi-Fiに接続中...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n接続完了!");
  udp.begin(Port);
}

void loop() {
  Bar_control(&LastBarTime, Interval, &BarCount, BCaddress, Port, udp);
  
  BPM_control(&BPM, &Interval, &LastPressTime, &Flag, BCaddress, Port, udp);
}
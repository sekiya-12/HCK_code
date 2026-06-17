#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "function.h"
#include "myLEDMatrix.h"

uint32_t LastBarTime = 0;
uint16_t Interval = 2000;
uint8_t BarCount = 0;
IPAddress BCaddress(255, 255, 255, 255);
uint16_t Port = 3000;

WiFiUDP udp;

uint8_t BPM = 120; // BPMの初期値
uint32_t LastPressTime = 0; // スイッチが最後に押された時間
bool Flag = false; // BPM変更が発生したかどうかのフラグ

char ssid[] = "hackathon003-WPA2";
char pass[] = "hackathon003";

void setup() {
  Serial.begin(115200);
  initLEDMatrix();
  
  pinMode(2, INPUT); 
  pinMode(3, INPUT);
  pinMode(4, OUTPUT);

  Serial.println("Wi-Fiに接続中...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n接続完了!");
  udp.begin(Port);
  updateDisplay(BPM);
}

void loop() {
  // 小節コントロール
  Bar_control(&LastBarTime, Interval, &BarCount, BCaddress, Port, udp);
  // === BPMコントロール ===
  BPM_control(&BPM, &Interval, &LastPressTime, &Flag, BCaddress, Port, udp);
}
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "function.h"
#include "myLEDMatrix.h"

uint16_t Interval = 2000;
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
  Stage_init();   // 演奏モードボタン・スポットライト・カーテンモータの初期化

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
  // 舞台・演奏コントロール（ボタン押下で開始）
  Stage_control(Interval, BCaddress, Port, udp);
  // === BPMコントロール（いつでも変更可能）===
  BPM_control(&BPM, &Interval, &LastPressTime, &Flag, BCaddress, Port, udp);
}
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "server_function.h"
#include "Arduino_LED_Matrix.h" 

// 【ネットワーク設定】
char ssid[] = "hackathon003-WPA2";
char pass[] = "hackathon003";
IPAddress BCaddress(255, 255, 255, 255); // 全員に一斉送信（ブロードキャスト）

uint16_t Port = 3000;
WiFiUDP udp;

// 【システム設定用変数】
ArduinoLEDMatrix matrix; 

// --- 小節管理用 ---
uint32_t LastBarTime = 0;
uint16_t Interval = 2000;
uint8_t BarCount = 39;

// --- BPM管理用 ---
uint8_t BPM = 120;
uint32_t LastPressTime = 0;
bool Flag = false;


void setup() {
  Serial.begin(115200);
  matrix.begin();
  
  // スイッチピンの設定
  pinMode(2, INPUT); 
  pinMode(3, INPUT);

  // 初期Intervalの計算
  Interval = (60000 / BPM) * 4;
  
  // Wi-Fi接続処理
  Serial.print("サーバー：Wi-Fiに接続中: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  // IPアドレス取得待機
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("ルーターからIPアドレスを取得中...");
    delay(500);
  }
  
  Serial.println("接続完了!");
  Serial.print("サーバー自身のIPアドレス: ");
  Serial.println(WiFi.localIP());

  udp.begin(Port);
}


void loop() {
  Bar_control(&LastBarTime, Interval, &BarCount, BCaddress, Port, udp);
  BPM_control(&BPM, &Interval, &LastPressTime, &Flag, BCaddress, Port, udp);
}
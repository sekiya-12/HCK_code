#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Servo.h>

WiFiUDP Udp;
Servo myServo;

//============================
// WiFi設定
//============================
char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASS";
unsigned int Port = 3000;

//============================
// PDF準拠変数
//============================
uint8_t Data = 0;
uint8_t CurrentBPM = 120;
uint8_t CurrentBar = 0;

bool Flag = false;

float ToneLength = 0.5;
uint32_t StartTime = 0;
uint16_t Interval = 500;

//============================
// 安定化用（追加のみ）
//============================
uint32_t LastBPMChangeTime = 0;
const uint16_t BPM_LOCK_MS = 200;   // BPM変更直後の安定待ち

//============================
// サーボ設定
//============================
bool Dir = false;
const int LEFT_ANGLE = 40;
const int RIGHT_ANGLE = 140;

//============================
// setup
//============================
void setup() {
  Serial.begin(9600);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(1000);
  }

  Udp.begin(Port);

  myServo.attach(9);
  myServo.write(LEFT_ANGLE);
}

//============================
// Parse_data（PDF準拠＋安全化）
//============================
bool Parse_data(uint8_t *Data) {

  int packet = Udp.parsePacket();

  if (packet <= 0) return false;   // ← 安定化①

  if (packet > 0) {
    *Data = Udp.read();

    if (*Data >= 40) return true;
    else return false;
  }

  return false;
}

//============================
// BPM_update（PDF準拠＋安定化）
//============================
void BPM_update(uint8_t *CurrentBPM, bool Flag, uint8_t Data, float *ToneLength, uint16_t *Interval) {

  if (!Flag) return;

  *CurrentBPM = Data;

  *ToneLength = 60.0 / (*CurrentBPM);
  *Interval = (uint16_t)(*ToneLength * 1000);

  // 安定化②：BPM変更直後の暴れ防止
  LastBPMChangeTime = millis();
}

//============================
// Performance（PDF準拠＋サーボ最適化）
//============================
void Performance(uint8_t *CurrentBar, uint8_t Data, uint32_t *StartTime, uint16_t *Interval) {

  // BPM変更直後は一瞬停止（重要）
  if (millis() - LastBPMChangeTime < BPM_LOCK_MS) return;

  // 小節更新時
  if (*CurrentBar != Data) {

    *CurrentBar = Data;

    Dir = !Dir;
    myServo.write(Dir ? RIGHT_ANGLE : LEFT_ANGLE);

    *StartTime = millis();
    return;
  }

  // 通常周期動作（安全なmillis比較）
  if ((uint32_t)(millis() - *StartTime) >= *Interval) {

    Dir = !Dir;
    myServo.write(Dir ? RIGHT_ANGLE : LEFT_ANGLE);

    *StartTime = millis();
  }
}


void loop() {

  Flag = Parse_data(&Data);

  BPM_update(&CurrentBPM, Flag, Data, &ToneLength, &Interval);

  if (!Flag) {
    Performance(&CurrentBar, Data, &StartTime, &Interval);
  }
}


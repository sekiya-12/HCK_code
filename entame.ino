#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Servo.h>

// ==================================================
// エンタメ性専用クライアント
// サーバー・楽器クライアント側は変更しない前提
//
// サーバーからのUDP 1Byteを受信する
// 0〜39     → 小節番号
// 40〜255  → BPM
// ==================================================

// =====================
// 楽器選択
// =====================
#define PIANO      1
#define TROMBONE   2
#define VIOLIN     3
#define CASTANET   4

// 使う人形に合わせてここだけ変更
#define INSTRUMENT PIANO
// #define INSTRUMENT TROMBONE
// #define INSTRUMENT VIOLIN
// #define INSTRUMENT CASTANET

// =====================
// Wi-Fi設定
// =====================
char ssid[] = "WIFI_SSID";
char pass[] = "WIFI_PASSWORD";

// =====================
// UDP設定
// サーバー側と同じポートにする
// =====================
WiFiUDP Udp;
const unsigned int localPort = 3000;

// =====================
// サーボピン設定
// =====================
const int RIGHT_ARM_PIN = 9;
const int LEFT_ARM_PIN  = 10;
const int EXTRA_PIN     = 5;

// ピアノ：右腕 D9，左腕 D10
// トロンボーン：腕 D9
// ヴァイオリン：腕 D9，首 D5
// カスタネット：腕 D9，口 D5

// =====================
// サーボオブジェクト
// =====================
Servo rightArmServo;
Servo leftArmServo;
Servo extraServo;

// =====================
// 基本角度
// 人形に引っかかる場合は角度を小さくする
// =====================
const int ARM_REST_ANGLE = 90;

// ピアノ
const int PIANO_RIGHT_PLAY_ANGLE = 70;
const int PIANO_LEFT_PLAY_ANGLE  = 110;

// トロンボーン
const int TROMBONE_FORWARD_ANGLE = 65;
const int TROMBONE_BACK_ANGLE    = 115;

// ヴァイオリン
const int VIOLIN_BOW_LEFT_ANGLE  = 65;
const int VIOLIN_BOW_RIGHT_ANGLE = 115;

// カスタネット
const int CASTANET_PLAY_ANGLE = 70;

// ヴァイオリン首
const int HEAD_CENTER_ANGLE = 90;
const int HEAD_LEFT_ANGLE   = 70;
const int HEAD_RIGHT_ANGLE  = 110;

// カスタネット口
const int MOUTH_CLOSE_ANGLE = 90;
const int MOUTH_OPEN_ANGLE  = 60;

// =====================
// BPM・小節管理
// =====================
uint8_t currentBPM = 120;
uint8_t currentBar = 0;

unsigned long beatInterval = 500;   // 1拍の時間 ms
unsigned long barStartTime = 0;
unsigned long nextBeatTime = 0;

int beatInBar = 0;
const int BEATS_PER_BAR = 4;

bool barActive = false;
bool armDirection = false;

// =====================
// サーボを戻すための管理
// =====================
bool rightArmReturnActive = false;
bool leftArmReturnActive  = false;
bool extraReturnActive    = false;

unsigned long rightArmReturnTime = 0;
unsigned long leftArmReturnTime  = 0;
unsigned long extraReturnTime    = 0;

// =====================
// BPM変更時の演出管理
// =====================
bool emotionActive = false;
unsigned long emotionStartTime = 0;
unsigned long emotionEndTime = 0;
unsigned long lastEmotionMoveTime = 0;
bool emotionDirection = false;

// BPM変更時の演出を何拍分続けるか
const int EMOTION_BEATS = 4;

// =====================
// setup
// =====================
void setup() {
  Serial.begin(115200);

  setupServos();
  calculateBeatInterval();

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(1000);
  }

  Udp.begin(localPort);

  Serial.println("Entertainment client started");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("UDP port: ");
  Serial.println(localPort);
}

// =====================
// loop
// =====================
void loop() {
  receiveUdpData();

  updateNormalMotion();
  updateEmotionMotion();
  updateServoReturn();
}

// =====================
// サーボ初期化
// =====================
void setupServos() {
#if INSTRUMENT == PIANO
  rightArmServo.attach(RIGHT_ARM_PIN);
  leftArmServo.attach(LEFT_ARM_PIN);

  rightArmServo.write(ARM_REST_ANGLE);
  leftArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == TROMBONE
  rightArmServo.attach(RIGHT_ARM_PIN);
  rightArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == VIOLIN
  rightArmServo.attach(RIGHT_ARM_PIN);
  extraServo.attach(EXTRA_PIN);

  rightArmServo.write(ARM_REST_ANGLE);
  extraServo.write(HEAD_CENTER_ANGLE);

#elif INSTRUMENT == CASTANET
  rightArmServo.attach(RIGHT_ARM_PIN);
  extraServo.attach(EXTRA_PIN);

  rightArmServo.write(ARM_REST_ANGLE);
  extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}

// =====================
// UDP受信
// 0〜39：小節番号
// 40以上：BPM
// =====================
void receiveUdpData() {
  int packetSize = Udp.parsePacket();

  if (packetSize > 0) {
    int data = Udp.read();

    while (Udp.available()) {
      Udp.read();
    }

    if (data < 0) {
      return;
    }

    if (data >= 40) {
      updateBPM((uint8_t)data);
    } else {
      startBarMotion((uint8_t)data);
    }
  }
}

// =====================
// BPM更新
// =====================
void updateBPM(uint8_t newBPM) {
  if (newBPM == currentBPM) {
    return;
  }

  currentBPM = newBPM;
  calculateBeatInterval();

  Serial.print("BPM updated: ");
  Serial.println(currentBPM);

  startEmotion();
}

// =====================
// 1拍の時間を計算
// =====================
void calculateBeatInterval() {
  beatInterval = 60000UL / currentBPM;
}

// =====================
// 小節番号を受信したとき
// =====================
void startBarMotion(uint8_t barNumber) {
  currentBar = barNumber;

  barStartTime = millis();
  nextBeatTime = barStartTime;
  beatInBar = 0;
  barActive = true;

  Serial.print("Bar received: ");
  Serial.println(currentBar);
}

// =====================
// 通常演奏サーボ動作
// サーバーは小節番号だけ送るので，
// 1小節の中の4拍はエンタメArduino側で刻む
// =====================
void updateNormalMotion() {
  if (!barActive) {
    return;
  }

  unsigned long now = millis();

  if (beatInBar < BEATS_PER_BAR && now >= nextBeatTime) {
    triggerBeatMotion(beatInBar);

    beatInBar++;
    nextBeatTime += beatInterval;
  }

  if (beatInBar >= BEATS_PER_BAR &&
      now - barStartTime >= beatInterval * BEATS_PER_BAR) {
    barActive = false;
    resetMainServos();
  }
}

// =====================
// 拍ごとの楽器動作
// =====================
void triggerBeatMotion(int beat) {
  unsigned long pulseTime = beatInterval / 4;

  if (pulseTime < 80) {
    pulseTime = 80;
  }

  if (pulseTime > 250) {
    pulseTime = 250;
  }

#if INSTRUMENT == PIANO
  // ピアノ：左右の腕で交互に打鍵
  if (beat % 2 == 0) {
    triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime);
  } else {
    triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime);
  }

#elif INSTRUMENT == TROMBONE
  // トロンボーン：スライドを前後に動かす
  armDirection = !armDirection;

  if (armDirection) {
    rightArmServo.write(TROMBONE_FORWARD_ANGLE);
  } else {
    rightArmServo.write(TROMBONE_BACK_ANGLE);
  }

#elif INSTRUMENT == VIOLIN
  // ヴァイオリン：弓を左右に動かす
  armDirection = !armDirection;

  if (armDirection) {
    rightArmServo.write(VIOLIN_BOW_LEFT_ANGLE);
  } else {
    rightArmServo.write(VIOLIN_BOW_RIGHT_ANGLE);
  }

#elif INSTRUMENT == CASTANET
  // カスタネット：拍頭で腕を動かす
  triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

// =====================
// BPM変更時の演出開始
// =====================
void startEmotion() {
#if INSTRUMENT == VIOLIN || INSTRUMENT == CASTANET
  emotionActive = true;
  emotionStartTime = millis();
  emotionEndTime = emotionStartTime + beatInterval * EMOTION_BEATS;
  lastEmotionMoveTime = 0;
  emotionDirection = false;
#endif
}

// =====================
// BPM変更時の第二駆動
// ヴァイオリン：首振り
// カスタネット：口開閉
// =====================
void updateEmotionMotion() {
  if (!emotionActive) {
    return;
  }

  unsigned long now = millis();

  if (now >= emotionEndTime) {
    resetEmotionServo();
    emotionActive = false;
    return;
  }

  unsigned long emotionInterval = beatInterval / 2;

  if (emotionInterval < 100) {
    emotionInterval = 100;
  }

  if (now - lastEmotionMoveTime >= emotionInterval) {
    lastEmotionMoveTime = now;
    emotionDirection = !emotionDirection;

#if INSTRUMENT == VIOLIN
    if (emotionDirection) {
      extraServo.write(HEAD_LEFT_ANGLE);
    } else {
      extraServo.write(HEAD_RIGHT_ANGLE);
    }

#elif INSTRUMENT == CASTANET
    if (emotionDirection) {
      extraServo.write(MOUTH_OPEN_ANGLE);
    } else {
      extraServo.write(MOUTH_CLOSE_ANGLE);
    }
#endif
  }
}

// =====================
// 右腕パルス
// =====================
void triggerRightArmPulse(int angle, unsigned long pulseTime) {
  rightArmServo.write(angle);
  rightArmReturnActive = true;
  rightArmReturnTime = millis() + pulseTime;
}

// =====================
// 左腕パルス
// =====================
void triggerLeftArmPulse(int angle, unsigned long pulseTime) {
  leftArmServo.write(angle);
  leftArmReturnActive = true;
  leftArmReturnTime = millis() + pulseTime;
}

// =====================
// 追加サーボパルス
// =====================
void triggerExtraPulse(int angle, unsigned long pulseTime) {
  extraServo.write(angle);
  extraReturnActive = true;
  extraReturnTime = millis() + pulseTime;
}

// =====================
// サーボを元に戻す
// delayを使わない
// =====================
void updateServoReturn() {
  unsigned long now = millis();

  if (rightArmReturnActive && now >= rightArmReturnTime) {
    rightArmServo.write(ARM_REST_ANGLE);
    rightArmReturnActive = false;
  }

  if (leftArmReturnActive && now >= leftArmReturnTime) {
    leftArmServo.write(ARM_REST_ANGLE);
    leftArmReturnActive = false;
  }

  if (extraReturnActive && now >= extraReturnTime) {
    resetEmotionServo();
    extraReturnActive = false;
  }
}

// =====================
// 通常演奏サーボを待機位置へ戻す
// =====================
void resetMainServos() {
#if INSTRUMENT == PIANO
  rightArmServo.write(ARM_REST_ANGLE);
  leftArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == TROMBONE
  rightArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == VIOLIN
  rightArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == CASTANET
  rightArmServo.write(ARM_REST_ANGLE);
#endif
}

// =====================
// 第二駆動サーボを初期位置へ戻す
// =====================
void resetEmotionServo() {
#if INSTRUMENT == VIOLIN
  extraServo.write(HEAD_CENTER_ANGLE);

#elif INSTRUMENT == CASTANET
  extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}
#include <Servo.h>

// ==================================================
// 全自動テスト専用クライアント
// シリアル入力やWi-Fiは不要です。電源を入れれば自動で
// 「演奏」と「BPM変更」のテストサイクルを繰り返します。
// ==================================================

// =====================
// 楽器選択
// =====================
#define PIANO      1
#define TROMBONE   2
#define VIOLIN     3
#define CASTANET   4

// テストしたい人形に合わせてここを変更
//#define INSTRUMENT PIANO
//#define INSTRUMENT TROMBONE
//#define INSTRUMENT VIOLIN
#define INSTRUMENT CASTANET

// =====================
// サーボピン設定
// =====================
const int RIGHT_ARM_PIN = 9;
const int LEFT_ARM_PIN  = 11;
const int EXTRA_PIN     = 5;

// =====================
// サーボオブジェクト
// =====================
Servo rightArmServo;
Servo leftArmServo;
Servo extraServo;

// =====================
// 基本角度
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
// 【追加】自動テスト用変数
// =====================
unsigned long lastAutoTriggerTime = 0;
int autoTestStep = 0;

// =====================
// setup
// =====================
void setup() {
  Serial.begin(9600);

  setupServos();
  calculateBeatInterval();

  Serial.println("===============================================");
  Serial.println("Servo FULL AUTOMATIC Test Client Started");
  Serial.println("The program will trigger motions automatically.");
  Serial.println("===============================================");
  
  lastAutoTriggerTime = millis();
}

// =====================
// loop
// =====================
void loop() {
  runAutomaticTestTimeline(); // 【変更】ここでタイムラインを自動進行

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
// 【追加】自動テストシナリオ進行
// delayを使わず、前の動作が終わったタイミングで次を発動
// =====================
void runAutomaticTestTimeline() {
  // 現在演奏中の場合は、次の自動トリガーは待つ
  if (barActive) {
    return;
  }

  unsigned long now = millis();
  
  // 小節演奏が終わってから、次のアクションまで1.5秒の「間」を開ける
  if (now - lastAutoTriggerTime >= 0) {
    lastAutoTriggerTime = now;

    // ステップごとのテストシナリオ
    switch (autoTestStep) {
      case 0:
        Serial.println("\n[Auto Test] --- Step 1: Start playing at BPM 120 ---");
        startBarMotion(1); // 1小節目を開始
        autoTestStep++;
        break;

      case 1: case 2: case 3: case 4:
        Serial.print("[Auto Test] Continue playing (Bar ");
        Serial.print(autoTestStep + 1);
        Serial.println(")");
        startBarMotion(autoTestStep + 1);
        autoTestStep++;
        break;

      case 5:
        Serial.println("\n[Auto Test] --- Step 2: Speed up! Change BPM to 180 ---");
        updateBPM(180); // BPMを180に変更（Emotion機能が発動）
        autoTestStep++;
        break;

      case 6: case 7: case 8: case 9: case 10:
        Serial.print("[Auto Test] Play with FAST tempo (Bar ");
        Serial.print(autoTestStep - 5);
        Serial.println(")");
        startBarMotion(autoTestStep - 5);
        autoTestStep++;
        break;

      case 11:
        Serial.println("\n[Auto Test] --- Step 3: Slow down! Reset BPM to 120 ---");
        updateBPM(120); // BPMを120に戻す
        autoTestStep = 0; // ループをリセットして最初に戻る
        break;
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

  Serial.print("-> BPM updated: ");
  Serial.print(currentBPM);
  Serial.print(" (1 beat = ");
  Serial.print(beatInterval);
  Serial.println(" ms)");

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
}

// =====================
// 通常演奏サーボ動作
// =====================
void updateNormalMotion() {
  if (!barActive) {
    return;
  }

  unsigned long now = millis();

  if (beatInBar < BEATS_PER_BAR && now >= nextBeatTime) {
    Serial.print("   Beat ");
    Serial.println(beatInBar + 1); 
    triggerBeatMotion(beatInBar);

    beatInBar++;
    nextBeatTime += beatInterval;
  }

  if (beatInBar >= BEATS_PER_BAR &&
      now - barStartTime >= beatInterval * BEATS_PER_BAR) {
    barActive = false;
    resetMainServos();
    // 演奏が終了した時刻を記録（次の自動トリガーの基準にする）
    lastAutoTriggerTime = millis(); 
  }
}

// =====================
// 拍ごとの楽器動作
// =====================
void triggerBeatMotion(int beat) {
  unsigned long pulseTime = beatInterval / 4;

  if (pulseTime < 80) pulseTime = 80;
  if (pulseTime > 250) pulseTime = 250;

#if INSTRUMENT == PIANO
  if (beat % 2 == 0) {
    triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime);
  } else {
    triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime);
  }

#elif INSTRUMENT == TROMBONE
  armDirection = !armDirection;
  if (armDirection) {
    rightArmServo.write(TROMBONE_FORWARD_ANGLE);
  } else {
    rightArmServo.write(TROMBONE_BACK_ANGLE);
  }

#elif INSTRUMENT == VIOLIN
  armDirection = !armDirection;
  if (armDirection) {
    rightArmServo.write(VIOLIN_BOW_LEFT_ANGLE);
  } else {
    rightArmServo.write(VIOLIN_BOW_RIGHT_ANGLE);
  }

#elif INSTRUMENT == CASTANET
  triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

// =====================
// BPM変更時の演出開始
// =====================
void startEmotion() {
#if INSTRUMENT == VIOLIN || INSTRUMENT == CASTANET
  Serial.println("   [Emotion Mode Activated!]");
  emotionActive = true;
  emotionStartTime = millis();
  emotionEndTime = emotionStartTime + beatInterval * EMOTION_BEATS;
  lastEmotionMoveTime = 0;
  emotionDirection = false;
#endif
}

// =====================
// BPM変更時の第二駆動
// =====================
void updateEmotionMotion() {
  if (!emotionActive) {
    return;
  }

  unsigned long now = millis();

  if (now >= emotionEndTime) {
    resetEmotionServo();
    emotionActive = false;
    Serial.println("   [Emotion Mode Finished]");
    return;
  }

  unsigned long emotionInterval = beatInterval / 2;
  if (emotionInterval < 100) emotionInterval = 100;

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
#else
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
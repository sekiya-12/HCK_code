#include <Servo.h>

// =====================
// 楽器の選択（ここで動かしたい楽器を1つだけ有効にしてください）
// =====================
#define INST_PIANO    1
//#define INST_TROMBONE 2
//#define INST_VIOLIN   3
//#define INST_CASTANET 4

// ★★★ ここで楽器を切り替えてください ★★★
#define INSTRUMENT INST_PIANO 

// =====================
// 単体テスト用の管理変数
// =====================
unsigned long debugBarStartTime = 0; // 現在の小節が始まった時間
unsigned long debugBpmChangeTime = 0; // 次にBPMを変更する時間
uint8_t debugVirtualServerBar = 0;   // 擬似的なサーバー小節番号
bool debugFirstRun = true;           // 初回起動フラグ

// =====================
// サーボピン設定
// =====================
const int RIGHT_ARM_PIN = 9;
const int LEFT_ARM_PIN  = 10;
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
const int CASTANET_PLAY_ANGLE = 50;

// ヴァイオリン首
const int HEAD_CENTER_ANGLE = 90;
const int HEAD_LEFT_ANGLE   = 70;
const int HEAD_RIGHT_ANGLE  = 110;

// カスタネット口
const int MOUTH_CLOSE_ANGLE = 90;
const int MOUTH_OPEN_ANGLE  = 60;

// =====================
// 小節・オフセット設定
// =====================
const uint8_t SERVER_BAR_COUNT = 25; // 全25小節に変更
const int PART_BAR_OFFSET = 0;

// 全休符テーブル (提示されたScoreのRESTに合わせて最適化)
const bool WHOLE_REST_BAR_TABLE[SERVER_BAR_COUNT] = {
  false, false, false, false, false, false, false, false, // 0〜7小節 (1周目)
  true,  true,  true,  true,  true,                       // 8〜12小節 (休符)
  false, false, false, false, false, false, false, false, // 13〜20小節 (2周目)
  true,  true,  true,  true                               // 21〜24小節 (休符)
};

// =====================
// 楽譜データ設定
// =====================
enum ScoreEventType {
  SCORE_END  = 0,
  SCORE_PLAY = 1,
  SCORE_REST = 2
};

enum NoteLengthUnit {
  LENGTH_SIXTEENTH = 1,
  LENGTH_EIGHTH    = 2,
  LENGTH_QUARTER   = 4,
  LENGTH_HALF      = 8,
  LENGTH_WHOLE     = 16
};

struct ServoScoreEvent {
  ScoreEventType type;
  uint8_t lengthUnits;
};

const uint8_t MAX_EVENTS_PER_BAR = 8;
const uint8_t BAR_LENGTH_UNITS = 16;
const uint8_t QUARTER_LENGTH_UNITS = 4;

#define SCORE_FILL         { SCORE_END, 0 }

// 4分音符×4 (1.0拍×4)
#define BAR_QUARTER_4      { \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

// 4分音符×3 + 4分休符×1 (1.0拍×3 + 1.0拍休)
#define BAR_QUARTER_3_REST_1 { \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

// 8分音符×8 (0.5拍×8)
#define BAR_EIGHTH_8       { \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH} \
}

// 1小節全休符 (4.0拍休)
#define BAR_WHOLE_REST     { \
  {SCORE_REST, LENGTH_WHOLE}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

// 4分音符と4分休符の交互 (1.0拍交互)
#define BAR_NOTE_REST_NOTE_REST { \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

const ServoScoreEvent SCORE_TABLE[SERVER_BAR_COUNT][MAX_EVENTS_PER_BAR] = {
  BAR_QUARTER_4,            // 0小節目  (ドレミファ)
  BAR_QUARTER_3_REST_1,     // 1小節目  (ミレド・休)
  BAR_QUARTER_4,            // 2小節目  (ミファソラ)
  BAR_QUARTER_3_REST_1,     // 3小節目  (ソファミ・休)
  BAR_NOTE_REST_NOTE_REST,  // 4小節目  (ド・休・ド・休)
  BAR_NOTE_REST_NOTE_REST,  // 5小節目  (ド・休・ド・休)
  BAR_EIGHTH_8,             // 6小節目  (ドドレレミミファファ)
  BAR_QUARTER_3_REST_1,     // 7小節目  (ミレド・休)
  
  BAR_WHOLE_REST,           // 8小節目  ★完全停止
  BAR_WHOLE_REST,           // 9小節目
  BAR_WHOLE_REST,           // 10小節目
  BAR_WHOLE_REST,           // 11小節目
  BAR_WHOLE_REST,           // 12小節目

  BAR_QUARTER_4,            // 13小節目 (1オクターブ上: ドレミファ)
  BAR_QUARTER_3_REST_1,     // 14小節目 (ミレド・休)
  BAR_QUARTER_4,            // 15小節目 (ミファソラ)
  BAR_QUARTER_3_REST_1,     // 16小節目 (ソファミ・休)
  BAR_NOTE_REST_NOTE_REST,  // 17小節目 (ド・休・ド・休)
  BAR_NOTE_REST_NOTE_REST,  // 18小節目 (ド・休・ド・休)
  BAR_EIGHTH_8,             // 19小節目 (ドドレレミミファファ)
  BAR_QUARTER_3_REST_1,     // 20小節目 (ミレド・休)

  BAR_WHOLE_REST,           // 21小節目 ★完全停止
  BAR_WHOLE_REST,           // 22小節目
  BAR_WHOLE_REST,           // 23小節目
  BAR_WHOLE_REST            // 24小節目
};

#undef SCORE_FILL
#undef BAR_QUARTER_4
#undef BAR_QUARTER_3_REST_1
#undef BAR_EIGHTH_8
#undef BAR_WHOLE_REST
#undef BAR_NOTE_REST_NOTE_REST

// =====================
// BPM・小節管理
// =====================
uint8_t currentBPM = 120;
uint8_t currentBar = 0;

unsigned long beatInterval = 500;
unsigned long nextEventTime = 0;

uint8_t scoreEventIndex = 0;
uint8_t usedUnitsInBar = 0;

bool barActive = false;
bool armDirection = false;
bool motionMutedByWholeRest = false;
bool scoreRestActive = true;
bool pianoUseRightArm = true;

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
unsigned long emotionEndTime = 0;
unsigned long lastEmotionMoveTime = 0;
bool emotionDirection = false;

const int EMOTION_BEATS = 4;

// =====================
// 関数宣言
// =====================
void setupServos();
void simulateServerBehavior(); 
void updateBPM(uint8_t newBPM);
void calculateBeatInterval();
unsigned long noteLengthToMs(uint8_t lengthUnits);
unsigned long calculatePulseTime(unsigned long eventDuration);
uint8_t toScoreBar(uint8_t serverBarNumber);
bool isWholeRestBar(uint8_t scoreBarNumber);
void startBarMotion(uint8_t barNumber);
void updateNormalMotion();
void triggerNextScoreEvent();
void triggerScoreMotion(unsigned long eventDuration);
void finishBarMotion();
void startEmotion();
void updateEmotionMotion();
void triggerRightArmPulse(int angle, unsigned long pulseTime);
void triggerLeftArmPulse(int angle, unsigned long pulseTime);
void triggerExtraPulse(int angle, unsigned long pulseTime);
void updateServoReturn();
void stopAllMotionForWholeRest();
void stopMainMotionForRest();
void cancelServoReturns();
void resetMainServos();
void resetEmotionServo();
bool timeReached(unsigned long targetTime);

// =====================
// Arduino 標準の setup と loop
// =====================
void setup() {
  Serial.begin(115200);

  setupServos();
  calculateBeatInterval();

  Serial.println("[Test Mode] Standalone test started. (No Wi-Fi)");
  
  debugBarStartTime = millis();
  debugBpmChangeTime = millis() + 15000UL; // 15秒後にテストでBPMを変更
}

void loop() {
  simulateServerBehavior(); 

  updateNormalMotion();
  updateEmotionMotion();
  updateServoReturn();
}

// =====================
// Wi-Fi受信のシミュレーター
// =====================
void simulateServerBehavior() {
  unsigned long now = millis();

  if (debugFirstRun) {
    debugFirstRun = false;
    Serial.println("\n--- [Test] Start Bar: 0 ---");
    startBarMotion(debugVirtualServerBar);
    debugBarStartTime = now;
  }

  unsigned long barDuration = beatInterval * 4;
  if (now - debugBarStartTime >= barDuration) {
    debugVirtualServerBar++;
    if (debugVirtualServerBar >= SERVER_BAR_COUNT) {
      debugVirtualServerBar = 0; 
    }
    
    Serial.print("\n--- [Test] Start Bar: ");
    Serial.print(debugVirtualServerBar);
    Serial.println(" ---");
    
    startBarMotion(debugVirtualServerBar);
    debugBarStartTime = now;
  }

  if (timeReached(debugBpmChangeTime)) {
    if (currentBPM == 120) {
      updateBPM(160); 
    } else {
      updateBPM(120); 
    }
    debugBpmChangeTime = millis() + 20000UL; 
  }
}

// =====================
// サーボ初期化
// =====================
void setupServos() {
#if INSTRUMENT == INST_PIANO
  rightArmServo.attach(RIGHT_ARM_PIN);
  leftArmServo.attach(LEFT_ARM_PIN);
  rightArmServo.write(ARM_REST_ANGLE);
  leftArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == INST_TROMBONE
  rightArmServo.attach(RIGHT_ARM_PIN);
  rightArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == INST_VIOLIN
  rightArmServo.attach(RIGHT_ARM_PIN);
  extraServo.attach(EXTRA_PIN);
  rightArmServo.write(ARM_REST_ANGLE);
  extraServo.write(HEAD_CENTER_ANGLE);

#elif INSTRUMENT == INST_CASTANET
  rightArmServo.attach(RIGHT_ARM_PIN);
  extraServo.attach(EXTRA_PIN);
  rightArmServo.write(ARM_REST_ANGLE);
  extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}

// =====================
// BPM更新
// =====================
void updateBPM(uint8_t newBPM) {
  if (newBPM == currentBPM) return;

  currentBPM = newBPM;
  calculateBeatInterval();

  Serial.print("[Test] BPM updated: ");
  Serial.println(currentBPM);

  startEmotion();
}

void calculateBeatInterval() {
  beatInterval = 60000UL / currentBPM;
}

unsigned long noteLengthToMs(uint8_t lengthUnits) {
  if (lengthUnits == 0) return 0;
  return (beatInterval * (unsigned long)lengthUnits) / QUARTER_LENGTH_UNITS;
}

unsigned long calculatePulseTime(unsigned long eventDuration) {
  if (eventDuration == 0) return 0;
  unsigned long pulseTime = (eventDuration * 7UL) / 10UL;

  if (pulseTime < 40UL) { pulseTime = 40UL; }
  if (eventDuration > 30UL && pulseTime > eventDuration - 20UL) {
    pulseTime = eventDuration - 20UL;
  }
  if (pulseTime > eventDuration) { pulseTime = eventDuration; }

  return pulseTime;
}

uint8_t toScoreBar(uint8_t serverBarNumber) {
  int scoreBar = (int)serverBarNumber + PART_BAR_OFFSET;
  scoreBar %= SERVER_BAR_COUNT;
  if (scoreBar < 0) { scoreBar += SERVER_BAR_COUNT; }
  return (uint8_t)scoreBar;
}

bool isWholeRestBar(uint8_t scoreBarNumber) {
  if (scoreBarNumber >= SERVER_BAR_COUNT) return false;
  return WHOLE_REST_BAR_TABLE[scoreBarNumber];
}

void startBarMotion(uint8_t barNumber) {
  uint8_t scoreBar = toScoreBar(barNumber);
  currentBar = scoreBar;

  if (isWholeRestBar(currentBar)) {
    motionMutedByWholeRest = true;
    scoreRestActive = true;
    stopAllMotionForWholeRest();
    Serial.println("[Test] Whole Rest Bar: Stopped");
    return;
  }

  motionMutedByWholeRest = false;
  scoreRestActive = false;

  nextEventTime = millis();
  scoreEventIndex = 0;
  usedUnitsInBar = 0;
  barActive = true;
}

void updateNormalMotion() {
  if (motionMutedByWholeRest || !barActive) return;

  while (barActive && timeReached(nextEventTime)) {
    triggerNextScoreEvent();
  }
}

void triggerNextScoreEvent() {
  if (scoreEventIndex >= MAX_EVENTS_PER_BAR || usedUnitsInBar >= BAR_LENGTH_UNITS) {
    finishBarMotion();
    return;
  }

  ServoScoreEvent event = SCORE_TABLE[currentBar][scoreEventIndex];
  scoreEventIndex++;

  if (event.type == SCORE_END || event.lengthUnits == 0) {
    finishBarMotion();
    return;
  }

  uint8_t lengthUnits = event.lengthUnits;
  if (usedUnitsInBar + lengthUnits > BAR_LENGTH_UNITS) {
    lengthUnits = BAR_LENGTH_UNITS - usedUnitsInBar;
  }

  unsigned long eventDuration = noteLengthToMs(lengthUnits);

  if (event.type == SCORE_REST) {
    scoreRestActive = true;
    stopMainMotionForRest();
  } else if (event.type == SCORE_PLAY) {
    scoreRestActive = false;
    triggerScoreMotion(eventDuration);
  }

  usedUnitsInBar += lengthUnits;
  nextEventTime += eventDuration;
}

void triggerScoreMotion(unsigned long eventDuration) {
  unsigned long pulseTime = calculatePulseTime(eventDuration);

#if INSTRUMENT == INST_PIANO
  if (pianoUseRightArm) {
    triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime);
  } else {
    triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime);
  }
  pianoUseRightArm = !pianoUseRightArm;

#elif INSTRUMENT == INST_TROMBONE
  armDirection = !armDirection;
  if (armDirection) {
    rightArmServo.write(TROMBONE_FORWARD_ANGLE);
  } else {
    rightArmServo.write(TROMBONE_BACK_ANGLE);
  }

#elif INSTRUMENT == INST_VIOLIN
  armDirection = !armDirection;
  if (armDirection) {
    rightArmServo.write(VIOLIN_BOW_LEFT_ANGLE);
  } else {
    rightArmServo.write(VIOLIN_BOW_RIGHT_ANGLE);
  }

#elif INSTRUMENT == INST_CASTANET
  triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

void finishBarMotion() {
  barActive = false;
  scoreRestActive = true;
  stopMainMotionForRest();
}

void startEmotion() {
  if (motionMutedByWholeRest || scoreRestActive) return;

#if INSTRUMENT == INST_VIOLIN || INSTRUMENT == INST_CASTANET
  emotionActive = true;
  emotionEndTime = millis() + beatInterval * EMOTION_BEATS;
  lastEmotionMoveTime = 0;
  emotionDirection = false;
#endif
}

void updateEmotionMotion() {
  if (motionMutedByWholeRest || scoreRestActive) {
    resetEmotionServo();
    emotionActive = false;
    return;
  }

  if (!emotionActive) return;

  unsigned long now = millis();
  if (timeReached(emotionEndTime)) {
    resetEmotionServo();
    emotionActive = false;
    return;
  }

  unsigned long emotionInterval = beatInterval / 2;
  if (emotionInterval < 100) { emotionInterval = 100; }

  if (now - lastEmotionMoveTime >= emotionInterval) {
    lastEmotionMoveTime = now;
    emotionDirection = !emotionDirection;

#if INSTRUMENT == INST_VIOLIN
    if (emotionDirection) {
      extraServo.write(HEAD_LEFT_ANGLE);
    } else {
      extraServo.write(HEAD_RIGHT_ANGLE);
    }
#elif INSTRUMENT == INST_CASTANET
    if (emotionDirection) {
      extraServo.write(MOUTH_OPEN_ANGLE);
    } else {
      extraServo.write(MOUTH_CLOSE_ANGLE);
    }
#endif
  }
}

void triggerRightArmPulse(int angle, unsigned long pulseTime) {
  rightArmServo.write(angle);
  rightArmReturnActive = true;
  rightArmReturnTime = millis() + pulseTime;
}

void triggerLeftArmPulse(int angle, unsigned long pulseTime) {
  leftArmServo.write(angle);
  leftArmReturnActive = true;
  leftArmReturnTime = millis() + pulseTime;
}

void triggerExtraPulse(int angle, unsigned long pulseTime) {
  extraServo.write(angle);
  extraReturnActive = true;
  extraReturnTime = millis() + pulseTime;
}

void updateServoReturn() {
  unsigned long now = millis();

  if (rightArmReturnActive && timeReached(rightArmReturnTime)) {
    rightArmServo.write(ARM_REST_ANGLE);
    rightArmReturnActive = false;
  }
  if (leftArmReturnActive && timeReached(leftArmReturnTime)) {
    leftArmServo.write(ARM_REST_ANGLE);
    leftArmReturnActive = false;
  }
  if (extraReturnActive && timeReached(extraReturnTime)) {
    resetEmotionServo();
    extraReturnActive = false;
  }
}

void stopAllMotionForWholeRest() {
  barActive = false;
  scoreEventIndex = 0;
  usedUnitsInBar = 0;
  emotionActive = false;
  cancelServoReturns();
  resetMainServos();
  resetEmotionServo();
}

void stopMainMotionForRest() {
  rightArmReturnActive = false;
  leftArmReturnActive = false;
  resetMainServos();
}

void cancelServoReturns() {
  rightArmReturnActive = false;
  leftArmReturnActive = false;
  extraReturnActive = false;
}

void resetMainServos() {
#if INSTRUMENT == INST_PIANO
  rightArmServo.write(ARM_REST_ANGLE);
  leftArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == INST_TROMBONE
  rightArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == INST_VIOLIN
  rightArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == INST_CASTANET
  rightArmServo.write(ARM_REST_ANGLE);
#endif
}

void resetEmotionServo() {
#if INSTRUMENT == INST_VIOLIN
  extraServo.write(HEAD_CENTER_ANGLE);
#elif INSTRUMENT == INST_CASTANET
  extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}

bool timeReached(unsigned long targetTime) {
  return (long)(millis() - targetTime) >= 0;
}

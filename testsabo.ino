#include <Servo.h>

// =====================
// 楽器の選択（有効なものを1つだけにしてください）
// =====================
#define INST_PIANO    1
//#define INST_TROMBONE 2
//#define INST_VIOLIN   3
//#define INST_CASTANET 4

#define INSTRUMENT INST_PIANO 

// =====================
// 管理変数
// =====================
unsigned long debugBarStartTime = 0; 
unsigned long debugBpmChangeTime = 0; 
uint8_t debugVirtualServerBar = 0;   
bool debugFirstRun = true;           

const int RIGHT_ARM_PIN = 9;
const int LEFT_ARM_PIN  = 10;
const int EXTRA_PIN     = 5;

Servo rightArmServo;
Servo leftArmServo;
Servo extraServo;

const int ARM_REST_ANGLE = 90;
const int PIANO_RIGHT_PLAY_ANGLE = 70;
const int PIANO_LEFT_PLAY_ANGLE  = 110;
const int TROMBONE_FORWARD_ANGLE = 65;
const int TROMBONE_BACK_ANGLE    = 115;
const int VIOLIN_BOW_LEFT_ANGLE  = 65;
const int VIOLIN_BOW_RIGHT_ANGLE = 115;
const int CASTANET_PLAY_ANGLE = 70;
const int HEAD_CENTER_ANGLE = 90;
const int HEAD_LEFT_ANGLE   = 70;
const int HEAD_RIGHT_ANGLE  = 110;
const int MOUTH_CLOSE_ANGLE = 90;
const int MOUTH_OPEN_ANGLE  = 60;

// =====================
// ★ 11回計測用の小節数設定
// =====================
const uint8_t SERVER_BAR_COUNT = 11; // 計11小節分
const int PART_BAR_OFFSET = 0;

const bool WHOLE_REST_BAR_TABLE[SERVER_BAR_COUNT] = {
  false, false, false, false, false, false, false, false, false, false, false
};

// =====================
// 楽譜データ設定
// =====================
enum ScoreEventType { SCORE_END = 0, SCORE_PLAY = 1, SCORE_REST = 2 };
enum NoteLengthUnit { LENGTH_EIGHTH = 2, LENGTH_QUARTER = 4, LENGTH_HALF = 8 };

struct ServoScoreEvent {
  ScoreEventType type;
  uint8_t lengthUnits;
};

const uint8_t MAX_EVENTS_PER_BAR = 12; // 11回対応のため拡張
const uint8_t BAR_LENGTH_UNITS = 16;
const uint8_t QUARTER_LENGTH_UNITS = 4;

#define SCORE_FILL         { SCORE_END, 0 }

// 【四分音符：11回データ】（計3小節必要。4回 + 4回 + 3回）
#define BAR_QUARTER_4_FOUR      { {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }
#define BAR_QUARTER_3_REST_1    { {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }

// 【八分音符：11回データ】（計2小節必要。8回 + 3回）
#define BAR_EIGHTH_8_EIGHT      { {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }
#define BAR_EIGHTH_3_REST_5     { {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_REST, LENGTH_HALF}, {SCORE_REST, LENGTH_QUARTER}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }

// 【二分音符：11回データ】（計6小節必要。2回×5小節 + 1回）
#define BAR_HALF_2_TWO          { {SCORE_PLAY, LENGTH_HALF}, {SCORE_PLAY, LENGTH_HALF}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }
#define BAR_HALF_1_REST_1       { {SCORE_PLAY, LENGTH_HALF}, {SCORE_REST, LENGTH_HALF}, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL }

const ServoScoreEvent SCORE_TABLE[SERVER_BAR_COUNT][MAX_EVENTS_PER_BAR] = {
  // --- 四分音符テスト (計11回トリガー) ---
  BAR_QUARTER_4_FOUR,   // 1〜4回目 (小節0)
  BAR_QUARTER_4_FOUR,   // 5〜8回目 (小節1)
  BAR_QUARTER_3_REST_1, // 9〜11回目 (小節2)

  // --- 八分音符テスト (計11回トリガー) ---
  BAR_EIGHTH_8_EIGHT,   // 1〜8回目 (小節3)
  BAR_EIGHTH_3_REST_5,  // 9〜11回目 (小節4)

  // --- 二分音符テスト (計11回トリガー) ---
  BAR_HALF_2_TWO,       // 1〜2回目 (小節5)
  BAR_HALF_2_TWO,       // 3〜4回目 (小節6)
  BAR_HALF_2_TWO,       // 5〜6回目 (小節7)
  BAR_HALF_2_TWO,       // 7〜8回目 (小節8)
  BAR_HALF_2_TWO,       // 9〜10回目 (小節9)
  BAR_HALF_1_REST_1     // 11回目 (小節10)
};

#undef SCORE_FILL
#undef BAR_QUARTER_4_FOUR
#undef BAR_QUARTER_3_REST_1
#undef BAR_EIGHTH_8_EIGHT
#undef BAR_EIGHTH_3_REST_5
#undef BAR_HALF_2_TWO
#undef BAR_HALF_1_REST_1

// =====================
// モーション制御・基本処理
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

bool rightArmReturnActive = false;
bool leftArmReturnActive  = false;
bool extraReturnActive    = false;
unsigned long rightArmReturnTime = 0;
unsigned long leftArmReturnTime  = 0;
unsigned long extraReturnTime    = 0;

bool emotionActive = false;
unsigned long emotionEndTime = 0;
unsigned long lastEmotionMoveTime = 0;
bool emotionDirection = false;
const int EMOTION_BEATS = 4;

void setupServos();
void simulateServerBehavior(); 
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
void triggerRightArmPulse(int angle, unsigned long pulseTime);
void triggerLeftArmPulse(int angle, unsigned long pulseTime);
void updateServoReturn();
void stopMainMotionForRest();
void resetMainServos();
void resetEmotionServo();
bool timeReached(unsigned long targetTime);

void setup() {
  Serial.begin(115200);
  setupServos();
  calculateBeatInterval();
  Serial.println("START_TEST");
  debugBarStartTime = millis();
}

void loop() {
  simulateServerBehavior(); 
  updateNormalMotion();
  updateServoReturn();
}

void simulateServerBehavior() {
  unsigned long now = millis();
  if (debugFirstRun) {
    debugFirstRun = false;
    startBarMotion(debugVirtualServerBar);
    debugBarStartTime = now;
  }
  unsigned long barDuration = beatInterval * 4;
  if (now - debugBarStartTime >= barDuration) {
    debugVirtualServerBar++;
    if (debugVirtualServerBar >= SERVER_BAR_COUNT) {
      debugVirtualServerBar = 0; 
      Serial.println("LOOP_TEST_RESTART");
    }
    startBarMotion(debugVirtualServerBar);
    debugBarStartTime = now;
  }
}

void setupServos() {
#if INSTRUMENT == INST_PIANO
  rightArmServo.attach(RIGHT_ARM_PIN); leftArmServo.attach(LEFT_ARM_PIN);
  rightArmServo.write(ARM_REST_ANGLE); leftArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == INST_TROMBONE
  rightArmServo.attach(RIGHT_ARM_PIN); rightArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == INST_VIOLIN
  rightArmServo.attach(RIGHT_ARM_PIN); extraServo.attach(EXTRA_PIN);
  rightArmServo.write(ARM_REST_ANGLE); extraServo.write(HEAD_CENTER_ANGLE);
#elif INSTRUMENT == INST_CASTANET
  rightArmServo.attach(RIGHT_ARM_PIN); extraServo.attach(EXTRA_PIN);
  rightArmServo.write(ARM_REST_ANGLE); extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}

void calculateBeatInterval() { beatInterval = 60000UL / currentBPM; }

unsigned long noteLengthToMs(uint8_t lengthUnits) {
  if (lengthUnits == 0) return 0;
  return (beatInterval * (unsigned long)lengthUnits) / QUARTER_LENGTH_UNITS;
}

unsigned long calculatePulseTime(unsigned long eventDuration) {
  if (eventDuration == 0) return 0;
  unsigned long pulseTime = (eventDuration * 7UL) / 10UL;
  if (pulseTime < 40UL) { pulseTime = 40UL; }
  if (eventDuration > 30UL && pulseTime > eventDuration - 20UL) { pulseTime = eventDuration - 20UL; }
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
  currentBar = toScoreBar(barNumber);
  motionMutedByWholeRest = false;
  scoreRestActive = false;
  nextEventTime = millis();
  scoreEventIndex = 0;
  usedUnitsInBar = 0;
  barActive = true;
}

void updateNormalMotion() {
  if (motionMutedByWholeRest || !barActive) return;
  while (barActive && timeReached(nextEventTime)) { triggerNextScoreEvent(); }
}

void triggerNextScoreEvent() {
  if (scoreEventIndex >= MAX_EVENTS_PER_BAR || usedUnitsInBar >= BAR_LENGTH_UNITS) {
    finishBarMotion(); return;
  }
  ServoScoreEvent event = SCORE_TABLE[currentBar][scoreEventIndex];
  scoreEventIndex++;
  if (event.type == SCORE_END || event.lengthUnits == 0) {
    finishBarMotion(); return;
  }
  uint8_t lengthUnits = event.lengthUnits;
  if (usedUnitsInBar + lengthUnits > BAR_LENGTH_UNITS) { lengthUnits = BAR_LENGTH_UNITS - usedUnitsInBar; }
  unsigned long eventDuration = noteLengthToMs(lengthUnits);

  Serial.print("DATA,"); Serial.print(event.type); Serial.print(",");
  Serial.print(lengthUnits); Serial.print(","); Serial.println(millis()); 

  if (event.type == SCORE_REST) {
    scoreRestActive = true; stopMainMotionForRest();
  } else if (event.type == SCORE_PLAY) {
    scoreRestActive = false; triggerScoreMotion(eventDuration);
  }
  usedUnitsInBar += lengthUnits;
  nextEventTime += eventDuration;
}

void triggerScoreMotion(unsigned long eventDuration) {
  unsigned long pulseTime = calculatePulseTime(eventDuration);
#if INSTRUMENT == INST_PIANO
  if (pianoUseRightArm) { triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime); } 
  else { triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime); }
  pianoUseRightArm = !pianoUseRightArm;
#elif INSTRUMENT == INST_TROMBONE
  armDirection = !armDirection;
  rightArmServo.write(armDirection ? TROMBONE_FORWARD_ANGLE : TROMBONE_BACK_ANGLE);
#elif INSTRUMENT == INST_VIOLIN
  armDirection = !armDirection;
  rightArmServo.write(armDirection ? VIOLIN_BOW_LEFT_ANGLE : VIOLIN_BOW_RIGHT_ANGLE);
#elif INSTRUMENT == INST_CASTANET
  triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

void finishBarMotion() { barActive = false; scoreRestActive = true; stopMainMotionForRest(); }
void triggerRightArmPulse(int angle, unsigned long pulseTime) { rightArmServo.write(angle); rightArmReturnActive = true; rightArmReturnTime = millis() + pulseTime; }
void triggerLeftArmPulse(int angle, unsigned long pulseTime) { leftArmServo.write(angle); leftArmReturnActive = true; leftArmReturnTime = millis() + pulseTime; }

void updateServoReturn() {
  unsigned long now = millis();
  if (rightArmReturnActive && timeReached(rightArmReturnTime)) { rightArmServo.write(ARM_REST_ANGLE); rightArmReturnActive = false; }
  if (leftArmReturnActive && timeReached(leftArmReturnTime)) { leftArmServo.write(ARM_REST_ANGLE); leftArmReturnActive = false; }
}

void stopMainMotionForRest() { rightArmReturnActive = false; leftArmReturnActive = false; resetMainServos(); }
void resetMainServos() {
#if INSTRUMENT == INST_PIANO
  rightArmServo.write(ARM_REST_ANGLE); leftArmServo.write(ARM_REST_ANGLE);
#else
  rightArmServo.write(ARM_REST_ANGLE);
#endif
}
bool timeReached(unsigned long targetTime) { return (long)(millis() - targetTime) >= 0; }
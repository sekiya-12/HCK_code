#include "entame.h"

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
// 小節・オフセット設定
// =====================
// サーバーから送られる小節番号は 0〜39 でループする想定
const uint8_t SERVER_BAR_COUNT = 40;

// サーバーの小節番号と，このエンタメ担当の譜面上の小節番号がずれる場合に使う
// scoreBar = serverBar + PART_BAR_OFFSET として扱う
// 例：サーバー0のとき譜面2小節目として見たい場合は 2
// 例：サーバー2のとき譜面0小節目として見たい場合は -2
const int PART_BAR_OFFSET = 0;

// true にした小節は「全休符」として扱い，サーボを動かさない
// 左から譜面上の 0, 1, 2, ... , 39 小節目に対応
const bool WHOLE_REST_BAR_TABLE[SERVER_BAR_COUNT] = {
  false, false, false, false, false, false, false, false, false, false,
  false, false, false, false, false, false, false, false, false, false,
  false, false, false, false, false, false, false, false, false, false,
  false, false, false, false, false, false, false, false, false, false
};

// =====================
// 楽譜データ設定
// =====================
// 1小節を16分音符16個分として考える
// 四分音符 = 4
// 八分音符 = 2
// 二分音符 = 8
// 全音符   = 16
//
// SCORE_PLAY → その音符の長さに合わせてサーボを動かす
// SCORE_REST → その長さだけサーボを動かさない
// SCORE_END  → その小節のデータ終わり

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

// 小節パターン
#define SCORE_FILL         { SCORE_END, 0 }

#define BAR_QUARTER_4      { \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_PLAY, LENGTH_QUARTER}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

#define BAR_EIGHTH_8       { \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH}, \
  {SCORE_PLAY, LENGTH_EIGHTH}, {SCORE_PLAY, LENGTH_EIGHTH} \
}

#define BAR_Q_REST_Q_REST  { \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, \
  {SCORE_PLAY, LENGTH_QUARTER}, {SCORE_REST, LENGTH_QUARTER}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

#define BAR_WHOLE_REST     { \
  {SCORE_REST, LENGTH_WHOLE}, \
  SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL, SCORE_FILL \
}

// ==================================================
// ここを楽譜に合わせて変更する
// ==================================================
// BAR_QUARTER_4      → 四分音符4つ
// BAR_EIGHTH_8       → 八分音符8つ
// BAR_Q_REST_Q_REST  → 四分音符，四分休符，四分音符，四分休符
// BAR_WHOLE_REST     → 1小節すべて休符
//
// 例：0小節目だけ八分音符にしたいなら
// BAR_EIGHTH_8,      // 0小節目
// に変更する
// ==================================================
const ServoScoreEvent SCORE_TABLE[SERVER_BAR_COUNT][MAX_EVENTS_PER_BAR] = {
  BAR_QUARTER_4,     // 0小節目
  BAR_QUARTER_4,     // 1小節目
  BAR_QUARTER_4,     // 2小節目
  BAR_QUARTER_4,     // 3小節目
  BAR_QUARTER_4,     // 4小節目
  BAR_QUARTER_4,     // 5小節目
  BAR_QUARTER_4,     // 6小節目
  BAR_QUARTER_4,     // 7小節目
  BAR_QUARTER_4,     // 8小節目
  BAR_QUARTER_4,     // 9小節目
  BAR_QUARTER_4,     // 10小節目
  BAR_QUARTER_4,     // 11小節目
  BAR_QUARTER_4,     // 12小節目
  BAR_QUARTER_4,     // 13小節目
  BAR_QUARTER_4,     // 14小節目
  BAR_QUARTER_4,     // 15小節目
  BAR_QUARTER_4,     // 16小節目
  BAR_QUARTER_4,     // 17小節目
  BAR_QUARTER_4,     // 18小節目
  BAR_QUARTER_4,     // 19小節目
  BAR_QUARTER_4,     // 20小節目
  BAR_QUARTER_4,     // 21小節目
  BAR_QUARTER_4,     // 22小節目
  BAR_QUARTER_4,     // 23小節目
  BAR_QUARTER_4,     // 24小節目
  BAR_QUARTER_4,     // 25小節目
  BAR_QUARTER_4,     // 26小節目
  BAR_QUARTER_4,     // 27小節目
  BAR_QUARTER_4,     // 28小節目
  BAR_QUARTER_4,     // 29小節目
  BAR_QUARTER_4,     // 30小節目
  BAR_QUARTER_4,     // 31小節目
  BAR_QUARTER_4,     // 32小節目
  BAR_QUARTER_4,     // 33小節目
  BAR_QUARTER_4,     // 34小節目
  BAR_QUARTER_4,     // 35小節目
  BAR_QUARTER_4,     // 36小節目
  BAR_QUARTER_4,     // 37小節目
  BAR_QUARTER_4,     // 38小節目
  BAR_QUARTER_4      // 39小節目
};

#undef SCORE_FILL
#undef BAR_QUARTER_4
#undef BAR_EIGHTH_8
#undef BAR_Q_REST_Q_REST
#undef BAR_WHOLE_REST

// =====================
// BPM・小節管理
// =====================
uint8_t currentBPM = 120;
uint8_t currentBar = 0;

unsigned long beatInterval = 500;   // 四分音符1つの時間 ms
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

// BPM変更時の演出を何拍分続けるか
const int EMOTION_BEATS = 4;

// =====================
// 関数宣言
// =====================
void setupServos();
void receiveUdpData();
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
// setup
// =====================
void entameSetup() {
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
void entameLoop() {
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

  if (packetSize <= 0) {
    return;
  }

  int data = Udp.read();

  while (Udp.available()) {
    Udp.read();
  }

  if (data < 0) {
    return;
  }

  if (data >= SERVER_BAR_COUNT) {
    updateBPM((uint8_t)data);
  } else {
    startBarMotion((uint8_t)data);
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
// 四分音符1つの時間を計算
// BPM120なら 60000 / 120 = 500ms
// =====================
void calculateBeatInterval() {
  beatInterval = 60000UL / currentBPM;
}

// =====================
// 音符の長さをmsへ変換
// lengthUnitsは16分音符を1とした長さ
// 四分音符 = 4なので，beatInterval * 4 / 4
// 八分音符 = 2なので，beatInterval * 2 / 4
// =====================
unsigned long noteLengthToMs(uint8_t lengthUnits) {
  if (lengthUnits == 0) {
    return 0;
  }

  return (beatInterval * (unsigned long)lengthUnits) / QUARTER_LENGTH_UNITS;
}

// =====================
// サーボを押し出してから戻すまでの時間
// 音符が短いほど短く，長いほど長くなる
// =====================
unsigned long calculatePulseTime(unsigned long eventDuration) {
  if (eventDuration == 0) {
    return 0;
  }

  unsigned long pulseTime = (eventDuration * 7UL) / 10UL;

  if (pulseTime < 40UL) {
    pulseTime = 40UL;
  }

  // 次の音符に食い込まないように少しだけ余裕を残す
  if (eventDuration > 30UL && pulseTime > eventDuration - 20UL) {
    pulseTime = eventDuration - 20UL;
  }

  if (pulseTime > eventDuration) {
    pulseTime = eventDuration;
  }

  return pulseTime;
}

// =====================
// サーバー小節番号を譜面上の小節番号へ変換
// =====================
uint8_t toScoreBar(uint8_t serverBarNumber) {
  int scoreBar = (int)serverBarNumber + PART_BAR_OFFSET;
  scoreBar %= SERVER_BAR_COUNT;

  if (scoreBar < 0) {
    scoreBar += SERVER_BAR_COUNT;
  }

  return (uint8_t)scoreBar;
}

// =====================
// 全休符小節かどうか
// =====================
bool isWholeRestBar(uint8_t scoreBarNumber) {
  if (scoreBarNumber >= SERVER_BAR_COUNT) {
    return false;
  }

  return WHOLE_REST_BAR_TABLE[scoreBarNumber];
}

// =====================
// 小節番号を受信したとき
// =====================
void startBarMotion(uint8_t barNumber) {
  uint8_t scoreBar = toScoreBar(barNumber);
  currentBar = scoreBar;

  Serial.print("Server bar received: ");
  Serial.print(barNumber);
  Serial.print("  Score bar: ");
  Serial.println(currentBar);

  if (isWholeRestBar(currentBar)) {
    motionMutedByWholeRest = true;
    scoreRestActive = true;
    stopAllMotionForWholeRest();

    Serial.println("Whole rest bar: servo motion skipped");
    return;
  }

  motionMutedByWholeRest = false;
  scoreRestActive = false;

  nextEventTime = millis();
  scoreEventIndex = 0;
  usedUnitsInBar = 0;
  barActive = true;

  Serial.print("Bar motion started: ");
  Serial.println(currentBar);
}

// =====================
// 通常演奏サーボ動作
// サーバーは小節番号だけ送るので，
// 1小節の中の四分音符・八分音符・休符はエンタメArduino側で刻む
// =====================
void updateNormalMotion() {
  if (motionMutedByWholeRest) {
    return;
  }

  if (!barActive) {
    return;
  }

  // 処理が少し遅れても，予定時刻を過ぎたイベントを進める
  while (barActive && timeReached(nextEventTime)) {
    triggerNextScoreEvent();
  }
}

// =====================
// 楽譜テーブルから次のイベントを実行する
// =====================
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

  // 1小節の長さを超えないように補正する
  if (usedUnitsInBar + lengthUnits > BAR_LENGTH_UNITS) {
    lengthUnits = BAR_LENGTH_UNITS - usedUnitsInBar;
  }

  unsigned long eventDuration = noteLengthToMs(lengthUnits);

  if (event.type == SCORE_REST) {
    scoreRestActive = true;
    stopMainMotionForRest();

    Serial.print("Rest event: units=");
    Serial.print(lengthUnits);
    Serial.print(" duration=");
    Serial.println(eventDuration);
  } else if (event.type == SCORE_PLAY) {
    scoreRestActive = false;
    triggerScoreMotion(eventDuration);

    Serial.print("Play event: units=");
    Serial.print(lengthUnits);
    Serial.print(" duration=");
    Serial.println(eventDuration);
  }

  usedUnitsInBar += lengthUnits;
  nextEventTime += eventDuration;
}

// =====================
// 音符イベントのサーボ動作
// =====================
void triggerScoreMotion(unsigned long eventDuration) {
  unsigned long pulseTime = calculatePulseTime(eventDuration);

#if INSTRUMENT == PIANO
  // ピアノ：左右の腕で交互に打鍵
  if (pianoUseRightArm) {
    triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime);
  } else {
    triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime);
  }

  pianoUseRightArm = !pianoUseRightArm;

#elif INSTRUMENT == TROMBONE
  // トロンボーン：音符ごとにスライドを前後へ動かす
  armDirection = !armDirection;

  if (armDirection) {
    rightArmServo.write(TROMBONE_FORWARD_ANGLE);
  } else {
    rightArmServo.write(TROMBONE_BACK_ANGLE);
  }

#elif INSTRUMENT == VIOLIN
  // ヴァイオリン：音符ごとに弓を左右へ動かす
  armDirection = !armDirection;

  if (armDirection) {
    rightArmServo.write(VIOLIN_BOW_LEFT_ANGLE);
  } else {
    rightArmServo.write(VIOLIN_BOW_RIGHT_ANGLE);
  }

#elif INSTRUMENT == CASTANET
  // カスタネット：音符ごとに腕を動かす
  triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

// =====================
// 1小節分のサーボ動作終了
// =====================
void finishBarMotion() {
  barActive = false;
  scoreRestActive = true;
  stopMainMotionForRest();
}

// =====================
// BPM変更時の演出開始
// =====================
void startEmotion() {
  if (motionMutedByWholeRest || scoreRestActive) {
    return;
  }

#if INSTRUMENT == VIOLIN || INSTRUMENT == CASTANET
  emotionActive = true;
  emotionEndTime = millis() + beatInterval * EMOTION_BEATS;
  lastEmotionMoveTime = 0;
  emotionDirection = false;
#endif
}

// =====================
// BPM変更時の第二駆動
// ヴァイオリン：首振り
// カスタネット：口開閉
// 休符中は動かさない
// =====================
void updateEmotionMotion() {
  if (motionMutedByWholeRest || scoreRestActive) {
    resetEmotionServo();
    emotionActive = false;
    return;
  }

  if (!emotionActive) {
    return;
  }

  unsigned long now = millis();

  if (timeReached(emotionEndTime)) {
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

// =====================
// 全休符時にすべての動作を止める
// =====================
void stopAllMotionForWholeRest() {
  barActive = false;
  scoreEventIndex = 0;
  usedUnitsInBar = 0;

  emotionActive = false;
  cancelServoReturns();

  resetMainServos();
  resetEmotionServo();
}

// =====================
// 休符時に通常サーボ動作を止める
// =====================
void stopMainMotionForRest() {
  rightArmReturnActive = false;
  leftArmReturnActive = false;

  resetMainServos();
}

// =====================
// delayなし戻し処理の予約を取り消す
// =====================
void cancelServoReturns() {
  rightArmReturnActive = false;
  leftArmReturnActive = false;
  extraReturnActive = false;
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

// =====================
// millis() のオーバーフロー対策込みの時刻判定
// =====================
bool timeReached(unsigned long targetTime) {
  return (long)(millis() - targetTime) >= 0;
}
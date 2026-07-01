#include <Servo.h>

// ==================================================
// ヴァイオリン人形：単体統合テスト版（Wi-Fi・サーバー不要）
//
// 動きの作り方は testtest.ino と同様：
//   ・弓(1基目 D9) … 音符ごとに反転（拍に正確）。休符では待機位置へ
//   ・口(2基目 D5) … BPMが変わったときに開閉する（歌唱演出 emotion）
// さらに testtest と同様、BPM を自動で 120 ↔ 160 に切り替えて確認する。
//
// 音(Processing) … 各音符を「freq,ms,弾き方」で送信（freq=0 は休符＝無音）。
// ※ Processingで音も確認する場合は、シリアル速度を 115200 に合わせること。
// ==================================================

// ---------- 楽譜データ構造 ----------
#define DOWN_BOW 0   // 下げ弓 → Processing art==0
#define UP_BOW   1   // 上げ弓 → Processing art==1
#define STACCATO 2   // 短く歯切れよく → Processing art==2

struct NoteData { float freq; float duration; int art; };
struct Pfm      { NoteData notes[8]; int noteCount; };
Pfm Score[40];

// ---------- 周波数定義 ----------
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define REST     0
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880

// ---------- サーボ ----------
Servo bowServo;    // D9：弓の腕
Servo mouthServo;  // D5：口（上顎）
const int BOW_PIN   = 9;
const int MOUTH_PIN = 5;

// ---------- 角度（実機に合わせて調整） ----------
const int ARM_REST_ANGLE    = 70;  // 弓の待機位置
const int BOW_LEFT_ANGLE     = 80;  // 下げ弓側
const int BOW_RIGHT_ANGLE    = 67;  // 上げ弓側
const int MOUTH_CLOSE_ANGLE  = 100;  // 口を閉じる
const int MOUTH_OPEN_ANGLE   = 40;  // 口を開く

// ---------- テンポ・小節管理 ----------
char Offset = 0;            // テストは0で即スタート（本番のviolin_clientは -4）
uint8_t currentBPM = 120;
float   ToneLength = 500.0;  // 4分音符=1拍の長さ(ms) = 60000/BPM
unsigned long beatInterval = 500; // = ToneLength（口の演出タイミング用）
const int BEATS_PER_BAR = 4;

// 仮想サーバー
uint8_t virtualServerBar = 255;
unsigned long barTimer = 0;
uint8_t currentBar = 255;   // 今演奏中の小節（Score配列のインデックス）

// 音（音符送信）管理
int NoteIndex = 0;
unsigned long noteStartTime = 0;
unsigned long noteInterval = 0;

// 弓（音符ごとに反転）
bool bowDirection = false;

// 口（BPM変化時の歌唱演出 emotion）
bool emotionActive = false;
unsigned long emotionEndTime = 0;
unsigned long lastEmotionMoveTime = 0;
bool emotionDirection = false;
const int EMOTION_BEATS = 4;        // BPM変化時に何拍ぶん口を動かすか

// BPM自動変更（testtest相当）
unsigned long bpmChangeTime = 0;
const unsigned long BPM_CHANGE_INTERVAL = 15000UL; // 15秒ごとに 120↔160

// ==========================================
// 楽譜：基本オクターブの「かえるのうた」（休符を詰めて連続）
//   ・オクターブ上は使わない
//   ・8小節を1セットとして全40小節に繰り返しコピー
// ==========================================
void setupScore() {
  Pfm unit[8];
  unit[0] = {{ {NOTE_C4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW} }, 4};
  unit[1] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,2.0,DOWN_BOW} }, 3};   // 最後のドを伸ばす
  unit[2] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_G4,1.0,DOWN_BOW}, {NOTE_A4,1.0,UP_BOW} }, 4};
  unit[3] = {{ {NOTE_G4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_E4,2.0,DOWN_BOW} }, 3};
  // クヮ：少し伸ばして最後は切る「クヮー！」（音1.0拍＋休符1.0拍）
  unit[4] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  unit[5] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  unit[6] = {{ {NOTE_C4,0.5,DOWN_BOW}, {NOTE_C4,0.5,UP_BOW}, {NOTE_D4,0.5,DOWN_BOW}, {NOTE_D4,0.5,UP_BOW},
               {NOTE_E4,0.5,DOWN_BOW}, {NOTE_E4,0.5,UP_BOW}, {NOTE_F4,0.5,DOWN_BOW}, {NOTE_F4,0.5,UP_BOW} }, 8}; // ケケケ
  unit[7] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,2.0,DOWN_BOW} }, 3};

  for (int i = 0; i < 40; i++) Score[i] = unit[i % 8];
}

void setup() {
  Serial.begin(115200);   // Processingと同じ速度
  bowServo.attach(BOW_PIN);
  mouthServo.attach(MOUTH_PIN);
  bowServo.write(ARM_REST_ANGLE);
  mouthServo.write(MOUTH_CLOSE_ANGLE);

  setupScore();
  calculateBeatInterval();

  bpmChangeTime = millis() + BPM_CHANGE_INTERVAL;
  Serial.println("VIOLIN 統合テスト開始（音符ごとの弓＋BPM自動変更で口パク, Wi-Fi不要）");
}

void loop() {
  unsigned long now = millis();

  // ---- BPM自動変更（testtest相当：15秒ごとに 120↔160）----
  if ((long)(now - bpmChangeTime) >= 0) {
    updateBPM(currentBPM == 120 ? 160 : 120);
    bpmChangeTime = now + BPM_CHANGE_INTERVAL;
  }

  // ---- 仮想サーバー：1小節ぶんの時間が経ったら次の小節へ ----
  unsigned long barDuration = (unsigned long)(ToneLength * BEATS_PER_BAR);
  if (now - barTimer >= barDuration) {
    barTimer = now;

    if (virtualServerBar == 255 || virtualServerBar >= 39) virtualServerBar = 0;
    else virtualServerBar++;

    int myBar = (int)virtualServerBar + Offset;
    if (myBar < 0) currentBar = 255;   // まだ自分の出番でない（Offsetが負のとき）
    else           startBar((uint8_t)myBar);
  }

  updateSound(now);    // 音符をProcessingへ送信＋弓を音符ごとに動かす
  updateEmotion(now);  // 口（BPM変化時の歌唱演出）
}

void calculateBeatInterval() {
  ToneLength = 60000.0 / currentBPM;
  beatInterval = (unsigned long)ToneLength;
}

// BPM更新（テンポを変えて口の演出を発動）
void updateBPM(uint8_t newBPM) {
  if (newBPM == currentBPM) return;
  currentBPM = newBPM;
  calculateBeatInterval();
  Serial.print("BPM -> ");
  Serial.println(currentBPM);
  startEmotion();
}

// 新しい小節を開始（音符を先頭から）
void startBar(uint8_t bar) {
  currentBar = bar;
  NoteIndex = 0;
  noteStartTime = millis();
  noteInterval = 0;   // すぐ最初の音を鳴らす
}

// ---- 音＋弓：音符を1つずつ送り、音符ごとに弓を反転（休符は待機へ）----
void updateSound(unsigned long now) {
  if (currentBar >= 40) return;
  if (now - noteStartTime < noteInterval) return;

  if (NoteIndex < Score[currentBar].noteCount) {
    float freq = Score[currentBar].notes[NoteIndex].freq;
    float dur  = Score[currentBar].notes[NoteIndex].duration;
    int   art  = Score[currentBar].notes[NoteIndex].art;

    noteInterval  = (unsigned long)(ToneLength * dur);
    noteStartTime = now;

    // Processingへ送信（freq=0 の休符は鳴らされない）
    Serial.print(freq); Serial.print(",");
    Serial.print(noteInterval); Serial.print(",");
    Serial.println(art);

    // 弓：音が出る音符のたびに反転。休符ではその場で保持（待機位置へ戻さない）
    //   → クヮの短い休符でも弓が大きく動かず、振り幅が他と揃う
    if (freq > 0) {
      bowDirection = !bowDirection;
      bowServo.write(bowDirection ? BOW_LEFT_ANGLE : BOW_RIGHT_ANGLE);
    }

    NoteIndex++;
  }
}

// BPM変化で口パク開始
void startEmotion() {
  emotionActive = true;
  emotionEndTime = millis() + (unsigned long)(ToneLength * EMOTION_BEATS);
  lastEmotionMoveTime = 0;
  emotionDirection = false;
}

// 口：BPM変化時に上下にパタパタ（emotion）
void updateEmotion(unsigned long now) {
  if (!emotionActive) return;

  if ((long)(now - emotionEndTime) >= 0) {
    mouthServo.write(MOUTH_CLOSE_ANGLE);
    emotionActive = false;
    return;
  }

  unsigned long interval = (unsigned long)(ToneLength / 2);
  if (interval < 100) interval = 100;

  if (now - lastEmotionMoveTime >= interval) {
    lastEmotionMoveTime = now;
    emotionDirection = !emotionDirection;
    mouthServo.write(emotionDirection ? MOUTH_OPEN_ANGLE : MOUTH_CLOSE_ANGLE);
  }
}

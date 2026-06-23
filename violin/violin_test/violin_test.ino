#include <Servo.h>

// ==================================================
// ヴァイオリン人形：単体統合テスト版（Wi-Fi・サーバー不要）
//
// 1ファイルで「ロボットの動き」と「音(Processingへ送信)」を両方テストする。
//   ・仮想サーバー … 小節番号を自動で 0→39 と進める（Wi-Fiの代わり）
//   ・音           … 各音符を「freq,ms,弾き方」でProcessingへ送信
//   ・動き(1基目 D9) … 弓を一定間隔で反転（弓引き）
//   ・動き(2基目 D5) … 口を一定間隔で開閉（歌唱演出）
//
// ★動きは「音が出ている音符(freq>0)の間だけ」連続して動き、
//   休符(freq=0)になった瞬間に止まる。音と動きがズレない。
//
// ※ Processingで音も確認する場合は、シリアル速度を 115200 に合わせること。
// ==================================================

// ---------- 楽譜データ構造 ----------
#define DOWN_BOW 0   // 下げ弓 → Processing art==0
#define UP_BOW   1   // 上げ弓 → Processing art==1

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
const int ARM_REST_ANGLE   = 80;   // 弓の待機位置
const int BOW_LEFT_ANGLE    = 75;  // 下げ弓側
const int BOW_RIGHT_ANGLE   = 90; // 上げ弓側
const int MOUTH_CLOSE_ANGLE = 90;  // 口を閉じる
const int MOUTH_OPEN_ANGLE  = 60;  // 口を開く

// ---------- ★動きの速さ（msを大きくするとゆっくりになる） ----------
// 音が出ている間、この間隔で弓を反転・口を開閉する
unsigned long BOW_MOVE_INTERVAL   = 550;  // 弓を反転する間隔(ms)
unsigned long MOUTH_MOVE_INTERVAL = 400;  // 口を開閉する間隔(ms)

// ---------- テンポ・小節管理 ----------
char Offset = 0;            // テストは0で即スタート（本番のviolin_clientは -4）
uint8_t currentBPM = 120;
float   ToneLength = 500.0; // 4分音符=1拍の長さ(ms) = 60000/BPM
const int BEATS_PER_BAR = 4;

// 仮想サーバー
uint8_t virtualServerBar = 255;
unsigned long barTimer = 0;
uint8_t currentBar = 255;   // 今演奏中の小節（Score配列のインデックス）

// 音（音符送信）管理
int NoteIndex = 0;
unsigned long noteStartTime = 0;
unsigned long noteInterval = 0;

// ★音が出ているか（休符でない音符を再生中か）。動きのON/OFFを決める
bool soundActive = false;

// 動き管理
bool bowDirection = false;
bool mouthDirection = false;
unsigned long lastBowMove = 0;
unsigned long lastMouthMove = 0;

// ==========================================
// 楽譜（新仕様）
//   0〜7   : デフォルト音階
//   8〜12  : 全休符
//   13〜20 : 1オクターブ上
//   21〜24 : 全休符
//   25〜39 : 終了（全休符）
// ==========================================
void setupScore() {
  Score[0] = {{ {NOTE_C4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW} }, 4};
  Score[1] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[2] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_G4,1.0,DOWN_BOW}, {NOTE_A4,1.0,UP_BOW} }, 4};
  Score[3] = {{ {NOTE_G4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_E4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[4] = {{ {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,UP_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[5] = {{ {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,UP_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[6] = {{ {NOTE_C4,0.5,DOWN_BOW}, {NOTE_C4,0.5,UP_BOW}, {NOTE_D4,0.5,DOWN_BOW}, {NOTE_D4,0.5,UP_BOW},
                {NOTE_E4,0.5,DOWN_BOW}, {NOTE_E4,0.5,UP_BOW}, {NOTE_F4,0.5,DOWN_BOW}, {NOTE_F4,0.5,UP_BOW} }, 8};
  Score[7] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};

  for (int i = 8; i <= 12; i++) Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};

  Score[13] = {{ {NOTE_C5,1.0,DOWN_BOW}, {NOTE_D5,1.0,UP_BOW}, {NOTE_E5,1.0,DOWN_BOW}, {NOTE_F5,1.0,UP_BOW} }, 4};
  Score[14] = {{ {NOTE_E5,1.0,DOWN_BOW}, {NOTE_D5,1.0,UP_BOW}, {NOTE_C5,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[15] = {{ {NOTE_E5,1.0,DOWN_BOW}, {NOTE_F5,1.0,UP_BOW}, {NOTE_G5,1.0,DOWN_BOW}, {NOTE_A5,1.0,UP_BOW} }, 4};
  Score[16] = {{ {NOTE_G5,1.0,DOWN_BOW}, {NOTE_F5,1.0,UP_BOW}, {NOTE_E5,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[17] = {{ {NOTE_C5,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW}, {NOTE_C5,1.0,UP_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[18] = {{ {NOTE_C5,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW}, {NOTE_C5,1.0,UP_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[19] = {{ {NOTE_C5,0.5,DOWN_BOW}, {NOTE_C5,0.5,UP_BOW}, {NOTE_D5,0.5,DOWN_BOW}, {NOTE_D5,0.5,UP_BOW},
                 {NOTE_E5,0.5,DOWN_BOW}, {NOTE_E5,0.5,UP_BOW}, {NOTE_F5,0.5,DOWN_BOW}, {NOTE_F5,0.5,UP_BOW} }, 8};
  Score[20] = {{ {NOTE_E5,1.0,DOWN_BOW}, {NOTE_D5,1.0,UP_BOW}, {NOTE_C5,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};

  for (int i = 21; i <= 24; i++) Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};
  for (int i = 25; i < 40; i++)  Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};
}

void setup() {
  Serial.begin(115200);   // Processingと同じ速度
  bowServo.attach(BOW_PIN);
  mouthServo.attach(MOUTH_PIN);
  bowServo.write(ARM_REST_ANGLE);
  mouthServo.write(MOUTH_CLOSE_ANGLE);

  setupScore();
  ToneLength = 60000.0 / currentBPM;

  Serial.println("VIOLIN 統合テスト開始（音＋弓引き＋口パク, Wi-Fi不要）");
}

void loop() {
  unsigned long now = millis();

  // ---- 仮想サーバー：1小節ぶんの時間が経ったら次の小節へ ----
  unsigned long barDuration = (unsigned long)(ToneLength * BEATS_PER_BAR);
  if (now - barTimer >= barDuration) {
    barTimer = now;

    if (virtualServerBar == 255 || virtualServerBar >= 39) virtualServerBar = 0;
    else virtualServerBar++;

    // BPMは120固定（テンポ変更はしない）

    int myBar = (int)virtualServerBar + Offset;
    if (myBar < 0) {
      currentBar = 255;   // まだ自分の出番でない（Offsetが負のとき）
      soundActive = false;
    } else {
      startBar((uint8_t)myBar);
    }
  }

  updateSound(now);     // 音符をProcessingへ送信（soundActiveを更新）
  updateMovement(now);  // 弓と口の動き（音が出ている間だけ）
}

// 新しい小節を開始（音符を先頭から）
void startBar(uint8_t bar) {
  currentBar = bar;
  NoteIndex = 0;
  noteStartTime = millis();
  noteInterval = 0;   // すぐ最初の音を鳴らす
}

// ---- 音：1音ずつProcessingへ「freq,ms,弾き方」を送る ----
//      送信のたびに「音が出ているか(soundActive)」を更新する
void updateSound(unsigned long now) {
  if (currentBar >= 40) { soundActive = false; return; }
  if (now - noteStartTime < noteInterval) return;

  if (NoteIndex < Score[currentBar].noteCount) {
    float freq = Score[currentBar].notes[NoteIndex].freq;
    float dur  = Score[currentBar].notes[NoteIndex].duration;
    int   art  = Score[currentBar].notes[NoteIndex].art;

    noteInterval  = (unsigned long)(ToneLength * dur);
    noteStartTime = now;

    // ★この音符が鳴る音か休符かで、動きのON/OFFを決める
    soundActive = (freq > 0);

    // Processingへ送信（freq=0 の休符は鳴らされない）
    Serial.print(freq); Serial.print(",");
    Serial.print(noteInterval); Serial.print(",");
    Serial.println(art);

    NoteIndex++;
  }
}

// ---- 弓と口：音が出ている間だけ、一定間隔で動かし続ける ----
void updateMovement(unsigned long now) {
  if (soundActive) {
    // 弓：一定間隔で反転（弓引きを表現）
    if (now - lastBowMove >= BOW_MOVE_INTERVAL) {
      lastBowMove = now;
      bowDirection = !bowDirection;
      bowServo.write(bowDirection ? BOW_LEFT_ANGLE : BOW_RIGHT_ANGLE);
    }
    // 口：一定間隔で開閉（歌っている演出）
    if (now - lastMouthMove >= MOUTH_MOVE_INTERVAL) {
      lastMouthMove = now;
      mouthDirection = !mouthDirection;
      mouthServo.write(mouthDirection ? MOUTH_OPEN_ANGLE : MOUTH_CLOSE_ANGLE);
    }
  } else {
    // 休符：弓も口もすぐ止める
    bowServo.write(ARM_REST_ANGLE);
    mouthServo.write(MOUTH_CLOSE_ANGLE);
  }
}

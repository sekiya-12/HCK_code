#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include "violin_client_function.h"

// =====================================================
// ヴァイオリン人形：本番用クライアント（Wi-Fi＋サーバーあり）
// 1台で「音(Processingへ送信)」と「ロボットの動き」を両方行う。
//
// ★動きは「音が出ている音符(freq>0)の間だけ」連続して動き、
//   休符(freq=0)になった瞬間に止まる。テスト版と同じ方式。
//   ・1基目 D9 … 弓を一定間隔で反転（弓引き）
//   ・2基目 D5 … 口を一定間隔で開閉（歌唱演出）
//
// ヴァイオリン担当として触る場所：
//   (1) Offset … 輪唱で何番手か
//   (2) setupScore() … ヴァイオリンの楽譜
//   (3) 下の角度・速さ定数 … 実機に合わせて調整
// violin_client_function.cpp / .h は触らなくてOK。
// =====================================================


// --- ネットワーク設定 ---
char ssid[] = "hackathon003-WPA2";
char pass[] = "hackathon003";
uint16_t Port = 3000;    // サーバーと同じポート番号

WiFiUDP Udp;

// --- クライアントサイドのグローバル変数（音）---
char Offset = -4;        // 輪唱に必要なオフセット値（ヴァイオリンは4番手 = -4）
uint8_t Data = 255;
uint8_t CurrentBPM = 120;
uint8_t CurrentBar = 255;
uint8_t NoteIndex = 255;
bool Flag = false;
float ToneLength = 0.0;
uint32_t StartTime = 0;
uint16_t Interval = 0;

Pfm Score[40];           // 楽譜（40小節分）

// --- 周波数定義 ---
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

// ===== ロボット（動き）設定 =====
Servo bowServo;    // D9：弓の腕
Servo mouthServo;  // D5：口（上顎）
const int BOW_PIN   = 9;
const int MOUTH_PIN = 5;

// 角度（実機に合わせて調整。テストで詰めた値）
const int ARM_REST_ANGLE   = 70;   // 弓の待機位置
const int BOW_LEFT_ANGLE    = 80;  // 下げ弓側
const int BOW_RIGHT_ANGLE   = 67;  // 上げ弓側
const int MOUTH_CLOSE_ANGLE = 100;  // 口を閉じる
const int 
]
\  = 35;  // 口を開く

// 動きの速さ（msを大きくするとゆっくり）
unsigned long BOW_MOVE_INTERVAL   = 550;  // 弓を反転する間隔(ms)＠BPM120基準。BPMに連動して伸縮
unsigned long MOUTH_MOVE_INTERVAL = 400;  // 口を開閉する間隔(ms)＠BPM120基準。BPMに連動して伸縮

// 動き管理（弓）
bool soundActive = false;
bool bowDirection = false;
unsigned long lastBowMove = 0;

// 口（BPM変化時の歌唱演出 emotion）
bool emotionActive = false;
unsigned long emotionEndTime = 0;
unsigned long lastEmotionMoveTime = 0;
bool emotionDirection = false;
const int EMOTION_BEATS = 4;          // BPM変化時に何拍ぶん口を動かすか
uint8_t lastBPMForEmotion = 120;      // BPM変化を検出するための前回値

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
  // クヮ：短いスタッカート＋短い休符で「クヮ！クヮ！」と歯切れよく
  // クヮ：少し伸ばして最後は切る「クヮー！」（音1.0拍＋休符1.0拍）
  Score[4] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  Score[5] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  Score[6] = {{ {NOTE_C4,0.5,DOWN_BOW}, {NOTE_C4,0.5,UP_BOW}, {NOTE_D4,0.5,DOWN_BOW}, {NOTE_D4,0.5,UP_BOW},
                {NOTE_E4,0.5,DOWN_BOW}, {NOTE_E4,0.5,UP_BOW}, {NOTE_F4,0.5,DOWN_BOW}, {NOTE_F4,0.5,UP_BOW} }, 8};
  Score[7] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};

  for (int i = 8; i <= 12; i++) Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};

  // 13〜20 も基本オクターブにする（オクターブ上は鳴らさない＝0〜7と同じ旋律）
  Score[13] = {{ {NOTE_C4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW} }, 4};
  Score[14] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[15] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_G4,1.0,DOWN_BOW}, {NOTE_A4,1.0,UP_BOW} }, 4};
  Score[16] = {{ {NOTE_G4,1.0,DOWN_BOW}, {NOTE_F4,1.0,UP_BOW}, {NOTE_E4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};
  Score[17] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  Score[18] = {{ {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW}, {NOTE_C4,1.0,STACCATO}, {REST,1.0,DOWN_BOW} }, 4};
  Score[19] = {{ {NOTE_C4,0.5,DOWN_BOW}, {NOTE_C4,0.5,UP_BOW}, {NOTE_D4,0.5,DOWN_BOW}, {NOTE_D4,0.5,UP_BOW},
                 {NOTE_E4,0.5,DOWN_BOW}, {NOTE_E4,0.5,UP_BOW}, {NOTE_F4,0.5,DOWN_BOW}, {NOTE_F4,0.5,UP_BOW} }, 8};
  Score[20] = {{ {NOTE_E4,1.0,DOWN_BOW}, {NOTE_D4,1.0,UP_BOW}, {NOTE_C4,1.0,DOWN_BOW}, {REST,1.0,DOWN_BOW} }, 4};

  for (int i = 21; i <= 24; i++) Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};
  for (int i = 25; i < 40; i++)  Score[i] = {{ {REST,4.0,DOWN_BOW} }, 1};
}

void setup() {
  Serial.begin(115200);

  Serial.println("Wi-Fiに接続中...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi接続完了!");

  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("ルーターからIPアドレスを取得中...");
    delay(500);
  }
  Serial.print("クライアントのIPアドレス: ");
  Serial.println(WiFi.localIP());

  Udp.begin(Port);

  setupScore();
  ToneLength = 60000.0 / CurrentBPM;

  // ロボット（サーボ）初期化
  bowServo.attach(BOW_PIN);
  mouthServo.attach(MOUTH_PIN);
  bowServo.write(ARM_REST_ANGLE);
  mouthServo.write(MOUTH_CLOSE_ANGLE);

  Serial.println("クライアント：譜面データの読み込みが完了しました．");
}

void loop() {
  // 1. 受信パケットの仕分けと小節番号のオフセット適用
  Flag = Parse_data(&Data, Offset, Udp);

  // 2. BPM同期（音側）
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);

  // 3. 演奏位置制御とProcessingへの音符送信（音側）
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, Score);

  // 4. いま鳴っている音符が「鳴る音」かどうかで動きのON/OFFを決める
  soundActive = isSounding();

  // 4.5 BPMが変わったら口の演出（emotion）を発動
  if (CurrentBPM != lastBPMForEmotion) {
    lastBPMForEmotion = CurrentBPM;
    startEmotion();
  }

  // 5. 動きの更新（弓＝演奏中、口＝BPM変化時の演出）
  updateMovement(millis());
  updateEmotion(millis());
}

// いま鳴っている音符が休符でない（freq>0）か？
bool isSounding() {
  if (CurrentBar >= 40) return false;
  int nc  = Score[CurrentBar].noteCount;
  int idx = (int)NoteIndex - 1;   // 直前に送った（＝今鳴っている）音符
  if (idx < 0 || idx >= nc) return false;
  return Score[CurrentBar].notes[idx].freq > 0;
}

// その小節が「全休符の小節」か（長い無音）。弓を待機位置へ戻す対象
bool isWholeRestBar(uint8_t bar) {
  if (bar >= 40) return true;   // 出番待ちなども待機扱い
  return (Score[bar].noteCount == 1 && Score[bar].notes[0].freq <= 0);
}

// 弓：音が出ている間だけ動かし続ける（間隔はBPMに連動して伸縮）
void updateMovement(unsigned long now) {
  // BPM120(ToneLength=500)を基準に、テンポが速いほど間隔を短く＝動きも速く
  float tempoScale = ToneLength / 500.0;
  unsigned long bowInt = (unsigned long)(BOW_MOVE_INTERVAL * tempoScale);
  if (bowInt < 30) bowInt = 30;   // 速すぎ防止の下限

  if (soundActive) {
    if (now - lastBowMove >= bowInt) {
      lastBowMove = now;
      bowDirection = !bowDirection;
      bowServo.write(bowDirection ? BOW_LEFT_ANGLE : BOW_RIGHT_ANGLE);
    }
  } else {
    // 休符：長い全休符のみ弓を待機位置へ（短い休符は弓を保持）
    if (isWholeRestBar(CurrentBar)) {
      bowServo.write(ARM_REST_ANGLE);
    }
  }
}

// BPM変化で口パク開始
void startEmotion() {
  emotionActive = true;
  emotionEndTime = millis() + (unsigned long)(ToneLength * EMOTION_BEATS);
  lastEmotionMoveTime = 0;
  emotionDirection = false;
}

// 口：BPM変化時に上下にパタパタ（emotion）。それ以外は閉じたまま
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

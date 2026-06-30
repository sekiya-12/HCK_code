// =====================================================================
//  trombone_serial_test.ino  ―  正常発音数テスト（演奏テスト）用スケッチ
// ---------------------------------------------------------------------
//  本番 client.ino の譜面配列（Score[0..24]）をそのまま参照し、
//  正常発音数テスト用に調整したスタンドアロン版。
//  WiFi / UDP / ロボットは使わず、仮想サーバーで小節番号を自動進行させ、
//  全25小節のメロディを Processing へシリアル送信する。
//  完全サイクルを2回送信したら停止する。
//
//  ・BPM=120固定（1拍=500ms、1小節=2000ms）
//  ・送信形式：「周波数,鳴らすミリ秒,強さ,BPM」 例: 262,500,92,120
//    （Processing側が4フィールドを要求するため。休符は周波数0で送る）
//  ・各送信ノートはそのままシリアルに出るので、受信ノート列の照合に使える
//
//  ※ client.ino / client_function.* / EntertainmentRobot.* は変更していない。
//    本ファイルは譜面配列を複製して参照するのみ。
// =====================================================================

// --- 譜面データの構造体（client_function.h と同形式：pitch, length, velocity）---
struct NoteData {
  int pitch;        // 音の高さ（周波数 Hz）
  float length;     // 音の長さ（4分音符=1.0 の相対値）
  uint8_t velocity; // 音の強さ（0〜127）
};
struct Pfm {
  NoteData notes[8]; // 1小節に最大8音
  int noteCount;     // この小節の音符数
};

// --- 周波数定義（client.ino と同一）---
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

// --- グローバル変数 ---
const uint8_t BAR_COUNT = 25;  // client.ino と同じ全25小節
Pfm Score[BAR_COUNT];

uint8_t  TestBPM    = 120;     // BPM固定
float    ToneLength = 0.0;     // 4分音符の基準長さ(ms) = 60000/BPM

uint32_t testLastTime = 0;     // 仮想サーバーの小節進行タイマー
int      virtualBar   = -1;    // 現在の小節（-1=未開始）
int      cycleCount   = 0;     // 完了したサイクル数
const int MAX_CYCLES  = 2;     // 完全サイクル2回で停止
bool     finished     = false;

uint8_t  CurrentBar = 255;
uint8_t  NoteIndex  = 255;
uint32_t StartTime  = 0;
uint16_t Interval   = 0;

int sentSounding = 0;          // 送信した発音数（休符以外）
int sentRest     = 0;          // 送信した休符数

// 本番 client.ino の Score[0..24] をそのまま複製（発音58音＋休符23音＝81音）
void setupScore() {
  // 第0小節〜第7小節（1周目）
  Score[0] = {{ {NOTE_C4, 1.0, 92}, {NOTE_D4, 1.0, 96}, {NOTE_E4, 1.0, 100}, {NOTE_F4, 1.0, 104} }, 4};
  Score[1] = {{ {NOTE_E4, 1.0, 100}, {NOTE_D4, 1.0, 96}, {NOTE_C4, 1.0, 90}, {REST, 1.0, 0} }, 4};
  Score[2] = {{ {NOTE_E4, 1.0, 96}, {NOTE_F4, 1.0, 100}, {NOTE_G4, 1.0, 106}, {NOTE_A4, 1.0, 112} }, 4};
  Score[3] = {{ {NOTE_G4, 1.0, 106}, {NOTE_F4, 1.0, 100}, {NOTE_E4, 1.0, 94}, {REST, 1.0, 0} }, 4};
  Score[4] = {{ {NOTE_C4, 1.0, 104}, {REST, 1.0, 0}, {NOTE_C4, 1.0, 104}, {REST, 1.0, 0} }, 4};
  Score[5] = {{ {NOTE_C4, 1.0, 98}, {REST, 1.0, 0}, {NOTE_C4, 1.0, 106}, {REST, 1.0, 0} }, 4};
  Score[6] = {{ {NOTE_C4, 0.5, 88}, {NOTE_C4, 0.5, 92}, {NOTE_D4, 0.5, 96}, {NOTE_D4, 0.5, 100}, {NOTE_E4, 0.5, 104}, {NOTE_E4, 0.5, 108}, {NOTE_F4, 0.5, 112}, {NOTE_F4, 0.5, 116} }, 8};
  Score[7] = {{ {NOTE_E4, 1.0, 108}, {NOTE_D4, 1.0, 100}, {NOTE_C4, 1.0, 92}, {REST, 1.0, 0} }, 4};

  // 第8小節〜（休符）
  Score[8] = {{ {REST, 4.0, 0} }, 1};
  Score[9] = {{ {REST, 4.0, 0} }, 1};
  Score[10] = {{ {REST, 4.0, 0} }, 1};
  Score[11] = {{ {REST, 4.0, 0} }, 1};
  Score[12] = {{ {REST, 4.0, 0} }, 1};

  // 第9小節〜第16小節（2周目：1オクターブ上）
  Score[13] = {{ {NOTE_C5, 1.0, 92}, {NOTE_D5, 1.0, 96}, {NOTE_E5, 1.0, 100}, {NOTE_F5, 1.0, 104} }, 4};
  Score[14] = {{ {NOTE_E5, 1.0, 100}, {NOTE_D5, 1.0, 96}, {NOTE_C5, 1.0, 90}, {REST, 1.0, 0} }, 4};
  Score[15] = {{ {NOTE_E5, 1.0, 96}, {NOTE_F5, 1.0, 100}, {NOTE_G5, 1.0, 106}, {NOTE_A5, 1.0, 112} }, 4};
  Score[16] = {{ {NOTE_G5, 1.0, 106}, {NOTE_F5, 1.0, 100}, {NOTE_E5, 1.0, 94}, {REST, 1.0, 0} }, 4};
  Score[17] = {{ {NOTE_C5, 1.0, 104}, {REST, 1.0, 0}, {NOTE_C5, 1.0, 104}, {REST, 1.0, 0} }, 4};
  Score[18] = {{ {NOTE_C5, 1.0, 98}, {REST, 1.0, 0}, {NOTE_C5, 1.0, 106}, {REST, 1.0, 0} }, 4};
  Score[19] = {{ {NOTE_C5, 0.5, 88}, {NOTE_C5, 0.5, 92}, {NOTE_D5, 0.5, 96}, {NOTE_D5, 0.5, 100}, {NOTE_E5, 0.5, 104}, {NOTE_E5, 0.5, 108}, {NOTE_F5, 0.5, 112}, {NOTE_F5, 0.5, 116} }, 8};
  Score[20] = {{ {NOTE_E5, 1.0, 108}, {NOTE_D5, 1.0, 100}, {NOTE_C5, 1.0, 92}, {REST, 1.0, 0} }, 4};

  // 第17小節〜（休符）
  Score[21] = {{ {REST, 4.0, 0} }, 1};
  Score[22] = {{ {REST, 4.0, 0} }, 1};
  Score[23] = {{ {REST, 4.0, 0} }, 1};
  Score[24] = {{ {REST, 4.0, 0} }, 1};
}

void setup() {
  Serial.begin(115200);
  setupScore();
  ToneLength = 60000.0 / TestBPM;
  testLastTime = millis() - 2000; // 最初の小節をすぐ開始させる
  StartTime = millis();
  Serial.println("[trombone_serial_test] start: 全25小節(発音58+休符23=81音) x2サイクル, BPM=120");
}

// Processingへ1音送信（周波数,ミリ秒,強さ,BPM）
void sendNote(int pitch, uint16_t interval, uint8_t velocity) {
  Serial.print(pitch);
  Serial.print(",");
  Serial.print(interval);
  Serial.print(",");
  Serial.print(velocity);
  Serial.print(",");
  Serial.println(TestBPM);
}

void loop() {
  if (finished) return;

  // 仮想サーバー：1小節(4拍)=2000ms ごとに小節を進める
  if (millis() - testLastTime >= 2000) {
    testLastTime = millis();
    virtualBar++;

    if (virtualBar >= BAR_COUNT) {
      virtualBar = 0;
      cycleCount++;
      if (cycleCount >= MAX_CYCLES) {
        finished = true;
        Serial.print("[trombone_serial_test] 完了 送信発音=");
        Serial.print(sentSounding);
        Serial.print(" 休符=");
        Serial.println(sentRest);
        return;
      }
    }

    CurrentBar = virtualBar;
    NoteIndex  = 0;
    StartTime  = millis();
    Interval   = 0; // 即座に最初の音を送る
  }

  if (CurrentBar >= BAR_COUNT) return;

  // 音を鳴らすタイミング（前の音から Interval 経過したか）
  if (millis() - StartTime >= Interval) {
    if (NoteIndex < Score[CurrentBar].noteCount) {
      StartTime = millis();
      int     pitch    = Score[CurrentBar].notes[NoteIndex].pitch;
      float   length   = Score[CurrentBar].notes[NoteIndex].length;
      uint8_t velocity = Score[CurrentBar].notes[NoteIndex].velocity;
      Interval = (uint16_t)(ToneLength * length);

      sendNote(pitch, Interval, velocity);
      if (pitch == REST) sentRest++; else sentSounding++;

      NoteIndex++;
    }
  }
}

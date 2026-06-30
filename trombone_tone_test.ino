#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "test_function.h"

// ==========================================
// 【テストモード専用の仮想サーバー変数】.   wifiの接続の必要なし
// ==========================================
uint32_t testLastTime = 0;
uint8_t virtualServerBar = 255;

WiFiUDP Udp; // コンパイルを通すためのダミー変数

// ==========================================
// 【クライアントシステム設定用変数】
// ==========================================
char Offset = 0;         // 楽器に合わせて変更（ヴァイオリンなら -2 など）
uint8_t Data = 255;      
uint8_t CurrentBPM = 120;
uint8_t CurrentBar = 255;  
uint8_t NoteIndex = 255;
bool Flag = false;       
float ToneLength = 0.0;  
uint32_t StartTime = 0;  
uint16_t Interval = 0;

Pfm Score[40];

// --- 周波数定義 ---
#define NOTE_C4  262 // ド
#define NOTE_D4  294 // レ
#define NOTE_E4  330 // ミ
#define NOTE_F4  349 // ファ
#define NOTE_G4  392 // ソ
#define NOTE_A4  440 // ラ
#define REST     0   // 休符

#define NOTE_C5  523 // 高いド
#define NOTE_D5  587 // 高いレ
#define NOTE_E5  659 // 高いミ
#define NOTE_F5  698 // 高いファ
#define NOTE_G5  784 // 高いソ
#define NOTE_A5  880 // 高いラ

void setupScore() {
  // 【テスト用】全小節を C4 の4秒ロングトーンにする
  // length 8.0 = ToneLength(500ms=4分音符) × 8 = 4000ms = 4秒
  // 休符は無し。小節進行(loop内の4000ms)と長さを揃えて、ほぼ途切れないC4が続く
  for (int i = 0; i < 40; i++) {
    Score[i] = {{ {NOTE_C4, 8.0} }, 1};
  }
}

// ==========================================
// 【初期設定 (setup)】
// ==========================================
void setup() {
  Serial.begin(115200);
  setupScore();
  ToneLength = 60000.0 / CurrentBPM;
  
  Serial.println("【テストモード】Wi-Fi無しで単体動作を開始します");
  Serial.println("クライアント：譜面データの読み込みが完了しました．");
}

// ==========================================
// 【メインループ (loop)】
// ==========================================
void loop() {
  // 4秒ごとに仮想のサーバーを進める（4秒ロングトーンに合わせる）
  if (millis() - testLastTime >= 4000) {
    testLastTime = millis();
    
    // 仮想サーバーの小節番号をカウントアップ
    if (virtualServerBar == 255 || virtualServerBar >= 39) {
      virtualServerBar = 0;
    } else {
      virtualServerBar++;
    }

    // オフセット計算
    int myBar = virtualServerBar + Offset;
    if (myBar < 0) {
      Data = 255;
    } else {
      Data = myBar;
    }
    Flag = false; // 小節番号として扱う
  } 

  // BPM同期と演奏機能は本番と全く同じものを使用する
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, CurrentBPM, Score);
}
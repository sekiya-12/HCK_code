// =====================================================================
//  test.ino  ―  カスタネット動作確認用テストスケッチ（Wi-Fi不要）
// ---------------------------------------------------------------------
//  トロンボーンのテストで使った test.ino のカスタネット版。
//  WiFi / UDP / サーバーは使わず、仮想サーバーで小節番号を自動進行させ、
//  カスタネットの打点パターンを Processing（HCK_castanets.pde）へ
//  シリアル送信する。送信は test_function.cpp の Performance() を使用。
//
//  ・BPM=120固定（1拍=500ms、1小節=2000ms）
//  ・送信形式：「周波数,鳴らすミリ秒,強さ,BPM」 例: 1000,500,100,120
//    （カスタネットは音程を持たないので pitch はトリガー用の非0値。
//      休符は 0。HCK_castanets.pde が pitch を特定値で扱う場合は HIT を調整）
// =====================================================================

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "test_function.h"

WiFiUDP Udp; // コンパイルを通すためのダミー（実際には使わない）

// --- テストモード用 仮想サーバー変数 ---
uint32_t testLastTime = 0;
uint8_t  virtualServerBar = 255;

// --- クライアント状態変数 ---
char     Offset = 0;
uint8_t  Data = 255;
uint8_t  CurrentBPM = 120;
uint8_t  CurrentBar = 255;
uint8_t  NoteIndex = 255;
bool     Flag = false;
float    ToneLength = 0.0;
uint32_t StartTime = 0;
uint16_t Interval = 0;

Pfm Score[40];

// --- 打点定義 ---
#define HIT   1000  // カスタネットを鳴らす（非0のトリガー値）
#define REST  0     // 休符（無音）

// 8小節ぶんのカスタネット打点パターン（ループ再生）
void setupScore() {
  // 第0〜3小節：1小節に1打だけ（クリック音を単発で録音・波形取得しやすい）
  Score[0] = {{ {HIT, 1.0}, {REST, 1.0}, {REST, 1.0}, {REST, 1.0} }, 4};
  Score[1] = {{ {HIT, 1.0}, {REST, 1.0}, {REST, 1.0}, {REST, 1.0} }, 4};
  Score[2] = {{ {HIT, 1.0}, {REST, 1.0}, {REST, 1.0}, {REST, 1.0} }, 4};
  Score[3] = {{ {HIT, 1.0}, {REST, 1.0}, {REST, 1.0}, {REST, 1.0} }, 4};

  // 第4〜5小節：4分打ち（等間隔の連打）
  Score[4] = {{ {HIT, 1.0}, {HIT, 1.0}, {HIT, 1.0}, {HIT, 1.0} }, 4};
  Score[5] = {{ {HIT, 1.0}, {HIT, 1.0}, {HIT, 1.0}, {HIT, 1.0} }, 4};

  // 第6小節：8分打ち（ロール気味の連打）
  Score[6] = {{ {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5}, {HIT, 0.5} }, 8};

  // 第7小節：全休符（無音の確認）
  Score[7] = {{ {REST, 4.0} }, 1};
}

void setup() {
  Serial.begin(115200);
  setupScore();
  ToneLength = 60000.0 / CurrentBPM;
  Serial.println("【カスタネット・テストモード】Wi-Fi無しで単体動作を開始します");
  Serial.println("譜面（打点パターン）の読み込みが完了しました．");
}

void loop() {
  // 2秒(BPM120の1小節)ごとに仮想サーバーを進める（0〜7小節でループ）
  if (millis() - testLastTime >= 2000) {
    testLastTime = millis();

    if (virtualServerBar == 255 || virtualServerBar >= 7) {
      virtualServerBar = 0;
    } else {
      virtualServerBar++;
    }

    int myBar = virtualServerBar + Offset;
    if (myBar < 0) {
      Data = 255;
    } else {
      Data = myBar;
    }
    Flag = false; // 小節番号として扱う
  }

  // BPM同期と演奏機能は本番と同じものを使用
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, CurrentBPM, Score);
}

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

// ==========================================
// 【楽譜データ (setupScore)】
// ==========================================
void setupScore() {
  // 第0小節〜第7小節（1周目）
  Score[0] = {{ {NOTE_C4, 1.0}, {NOTE_D4, 1.0}, {NOTE_E4, 1.0}, {NOTE_F4, 1.0} }, 4};
  Score[1] = {{ {NOTE_E4, 1.0}, {NOTE_D4, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[2] = {{ {NOTE_E4, 1.0}, {NOTE_F4, 1.0}, {NOTE_G4, 1.0}, {NOTE_A4, 1.0} }, 4};
  Score[3] = {{ {NOTE_G4, 1.0}, {NOTE_F4, 1.0}, {NOTE_E4, 1.0}, {REST, 1.0} }, 4};
  Score[4] = {{ {NOTE_C4, 1.0}, {REST, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[5] = {{ {NOTE_C4, 1.0}, {REST, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[6] = {{ {NOTE_C4, 0.5}, {NOTE_C4, 0.5}, {NOTE_D4, 0.5}, {NOTE_D4, 0.5}, {NOTE_E4, 0.5}, {NOTE_E4, 0.5}, {NOTE_F4, 0.5}, {NOTE_F4, 0.5} }, 8};
  Score[7] = {{ {NOTE_E4, 1.0}, {NOTE_D4, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};

  // 第8小節（休符）
  Score[8] = {{ {REST, 4.0} }, 1};

  // 第9小節〜第16小節（2周目：1オクターブ上）
  Score[9]  = {{ {NOTE_C5, 1.0}, {NOTE_D5, 1.0}, {NOTE_E5, 1.0}, {NOTE_F5, 1.0} }, 4};
  Score[10] = {{ {NOTE_E5, 1.0}, {NOTE_D5, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[11] = {{ {NOTE_E5, 1.0}, {NOTE_F5, 1.0}, {NOTE_G5, 1.0}, {NOTE_A5, 1.0} }, 4};
  Score[12] = {{ {NOTE_G5, 1.0}, {NOTE_F5, 1.0}, {NOTE_E5, 1.0}, {REST, 1.0} }, 4};
  Score[13] = {{ {NOTE_C5, 1.0}, {REST, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[14] = {{ {NOTE_C5, 1.0}, {REST, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[15] = {{ {NOTE_C5, 0.5}, {NOTE_C5, 0.5}, {NOTE_D5, 0.5}, {NOTE_D5, 0.5}, {NOTE_E5, 0.5}, {NOTE_E5, 0.5}, {NOTE_F5, 0.5}, {NOTE_F5, 0.5} }, 8};
  Score[16] = {{ {NOTE_E5, 1.0}, {NOTE_D5, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};

  // 第17小節〜第18小節（休符）
  Score[17] = {{ {REST, 4.0} }, 1};
  Score[18] = {{ {REST, 4.0} }, 1};

  // 第19小節〜第26小節（3周目：通常）
  Score[19] = {{ {NOTE_C4, 1.0}, {NOTE_D4, 1.0}, {NOTE_E4, 1.0}, {NOTE_F4, 1.0} }, 4};
  Score[20] = {{ {NOTE_E4, 1.0}, {NOTE_D4, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[21] = {{ {NOTE_E4, 1.0}, {NOTE_F4, 1.0}, {NOTE_G4, 1.0}, {NOTE_A4, 1.0} }, 4};
  Score[22] = {{ {NOTE_G4, 1.0}, {NOTE_F4, 1.0}, {NOTE_E4, 1.0}, {REST, 1.0} }, 4};
  Score[23] = {{ {NOTE_C4, 1.0}, {REST, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[24] = {{ {NOTE_C4, 1.0}, {REST, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};
  Score[25] = {{ {NOTE_C4, 0.5}, {NOTE_C4, 0.5}, {NOTE_D4, 0.5}, {NOTE_D4, 0.5}, {NOTE_E4, 0.5}, {NOTE_E4, 0.5}, {NOTE_F4, 0.5}, {NOTE_F4, 0.5} }, 8};
  Score[26] = {{ {NOTE_E4, 1.0}, {NOTE_D4, 1.0}, {NOTE_C4, 1.0}, {REST, 1.0} }, 4};

  // 第27小節（休符）
  Score[27] = {{ {REST, 4.0} }, 1};

  // 第28小節〜第35小節（4周目：1オクターブ上）
  Score[28] = {{ {NOTE_C5, 1.0}, {NOTE_D5, 1.0}, {NOTE_E5, 1.0}, {NOTE_F5, 1.0} }, 4};
  Score[29] = {{ {NOTE_E5, 1.0}, {NOTE_D5, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[30] = {{ {NOTE_E5, 1.0}, {NOTE_F5, 1.0}, {NOTE_G5, 1.0}, {NOTE_A5, 1.0} }, 4};
  Score[31] = {{ {NOTE_G5, 1.0}, {NOTE_F5, 1.0}, {NOTE_E5, 1.0}, {REST, 1.0} }, 4};
  Score[32] = {{ {NOTE_C5, 1.0}, {REST, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[33] = {{ {NOTE_C5, 1.0}, {REST, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};
  Score[34] = {{ {NOTE_C5, 0.5}, {NOTE_C5, 0.5}, {NOTE_D5, 0.5}, {NOTE_D5, 0.5}, {NOTE_E5, 0.5}, {NOTE_E5, 0.5}, {NOTE_F5, 0.5}, {NOTE_F5, 0.5} }, 8};
  Score[35] = {{ {NOTE_E5, 1.0}, {NOTE_D5, 1.0}, {NOTE_C5, 1.0}, {REST, 1.0} }, 4};

  // 第36小節〜第39小節（終了・全休符）
  for (int i = 36; i < 40; i++) {
    Score[i] = {{ {REST, 4.0} }, 1};
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
  // 2秒(BPM120の1小節)ごとに仮想のサーバーを進める
  if (millis() - testLastTime >= 2000) {
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
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, Score);
}
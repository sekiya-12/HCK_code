#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "client_function.h"

//Offset変数とsetupScore関数を楽器に合わせて下さい．その他の，client_function.cpp，client_function.hはいじらなくても大丈夫です．


// --- ネットワーク設定 ---
char ssid[] = "hackathon003-WPA2";     
char pass[] = "hackathon003"; 
uint16_t Port = 3000;    // サーバーと同じポート番号

WiFiUDP Udp;

// --- クライアントサイドのグローバル変数 ---
char Offset = 0;         // 輪唱に必要なオフセット値（楽器ごとに設定，2番手だと-2）
uint8_t Data = 255;        // 受信データ
uint8_t CurrentBPM = 120;// 現在のBPM（初期値はサーバーに合わせて120とする）
uint8_t CurrentBar = 255;  // 現在演奏中の小節番号
uint8_t NoteIndex = 255; // 現在の小節の中で、何番目の音符を鳴らしているかのインデックス
bool Flag = false;       // 受信データが小節番号かBPMかを判定するフラグ
float ToneLength = 0.0;  // 基準音符（4分音符）の音の長さ（ミリ秒）
uint32_t StartTime = 0;  // 前回の音符を鳴らした時間（millis）
uint16_t Interval = 0;   // 実際に演奏する音の長さ（ミリ秒）

Pfm Score[40];           // 楽譜（40小節分の配列）


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


void setup() {
  Serial.begin(115200);

  Serial.println("Wi-Fiに接続中...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi接続完了!");

  // IPアドレス取得待機
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("ルーターからIPアドレスを取得中...");
    delay(500);
  }
  Serial.print("クライアントのIPアドレス: ");
  Serial.println(WiFi.localIP());
  
  Udp.begin(Port);

  setupScore();
  ToneLength = 60000.0 / CurrentBPM;
  Serial.println("クライアント：譜面データの読み込みが完了しました．");
}

void loop() {
  // 1. 受信パケットの仕分けと小節番号のオフセット適用
  Flag = Parse_data(&Data, Offset, Udp);
  
  // 2. データがBPMだった場合の同期処理
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);

  // 3. 演奏位置制御とPCへのデータ送信（今回追加）
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, Score);
}

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "function.h"

//Offset変数とsetupScore関数を楽器に合わせて下さい．その他の，function.cpp，function.hはいじらなくても大丈夫です．


// --- ネットワーク設定 ---
char ssid[] = "WIFI_SSID";     
char pass[] = "WIFI_PASSWORD"; 
uint16_t Port = 3000;          // サーバーと同じポート番号

WiFiUDP Udp;

// --- クライアントサイドのグローバル変数 ---
char Offset = 0;         // 輪唱に必要なオフセット値（楽器ごとに設定，2番手だと-2）
uint8_t Data = 0;        // 受信データ
uint8_t CurrentBPM = 120;// 現在のBPM（初期値はサーバーに合わせて120とする）
uint8_t CurrentBar = 0;  // 現在演奏中の小節番号
bool Flag = false;       // 受信データが小節番号かBPMかを判定するフラグ
float ToneLength = 0.0;  // 基準音符（4分音符）の音の長さ（ミリ秒）
uint32_t StartTime = 0;  // 前回の音符を鳴らした時間（millis）
uint16_t Interval = 0;   // 実際に演奏する音の長さ（ミリ秒）

Pfm Score[40];           // 楽譜（40小節分の配列）

uint8_t NoteIndex = 0; // 現在の小節の中で、何番目の音符を鳴らしているかのインデックス


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
  // --- 第0小節：「ド・レ・ミ・ファ」（か・え・る・の） ---
  Score[0].noteCount = 4;
  Score[0].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[0].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[0].notes[2] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[0].notes[3] = {NOTE_F4, 1.0}; // ファ, 4分音符

  // --- 第1小節：「ミ・レ・ド・休」（う・た・が・休） ---
  Score[1].noteCount = 4;
  Score[1].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[1].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[1].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[1].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第2小節：「ミ・ファ・ソ・ラ」（き・こ・え・て） ---
  Score[2].noteCount = 4;
  Score[2].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[2].notes[1] = {NOTE_F4, 1.0}; // ファ, 4分音符
  Score[2].notes[2] = {NOTE_G4, 1.0}; // ソ, 4分音符
  Score[2].notes[3] = {NOTE_A4, 1.0}; // ラ, 4分音符

  // --- 第3小節：「ソ・ファ・ミ・休」（く・る・よ・休） ---
  Score[3].noteCount = 4;
  Score[3].notes[0] = {NOTE_G4, 1.0}; // ソ, 4分音符
  Score[3].notes[1] = {NOTE_F4, 1.0}; // ファ, 4分音符
  Score[3].notes[2] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[3].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第4小節：「ド・休・ド・休」（ぐわ・休・ぐわ・休） ---
  Score[4].noteCount = 4;
  Score[4].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[4].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[4].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[4].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第5小節：「ド・休・ド・休」（ぐわ・休・ぐわ・休） ---
  Score[5].noteCount = 4;
  Score[5].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[5].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[5].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[5].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第6小節：「ド・ド・レ・レ・ミ・ミ・ファ・ファ」（ケロケロケロケロ） ---
  Score[6].noteCount = 8;
  Score[6].notes[0] = {NOTE_C4, 0.5}; // ド, 8分音符
  Score[6].notes[1] = {NOTE_C4, 0.5}; // ド, 8分音符
  Score[6].notes[2] = {NOTE_D4, 0.5}; // レ, 8分音符
  Score[6].notes[3] = {NOTE_D4, 0.5}; // レ, 8分音符
  Score[6].notes[4] = {NOTE_E4, 0.5}; // ミ, 8分音符
  Score[6].notes[5] = {NOTE_E4, 0.5}; // ミ, 8分音符
  Score[6].notes[6] = {NOTE_F4, 0.5}; // ファ, 8分音符
  Score[6].notes[7] = {NOTE_F4, 0.5}; // ファ, 8分音符

  // --- 第7小節：「ミ・レ・ド・休」（ぐわ・ぐわ・ぐわ・休） ---
  Score[7].noteCount = 4;
  Score[7].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[7].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[7].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[7].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第8小節：演奏終了（全休符） ---
  Score[8].noteCount = 1;
  Score[8].notes[0] = {REST, 4.0};    // 休符, 全音符（4拍分）

  // --- 第9小節：「ド・レ・ミ・ファ」（1オクターブ上） ---
  Score[9].noteCount = 4;
  Score[9].notes[0] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[9].notes[1] = {NOTE_D5, 1.0}; // レ, 4分音符
  Score[9].notes[2] = {NOTE_E5, 1.0}; // ミ, 4分音符
  Score[9].notes[3] = {NOTE_F5, 1.0}; // ファ, 4分音符

  // --- 第10小節：「ミ・レ・ド・休」 ---
  Score[10].noteCount = 4;
  Score[10].notes[0] = {NOTE_E5, 1.0}; // ミ, 4分音符
  Score[10].notes[1] = {NOTE_D5, 1.0}; // レ, 4分音符
  Score[10].notes[2] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[10].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第11小節：「ミ・ファ・ソ・ラ」 ---
  Score[11].noteCount = 4;
  Score[11].notes[0] = {NOTE_E5, 1.0}; // ミ, 4分音符
  Score[11].notes[1] = {NOTE_F5, 1.0}; // ファ, 4分音符
  Score[11].notes[2] = {NOTE_G5, 1.0}; // ソ, 4分音符
  Score[11].notes[3] = {NOTE_A5, 1.0}; // ラ, 4分音符

  // --- 第12小節：「ソ・ファ・ミ・休」 ---
  Score[12].noteCount = 4;
  Score[12].notes[0] = {NOTE_G5, 1.0}; // ソ, 4分音符
  Score[12].notes[1] = {NOTE_F5, 1.0}; // ファ, 4分音符
  Score[12].notes[2] = {NOTE_E5, 1.0}; // ミ, 4分音符
  Score[12].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第13小節：「ド・休・ド・休」 ---
  Score[13].noteCount = 4;
  Score[13].notes[0] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[13].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[13].notes[2] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[13].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第14小節：「ド・休・ド・休」 ---
  Score[14].noteCount = 4;
  Score[14].notes[0] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[14].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[14].notes[2] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[14].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第15小節：「ド・ド・レ・レ・ミ・ミ・ファ・ファ」 ---
  Score[15].noteCount = 8;
  Score[15].notes[0] = {NOTE_C5, 0.5}; // ド, 8分音符
  Score[15].notes[1] = {NOTE_C5, 0.5}; // ド, 8分音符
  Score[15].notes[2] = {NOTE_D5, 0.5}; // レ, 8分音符
  Score[15].notes[3] = {NOTE_D5, 0.5}; // レ, 8分音符
  Score[15].notes[4] = {NOTE_E5, 0.5}; // ミ, 8分音符
  Score[15].notes[5] = {NOTE_E5, 0.5}; // ミ, 8分音符
  Score[15].notes[6] = {NOTE_F5, 0.5}; // ファ, 8分音符
  Score[15].notes[7] = {NOTE_F5, 0.5}; // ファ, 8分音符

  // --- 第16小節：「ミ・レ・ド・休」 ---
  Score[16].noteCount = 4;
  Score[16].notes[0] = {NOTE_E5, 1.0}; // ミ, 4分音符
  Score[16].notes[1] = {NOTE_D5, 1.0}; // レ, 4分音符
  Score[16].notes[2] = {NOTE_C5, 1.0}; // ド, 4分音符
  Score[16].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第17小節：全休符 ---
  Score[17].noteCount = 1;
  Score[17].notes[0] = {REST, 4.0};    // 休符, 全音符（4拍分）

  // --- 第18小節：全休符 ---
  Score[18].noteCount = 1;
  Score[18].notes[0] = {REST, 4.0};    // 休符, 全音符（4拍分）

  // --- 第19小節：「ド・レ・ミ・ファ」（3周目・通常音高に戻る） ---
  Score[19].noteCount = 4;
  Score[19].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[19].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[19].notes[2] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[19].notes[3] = {NOTE_F4, 1.0}; // ファ, 4分音符

  // --- 第20小節：「ミ・レ・ド・休」 ---
  Score[20].noteCount = 4;
  Score[20].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[20].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[20].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[20].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第21小節：「ミ・ファ・ソ・ラ」 ---
  Score[21].noteCount = 4;
  Score[21].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[21].notes[1] = {NOTE_F4, 1.0}; // ファ, 4分音符
  Score[21].notes[2] = {NOTE_G4, 1.0}; // ソ, 4分音符
  Score[21].notes[3] = {NOTE_A4, 1.0}; // ラ, 4分音符

  // --- 第22小節：「ソ・ファ・ミ・休」 ---
  Score[22].noteCount = 4;
  Score[22].notes[0] = {NOTE_G4, 1.0}; // ソ, 4分音符
  Score[22].notes[1] = {NOTE_F4, 1.0}; // ファ, 4分音符
  Score[22].notes[2] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[22].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第23小節：「ド・休・ド・休」 ---
  Score[23].noteCount = 4;
  Score[23].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[23].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[23].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[23].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第24小節：「ド・休・ド・休」 ---
  Score[24].noteCount = 4;
  Score[24].notes[0] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[24].notes[1] = {REST, 1.0};    // 休符, 4分音符
  Score[24].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[24].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第25小節：「ド・ド・レ・レ・ミ・ミ・ファ・ファ」 ---
  Score[25].noteCount = 8;
  Score[25].notes[0] = {NOTE_C4, 0.5}; // ド, 8分音符
  Score[25].notes[1] = {NOTE_C4, 0.5}; // ド, 8分音符
  Score[25].notes[2] = {NOTE_D4, 0.5}; // レ, 8分音符
  Score[25].notes[3] = {NOTE_D4, 0.5}; // レ, 8分音符
  Score[25].notes[4] = {NOTE_E4, 0.5}; // ミ, 8分音符
  Score[25].notes[5] = {NOTE_E4, 0.5}; // ミ, 8分音符
  Score[25].notes[6] = {NOTE_F4, 0.5}; // ファ, 8分音符
  Score[25].notes[7] = {NOTE_F4, 0.5}; // ファ, 8分音符

  // --- 第26小節：「ミ・レ・ド・休」 ---
  Score[26].noteCount = 4;
  Score[26].notes[0] = {NOTE_E4, 1.0}; // ミ, 4分音符
  Score[26].notes[1] = {NOTE_D4, 1.0}; // レ, 4分音符
  Score[26].notes[2] = {NOTE_C4, 1.0}; // ド, 4分音符
  Score[26].notes[3] = {REST, 1.0};    // 休符, 4分音符

  // --- 第27小節：間奏（全休符） ---
  Score[27].noteCount = 1;
  Score[27].notes[0] = {REST, 4.0};

  // --- 第28小節：「ド・レ・ミ・ファ」（4周目・1オクターブ上） ---
  Score[28].noteCount = 4;
  Score[28].notes[0] = {NOTE_C5, 1.0}; // 高いド, 4分音符
  Score[28].notes[1] = {NOTE_D5, 1.0}; // 高いレ, 4分音符
  Score[28].notes[2] = {NOTE_E5, 1.0}; // 高いミ, 4分音符
  Score[28].notes[3] = {NOTE_F5, 1.0}; // 高いファ, 4分音符

  // --- 第29小節：「ミ・レ・ド・休」 ---
  Score[29].noteCount = 4;
  Score[29].notes[0] = {NOTE_E5, 1.0};
  Score[29].notes[1] = {NOTE_D5, 1.0};
  Score[29].notes[2] = {NOTE_C5, 1.0};
  Score[29].notes[3] = {REST, 1.0};

  // --- 第30小節：「ミ・ファ・ソ・ラ」 ---
  Score[30].noteCount = 4;
  Score[30].notes[0] = {NOTE_E5, 1.0};
  Score[30].notes[1] = {NOTE_F5, 1.0};
  Score[30].notes[2] = {NOTE_G5, 1.0};
  Score[30].notes[3] = {NOTE_A5, 1.0};

  // --- 第31小節：「ソ・ファ・ミ・休」 ---
  Score[31].noteCount = 4;
  Score[31].notes[0] = {NOTE_G5, 1.0};
  Score[31].notes[1] = {NOTE_F5, 1.0};
  Score[31].notes[2] = {NOTE_E5, 1.0};
  Score[31].notes[3] = {REST, 1.0};

  // --- 第32小節：「ド・休・ド・休」 ---
  Score[32].noteCount = 4;
  Score[32].notes[0] = {NOTE_C5, 1.0};
  Score[32].notes[1] = {REST, 1.0};
  Score[32].notes[2] = {NOTE_C5, 1.0};
  Score[32].notes[3] = {REST, 1.0};

  // --- 第33小節：「ド・休・ド・休」 ---
  Score[33].noteCount = 4;
  Score[33].notes[0] = {NOTE_C5, 1.0};
  Score[33].notes[1] = {REST, 1.0};
  Score[33].notes[2] = {NOTE_C5, 1.0};
  Score[33].notes[3] = {REST, 1.0};

  // --- 第34小節：「ド・ド・レ・レ・ミ・ミ・ファ・ファ」 ---
  Score[34].noteCount = 8;
  Score[34].notes[0] = {NOTE_C5, 0.5};
  Score[34].notes[1] = {NOTE_C5, 0.5};
  Score[34].notes[2] = {NOTE_D5, 0.5};
  Score[34].notes[3] = {NOTE_D5, 0.5};
  Score[34].notes[4] = {NOTE_E5, 0.5};
  Score[34].notes[5] = {NOTE_E5, 0.5};
  Score[34].notes[6] = {NOTE_F5, 0.5};
  Score[34].notes[7] = {NOTE_F5, 0.5};

  // --- 第35小節：「ミ・レ・ド・休」 ---
  Score[35].noteCount = 4;
  Score[35].notes[0] = {NOTE_E5, 1.0};
  Score[35].notes[1] = {NOTE_D5, 1.0};
  Score[35].notes[2] = {NOTE_C5, 1.0};
  Score[35].notes[3] = {REST, 1.0};

  // --- 第36小節〜第39小節：演奏終了（すべて全休符で埋める） ---
  for (int i = 36; i < 40; i++) {
    Score[i].noteCount = 1;
    Score[i].notes[0] = {REST, 4.0}; // 全休符
  }
}



void setup() {

  Serial.begin(115200);

  setupScore();// 譜面配列の初期化を実行
  ToneLength = 60000.0 / CurrentBPM;
  Serial.println("譜面データの読み込みが完了しました．");
}

void loop() {
  // 1. 受信パケットの仕分けと小節番号のオフセット適用
  Flag = Parse_data(&Data, Offset, Udp);
  
  // 2. データがBPMだった場合の同期処理
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);

  // 3. 演奏位置制御とPCへのデータ送信（今回追加）
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, Score);
}

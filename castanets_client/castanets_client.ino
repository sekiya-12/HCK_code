#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "castanets_client_function.h"

// Offset変数とsetupScore関数を楽器に合わせて下さい．その他の，client_function.cpp，client_function.hはいじらなくても大丈夫です．

// --- ネットワーク設定 ---
char ssid[] = "xg100n-430175-1"; //hackathon003-WPA2
char pass[] = "54abbe1550e21"; //hackathon003
uint16_t Port = 3000; // サーバーと同じポート番号

WiFiUDP Udp;

// --- クライアントサイドのグローバル変数 ---
char Offset = -2;         // 輪唱に必要なオフセット値（楽器ごとに設定，2番手だと-2）
uint8_t Data = 255;       // 受信データ
uint8_t CurrentBPM = 120; // 現在のBPM（初期値はサーバーに合わせて120とする）
uint8_t CurrentBar = 255; // 現在演奏中の小節番号
uint8_t NoteIndex = 255;  // 現在の小節の中で、何番目の音符を鳴らしているかのインデックス
bool Flag = false;        // 受信データが小節番号かBPMかを判定するフラグ
float ToneLength = 0.0;   // 基準音符（4分音符）の音の長さ（ミリ秒）
uint32_t StartTime = 0;   // 前回の音符を鳴らした時間（millis）
uint16_t Interval = 0;    // 実際に演奏する音の長さ（ミリ秒）

Pfm Score[40]; // 楽譜（40小節分の配列）

// --- 周波数定義 ---
constexpr int kFreqValue = 1800; // 音階なしのため固定の周波数
constexpr int kRest = 0;

void setupScore()
{
  // 第0小節〜第7小節（1周目）
  Score[0] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[1] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[2] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[3] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[4] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[5] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[6] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[7] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第8小節（休符）
  Score[8] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第9小節〜第16小節（2周目：1オクターブ上）
  Score[9] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[10] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[11] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[12] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[13] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[14] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[15] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[16] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第17小節〜第18小節（休符）
  Score[17] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[18] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第19小節〜第26小節（3周目：通常）
  Score[19] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[20] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[21] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[22] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[23] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[24] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[25] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[26] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第27小節（休符）
  Score[27] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第28小節〜第35小節（4周目：1オクターブ上）
  Score[28] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[29] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[30] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[31] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[32] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[33] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[34] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};
  Score[35] = {{{kFreqValue, 0.2f}, {kRest, 0.2f}, {kFreqValue, 0.2f}, {kRest, 0.2f}}, 4};

  // 第36小節〜第39小節（終了・全休符）
  for (int i = 36; i < 40; i++)
  {
    Score[i] = {{{kRest, 4.0f}}, 1};
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.println("Wi-Fiに接続中...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi接続完了!");

  // IPアドレス取得待機
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0))
  {
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

void loop()
{
  // 1. 受信パケットの仕分けと小節番号のオフセット適用
  Flag = Parse_data(&Data, Offset, Udp);

  // 2. データがBPMだった場合の同期処理
  BPM_update(&CurrentBPM, Flag, Data, &ToneLength);

  // 3. 演奏位置制御とPCへのデータ送信（今回追加）
  Performance(Data, &CurrentBar, &NoteIndex, &StartTime, &Interval, ToneLength, Score);
}
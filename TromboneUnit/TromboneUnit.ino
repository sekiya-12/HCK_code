#include <WiFi.h>
#include <WiFiUdp.h> 
#include "Secret.h"
#include "SynthTrigger.h"
#include "UdpHandler.h"

SynthTrigger synth;
UdpHandler udpHandler;

// ==========================================================
//  【トロンボーン専用・楽譜配列定義】
// ==========================================================
String melody[] = {
    "C4", "D4", "E4", "F4", "E4", "D4", "C4",
    "E4", "F4", "G4", "A4", "G4", "F4", "E4",
    "C4", "C4", "C4", "C4", "C4", "C4",
    "D4", "D4", "E4", "E4", "F4", "F4", "E4", "D4", "C4"};

float duration[] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f,
    2.0f, 2.0f, 2.0f, 2.0f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    1.0f, 1.0f, 2.0f};

// 楽譜の全音数
const int totalNotes = sizeof(melody) / sizeof(melody[0]);

// クライアント（Arduino）側で現在の状態を保持する変数
int currentMeasure = -1;
int currentBpm = 80;
int noteIndex = 0;               // 現在何番目の音を指しているか
unsigned long noteStartTime = 0; // 現在の音が鳴り始めた時間(ミリ秒)
float currentNoteDurationMs = 0; // 現在の音の長さ(ミリ秒)

void setup()
{
    synth.init(115200); // 授業仕様の高速シリアル通信を開始

    //================クライアントのみのProcessing送信テスト時に一時的にコメントアウトする必要あり============

    //Wi-Fi接続処理
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    udpHandler.init(localUdpPort);
    //================================================
}

void loop()
{
    int nextMeasure = currentMeasure;
    int nextBpm = currentBpm;

    // ==============【テスト用】==================
    //static boolean firstRun = true;
    //if (firstRun)
    //{
    //    nextMeasure = 0; 
    //    nextBpm = 120;   
    //    currentMeasure = nextMeasure;
    //    currentBpm = nextBpm;
    //    
    //   synth.sendNoteData(melody[0], duration[0], currentBpm, 100);
    //    noteIndex = 1;
    //    noteStartTime = millis();
    //    
    //    currentNoteDurationMs = duration[0] * (60000.0 / nextBpm);

    //    firstRun = false; 
    //}
    //====================================================

    // ==============================クライアントのみのProcessing送信テスト時に一時的にコメントアウトする必要あり========================

    // 1. サーバーからの情報を取得し、判定・保存する
    if (udpHandler.parseUdp(nextMeasure, nextBpm))
    {
        // 新しい小節番号(0~39)を受信した瞬間
        currentMeasure = nextMeasure;

        // ハッカソン同期仕様：新しい小節が来たら、演奏位置をリセット、またはその小節の頭に合わせる
        // (ここではデモとして、小節信号が届くたびに楽譜を最初からリフレッシュしてタイマーを同期させます)
        noteIndex = 0;
        noteStartTime = millis();

        // 現在のBPMを元に、最初の音の長さをミリ秒(ms)に計算
        // 計算式: duration(1.0f等) × 1拍の長さ(60000ms / BPM)
       currentNoteDurationMs = duration[noteIndex] * (60000.0 / nextBpm);
    }
    currentBpm = nextBpm; // 最新のBPM情報を常にArduino側に保存

    // ==========================================================

    // 2. 自律的な演奏タイミング制御
    if (currentMeasure >= 0 && noteIndex < totalNotes)
    {
        unsigned long elapsedTime = millis() - noteStartTime;

        // 前の音の長さ（ミリ秒）が経過したら、次の音へ進む
        if (elapsedTime >= currentNoteDurationMs)
        {
            synth.sendNoteData(melody[noteIndex], duration[noteIndex], currentBpm, 100);
            
            noteStartTime = noteStartTime + (unsigned long)currentNoteDurationMs;
            
            currentNoteDurationMs = duration[noteIndex] * (60000.0 / currentBpm);
            
            noteIndex++;
        }
    }
}

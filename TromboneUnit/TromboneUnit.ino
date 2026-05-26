#include <ESP8266WiFi.h> // ※ESP32を使用している場合は <WiFi.h> に変更してください
#include "Secret.h"
#include "SynthTrigger.h"
#include "UdpHandler.h"

SynthTrigger synth;
UdpHandler udpHandler;

// ==========================================================
// 🎵 【トロンボーン専用・楽譜配列定義】
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

    // Wi-Fi接続処理
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    udpHandler.init(localUdpPort);
}

void loop()
{
    int nextMeasure = currentMeasure;
    int nextBpm = currentBpm;

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

    // 2. 自律的な演奏タイミング制御
    if (currentMeasure >= 0 && noteIndex < totalNotes)
    {
        unsigned long elapsedTime = millis() - noteStartTime;

        // 前の音の長さ（ミリ秒）が経過したら、次の音へ進む
        if (elapsedTime >= currentNoteDurationMs)
        {

            // 発音タイミングが来たので、Processingへ【複数のデータ】をシリアル送信！
            // 送るデータ：音名(String), 長さ(float), 最新BPM(int), 強さ(仮で100固定)
            synth.sendNoteData(melody[noteIndex], duration[noteIndex], currentBpm, 100);

            // 次の音へインデックスを進める
            noteIndex++;
            noteStartTime = millis(); // 新しい音の開始時間を保存

            if (noteIndex < totalNotes)
            {
                // 次の音の長さを最新のBPMを元に再計算して保存
                currentNoteDurationMs = duration[noteIndex] * (60000.0 / currentBpm);
            }
        }
    }
}
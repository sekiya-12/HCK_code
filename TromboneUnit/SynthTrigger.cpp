#include "SynthTrigger.h"

void SynthTrigger::init(long baudRate)
{
    Serial.begin(baudRate);
    while (!Serial)
    {
        ;
    } // シリアルポートの準備ができるまで待機
}

// 複数のデータをカンマ区切りで一気に送信する処理
void SynthTrigger::sendNoteData(String note, float duration, int bpm, int velocity)
{
    Serial.print(note);        // 1つ目: 音名（例: "C4"）
    Serial.print(",");         // 区切り文字
    Serial.print(duration, 2); // 2つ目: 長さ（例: "1.00"）
    Serial.print(",");         // 区切り文字
    Serial.print(bpm);         // 3つ目: BPM（例: "80"）
    Serial.print(",");         // 区切り文字
    Serial.println(velocity);  // 4つ目: 強さ（例: "100"） + 最後に改行(\n)を送る
}
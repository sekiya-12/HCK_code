#ifndef VIOLIN_CLIENT_FUNCTION_H
#define VIOLIN_CLIENT_FUNCTION_H
#include <Arduino.h>
#include <WiFiUdp.h>

#define LEGATO   0  // 通常の滑らかな弾き方（カァ〜）
#define STACCATO 1  // 短く鋭く跳ねる弾き方（クァッ！）
#define DETACHE  2  // 細かく連続する弾き方（ケケケケ）

// --- 譜面データの構造体定義 ---
// 1つの音符の情報を表す構造体
struct NoteData {
  float freq;         // 周波数
  float duration;     // 音の長さ
  int articulation;   // ★追加：弾き方（0:LEGATO, 1:STACCATO, 2:DETACHE）
};

// 1つの小節（Pfm）の情報を表す構造体
struct Pfm {
  NoteData notes[8]; // 1小節に最大8つの音符が入ると仮定
  int noteCount;     // この小節に実際に含まれる音符の数
};

bool Parse_data(uint8_t *Data, char Offset, WiFiUDP &Udp);
void BPM_update(uint8_t *CurrentBPM, bool Flag, uint8_t Data, float *ToneLength);
void Performance(uint8_t Data, uint8_t *CurrentBar, uint8_t *NoteIndex, uint32_t *StartTime, uint16_t *Interval, float ToneLength, Pfm *Score);

#endif
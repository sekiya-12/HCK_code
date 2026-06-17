#ifndef CLIENT_FUNCTION_H
#define CLIENT_FUNCTION_H
#include <Arduino.h>
#include <WiFiUdp.h>

// --- 譜面データの構造体定義 ---
// 1つの音符の情報を表す構造体
struct NoteData {
  int pitch;      // 音の高さ（周波数 Hz）
  float length;   // 音の長さ（基準となる4分音符を 1.0 とした相対値）
  uint8_t velocity; // 音の強さ（0〜127）
};

// 1つの小節（Pfm）の情報を表す構造体
struct Pfm {
  NoteData notes[8]; // 1小節に最大8つの音符が入ると仮定
  int noteCount;     // この小節に実際に含まれる音符の数
};

bool Parse_data(uint8_t *Data, char Offset, WiFiUDP &Udp);// 受信データがBPM値ならtrue（フラグON），小節番号ならfalse（フラグOFF）を返す関数

void BPM_update(uint8_t *CurrentBPM, bool Flag, uint8_t Data, float *ToneLength);

void Performance(uint8_t Data, uint8_t *CurrentBar, uint8_t *NoteIndex, uint32_t *StartTime, uint16_t *Interval, float ToneLength, uint8_t CurrentBPM, Pfm *Score);
#endif

#ifndef VIOLIN_CLIENT_FUNCTION_H
#define VIOLIN_CLIENT_FUNCTION_H
#include <Arduino.h>
#include <WiFiUdp.h>

// ==========================================
// 【弾き方（ボウイング）の定義】
// ==========================================
// 弓の方向（ボウイング）→ Processing の art パラメータに対応
#define DOWN_BOW 0  // 下げ弓（力強い入り）→ Processing: art==0
#define UP_BOW   1  // 上げ弓（柔らかい入り）→ Processing: art==1
#define STACCATO 2  // 短く鋭く跳ねる       → Processing: art==2

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
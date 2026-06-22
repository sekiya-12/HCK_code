#ifndef ENTAME_H
#define ENTAME_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Servo.h>

// ==================================================
// エンタメ性専用クライアント
// サーバー・楽器クライアント側は変更しない前提
//
// サーバーからのUDP 1Byteを受信する
// 0〜39     → 小節番号
// 40〜255  → BPM
//
// 今回追加した機能
// ・受信したBPMから四分音符，八分音符，休符などの時間を計算する
// ・SCORE_TABLEに書いた楽譜データ通りにサーボを動かす
// ・休符の部分ではサーボを動かさない
// ==================================================

// =====================
// 楽器選択
// =====================
#define PIANO      1
#define TROMBONE   2
#define VIOLIN     3
#define CASTANET   4

// 使う人形に合わせてここだけ変更
// 例：ピアノなら PIANO，トロンボーンなら TROMBONE
#ifndef INSTRUMENT
#define INSTRUMENT PIANO
#endif

void entameSetup();
void entameLoop();

#endif
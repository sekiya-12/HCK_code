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
// ==================================================

// =====================
// 楽器選択
// =====================
#define PIANO      1
#define TROMBONE   2
#define VIOLIN     3
#define CASTANET   4

// 使う人形に合わせてここだけ変更
#define INSTRUMENT PIANO
// #define INSTRUMENT TROMBONE
// #define INSTRUMENT VIOLIN
// #define INSTRUMENT CASTANET

void entameSetup();
void entameLoop();

#endif

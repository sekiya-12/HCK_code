#ifndef FUNCTION_H
#define FUNCTION_H
#include <Arduino.h>
#include <WiFiUdp.h>
#include "Arduino_LED_Matrix.h"

// 舞台・演奏制御の初期化（モードボタン・スポットライト・カーテンモータのピン設定）
void Stage_init();

// 舞台・演奏の状態制御
// ボタン押下で開始 → カーテンを開く → 演奏（小節送信＋スポットライト）→ カーテンを閉じる → 待機へ戻る
void Stage_control(uint16_t Interval, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp);

void BPM_control(uint8_t *BPM, uint16_t *Interval, uint32_t *LastPressTime, bool *Flag, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp);

#endif

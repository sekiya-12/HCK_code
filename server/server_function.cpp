#include "server_function.h"

// ============================================================
//  スイッチ押下検出ピン
// ============================================================
#define PIN_BPM_UP    2   // BPMアップ
#define PIN_BPM_DOWN  3   // BPMダウン
// ============================================================
//  デバウンスタイム
// ============================================================
#define DEBOUNCE_MS 30

void Bar_control(uint32_t *LastBarTime, uint16_t Interval, uint8_t *BarCount, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp) {
  uint32_t temp = millis();
  
  // Interval（間隔）以上時間が経過したかチェック
  if (temp - *LastBarTime >= Interval) {
    *LastBarTime = temp;
    
    // 小節番号のカウント（上限39）
    if (*BarCount >= 39) {
      *BarCount = 0;
    } else {
      *BarCount += 1;
    }
    
    // UDPブロードキャスト送信
    udp.beginPacket(BCaddress, Port);
    udp.write(*BarCount);
    udp.endPacket();

    // デバッグ用出力
    Serial.print("送信した小節番号: ");
    Serial.println(*BarCount);
  }
}
// ------------------------------------------------------------
//  チャタリング制御用関数
// ------------------------------------------------------------
static bool detectPress(uint8_t pin, int *lastReading, int *stableState, uint32_t *lastChangeTime) {
  int reading = digitalRead(pin);

  // 読み値が変化したら、変化時刻を更新
  if (reading != *lastReading) {
    *lastChangeTime = millis();
    *lastReading = reading;
  }

  // 一定時間読み値が安定していたら確定状態を更新する
  if (millis() - *lastChangeTime > DEBOUNCE_MS) {
    if (reading != *stableState) {
      *stableState = reading;
      // 確定状態が HIGH に変わった瞬間 ＝ 押された瞬間
      if (*stableState == HIGH) {
        return true;
      }
    }
  }
  return false;
}


void BPM_control(uint8_t *BPM, uint16_t *Interval, uint32_t *LastPressTime, bool *Flag, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp) {

  // --- 各ボタンのデバウンス状態（関数をまたいで保持するため static） ---
  static int      upLastReading   = LOW;
  static int      upStableState   = LOW;
  static uint32_t upLastChange    = 0;

  static int      downLastReading = LOW;
  static int      downStableState = LOW;
  static uint32_t downLastChange  = 0;

  // === BPMアップ（D2）===
  if (detectPress(PIN_BPM_UP, &upLastReading, &upStableState, &upLastChange)) {
    if (*BPM <= 250) {              // 上限255を超えない範囲で（+5してもオーバーフローしない）
      *BPM += 5;
      *Flag = true;
      *LastPressTime = millis();    // 最後に押した時刻
      Serial.print("BPM UP! 現在のBPM: "); Serial.println(*BPM);
    }
  }

  // === BPMダウン（D3）===
  if (detectPress(PIN_BPM_DOWN, &downLastReading, &downStableState, &downLastChange)) {
    if (*BPM >= 45) {               // 下限40を下回らない範囲で（-5してもアンダーフローしない）
      *BPM -= 5;
      *Flag = true;
      *LastPressTime = millis();
      Serial.print("BPM DOWN! 現在のBPM: "); Serial.println(*BPM);
    }
  }

  // === BPM変更後の送信処理 ===
  // 最後の押下から1秒以上 何も押されなければ、まとめて1回だけ送信する
  if (*Flag) {
    if (millis() - *LastPressTime > 1000) {
      udp.beginPacket(BCaddress, Port);
      udp.write(*BPM);
      udp.endPacket();
      
      *Flag = false;
      
      // 新しいBPMから小節の時間間隔(Interval)を再計算
      // 1分=60000ミリ秒。4/4拍子なので4倍
      *Interval = (60000 / *BPM) * 4; 
      
      Serial.print("【送信完了】BPM値を全楽器に送信しました。新Interval: ");
      Serial.println(*Interval);
    }
  }
}

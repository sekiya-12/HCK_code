#include "function.h"

// ==========================================
// 小節コントロール (Bar_control)
// ==========================================
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
    Serial.print("小節番号: ");
    Serial.println(*BarCount);
  }
}

// ==========================================
// BPMコントロール (BPM_control)
// ==========================================
void BPM_control(uint8_t *BPM, uint16_t *Interval, uint32_t *LastPressTime, bool *Flag, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp) {
  
  // D2ピン（BPMアップ）
  if (digitalRead(2) == HIGH) { 
    if (*BPM != 255) { // 上限255
      *LastPressTime = millis();
      *BPM += 5;       // BPMを5増やす
      *Flag = true;
      Serial.print("BPM UP! 現在のBPM: "); Serial.println(*BPM);
      delay(200); // 連続入力防止
    }
  }
  
  // D3ピン（BPMダウン）
  if (digitalRead(3) == HIGH) {
    if (*BPM != 40) { // 下限40
      *LastPressTime = millis();
      *BPM -= 5;      // BPMを5減らす
      *Flag = true;
      Serial.print("BPM DOWN! 現在のBPM: "); Serial.println(*BPM);
      delay(200); 
    }
  }

  // BPMが変更された後の処理
  if (*Flag) {
    // スイッチ入力から1秒以上経過したら通信を行う
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
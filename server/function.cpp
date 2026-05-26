#include "function.h"

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
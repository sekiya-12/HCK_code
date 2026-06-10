#include "castanets_client_function.h"

bool Parse_data(uint8_t *Data, char Offset, WiFiUDP &Udp) {
  int packetSize = Udp.parsePacket();
  
  // 1. パケットを受信したかどうかの判定
  if (packetSize) {
    uint8_t received = Udp.read();
    Udp.flush();

    // === 【生存確認用デバッグ】受信した生の数値をそのままProcessingへ送る ===
    Serial.print("RAW_DATA:");
    Serial.println(received);
    // ===================================================================
    
    // 2. 受信データが40以上であればBPM値と判定（フラグONを返す）
    if (received >= 40) {
      *Data = received;
      return true; // フラグON
    }
    // 3. 40未満であれば小節番号と判定（オフセットを適用し，フラグOFFを返す）
    else {
      // サーバーの小節番号とオフセットを足す
      int myBar = received + Offset; 
      
      // マイナスになる場合（まだ自分の出番ではない待機期間）
      if (myBar < 0) {
        *Data = 255; // 待機状態を維持するための無効な値を入れる
      } else {
        *Data = myBar; // 自分の小節番号として適用
      }
      return false; // フラグOFF
    }
  }
  return false;
}

void BPM_update(uint8_t *CurrentBPM, bool Flag, uint8_t Data, float *ToneLength) {
  // フラグがOFF（小節番号）の場合は何もせず終了
  if (!Flag) {
    return;
  } 
  // フラグがON（BPM値）の場合の更新処理
  else {
    *CurrentBPM = Data;
    
    // 基準となる4分音符の長さをミリ秒単位で計算 (60000 / BPM)
    *ToneLength = 60000.0 / (*CurrentBPM);
    
    // 確認用のシリアル出力
    Serial.print("【BPM同期】新しいBPM: ");
    Serial.print(*CurrentBPM);
    Serial.print(" -> 4分音符の基準長さ(ms): ");
    Serial.println(*ToneLength);
  }
}

void Performance(uint8_t Data, uint8_t *CurrentBar, uint8_t *NoteIndex, uint32_t *StartTime, uint16_t *Interval, float ToneLength, Pfm *Score) {
  
  // 1. 新しい小節番号を受信した時のリセット処理
  // （Dataが40未満で、かつ現在演奏中の小節と異なる場合）
  if (Data < 40 && Data != *CurrentBar) {
    *CurrentBar = Data;    // 現在の小節番号を更新
    *NoteIndex = 0;        // 読み込む音符を最初(0番目)に戻す
    *StartTime = millis(); // 演奏開始タイマーをリセット
    *Interval = 0;         // 即座に最初の音が鳴るようにIntervalを0にする
  }

  // フリーズ防止
  if (*CurrentBar >= 40) {
    return; 
  }

  // 2. 音を鳴らすタイミング（時間が経過したか）の判定
  if (millis() - *StartTime >= *Interval) {
    
    // その小節にある音符の数(noteCount)に達するまで処理を行う
    if (*NoteIndex < Score[*CurrentBar].noteCount) {
      
      *StartTime = millis(); // 次の音の基準時間を現在時刻に更新
      
      // 譜面配列から「音の高さ(pitch)」と「音の長さの割合(length)」を取得
      int pitch = Score[*CurrentBar].notes[*NoteIndex].pitch;
      float length = Score[*CurrentBar].notes[*NoteIndex].length;
      
      // 次の音までの間隔（Interval）を計算（ミリ秒）
      *Interval = ToneLength * length;
      
      // PC（Processing）へシリアル通信で送信
      // 形式：「周波数,鳴らすミリ秒」 (例： 262,500)
      Serial.print(pitch);
      Serial.print(",");
      Serial.println(*Interval);
      
      *NoteIndex = *NoteIndex + 1; // 次の音符へ進む
    }
  }
}
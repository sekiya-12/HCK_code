#include "function.h"
#include "myLEDMatrix.h"

// ============================================================
//  スイッチ押下検出ピン
// ============================================================
#define PIN_BPM_UP    2   // BPMアップ
#define PIN_BPM_DOWN  3   // BPMダウン

// ============================================================
//  演奏モード選択ボタン（押下で演奏開始）
// ============================================================
#define PIN_MODE_DEFAULT 4   // デフォルトの音階での演奏（小節番号 0~11 を送信）
#define PIN_MODE_OCTAVE  5   // 1オクターブ上の音階での演奏（小節番号 13~24 を送信）

// ============================================================
//  スポットライトLED（演奏中の人形を照らす）
// ============================================================
#define PIN_LED_MAIN1   6   // メイン楽器1
#define PIN_LED_MAIN2   7   // メイン楽器2
#define PIN_LED_MAIN3   8   // メイン楽器3
#define PIN_LED_RHYTHM  9   // リズム楽器

// ============================================================
//  カーテン開閉モータ（Hブリッジ駆動）
// ============================================================
#define PIN_MOTOR_IN1   10  // 正転（カーテンを開く）
#define PIN_MOTOR_IN2   11  // 逆転（カーテンを閉じる）

// ============================================================
//  デバウンスタイム
// ============================================================
#define DEBOUNCE_MS 30

// ============================================================
//  演奏・舞台のパラメータ（実機に合わせて調整）
// ============================================================
#define BARS_PER_PERFORMANCE 12     // 輪唱1回あたりの小節数（小節 0~11）
#define OFFSET_DEFAULT       0      // デフォルト音階の先頭小節番号
#define OFFSET_OCTAVE        13     // 1オクターブ上の先頭小節番号
#define CURTAIN_MOVE_MS      3000   // カーテンを開閉しきるまでの時間 [ms]
#define WAIT_MS              1000   // 演奏開始前・終了後の待機時間 [ms]
#define MOTOR_SPEED          120    // カーテンモータの速度（ゆっくり） 0~255

// ============================================================
//  演奏フローの状態
//  ボタン待機 → カーテンを開く → 待機 → 演奏 → 待機 → カーテンを閉じる
// ============================================================
enum StageState {
  STAGE_WAIT_BUTTON,   // ボタン押下待機
  STAGE_OPEN_CURTAIN,  // カーテンを開ける
  STAGE_WAIT_BEFORE,   // 演奏開始前の待機
  STAGE_PLAYING,       // 演奏中（小節送信＋スポットライト）
  STAGE_WAIT_AFTER,    // 演奏終了後の待機
  STAGE_CLOSE_CURTAIN  // カーテンを閉じる
};

// ------------------------------------------------------------
//  カーテンモータ制御
// ------------------------------------------------------------
static void curtainOpen() {   // カーテンを開く（正転）
  analogWrite(PIN_MOTOR_IN1, MOTOR_SPEED);
  digitalWrite(PIN_MOTOR_IN2, LOW);
}
static void curtainClose() {  // カーテンを閉じる（逆転）
  digitalWrite(PIN_MOTOR_IN1, LOW);
  analogWrite(PIN_MOTOR_IN2, MOTOR_SPEED);
}
static void curtainStop() {   // モータ停止
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, LOW);
}

// ------------------------------------------------------------
//  スポットライト制御
//  かえるの歌の輪唱：メイン3楽器が2小節ずつずれて各8小節演奏し、
//  リズム楽器は全小節を通して演奏する。演奏中の楽器のみ点灯する。
// ------------------------------------------------------------
static void updateSpotlights(int step) {
  digitalWrite(PIN_LED_MAIN1,  (step <= 7)              ? HIGH : LOW); // 小節 0~7
  digitalWrite(PIN_LED_MAIN2,  (step >= 2 && step <= 9) ? HIGH : LOW); // 小節 2~9
  digitalWrite(PIN_LED_MAIN3,  (step >= 4)              ? HIGH : LOW); // 小節 4~11
  digitalWrite(PIN_LED_RHYTHM, HIGH);                                  // 全小節
}
static void allSpotlightsOff() {  // 全スポットライト消灯
  digitalWrite(PIN_LED_MAIN1,  LOW);
  digitalWrite(PIN_LED_MAIN2,  LOW);
  digitalWrite(PIN_LED_MAIN3,  LOW);
  digitalWrite(PIN_LED_RHYTHM, LOW);
}

// ------------------------------------------------------------
//  舞台・演奏制御の初期化（ピン設定と初期状態）
// ------------------------------------------------------------
void Stage_init() {
  pinMode(PIN_MODE_DEFAULT, INPUT);
  pinMode(PIN_MODE_OCTAVE,  INPUT);

  pinMode(PIN_LED_MAIN1,  OUTPUT);
  pinMode(PIN_LED_MAIN2,  OUTPUT);
  pinMode(PIN_LED_MAIN3,  OUTPUT);
  pinMode(PIN_LED_RHYTHM, OUTPUT);

  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);

  allSpotlightsOff();  // スポットライト消灯
  curtainStop();       // カーテンモータ停止
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

// ------------------------------------------------------------
//  舞台・演奏の状態制御
//  ボタン押下で開始 → カーテンを開く → 1秒待機 →
//  小節番号を送信しながら演奏（スポットライト制御）→
//  1秒待機 → カーテンを閉じる → ボタン待機に戻る
// ------------------------------------------------------------
void Stage_control(uint16_t Interval, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp) {

  // --- 状態機械の内部状態（呼び出しをまたいで保持するため static） ---
  static StageState state     = STAGE_WAIT_BUTTON;
  static uint8_t    offset    = OFFSET_DEFAULT;  // 送信する小節番号のオフセット（演奏モード）
  static uint8_t    step      = 0;               // 演奏中の小節カウンタ(0~11)
  static uint32_t   phaseTime = 0;               // 各フェーズの開始時刻
  static uint32_t   barTime   = 0;               // 直前に小節を送信した時刻

  // --- モード選択ボタンのデバウンス状態（関数をまたいで保持するため static） ---
  static int      defLastReading = LOW, defStableState = LOW;
  static uint32_t defLastChange  = 0;
  static int      octLastReading = LOW, octStableState = LOW;
  static uint32_t octLastChange  = 0;

  uint32_t now = millis();

  switch (state) {

    // === ボタン押下待機（演奏モードの選択）===
    case STAGE_WAIT_BUTTON: {
      bool startDefault = detectPress(PIN_MODE_DEFAULT, &defLastReading, &defStableState, &defLastChange);
      bool startOctave  = detectPress(PIN_MODE_OCTAVE,  &octLastReading, &octStableState, &octLastChange);

      if (startDefault || startOctave) {
        // 押されたボタンに応じて送信する小節番号の範囲を決定
        offset = startOctave ? OFFSET_OCTAVE : OFFSET_DEFAULT;
        Serial.print("演奏開始 モード: ");
        Serial.println(startOctave ? "1オクターブ上" : "デフォルト");

        // カーテンを開け始める
        curtainOpen();
        phaseTime = now;
        state = STAGE_OPEN_CURTAIN;
      }
      break;
    }

    // === カーテンを開ける ===
    case STAGE_OPEN_CURTAIN:
      if (now - phaseTime >= CURTAIN_MOVE_MS) {
        curtainStop();              // 開ききったらモータ停止
        phaseTime = now;
        state = STAGE_WAIT_BEFORE;
        Serial.println("カーテンを開けました");
      }
      break;

    // === 演奏開始前の待機（1秒）===
    case STAGE_WAIT_BEFORE:
      if (now - phaseTime >= WAIT_MS) {
        // 最初の小節を送信して演奏開始
        step = 0;
        udp.beginPacket(BCaddress, Port);
        udp.write((uint8_t)(offset + step));
        udp.endPacket();
        updateSpotlights(step);
        barTime = now;
        state = STAGE_PLAYING;

        Serial.print("送信した小節番号: ");
        Serial.println(offset + step);
      }
      break;

    // === 演奏中（Intervalごとに次の小節を送信）===
    case STAGE_PLAYING:
      if (now - barTime >= Interval) {
        barTime = now;
        step++;

        if (step < BARS_PER_PERFORMANCE) {
          // 次の小節を送信
          udp.beginPacket(BCaddress, Port);
          udp.write((uint8_t)(offset + step));
          udp.endPacket();
          updateSpotlights(step);

          Serial.print("送信した小節番号: ");
          Serial.println(offset + step);
        } else {
          // 全小節の演奏が終了 → スポットライトを全て消す
          allSpotlightsOff();
          phaseTime = now;
          state = STAGE_WAIT_AFTER;
          Serial.println("演奏終了");
        }
      }
      break;

    // === 演奏終了後の待機（1秒）===
    case STAGE_WAIT_AFTER:
      if (now - phaseTime >= WAIT_MS) {
        curtainClose();             // カーテンを閉じ始める
        phaseTime = now;
        state = STAGE_CLOSE_CURTAIN;
      }
      break;

    // === カーテンを閉じる ===
    case STAGE_CLOSE_CURTAIN:
      if (now - phaseTime >= CURTAIN_MOVE_MS) {
        curtainStop();              // 閉じきったらモータ停止
        state = STAGE_WAIT_BUTTON;  // 次の演奏のボタン待機へ戻る
        Serial.println("カーテンを閉じました。ボタン待機に戻ります");
      }
      break;
  }
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
      updateDisplay(*BPM);
      Serial.print("BPM UP! 現在のBPM: "); Serial.println(*BPM);
    }
  }

  // === BPMダウン（D3）===
  if (detectPress(PIN_BPM_DOWN, &downLastReading, &downStableState, &downLastChange)) {
    if (*BPM >= 45) {               // 下限40を下回らない範囲で（-5してもアンダーフローしない）
      *BPM -= 5;
      *Flag = true;
      *LastPressTime = millis();
      updateDisplay(*BPM);
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

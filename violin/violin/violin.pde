// =====================================================================
// 「かえるのうた」 ヴァイオリン・パート（Processing側）― 最小表示版 v4
// ---------------------------------------------------------------------
// クライアントArduino（violin_client_function.cpp の Performance()）から
//   "周波数,鳴らすミリ秒,弾き方\n"   例） 392.00,500,0
// を受信し，ヴァイオリン音色で演奏します。
//
//   弾き方の値（Arduino から届く数値）：
//     0 = 下げ弓（しっかりした弓のストローク）
//     1 = 上げ弓（軽い弓のストローク）   ← 0と1の交互で「弓の上下」を表現
//     2 = デタシェ（テスト用）
//   ※「クヮ」も「ケケケ」も，譜面側で 0/1 を交互に並べてあるので，
//     下げ弓・上げ弓が交互に鳴り，弓を上下に返す質感になります。
//   ※音符の長さに応じて立ち上がりの鋭さを自動調整（速い刻みは鋭く，
//     ゆっくりした旋律は表情豊かに）。
//
// 【必要なライブラリ】 Minim
// 【セットアップ】 portName を自分の環境のポート名に書き換える。
// 【Arduino無しのテスト】
//   1〜6 = ド〜ラ（押すたびに下げ弓→上げ弓を交互），0 = 休符，
//   t = 「ケケケケ」走句（下げ/上げ交互）を自動で鳴らす。
// =====================================================================

import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Minim minim;
AudioOutput out;
Serial port;

Wavetable customViolinWave;

// ★自分の環境のポート名に書き換えてください
String portName = "/dev/cu.usbmodem34B7DA6377FC2";

// ---- 表示用の状態 ----
String  currentNoteName    = "ーー";
boolean currentIsRest      = true;
boolean hasReceivedAnyData = false;

// =====================================================================
// ★音色つまみ（演奏を聞きながらキーで調整できる）
//   金管/ポォー → 弦らしく したいときの目安：
//     ・暗くこもる(ポォー)  → gBright を上げる(q)
//     ・弦の擦れ感が欲しい   → gBowNoise を上げる(w)
//     ・金管っぽい鳴き       → gReso を下げる(d)
//     ・丸い/ホルンっぽい    → gSaw を上げ gBody を下げる(r)
//   キー： q/a=明るさ  w/s=擦れ音  e/d=共振  r/f=ノコギリ⇔胴
//   現在値はコンソールと画面左上に表示。良い値が決まったら教えてください。
// =====================================================================
float gBright   = 6000;   // フィルタ明るさ(Hz)。低いと倍音が消えて純音(ポォー)化する
float gReso     = 0.0;    // フィルタ共振。高い=金管っぽい鳴き／0=素直
float gBowNoise = 0.22;   // 弓の擦れ音の量。高い=弦らしいザラつき
float gSaw      = 0.60;   // ノコギリ波(弦成分=倍音が豊か)の量
float gBody     = 0.20;   // 胴成分(丸み・純音寄り)の量

void setup() {
  size(800, 400);

  minim = new Minim(this);
  out = minim.getLineOut();

  PFont jpFont = findJapaneseFont(28);
  if (jpFont != null) textFont(jpFont);

  // ヴァイオリンらしい倍音波形（violin.pde と同じ作り方）
  float[] harmonics = {0.8729, 1.0000, 0.1919, 0.1815, 0.1828, 0.0367, 0.0338, 0.0533, 0.0968, 0.0600};
  float[] waveData = new float[1024];
  float maxVal = 0;
  for (int i = 0; i < 1024; i++) {
    float t = (float)i / 1024.0 * TWO_PI;
    float val = 0;
    for (int h = 0; h < harmonics.length; h++) val += harmonics[h] * sin((h + 1) * t);
    waveData[i] = val;
    if (abs(val) > maxVal) maxVal = abs(val);
  }
  for (int i = 0; i < 1024; i++) waveData[i] /= maxVal;
  customViolinWave = new Wavetable(waveData);

  println("利用可能なシリアルポート一覧:");
  printArray(Serial.list());
  openSerialPort();
  println("「かえるのうた」ヴァイオリン・パート 準備完了。");
}

void openSerialPort() {
  try {
    port = new Serial(this, portName, 115200);
  } catch (RuntimeException e) {
    println("ポート [" + portName + "] を開けませんでした: " + e.getMessage());
    String[] avail = Serial.list();
    if (avail.length > 0) {
      println("代わりに先頭のポート [" + avail[0] + "] を使用します。");
      port = new Serial(this, avail[0], 115200);
    } else {
      println("利用可能なシリアルポートが見つかりません。");
      return;
    }
  }
  port.bufferUntil('\n');
}

PFont findJapaneseFont(int fontSize) {
  String[] candidates = {
    "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Yu Gothic", "YuGothic",
    "Meiryo", "MS Gothic", "MS PGothic", "Noto Sans CJK JP", "Noto Sans JP",
    "Source Han Sans JP", "IPAGothic", "TakaoGothic"
  };
  String[] available = PFont.list();
  for (String cand : candidates) {
    for (String avail : available) {
      if (avail.toLowerCase().contains(cand.toLowerCase())) {
        return createFont(avail, fontSize, true);
      }
    }
  }
  return null;
}

// ==========================================
// 描画：波形（上半分）＋ 音階（下半分）だけの最小構成
// ==========================================
void draw() {
  background(20, 30, 40);

  updateTestRun(); // テスト走句スケジューラ

  // --- 1. 波形（オシロスコープ）：中心Y=100 ---
  stroke(150, 255, 150);
  strokeWeight(2);
  noFill();
  beginShape();
  for (int i = 0; i < out.bufferSize(); i++) {
    float x = map(i, 0, out.bufferSize() - 1, 0, width);
    vertex(x, 100 + out.left.get(i) * 100);
  }
  endShape();
  strokeWeight(1);

  // --- 2. 音階：画面下半分に大きく表示 ---
  textAlign(CENTER, CENTER);
  fill(currentIsRest ? color(120, 140, 130) : color(255, 244, 214));
  textSize(120);
  String shown = hasReceivedAnyData ? currentNoteName : "ーー";
  text(shown, width / 2, 290);

  // --- 3. 音色つまみの現在値（左上）---
  textAlign(LEFT, TOP);
  fill(180, 200, 220);
  textSize(13);
  text("q/a 明るさ:" + int(gBright)
     + "  w/s 擦れ:" + nf(gBowNoise,1,2)
     + "  e/d 共振:" + nf(gReso,1,2)
     + "  r/f ノコギリ:" + nf(gSaw,1,2) + "/胴:" + nf(gBody,1,2),
       10, 8);
}

// ==========================================
// シリアル通信：3つのデータを受け取る
// ==========================================
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString == null) return;
  inString = trim(inString);
  String[] data = split(inString, ',');
  if (data.length != 3) return; // RAW_DATA: 等のデバッグ行は無視

  float freq = float(data[0]);
  float durationMs = float(data[1]);
  int articulation = int(data[2]);
  triggerNote(freq, durationMs, articulation);
}

void triggerNote(float freq, float durationMs, int articulation) {
  currentNoteName = noteNameFor(freq);
  currentIsRest = (freq <= 0);
  hasReceivedAnyData = true;

  if (freq > 0) {
    float durationSec = durationMs / 1000.0;
    float playDuration = durationSec * 0.85;
    if (playDuration < 0.05) playDuration = 0.05;
    // 音符の長さを合成側に渡し，立ち上がりの鋭さを自動調整
    out.playNote(0, playDuration, new ViolinInstrument(freq, 0.8, articulation, playDuration));
  }
}

String noteNameFor(float freq) {
  if (freq <= 0) return "休符";
  float[]  freqs = {262, 294, 330, 349, 392, 440, 523, 587, 659, 698, 784, 880};
  String[] names = {"ド", "レ", "ミ", "ファ", "ソ", "ラ", "高ド", "高レ", "高ミ", "高ファ", "高ソ", "高ラ"};
  int best = 0;
  float bestDiff = 999999.0;
  for (int i = 0; i < freqs.length; i++) {
    float diff = abs(freq - freqs[i]);
    if (diff < bestDiff) { bestDiff = diff; best = i; }
  }
  return names[best];
}

// ---- テスト用 ----
int   testBow = 0; // 0=下げ弓, 1=上げ弓 を交互に
float[] testRunFreqs;
int     testRunIndex = -1;
int     testRunNextMs = 0;
int     testRunStepMs = 220;

void keyPressed() {
  float[] tf = {262, 294, 330, 349, 392, 440};
  if (key >= '1' && key <= '6') {
    triggerNote(tf[key - '1'], 500, testBow);
    testBow = 1 - testBow;
  } else if (key == '0') {
    triggerNote(0, 500, 0);
  } else if (key == 't' || key == 'T') {
    testRunFreqs = new float[]{262, 262, 294, 294, 330, 330, 349, 349};
    testRunIndex = 0;
    testRunNextMs = millis();
  }
  // ---- 音色つまみ（聞きながら調整）----
  else if (key == 'q') { gBright += 200;                 printTone(); }
  else if (key == 'a') { gBright = max(500, gBright-200); printTone(); }
  else if (key == 'w') { gBowNoise += 0.03;              printTone(); }
  else if (key == 's') { gBowNoise = max(0, gBowNoise-0.03); printTone(); }
  else if (key == 'e') { gReso = min(0.6, gReso+0.03);   printTone(); }
  else if (key == 'd') { gReso = max(0, gReso-0.03);     printTone(); }
  else if (key == 'r') { gSaw += 0.05; gBody = max(0, gBody-0.05); printTone(); } // 弦寄り
  else if (key == 'f') { gSaw = max(0, gSaw-0.05); gBody += 0.05;  printTone(); } // 胴/丸み寄り
}

void printTone() {
  println("[音色] 明るさ gBright=" + int(gBright)
        + "  共振 gReso=" + nf(gReso,1,2)
        + "  擦れ gBowNoise=" + nf(gBowNoise,1,2)
        + "  ノコギリ gSaw=" + nf(gSaw,1,2)
        + "  胴 gBody=" + nf(gBody,1,2));
}

void updateTestRun() {
  if (testRunIndex < 0) return;
  if (millis() >= testRunNextMs) {
    if (testRunIndex < testRunFreqs.length) {
      // 下げ弓(0)→上げ弓(1)を交互に（実際の譜面と同じ）
      triggerNote(testRunFreqs[testRunIndex], testRunStepMs, testRunIndex % 2);
      testRunIndex++;
      testRunNextMs += testRunStepMs;
    } else {
      testRunIndex = -1;
    }
  }
}

void stop() {
  out.close();
  minim.stop();
  super.stop();
}

// ==========================================
// ヴァイオリンクラス（v4）
//   ・ノコギリ波の太さを復活（弦の胴鳴り重視）
//   ・低い音（クヮのC4等）はフィルターを暖かい帯域まで下げて金管化を回避
//   ・弓の擦れ音（PINKノイズ）を全弓使いで維持して弦の質感を確保
//   ・0=下げ弓 / 1=上げ弓 を弾き分け，交互で「弓の上下」を表現
//   ・音符の長さに応じて立ち上がり/余韻を自動調整
// ==========================================
class ViolinInstrument implements Instrument {
  Oscil waveBody, waveString1, waveString2, vibrato;
  Noise bowAttack, bowFriction;
  ADSR attackEnv, frictionEnv, ampEnv;
  Summer mix;
  MoogFilter filter;
  Line pitchSlide;

  ViolinInstrument(float frequency, float amplitude, int art, float noteLenSec) {

    // 短い音ほど立ち上がり・余韻を詰める係数（速い刻みは鋭く，旋律は豊かに）
    float lenF = constrain(noteLenSec / 0.45, 0.40, 1.25);

    // 弦成分(ノコギリ gSaw) ＋ 胴成分(丸み gBody)。つまみで比率を変えられる
    waveBody    = new Oscil(frequency,         amplitude * gBody,      customViolinWave);
    waveString1 = new Oscil(frequency * 1.002, amplitude * gSaw * 0.5, Waves.SAW);
    waveString2 = new Oscil(frequency * 0.998, amplitude * gSaw * 0.5, Waves.SAW);

    // ピッチのしゃくり（下げ弓は強め，上げ弓は軽め）
    float slideTime, slideStart;
    if (art == 1) {            // 上げ弓
      slideTime  = 0.05 * lenF; slideStart = frequency - 7;
    } else if (art == 2) {     // デタシェ
      slideTime  = 0.03 * lenF; slideStart = frequency - 5;
    } else {                   // 下げ弓
      slideTime  = 0.09 * lenF; slideStart = frequency - 11;
    }
    pitchSlide = new Line(slideTime, slideStart, frequency);

    vibrato = new Oscil(5.5, frequency * 0.012, Waves.SINE);
    pitchSlide.patch(vibrato.offset);
    vibrato.patch(waveBody.frequency);
    vibrato.patch(waveString1.frequency);
    vibrato.patch(waveString2.frequency);

    mix = new Summer();
    waveBody.patch(mix);
    waveString1.patch(mix);
    waveString2.patch(mix);

    // フィルタ（つまみ：明るさ gBright・共振 gReso）。固定（スイープ無し）
    filter = new MoogFilter(gBright, gReso, MoogFilter.Type.LP);
    mix.patch(filter);

    // 弓ノイズ・エンベロープ（弾き方ごと）。擦れ音の量は つまみ gBowNoise
    float attackNoiseAmp, frictionNoiseAmp;
    if (art == 1) {            // 上げ弓（軽い）
      attackNoiseAmp   = amplitude * 0.30;
      frictionNoiseAmp = amplitude * gBowNoise;
      attackEnv   = new ADSR(1.0, 0.016 * lenF, 0.04, 0.0,  0.07 * lenF);
      frictionEnv = new ADSR(1.0, 0.025,        0.05, 0.85, 0.09 * lenF);
      ampEnv      = new ADSR(1.0, 0.022 * lenF, 0.06, 0.82, 0.10 * lenF);
    } else if (art == 2) {     // デタシェ
      attackNoiseAmp   = amplitude * 0.34;
      frictionNoiseAmp = amplitude * gBowNoise;
      attackEnv   = new ADSR(1.0, 0.010 * lenF, 0.03, 0.0,  0.05 * lenF);
      frictionEnv = new ADSR(1.0, 0.015,        0.03, 0.90, 0.06 * lenF);
      ampEnv      = new ADSR(1.0, 0.014 * lenF, 0.04, 0.80, 0.06 * lenF);
    } else {                   // 下げ弓（しっかり）
      attackNoiseAmp   = amplitude * 0.38;
      frictionNoiseAmp = amplitude * gBowNoise;
      attackEnv   = new ADSR(1.0, 0.020 * lenF, 0.05, 0.0,  0.10 * lenF);
      frictionEnv = new ADSR(1.0, 0.045,        0.07, 0.88, 0.14 * lenF);
      ampEnv      = new ADSR(1.0, 0.040 * lenF, 0.08, 0.85, 0.16 * lenF);
    }

    bowAttack   = new Noise(attackNoiseAmp,   Noise.Tint.WHITE);
    bowFriction = new Noise(frictionNoiseAmp, Noise.Tint.PINK); // 弓の擦れ音

    bowAttack.patch(attackEnv);
    attackEnv.patch(mix);
    bowFriction.patch(frictionEnv);
    frictionEnv.patch(mix);
    filter.patch(ampEnv);
  }

  void noteOn(float duration) {
    ampEnv.noteOn();
    attackEnv.noteOn();
    frictionEnv.noteOn();
    pitchSlide.activate();
    ampEnv.patch(out);
  }

  void noteOff() {
    ampEnv.noteOff();
    attackEnv.noteOff();
    frictionEnv.noteOff();
    ampEnv.unpatchAfterRelease(out);
  }
}

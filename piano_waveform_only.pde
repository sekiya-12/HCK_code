import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

// ==================================================
// 軸の余白（左に縦軸ラベル、下に横軸ラベルのスペースを確保）
// ==================================================
final int MARGIN_L = 50;   // 左の余白（縦軸ラベル用）
final int MARGIN_B = 30;   // 下の余白（横軸ラベル用）

// グリッド・目盛りの刻み
final float TICK_MS  = 5;    // 横軸（時間）の目盛り間隔 [ms]（キリのいい値）
final float AMP_STEP = 0.1;  // 縦軸（振幅）の目盛り間隔（0.1刻み）

// ==================================================
// シリアル通信
// ==================================================
Serial myPort;

// 前に確認したArduinoのポート番号
// [2] "/dev/cu.usbmodem34B7DA6537F82"
final int PORT_INDEX = 2;

// Arduino側が Serial.begin(115200); なので合わせる
final int BAUD_RATE = 115200;

// ==================================================
// Minim
// ==================================================
Minim minim;
AudioOutput out;

// ==================================================
// 現在の状態
// ==================================================
String currentNote = "REST";
float currentFreq = 0;
int currentDurationMs = 500;
int currentBpm = 120;
String lastReceived = "";

// ==================================================
// テスト再生用
// ==================================================
boolean testPlaying = false;
int testIndex = 0;
int nextNoteTime = 0;

float[] testFreqs = {
  262, 294, 330, 349,
  330, 294, 262, 0,
  330, 349, 392, 440,
  392, 349, 330, 0,
  262, 262, 262, 262,
  262, 294, 330, 349,
  330, 294, 262, 0
};


// ==================================================
// setup
// ==================================================
void setup() {
  size(600, 300);

  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 2048);

  // 軸ラベル用の日本語フォント（macの標準フォント）
  textFont(createFont("Hiragino Sans", 14));

  println("=== Serial Port List ===");
  printArray(Serial.list());

  if (Serial.list().length > PORT_INDEX) {
    String portName = Serial.list()[PORT_INDEX];
    println("接続ポート: " + portName);

    myPort = new Serial(this, portName, BAUD_RATE);
    myPort.bufferUntil('\n');
    myPort.clear();
  } else {
    println("注意: PORT_INDEX が範囲外です．");
    println("Serial.list() の番号を確認してください．");
    println("pキーによるProcessing単体のテスト再生はできます．");
  }

  println("Processing起動完了");
  println("受信形式：周波数,長さms");
  println("例：262,500");
  println("休符：0,500");
  println("pキー：Processing単体でテスト再生");
}


// ==================================================
// draw
// 表示部分は波形だけ
// 黒背景 + 緑波形
// ==================================================
void draw() {
  background(0); // 画面を黒で塗りつぶしてリセット（オシロスコープ風）

  // プロット領域（左に縦軸ラベル用の余白を確保）
  float plotL    = MARGIN_L;      // 縦軸の位置（左端）
  float centerY  = height / 2.0;  // 横軸（時間軸）＝画面の真ん中
  float bufferMs = out.bufferSize() / out.sampleRate() * 1000.0; // 1画面ぶんの時間[ms]

  // マス目（グリッド）を波形の背面に描画
  drawGrid(plotL, centerY, bufferMs);

  // 出力バッファ（out.mix）の波形を描画
  // 振幅が小さいので見やすいように縦方向を拡大して表示
  float gain = height * 1.5f;
  stroke(0, 255, 120); // 波形の色（緑）
  strokeWeight(1.5f);
  noFill();

  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i,     0, out.bufferSize(), plotL, width);
    float x2 = map(i + 1, 0, out.bufferSize(), plotL, width);
    float y1 = centerY - out.mix.get(i)     * gain; // 上がプラスになるように反転
    float y2 = centerY - out.mix.get(i + 1) * gain;
    line(x1, y1, x2, y2);
  }

  // 縦軸・横軸を描画（横軸の目盛りに使う1画面ぶんの時間[ms]も渡す）
  drawAxes(plotL, centerY, bufferMs);

  // Pキーによるテスト再生
  // 表示は波形だけにしつつ，元のテスト再生処理は残す
  if (testPlaying && millis() >= nextNoteTime) {
    playTestNote();

    float beatLength = 60.0 / currentBpm;
    nextNoteTime = millis() + int(beatLength * 1000);

    testIndex++;
  }

  if (testPlaying && testIndex >= testFreqs.length) {
    testPlaying = false;
    currentNote = "REST";
    currentFreq = 0;
  }
 updateC4Test();
 updateC4Record();
}


// ==================================================
// マス目（グリッド）を描画する
// GRID_STEP px 間隔で縦横の細い線を引く
// ==================================================
void drawGrid(float axisX, float centerY, float bufferMs) {
  stroke(35);
  strokeWeight(1);

  // 縦のマス目線（キリのいい時間 TICK_MS[ms] ごと）
  for (float t = 0; t <= bufferMs; t += TICK_MS) {
    float x = map(t, 0, bufferMs, axisX, width);
    line(x, 0, x, height);
  }

  // 横のマス目線（中央線を基準に 振幅 AMP_STEP ごとに上下へ・均等）
  float gain = height * 1.5f; // draw() の波形表示と同じ拡大率
  for (float a = 0; centerY - a * gain >= 0; a += AMP_STEP) {
    line(axisX, centerY - a * gain, width, centerY - a * gain);
    line(axisX, centerY + a * gain, width, centerY + a * gain);
  }
}


// ==================================================
// 縦軸（振幅）・横軸（時間）を描画する
// 横軸は画面の真ん中（centerY）に置く
// ==================================================
void drawAxes(float axisX, float centerY, float bufferMs) {
  stroke(150);
  strokeWeight(1);
  line(axisX, 0, axisX, height);        // 縦軸（左・画面全体）
  line(axisX, centerY, width, centerY); // 横軸（真ん中）

  // 矢印
  fill(150);
  noStroke();
  triangle(axisX, 0, axisX - 4, 9, axisX + 4, 9);                       // 縦軸：上向き
  triangle(width, centerY, width - 9, centerY - 4, width - 9, centerY + 4); // 横軸：右向き

  // 横軸（時間）の目盛り＋数値[ms]（キリのいい TICK_MS ごと）
  textSize(11);
  textAlign(CENTER, TOP);
  for (float t = 0; t <= bufferMs; t += TICK_MS) {
    float x = map(t, 0, bufferMs, axisX, width);
    stroke(150);
    line(x, centerY - 3, x, centerY + 3); // 目盛り線
    if (t > 0) {
      fill(160);
      noStroke();
      text(nf(t, 0, 0), x, centerY + 5);  // 数値[ms]（整数）
    }
  }
  noStroke();

  // 縦軸（振幅）の目盛り＋数値（AMP_STEP＝0.1 ごと・上がプラス）
  float gain = height * 1.5f; // 波形表示と同じ拡大率
  textSize(11);
  textAlign(RIGHT, CENTER);
  fill(160);
  text("0", axisX - 6, centerY);
  for (float a = AMP_STEP; centerY - a * gain >= 0; a += AMP_STEP) {
    float yPos = centerY - a * gain; // プラス側（上）
    float yNeg = centerY + a * gain; // マイナス側（下）
    stroke(150);
    line(axisX - 3, yPos, axisX + 3, yPos); // 目盛り線
    line(axisX - 3, yNeg, axisX + 3, yNeg);
    noStroke();
    fill(160);
    text(nf(a, 0, 1), axisX - 6, yPos);        // 例：0.1
    text("-" + nf(a, 0, 1), axisX - 6, yNeg);  // 例：-0.1
  }

  // 軸ラベル（単位つき）
  fill(220);
  textSize(14);
  textAlign(LEFT, TOP);
  text("振幅", axisX + 6, 4);                 // 縦軸ラベル
  textAlign(RIGHT, BOTTOM);
  text("時間 [ms]", width - 4, centerY - 6);  // 横軸ラベル
}


// ==================================================
// keyPressed
// ==================================================
void keyPressed() {
  if (key == 't' || key == 'T') startC4Test();
  if (key == 'r' || key == 'R') startC4Record();
  if (key == 'p' || key == 'P') {
    testPlaying = true;
    testIndex = 0;
    nextNoteTime = millis();
    println("テスト再生開始");
  }
}


// ==================================================
// テスト再生
// ==================================================
void playTestNote() {
  if (testIndex < testFreqs.length) {
    float freq = testFreqs[testIndex];

    currentFreq = freq;
    currentNote = freqToName(freq);

    if (freq > 0) {
      float beatLength = 60.0 / currentBpm;

      // 透明感ある鍵盤音版
      out.playNote(0, beatLength, new PianoSound(freq, 1.5, beatLength));
    }
  }
}


// ==================================================
// Arduinoから受信したときに呼ばれる
//
// 今のArduino側の基本形式：
// 周波数,長さms
//
// 例：
// 262,500
// 294,500
// 0,500
//
// 0は休符
// ==================================================
void serialEvent(Serial p) {
  String data = p.readStringUntil('\n');

  if (data == null) {
    return;
  }

  data = trim(data);

  if (data.length() == 0) {
    return;
  }

  lastReceived = data;
  println("受信: " + data);

  // Arduino側のデバッグ出力は演奏データではないので無視
  if (data.startsWith("RAW_DATA:")) {
    println("デバッグデータなので無視: " + data);
    return;
  }

  // Wi-Fi接続メッセージなど，カンマがない行は無視
  if (data.indexOf(",") == -1) {
    println("演奏データではないため無視: " + data);
    return;
  }

  String[] values = split(data, ',');

  // ==================================================
  // 設計書3.4.2の形式：音名,BPM（両方対応）
  // 例：C4,120 / REST,120
  // 先頭が数字でない（=音名）ならこちらで処理する。
  // 音長は「1拍ぶん」= 60/BPM 秒として算出する。
  // ==================================================
  if (values.length >= 2 && !isNumeric(trim(values[0]))) {
    String noteName = trim(values[0]);

    int bpm = currentBpm;
    try {
      bpm = int(trim(values[1]));
    }
    catch (Exception e) {
      println("BPMの読み取りに失敗: " + values[1]);
      bpm = currentBpm;
    }
    if (bpm <= 0) bpm = currentBpm;

    float freq = nameToFreq(noteName);          // 音名→周波数
    int durationMs = int(60.0 / bpm * 1000);    // 1拍の長さ

    currentFreq = freq;
    currentDurationMs = durationMs;
    currentBpm = bpm;
    currentNote = (freq <= 0) ? "REST" : noteName;

    playFrequency(freq, durationMs);
    return;
  }

  // ==================================================
  // 今のArduino側の形式：周波数,長さms
  // 例：262,500
  // ==================================================
  if (values.length == 2) {
    float freq = 0;
    int durationMs = 500;

    try {
      freq = float(trim(values[0]));
    }
    catch (Exception e) {
      println("周波数の読み取りに失敗: " + values[0]);
      return;
    }

    try {
      durationMs = int(trim(values[1]));
    }
    catch (Exception e) {
      println("長さmsの読み取りに失敗: " + values[1]);
      durationMs = 500;
    }

    currentFreq = freq;
    currentDurationMs = durationMs;
    currentNote = freqToName(freq);

    playFrequency(freq, durationMs);
    return;
  }

  // ==================================================
  // 将来用：周波数,長さms,BPM
  // 例：262,500,120
  // ==================================================
  if (values.length >= 3) {
    float freq = 0;
    int durationMs = 500;
    int bpm = currentBpm;

    try {
      freq = float(trim(values[0]));
    }
    catch (Exception e) {
      println("周波数の読み取りに失敗: " + values[0]);
      return;
    }

    try {
      durationMs = int(trim(values[1]));
    }
    catch (Exception e) {
      println("長さmsの読み取りに失敗: " + values[1]);
      durationMs = 500;
    }

    try {
      bpm = int(trim(values[2]));
    }
    catch (Exception e) {
      println("BPMの読み取りに失敗: " + values[2]);
      bpm = currentBpm;
    }

    currentFreq = freq;
    currentDurationMs = durationMs;
    currentBpm = bpm;
    currentNote = freqToName(freq);

    playFrequency(freq, durationMs);
    return;
  }

  println("形式が不明なため無視: " + data);
}


// ==================================================
// 周波数を鳴らす
// ==================================================
void playFrequency(float freq, int durationMs) {
  if (freq <= 0) {
    println("休符");
    currentNote = "REST";
    return;
  }

  float durationSec = durationMs / 1000.0;

  println("再生: " + freq + " Hz / " + durationMs + " ms");

  // 透明感ある鍵盤音版
  out.playNote(0, durationSec, new PianoSound(freq, 1.5, durationSec));
}


// ==================================================
// 文字列が数値かどうか（音名 vs 周波数 の判定用）
// ==================================================
boolean isNumeric(String s) {
  if (s == null || s.length() == 0) return false;
  for (int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (!(Character.isDigit(c) || c == '.' || c == '-' || c == '+')) return false;
  }
  return true;
}

// ==================================================
// 音名を周波数に変換（freqToName の逆）
// "REST"/"R" は休符（0Hz）。それ以外は Minim の Frequency を使う。
// 例：C4 -> 261.63Hz
// ==================================================
float nameToFreq(String name) {
  if (name == null) return 0;
  name = trim(name);
  if (name.length() == 0) return 0;
  if (name.equalsIgnoreCase("REST") || name.equalsIgnoreCase("R")) return 0;
  try {
    return Frequency.ofPitch(name).asHz();
  }
  catch (Exception e) {
    println("音名の変換に失敗: " + name);
    return 0;
  }
}

// ==================================================
// 周波数を音名に変換
// ==================================================
String freqToName(float freq) {
  if (freq <= 0) {
    return "REST";
  }

  if (abs(freq - 262) < 3) return "C4";
  if (abs(freq - 277) < 3) return "C#4";
  if (abs(freq - 294) < 3) return "D4";
  if (abs(freq - 311) < 3) return "D#4";
  if (abs(freq - 330) < 3) return "E4";
  if (abs(freq - 349) < 3) return "F4";
  if (abs(freq - 370) < 3) return "F#4";
  if (abs(freq - 392) < 3) return "G4";
  if (abs(freq - 415) < 3) return "G#4";
  if (abs(freq - 440) < 3) return "A4";
  if (abs(freq - 466) < 3) return "A#4";
  if (abs(freq - 494) < 3) return "B4";
  if (abs(freq - 523) < 3) return "C5";
  if (abs(freq - 587) < 3) return "D5";
  if (abs(freq - 659) < 3) return "E5";
  if (abs(freq - 698) < 3) return "F5";
  if (abs(freq - 784) < 3) return "G5";
  if (abs(freq - 880) < 3) return "A5";

  return nf(freq, 0, 1) + "Hz";
}


// ==================================================
// Piano sound ピアノ感強化版 V2
//
// ・整数倍音ではなく，ピアノ弦っぽい少しズレた倍音にする
// ・高い倍音ほど急速に消えるようにする
// ・薄い余韻成分を足す
// ==================================================
class PianoSound implements Instrument {
  Oscil[] toneWaves;
  ADSR[] toneEnvs;

  Oscil[] shineWaves;
  ADSR[] shineEnvs;

  Oscil[] tailWaves;
  ADSR[] tailEnvs;

  float freq;
  float amp;
  float durationSec;

  final int NUM_TONES = 10;
  final int NUM_SHINE = 5;
  final int NUM_TAIL = 3;

  PianoSound(float freq, float amp, float durationSec) {
    this.freq = freq;
    this.amp = amp;
    this.durationSec = durationSec;

    toneWaves = new Oscil[NUM_TONES];
    toneEnvs = new ADSR[NUM_TONES];

    shineWaves = new Oscil[NUM_SHINE];
    shineEnvs = new ADSR[NUM_SHINE];

    tailWaves = new Oscil[NUM_TAIL];
    tailEnvs = new ADSR[NUM_TAIL];

    // --------------------------------------------------
    // 透明感ある鍵盤音版
    // --------------------------------------------------
    float noteAmp = amp * 0.44;
    if (freq >= 500) noteAmp *= 0.88;
    if (freq >= 700) noteAmp *= 0.82;

    // 高音ほど高倍音を減らす
    float highScale = map(freq, 262, 880, 0.88, 0.28);
    highScale = constrain(highScale, 0.28, 0.88);

    // 低音は少しだけ太くする
    float lowScale = map(freq, 262, 880, 1.08, 0.82);
    lowScale = constrain(lowScale, 0.82, 1.08);

    // 少しだけズレた倍音
    float[] toneRatio = {
      0.9992,
      1.0000,
      1.0008,
      2.004,
      3.010,
      4.018,
      5.030,
      6.045,
      8.070,
      10.100
    };

    float[] toneAmp = {
      noteAmp * 0.20 * lowScale,
      noteAmp * 0.34 * lowScale,
      noteAmp * 0.18 * lowScale,
      noteAmp * 0.100 * highScale,
      noteAmp * 0.055 * highScale,
      noteAmp * 0.030 * highScale,
      noteAmp * 0.018 * highScale,
      noteAmp * 0.011 * highScale,
      noteAmp * 0.006 * highScale,
      noteAmp * 0.0035 * highScale
    };

    // 打鍵音を少し丸くする
    float attackTime = map(freq, 262, 880, 0.008, 0.004);
    attackTime = constrain(attackTime, 0.004, 0.008);

    float baseDecay = constrain(durationSec * 1.05, 0.22, 0.95);

    float[] toneDecay = {
      baseDecay * 1.12,
      baseDecay * 1.22,
      baseDecay * 1.08,
      baseDecay * 0.52,
      baseDecay * 0.36,
      baseDecay * 0.25,
      baseDecay * 0.18,
      baseDecay * 0.13,
      baseDecay * 0.09,
      baseDecay * 0.065
    };

    float[] toneSustain = {
      0.0030,
      0.0035,
      0.0028,
      0.00070,
      0.00045,
      0.00028,
      0.00018,
      0.00010,
      0.00006,
      0.00000
    };

    float releaseTime = map(freq, 262, 880, 0.34, 0.14);
    releaseTime = constrain(releaseTime, 0.14, 0.34);

    for (int i = 0; i < NUM_TONES; i++) {
      toneWaves[i] = new Oscil(freq * toneRatio[i], 1.0, Waves.SINE);

      toneEnvs[i] = new ADSR(
        toneAmp[i],
        attackTime,
        toneDecay[i],
        toneSustain[i],
        releaseTime
      );

      toneWaves[i].patch(toneEnvs[i]);
    }

    // --------------------------------------------------
    // きらっとした透明感成分
    // --------------------------------------------------
    float shineScale = map(freq, 262, 880, 0.70, 0.24);
    shineScale = constrain(shineScale, 0.24, 0.70);

    float[] shineRatio = {
      2.414,
      3.018,
      5.027,
      7.080,
      11.120
    };

    float[] shineAmp = {
      noteAmp * 0.013 * shineScale,
      noteAmp * 0.010 * shineScale,
      noteAmp * 0.0065 * shineScale,
      noteAmp * 0.0036 * shineScale,
      noteAmp * 0.0019 * shineScale
    };

    float[] shineDecay = {
      0.115,
      0.090,
      0.065,
      0.045,
      0.032
    };

    for (int i = 0; i < NUM_SHINE; i++) {
      shineWaves[i] = new Oscil(freq * shineRatio[i], 1.0, Waves.SINE);

      shineEnvs[i] = new ADSR(
        shineAmp[i],
        0.006,
        shineDecay[i],
        0.0,
        0.055
      );

      shineWaves[i].patch(shineEnvs[i]);
    }

    // --------------------------------------------------
    // 薄い余韻成分
    // --------------------------------------------------
    float tailScale = map(freq, 262, 880, 0.75, 0.35);
    tailScale = constrain(tailScale, 0.35, 0.75);

    float[] tailRatio = {
      1.0000,
      2.0020,
      3.0060
    };

    float[] tailAmp = {
      noteAmp * 0.030 * tailScale,
      noteAmp * 0.012 * tailScale,
      noteAmp * 0.006 * tailScale
    };

    for (int i = 0; i < NUM_TAIL; i++) {
      tailWaves[i] = new Oscil(freq * tailRatio[i], 1.0, Waves.SINE);

      tailEnvs[i] = new ADSR(
        tailAmp[i],
        0.020,
        constrain(durationSec * 1.20, 0.30, 1.10),
        0.0,
        releaseTime * 1.25
      );

      tailWaves[i].patch(tailEnvs[i]);
    }
  }

  void noteOn(float dur) {
    for (int i = 0; i < NUM_TONES; i++) {
      toneEnvs[i].patch(out);
      toneEnvs[i].noteOn();
    }

    for (int i = 0; i < NUM_SHINE; i++) {
      shineEnvs[i].patch(out);
      shineEnvs[i].noteOn();
    }

    for (int i = 0; i < NUM_TAIL; i++) {
      tailEnvs[i].patch(out);
      tailEnvs[i].noteOn();
    }
  }

  void noteOff() {
    for (int i = 0; i < NUM_TONES; i++) {
      toneEnvs[i].noteOff();
      toneEnvs[i].unpatchAfterRelease(out);
    }

    for (int i = 0; i < NUM_SHINE; i++) {
      shineEnvs[i].noteOff();
      shineEnvs[i].unpatchAfterRelease(out);
    }

    for (int i = 0; i < NUM_TAIL; i++) {
      tailEnvs[i].noteOff();
      tailEnvs[i].unpatchAfterRelease(out);
    }
  }
}

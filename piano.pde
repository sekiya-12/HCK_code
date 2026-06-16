import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

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

  textSize(24);

  println("Processing起動完了");
  println("受信形式：周波数,長さms");
  println("例：262,500");
  println("休符：0,500");
  println("pキー：Processing単体でテスト再生");
}


// ==================================================
// draw
// ==================================================
void draw() {
  background(255);

  fill(0);
  textSize(24);
  text("Piano Part", 230, 70);

  textSize(18);
  text("Press P to test", 220, 115);
  text("Note: " + currentNote, 220, 155);
  text("Freq: " + currentFreq + " Hz", 220, 185);
  text("Duration: " + currentDurationMs + " ms", 180, 215);
  text("BPM: " + currentBpm, 245, 245);

  textSize(12);
  text("Last: " + lastReceived, 80, 280);

  // Pキーによるテスト再生
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
}


// ==================================================
// keyPressed
// ==================================================
void keyPressed() {
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
      out.playNote(0, beatLength, new PianoSound(freq, 0.50, beatLength));
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
  out.playNote(0, durationSec, new PianoSound(freq, 0.50, durationSec));
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
// 前回より大きく変更した点：
// ・整数倍音ではなく，ピアノ弦っぽい少しズレた倍音にする
// ・打鍵直後だけ「コツン」と鳴る高い成分を足す
// ・高い倍音ほど急速に消えるようにする
// ・木の箱っぽい共鳴成分を少しだけ足す
//
// 注意：Processing の合成音だけなので，本物の録音サンプルほどにはならない．
// ただし，前回より「ピアノらしい減衰」と「打鍵感」は強くなる．
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
    // 前回の「ハンマー感」「箱鳴り」を弱める．
    // 機械音っぽさを減らすため，音の立ち上がりを少し丸くし，
    // 高い倍音は薄く短く入れる．

    // 全体音量．大きいとすぐ音割れして機械音になるので控えめ．
    float noteAmp = amp * 0.44;
    if (freq >= 500) noteAmp *= 0.88;
    if (freq >= 700) noteAmp *= 0.82;

    // 高音ほど高倍音を減らす．キンキン感を抑えるため．
    float highScale = map(freq, 262, 880, 0.88, 0.28);
    highScale = constrain(highScale, 0.28, 0.88);

    // 低音は少しだけ太くする．ただし濁らない程度．
    float lowScale = map(freq, 262, 880, 1.08, 0.82);
    lowScale = constrain(lowScale, 0.82, 1.08);

    // 3本弦っぽい薄い揺れ + 少しだけズレた倍音．
    // 完全な整数倍音だと電子音っぽくなるので，ほんの少しだけズラす．
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

    // メイン成分．
    // 高い倍音を入れすぎないことで透明感を出す．
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

    // 打鍵音を丸くするため，Attackは前回より少し遅め．
    float attackTime = map(freq, 262, 880, 0.008, 0.004);
    attackTime = constrain(attackTime, 0.004, 0.008);

    // 透明感を残すため，低い成分は少し長め，高い成分は短め．
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

    // 余韻．短すぎると電子音のブツ切れ感が出るので少し長め．
    float releaseTime = map(freq, 262, 880, 0.34, 0.14);
    releaseTime = constrain(releaseTime, 0.14, 0.34);

    for (int i = 0; i < NUM_TONES; i++) {
      // 透明感重視なので基本はSINEだけ．TRIANGLEは機械音感が出やすいので使わない．
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
    // 打鍵音ではなく，ガラスっぽい薄い響き．
    // 強すぎると鉄琴になるのでかなり小さくする．
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
    // リバーブの代わりに，かなり小さい音量で長く残る成分を足す．
    // これで音の終わりが少し空気っぽくなる．
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

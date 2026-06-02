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
  println("受信形式：周波数,長さms,BPM");
  println("例：262,500,120");
  println("休符：0,500,120");
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

      // 音量を0.55にして少し柔らかくした
      out.playNote(0, beatLength, new PianoSound(freq, 0.55, beatLength));
    }
  }
}


// ==================================================
// Arduinoから受信したときに呼ばれる
//
// Arduino側に合わせた基本形式：
// 周波数,長さms,BPM
//
// 例：
// 262,500,120
// 294,500,120
// 0,500,120
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

  // 起動時メッセージなど，カンマがない行は無視
  if (data.indexOf(",") == -1) {
    println("演奏データではないため無視: " + data);
    return;
  }

  String[] values = split(data, ',');

  // 推奨形式：周波数,長さms,BPM
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
      println("長さの読み取りに失敗: " + values[1]);
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
  }

  // 念のため，古い形式：周波数,BPM にも対応
  // 例：262,120
  // この場合だけProcessing側で長さをBPMから仮計算する
  else if (values.length == 2) {
    float freq = 0;
    int bpm = currentBpm;

    try {
      freq = float(trim(values[0]));
    }
    catch (Exception e) {
      println("周波数の読み取りに失敗: " + values[0]);
      return;
    }

    try {
      bpm = int(trim(values[1]));
    }
    catch (Exception e) {
      println("BPMの読み取りに失敗: " + values[1]);
      bpm = currentBpm;
    }

    int durationMs = int(60000.0 / bpm);

    currentFreq = freq;
    currentDurationMs = durationMs;
    currentBpm = bpm;
    currentNote = freqToName(freq);

    println("注意: 長さmsが送られていないため，BPMから仮計算しました．");
    playFrequency(freq, durationMs);
  }
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

  println("再生: " + freq + " Hz / " + durationMs + " ms / BPM " + currentBpm);

  // 音量を0.55にして少し柔らかくした
  out.playNote(0, durationSec, new PianoSound(freq, 0.55, durationSec));
}


// ==================================================
// Piano sound
// 元のピアノ音に近い構成
// SINE波の基音＋2倍音＋3倍音＋4倍音
// LineではなくADSRで実装
// 少しだけ柔らかく調整した版
// ==================================================
class PianoSound implements Instrument {
  Oscil wave1;
  Oscil wave2;
  Oscil wave3;
  Oscil wave4;

  ADSR env1;
  ADSR env2;
  ADSR env3;
  ADSR env4;

  float freq;
  float amp;
  float durationSec;

  // Attack：
  // ピアノは打鍵した瞬間に音が出るため短め
  final float ATTACK_TIME = 0.003;

  // Decay：
  // 音の長さに合わせて後で決める
  float decayTime;

  // Sustain：
  // ピアノは鳴りっぱなしではないのでかなり低め
  final float SUSTAIN_LEVEL = 0.01;

  // Release：
  // 少しだけ余韻を残す
  final float RELEASE_TIME = 0.12;

  PianoSound(float freq, float amp, float durationSec) {
    this.freq = freq;
    this.amp = amp;
    this.durationSec = durationSec;

    // 元のLineの amp → 0 に近づける
    // ただし少し自然に残るようにする
    decayTime = max(0.06, durationSec * 0.88);

    // 元のPianoSoundと同じSINE波構成
    wave1 = new Oscil(freq,     1.0, Waves.SINE);
    wave2 = new Oscil(freq * 2, 1.0, Waves.SINE);
    wave3 = new Oscil(freq * 3, 1.0, Waves.SINE);
    wave4 = new Oscil(freq * 4, 1.0, Waves.SINE);

    // ADSR
    // 2〜4倍音を少し弱めて，電子音っぽさを減らす
    env1 = new ADSR(
      amp,
      ATTACK_TIME,
      decayTime,
      SUSTAIN_LEVEL,
      RELEASE_TIME
    );

    env2 = new ADSR(
      amp * 0.28,
      ATTACK_TIME,
      decayTime * 0.70,
      SUSTAIN_LEVEL * 0.6,
      RELEASE_TIME
    );

    env3 = new ADSR(
      amp * 0.14,
      ATTACK_TIME,
      decayTime * 0.50,
      SUSTAIN_LEVEL * 0.4,
      RELEASE_TIME * 0.8
    );

    env4 = new ADSR(
      amp * 0.06,
      ATTACK_TIME,
      decayTime * 0.35,
      SUSTAIN_LEVEL * 0.2,
      RELEASE_TIME * 0.6
    );

    wave1.patch(env1);
    wave2.patch(env2);
    wave3.patch(env3);
    wave4.patch(env4);
  }

  void noteOn(float dur) {
    env1.patch(out);
    env2.patch(out);
    env3.patch(out);
    env4.patch(out);

    env1.noteOn();
    env2.noteOn();
    env3.noteOn();
    env4.noteOn();
  }

  void noteOff() {
    env1.noteOff();
    env2.noteOff();
    env3.noteOff();
    env4.noteOff();

    env1.unpatchAfterRelease(out);
    env2.unpatchAfterRelease(out);
    env3.unpatchAfterRelease(out);
    env4.unpatchAfterRelease(out);
  }
}


// ==================================================
// 周波数を表示用の音名に変換
// Arduino側の定義に合わせる
// ==================================================
String freqToName(float freq) {
  int f = round(freq);

  if (f == 0) return "REST";

  if (abs(f - 262) <= 2) return "C4";
  if (abs(f - 294) <= 2) return "D4";
  if (abs(f - 330) <= 2) return "E4";
  if (abs(f - 349) <= 2) return "F4";
  if (abs(f - 392) <= 2) return "G4";
  if (abs(f - 440) <= 2) return "A4";

  if (abs(f - 523) <= 2) return "C5";
  if (abs(f - 587) <= 2) return "D5";
  if (abs(f - 659) <= 2) return "E5";
  if (abs(f - 698) <= 2) return "F5";
  if (abs(f - 784) <= 2) return "G5";
  if (abs(f - 880) <= 2) return "A5";

  return str(f) + "Hz";
}


// ==================================================
// 終了処理
// ==================================================
void stop() {
  if (myPort != null) {
    myPort.stop();
  }

  if (out != null) {
    out.close();
  }

  if (minim != null) {
    minim.stop();
  }

  super.stop();
}

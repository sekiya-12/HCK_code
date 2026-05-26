import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;

Minim minim;
AudioOutput out;

String currentNote = "REST";
int currentBpm = 120;

boolean testPlaying = false;
int testIndex = 0;
int nextNoteTime = 0;

String currentDurationName = "REST";

// Test melody
String[] testScore = {
  "C4", "D4", "E4", "F4",
  "E4", "D4", "C4",
  "E4", "F4", "G4", "A4",
  "G4", "F4", "E4",
  "C4", "C4", "C4", "C4",
  "C4", "C4", "D4", "D4", "E4", "E4", "F4", "F4",
  "E4", "D4", "C4","REST"
};

// 音の長さ
// 2.0 = 二分音符
// 1.0 = 四分音符
// 0.5 = 八分音符
// 0.25 = 十六分音符
float[] testDuration = {
  1.0f, 1.0f, 1.0f, 1.0f,
  1.0f, 1.0f, 2.0f, 
  1.0f, 1.0f, 1.0f, 1.0f, 
  1.0f, 1.0f, 2.0f, 
  2.0f, 2.0f,
  2.0f, 2.0f,
  0.5f, 0.5f, 0.5f, 0.5f,
  0.5f, 0.5f, 0.5f, 0.5f,
  1.0f, 1.0f, 2.0f
};

void setup() {
  size(600, 300);

  minim = new Minim(this);
  out = minim.getLineOut();

  printArray(Serial.list());

  // 自分のArduinoのポート番号に合わせて変更する
  myPort = new Serial(this, Serial.list()[0], 9600);
  myPort.bufferUntil('\n');

  textSize(24);
}

void draw() {
  background(255);

  fill(0);
  textSize(24);
  text("Piano Part", 230, 70);

  textSize(18);
  text("Press P to test", 220, 120);
  text("Note: " + currentNote, 240, 160);
  text("BPM: " + currentBpm, 245, 200);
  text("Length: " + currentDurationName, 220, 240);

  // Pキーによるテスト再生
  if (testPlaying && millis() >= nextNoteTime) {
    float noteLength = playTestNote();

    nextNoteTime = millis() + int(noteLength * 1000);

    testIndex++;
  }

  if (testPlaying && testIndex >= testScore.length) {
    testPlaying = false;
    currentNote = "REST";
    currentDurationName = "REST";
  }
}

void keyPressed() {
  if (key == 'p' || key == 'P') {
    testPlaying = true;
    testIndex = 0;
    nextNoteTime = millis();
  }
}

float playTestNote() {
  if (testIndex < testScore.length) {
    String note = testScore[testIndex];
    float durationRate = getDurationRate(testIndex);

    currentNote = note;
    currentDurationName = durationName(durationRate);

    // BPMから四分音符1個分の時間を求める
    float beatLength = 60.0f / currentBpm;

    // 音符の種類に応じて，実際の音の長さを決める
    float noteLength = beatLength * durationRate;

    if (!note.equals("REST")) {
      float freq = noteToFreq(note);

      out.playNote(0, noteLength, new PianoSound(freq, 0.6));
    }

    return noteLength;
  }

  return 60.0f / currentBpm;
}

float getDurationRate(int index) {
  if (index < testDuration.length) {
    return testDuration[index];
  }

  // 万が一，音名配列と長さ配列の数がずれた場合は四分音符にする
  return 1.0f;
}

String durationName(float rate) {
  if (rate == 2.0f) return "Half note";
  if (rate == 1.0f) return "Quarter note";
  if (rate == 0.5f) return "Eighth note";
  if (rate == 0.25f) return "Sixteenth note";

  return "Custom";
}

void serialEvent(Serial p) {
  String data = p.readStringUntil('\n');

  if (data != null) {
    data = trim(data);

    String[] values = split(data, ',');

    if (values.length == 2) {
      String note = values[0];
      int bpm = int(values[1]);

      currentNote = note;
      currentBpm = bpm;

      // Arduinoから送られた音は，ここでは四分音符として扱う
      float durationRate = 1.0f;
      currentDurationName = durationName(durationRate);

      float beatLength = 60.0f / currentBpm;
      float noteLength = beatLength * durationRate;

      if (!note.equals("REST")) {
        float freq = noteToFreq(note);

        out.playNote(0, noteLength, new PianoSound(freq, 0.6));
      }
    }
  }
}

// Piano sound
class PianoSound implements Instrument {
  Oscil wave1;
  Oscil wave2;
  Oscil wave3;
  Oscil wave4;

  Line env1;
  Line env2;
  Line env3;
  Line env4;

  float freq;
  float amp;

  PianoSound(float freq, float amp) {
    this.freq = freq;
    this.amp = amp;

    wave1 = new Oscil(freq, 0, Waves.SINE);
    wave2 = new Oscil(freq * 2, 0, Waves.SINE);
    wave3 = new Oscil(freq * 3, 0, Waves.SINE);
    wave4 = new Oscil(freq * 4, 0, Waves.SINE);

    env1 = new Line();
    env2 = new Line();
    env3 = new Line();
    env4 = new Line();

    env1.patch(wave1.amplitude);
    env2.patch(wave2.amplitude);
    env3.patch(wave3.amplitude);
    env4.patch(wave4.amplitude);
  }

  void noteOn(float dur) {
    env1.activate(dur, amp, 0);
    env2.activate(dur, amp * 0.35, 0);
    env3.activate(dur, amp * 0.20, 0);
    env4.activate(dur, amp * 0.10, 0);

    wave1.patch(out);
    wave2.patch(out);
    wave3.patch(out);
    wave4.patch(out);
  }

  void noteOff() {
    wave1.unpatch(out);
    wave2.unpatch(out);
    wave3.unpatch(out);
    wave4.unpatch(out);
  }
}

// Convert note name to frequency
float noteToFreq(String note) {
  if (note.equals("C4")) return 261.63;
  if (note.equals("D4")) return 293.66;
  if (note.equals("E4")) return 329.63;
  if (note.equals("F4")) return 349.23;
  if (note.equals("G4")) return 392.00;
  if (note.equals("A4")) return 440.00;
  if (note.equals("B4")) return 493.88;
  if (note.equals("C5")) return 523.25;

  return 440.00;
}

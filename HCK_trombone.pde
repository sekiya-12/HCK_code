import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;
Minim minim;
AudioOutput out;
Waveform currentWaveform;
// --- 表示用変数 ---
String currentNote = "REST";
int currentBpm = 80;
String currentDurationName = "REST";

// --- 配列データ（変数名は維持） ---
String[] melody = {
  "C4", "D4", "E4", "F4",
  "E4", "D4", "C4",
  "E4", "F4", "G4", "A4",
  "G4", "F4", "E4",
  "C4", "C4",
  "C4", "C4",
  "C4", "C4", "D4", "D4", "E4", "E4", "F4", "F4",
  "E4", "D4", "C4"
};

float[] duration = {
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
// 基準音符長（デフォルト: 1.0 = 1拍単位）
float ToneLength = 1.0f;

float[] maxAmp = new float[29];
// 再生中の情報トラッキング
boolean isPlayingSequence = false;
float[] scheduledStartTimes = null;
float playStartTimeSec = 0.0f;
long lastNoteEndMillis = 0;
float currentToneLengthSec = 0.0f; // 現在鳴っている音の秒数
float currentToneLengthBeats = 0.0f; // 現在鳴っている音の拍数

// --- トロンボーンの特性を反映したクラス ---
class HackInstrument implements Instrument
{
  Oscil wave;
  ADSR ampEnv;
  Line freqEnv;
  float baseFreq;
  float maxAmpValue;

  HackInstrument(float frequency, float amplitude, Waveform wf)
  {
    baseFreq = frequency;
    maxAmpValue = amplitude;
    wave = new Oscil(frequency * 0.96f, 1.0, wf);

    // ADSRの引数: (最大音量, アタック秒, ディケイ秒, サステイン比率, リリース秒)
    // 管楽器らしく 0.08秒かけて音が立ち上がり、吹いている間は 80% の音量をキープします
    ampEnv = new ADSR(amplitude, 0.08f, 0.05f, 0.8f, 0.1f);
    
    // 波形の出力を音量エンベロープ（ampEnv）にパッチする
    wave.patch(ampEnv);
    
    freqEnv = new Line();
    freqEnv.patch(wave.frequency);
  }

  void noteOn(float duration)
  {
    ampEnv.noteOn(); // ADSRのトリガー開始
    ampEnv.patch(out); // 出力ポートへ接続
    freqEnv.activate(0.12f, baseFreq * 0.96f, baseFreq);
  }

  void noteOff()
  {
    ampEnv.noteOff(); // 音を消し始める（リリースタイム開始）
    ampEnv.unpatchAfterRelease(out); // 音が完全に消え去った後に自動で接続を解除（ブツ切り防止）
  }
}

void setup()
{
  size(600, 300);
  textSize(24);
  minim = new Minim(this);
  out = minim.getLineOut();
  out.setTempo(currentBpm);
  
  // シリアルポートの設定（エラー防止のためリストが空でないか確認）
  if (Serial.list().length > 0) {
    myPort = new Serial(this, Serial.list()[0], 115200);
    myPort.bufferUntil('\n');
  }
  
  for(int i=0; i<maxAmp.length; i++) maxAmp[i] = 0.15f;
  
  setTromboneWave();
  textSize(24);
}

void setTromboneWave() {
  currentWaveform = WavetableGenerator.gen10(
    4096, 
    new float[] {
  1.0000f, 0.9554f, 0.8980f, 0.8390f, 0.7630f, 
  0.7153f, 0.7132f, 0.6323f, 0.5703f, 0.5641f
}
  );
}

void playSong() {
  out.pauseNotes();
  // BPM に基づいて拍の長さ（秒）を計算
  float beatSec = 60.0f / max(1, currentBpm);
  scheduledStartTimes = new float[melody.length];
  float t = 0.0f; // 再生開始時刻（秒）
  for (int i = 0; i < melody.length; i++) {
    // 1. 次の音に進むための「本来の枠の長さ」（リズムの骨組み）
    float stepDuration = duration[i] * ToneLength * beatSec; 
    
    // 2. 実際に音を鳴らす長さ（ここでは本来の長さの 85% に縮め、15% の隙間・無音を作っています）
    float noteDuration = stepDuration * 0.85f; 
    
    scheduledStartTimes[i] = t;
    // 【変更】第2引数（音の長さ）を、短くした「noteDuration」に変える
    out.playNote(t, noteDuration,
      new HackInstrument(Frequency.ofPitch(melody[i]).asHz() * 0.5f, 
      maxAmp[i], currentWaveform));
      
    // 【変更】次の音への移動時刻は、本来の枠である「stepDuration」分だけ進める
    t += stepDuration;
  }
  // トラッキング用
  playStartTimeSec = millis() / 1000.0f;
  isPlayingSequence = true;
  currentToneLengthSec = (melody.length>0) ? duration[0] * ToneLength * beatSec : 0.0f;
  currentToneLengthBeats = (melody.length>0) ? duration[0] * ToneLength : 0.0f;
  out.resumeNotes();
}

void draw() {
  background(255); // 画面を白（255）で塗りつぶしてリセット

  fill(0);         // 文字の色を黒（0）に設定
  
  // タイトルの表示
  textSize(24);
  text("TrombonePart", 230, 70); // 指定した座標（x:230, y:70）に表示

  // 各種情報の表示
  textSize(18);
  text("Press P to test", 220, 120);              // 操作ガイド
  text("Note: " + currentNote, 240, 160);         // 演奏中の音名
  text("BPM: " + currentBpm, 245, 200);           // 現在のテンポ
  // リアルタイムの音長表示（拍数と秒数）
  String lenStr = "REST";
  if (isPlayingSequence) {
    float elapsed = millis() / 1000.0f - playStartTimeSec;
    int idx = -1;
    for (int i = 0; i < melody.length; i++) {
      float s = scheduledStartTimes[i];
      float d = duration[i] * ToneLength * (60.0f / max(1, currentBpm));
      if (elapsed >= s && elapsed < s + d) { idx = i; break; }
    }
    if (idx >= 0) {
      currentToneLengthBeats = duration[idx] * ToneLength;
      currentToneLengthSec = duration[idx] * ToneLength * (60.0f / max(1, currentBpm));
      lenStr = nf(currentToneLengthBeats, 1, 2) + " beats / " + nf(currentToneLengthSec, 1, 2) + " s";
    } else {
      // シーケンスが終了したかもしれない
      float lastEnd = (melody.length>0) ? (scheduledStartTimes[melody.length-1] + duration[melody.length-1] * ToneLength * (60.0f / max(1, currentBpm))) : 0;
      if (elapsed > lastEnd) { isPlayingSequence = false; currentToneLengthSec = 0; currentToneLengthBeats = 0; }
    }
  }
  // シリアルでの即時再生表示が優先
  if (millis() < lastNoteEndMillis) {
    lenStr = nf(currentToneLengthBeats, 1, 2) + " beats / " + nf(currentToneLengthSec, 1, 2) + " s";
  }
  text("Length: " + lenStr, 220, 240); // 音符の長さ
}

void serialEvent(Serial p) {
  String data = p.readStringUntil('\n');
  if (data != null) {
    data = trim(data);
    String[] values = split(data, ',');
    if (values.length == 4) {
      currentNote = values[0];
      float noteDuration = float(values[1]); // ★追加: 音の長さを取得
      currentBpm = int(values[2]);          // ★変更: インデックスを2に変更
      int rawVelocity = int(values[3]);     // ★追加: 音の強さを取得
      float velocity = rawVelocity / 100.0f * 0.15f; // ★追加: Minim用に音量を0.0〜0.15fに調整

      out.setTempo(currentBpm);
      
      if (!currentNote.equals("REST")) {
        float beatSec = 60.0f / max(1, currentBpm);
        float dsec = 1.0f * ToneLength * beatSec;
        out.playNote(0, dsec, new HackInstrument(Frequency.ofPitch(currentNote).asHz() * 0.5f, 0.15f, currentWaveform));
        // 表示用の更新
        currentToneLengthSec = dsec;
        currentToneLengthBeats = 1.0f * ToneLength;
        lastNoteEndMillis = millis() + (long)(dsec * 1000);
      }
    }
  }
}

void keyPressed() {
  switch (key)
  {
    case '1': currentWaveform = Waves.SINE; break;
    case '2': currentWaveform = Waves.TRIANGLE; break;
    case '3': currentWaveform = Waves.SAW; break;
    case '4': currentWaveform = Waves.SQUARE; break;
    case '6': setTromboneWave(); break;
    case 'p': playSong(); break;
  }
}

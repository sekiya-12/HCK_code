import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;
Minim minim;
AudioOutput out;
Waveform currentWaveform;
// --- 表示用変数 ---
String currentNote = "REST";
int currentBpm = 120;
String currentDurationName = "REST";

// 基準音符長（デフォルト: 1.0 = 1拍単位）
float ToneLength = 1.0f;

float[] maxAmp = new float[29];
long lastNoteEndMillis = 0;
float currentToneLengthSec = 0.0f; // 現在鳴っている音の秒数
float currentToneLengthBeats = 0.0f; // 現在鳴っている音の拍数

// --- トロンボーンの特性を反映したクラス ---
class HackInstrument implements Instrument
{
  Oscil wave;
  MoogFilter filter; 
  Delay delay;
  ADSR ampEnv;
  Line freqEnv;
  float baseFreq;
  float maxAmpValue;

  HackInstrument(float frequency, float amplitude, Waveform wf)
  {
    baseFreq = frequency;
    maxAmpValue = amplitude;
    wave = new Oscil(frequency * 0.96f, 1.0, wf);

    filter = new MoogFilter(1200.0f, 0.2f, MoogFilter.Type.LP);
    delay = new Delay(0.5f, 0.2f, 0.3f, 0.3f);

    // ADSRの引数: (最大音量, アタック秒, ディケイ秒, サステイン比率, リリース秒)
    // 管楽器らしく 0.08秒かけて音が立ち上がり、吹いている間は 80% の音量をキープします
    ampEnv = new ADSR(amplitude, 0.08f, 0.05f, 0.8f, 0.3f);
    
    // 波形の出力を音量エンベロープ（ampEnv）にパッチする
    wave.patch(filter).patch(delay).patch(ampEnv);
    
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
  
  // 確定したポート番号 [2] を指定して初期化
  if (Serial.list().length > 2) {
    myPort = new Serial(this, "dev/cu.usbmodem34B7DA64C6082", 115200);
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

void draw() {
  background(255); // 画面を白（255）で塗りつぶしてリセット

  fill(0);         // 文字の色を黒（0）に設定
  
  // タイトルの表示
  textSize(24);
  text("TrombonePart", 230, 70); // 指定した座標（x:230, y:70）に表示

  // 各種情報の表示
  textSize(18);
  //text("Press P to test", 220, 120);              // 操作ガイド
  text("Note: " + currentNote, 240, 160);         // 演奏中の音名
  text("BPM: " + currentBpm, 245, 200);           // 現在のテンポ
  
  // リアルタイムの音長表示（拍数と秒数）
  String lenStr = "REST";
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
    
    if (values.length == 2) {
      try {
        // 1. 周波数(Hz)とミリ秒を、それぞれ数値として読み込む
        float freq = float(values[0]) / 2.0f; // 周波数を半分にして1オクターブ下げる
        float durationMs = float(values[1]);

        // 2. 周波数が0（休符：REST）より大きいときだけ音を鳴らす
        if (freq > 0) {
          // 3. ミリ秒を1000で割って「秒」に変換する
          float durationSec = durationMs / 1000.0f;

          currentNote = freq + " Hz";
          currentToneLengthSec = durationSec;
          currentToneLengthBeats = durationMs / (60000.0f / currentBpm);
          lastNoteEndMillis = millis() + (long)durationMs;
        
        // 4. HackInstrumentに直接周波数（freq）と秒数を渡す
        out.playNote(0.0f, durationSec, new HackInstrument(freq, 0.15f, currentWaveform));
        }
      }
    catch (Exception e) {
      // 例外が発生した場合は無視して続行
    }
    }
  }
}

import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;
Minim minim;
AudioOutput out;
Waveform currentWaveform;

// --- 表示用変数 ---
String currentNote = "REST";
int currentBpm = -1; // 初期値は未設定にして外部から読み込む
String currentDurationName = "REST";

// 基準音符長（デフォルト: 1.0 = 1拍単位）
float ToneLength = 1.0f;

float[] maxAmp = new float[29];
long lastNoteEndMillis = 0;
float currentToneLengthSec = 0.0f;   // 現在鳴っている音の秒数
float currentToneLengthBeats = 0.0f; // 現在鳴っている音の拍数

// --- カスタネットの特性を反映したクラス ---
// --- カスタネット：Arduino連動 ＆ ユーザー微調整反映版 ---
class WoodPercussion implements Instrument
{
  // 1. 表面の硬い音（アタック成分）
  Noise clickNoise;
  ADSR clickEnv;
  MoogFilter clickFilter; 

  // 2. 木の内部の響き（ボディ成分）
  Noise bodyNoise;
  ADSR bodyEnv;
  MoogFilter bodyFilter;

  // 3. 高域の乾いた成分（理想に近い参考版に合わせて追加）
  Noise dryNoise;
  ADSR dryEnv;
  MoogFilter dryFilter;

  WoodPercussion(float inputFreq, float amplitude)
  {
    // 参考版の音作り：周波数を木の共鳴帯域に寄せ、毎回わずかに揺らす
    float bodyFreq = constrain(inputFreq, 1000.0f, 2600.0f);
    bodyFreq += random(-90.0f, 90.0f);
    float clickFreq = bodyFreq + random(2500.0f, 3600.0f);
    float dryFreq = bodyFreq + random(4200.0f, 5200.0f);
    float ampRand = amplitude * random(0.80f, 1.05f);

    // ①【表面の硬い音】カチッという入りを残す
    clickNoise = new Noise(ampRand * 1.15f, Noise.Tint.WHITE);
    clickEnv = new ADSR(1.0f, 0.0005f, 0.007f, 0.0f, 0.003f);
    clickFilter = new MoogFilter(clickFreq, 0.65f, MoogFilter.Type.BP);
    clickNoise.patch(clickEnv).patch(clickFilter);

    // ②【木の胴鳴り成分】「カッ」だけでなく「コッ」と響く感じを足す
    bodyNoise = new Noise(ampRand * 0.40f, Noise.Tint.PINK);
    bodyEnv = new ADSR(1.0f, 0.001f, 0.060f, 0.0f, 0.012f);
    bodyFilter = new MoogFilter(bodyFreq, 0.85f, MoogFilter.Type.BP);
    bodyNoise.patch(bodyEnv).patch(bodyFilter);

    // ③【高域のパキッとした成分】硬さを残しつつ余韻を作る
    dryNoise = new Noise(ampRand * 0.33f, Noise.Tint.WHITE);
    dryEnv = new ADSR(1.0f, 0.0005f, 0.018f, 0.0f, 0.006f);
    dryFilter = new MoogFilter(dryFreq, 0.55f, MoogFilter.Type.BP);
    dryNoise.patch(dryEnv).patch(dryFilter);
  }

  void noteOn(float duration)
  {
    clickEnv.noteOn();
    bodyEnv.noteOn();
    dryEnv.noteOn();
    clickFilter.patch(out);
    bodyFilter.patch(out);
    dryFilter.patch(out);
  }

  void noteOff()
  {
    clickEnv.noteOff();
    bodyEnv.noteOff();
    dryEnv.noteOff();
    clickFilter.unpatch(out);
    bodyFilter.unpatch(out);
    dryFilter.unpatch(out);
  }
}

void setup()
{
  size(600, 300);
  textSize(24);
  minim = new Minim(this);
  out = minim.getLineOut();
  out.setTempo(currentBpm);
  
  // シリアルポートの初期化処理
  if (Serial.list().length > 2) {
    myPort = new Serial(this, "dev/cu.usbmodem34B7DA64C6082", 115200);
    myPort.bufferUntil('\n');
  }
  
  for(int i=0; i<maxAmp.length; i++) maxAmp[i] = 0.15f;
  
  setCastanetWave();
  textSize(24);
}

// カスタネットの「カチッ」とした乾いたクラック音を再現するため、高次倍音を豊富に含んだ硬い波形を生成
void setCastanetWave() {
  currentWaveform = WavetableGenerator.gen10(
    4096, 
    new float[] { 0.1f, 0.3f, 0.5f, 0.8f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f }
  );
}

void draw() {
  background(255); // 画面を白で塗りつぶしてリセット
  fill(0);         // 文字の色を黒に設定
  
  // パートタイトルの表示
  textSize(24);
  text("CastanetPart", 230, 70); 

  // リアルタイム演奏情報の表示
  textSize(18);
  text("Note: " + currentNote, 240, 160);         // 発音中の周波数
  
  // リアルタイムの打音長表示（拍数と秒数）
  String lenStr = "REST";
  if (millis() < lastNoteEndMillis) {
    lenStr = nf(currentToneLengthBeats, 1, 2) + " beats / " + nf(currentToneLengthSec, 1, 2) + " s";
  }
  text("Length: " + lenStr, 220, 240); 
}

void serialEvent(Serial p) {
  String data = p.readStringUntil('\n');
  if (data != null) {
    data = trim(data);
    String[] values = split(data, ',');
    
    if (values.length == 2) {
      try {
        // 1. 周波数(Hz)とミリ秒をそれぞれ数値としてパース
        // カスタネットらしい抜けの良い高域を引き出すため、送られてきた周波数をそのまま使用
        float freq = float(values[0]); 
        float durationMs = float(values[1]);

        // 2. 周波数が0（休符）より大きいときだけ打音を生成
        if (freq > 0) {
          float durationSec = durationMs / 1000.0f;

          currentNote = freq + " Hz";
          currentToneLengthSec = durationSec;
          currentToneLengthBeats = durationMs / (60000.0f / currentBpm);
          lastNoteEndMillis = millis() + (long)durationMs;
        
          // 3. カスタネット特性を持たせたインストゥルメントオブジェクトを生成して再生
          out.playNote(0.0f, 0.8f, new WoodPercussion(freq, 1.0f));
        }
      }
      catch (Exception e) {
        // データ破損などの例外発生時は安全に処理をスキップ
      }
    }
  }
}

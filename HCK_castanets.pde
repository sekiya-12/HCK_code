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

  WoodPercussion(float inputFreq, float amplitude)
  {
    // ①【表面の硬い音】
    clickNoise = new Noise(amplitude, Noise.Tint.WHITE);
    clickEnv = new ADSR(1.0f, 0.001f, 0.01f, 0.0f, 0.01f);
    // 元の「3300(アタック)と1800(ボディ)」の差分である 1500Hz を足して関係性をキープ
    clickFilter = new MoogFilter(inputFreq + 1500.0f, 0.4f, MoogFilter.Type.BP);
    clickNoise.patch(clickEnv).patch(clickFilter);

    // ②【木材の響き】
    bodyNoise = new Noise(amplitude * 0.7f, Noise.Tint.PINK);
    
    bodyEnv = new ADSR(1.0f, 0.001f, 0.22f, 0.0f, 0.01f);
    // Arduinoから送られてきたHz（inputFreq）をそのままボディの鳴りとして適用
    bodyFilter = new MoogFilter(inputFreq, 0.7f, MoogFilter.Type.BP);
    bodyNoise.patch(bodyEnv).patch(bodyFilter);
  }

  void noteOn(float duration)
  {
    clickEnv.noteOn();   
    bodyEnv.noteOn();   
    clickFilter.patch(out); 
    bodyFilter.patch(out); 
  }

  void noteOff()
  {
    clickEnv.noteOff();  
    bodyEnv.noteOff();  
    clickFilter.unpatch(out); 
    bodyFilter.unpatch(out); 
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
  text("BPM: " + currentBpm, 245, 200);           // 現在のテンポ
  
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

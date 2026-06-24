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
    delay = new Delay(0.2f, 0.3f, true, true);

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
  pixelDensity(1); // 高密度ディスプレイでの自動2倍描画を無効化（従来の見た目に戻す）
  textSize(24);
  minim = new Minim(this);
  out = minim.getLineOut();
  currentBpm = loadBpm();
  out.setTempo(currentBpm);
  
  // シリアルポートの初期化処理
  // 利用可能なポートを表示し、Arduino（usbmodem / usbserial）を自動で探して接続する
  printArray(Serial.list());
  String portName = null;
  for (int i = 0; i < Serial.list().length; i++) {
    String s = Serial.list()[i];
    if (s.indexOf("usbmodem") >= 0 || s.indexOf("usbserial") >= 0) {
      portName = s;
      break;
    }
  }
  if (portName != null) {
    myPort = new Serial(this, portName, 115200);
    myPort.bufferUntil('\n');
    println("Serial connected: " + portName);
  } else {
    println("Arduinoのポートが見つかりません。上のリストから手動で指定してください。");
  }
  
  for(int i=0; i<maxAmp.length; i++) maxAmp[i] = 0.15f;
  
  setTromboneWave();
  textSize(24);
}

void setTromboneWave() {
  currentWaveform = WavetableGenerator.gen10(
    4096, 
    new float[] {
      1.0000f, 0.96f, 0.90f, 0.84f, 0.76f, 
      0.72f, 0.71f, 0.63f, 0.57f, 0.56f
    }
  );
}

// BPMを外部から取得する: 環境変数 HCK_BPM または BPM、
// もしくはスケッチの data フォルダに置いた "bpm.txt" の1行目を読み込む。
int loadBpm() {
  // 1) 環境変数
  try {
    String env = System.getenv("HCK_BPM");
    if (env == null) env = System.getenv("BPM");
    if (env != null) {
      env = trim(env);
      if (env.length() > 0) {
        try { return Integer.parseInt(env); } catch (Exception e) {}
      }
    }
  } catch (Exception e) {
    // ignore
  }

  // 2) data/bpm.txt
  try {
    String[] lines = loadStrings("bpm.txt");
    if (lines != null && lines.length > 0) {
      String s = trim(lines[0]);
      if (s.length() > 0) {
        try { return Integer.parseInt(s); } catch (Exception e) {}
      }
    }
  } catch (Exception e) {
    // ファイルが無ければ無視
  }

  // 3) フォールバック
  return 120;
}

void draw() {
  background(0); // 画面を黒で塗りつぶしてリセット（オシロスコープ風）

  // 中央の基準線（無音時の0レベル）
  stroke(40);
  strokeWeight(1);
  line(0, height / 2, width, height / 2);

  // 出力バッファ（out.mix）の波形を描画
  // 振幅が小さいので見やすいように縦方向を拡大して表示
  float gain = height * 1.5f;
  stroke(0, 255, 120); // 波形の色（緑）
  strokeWeight(1.5f);
  noFill();
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i,     0, out.bufferSize(), 0, width);
    float x2 = map(i + 1, 0, out.bufferSize(), 0, width);
    float y1 = height / 2 + out.mix.get(i)     * gain;
    float y2 = height / 2 + out.mix.get(i + 1) * gain;
    line(x1, y1, x2, y2);
  }
}

void serialEvent(Serial p) {
  String data = p.readStringUntil('\n');
  if (data != null) {
    data = trim(data);
    String[] values = split(data, ',');

    // クライアントの送信形式：「周波数,鳴らすミリ秒,強さ,BPM」 例: 262,500,100,120
    // "RAW_DATA:5" や "【BPM同期】…" などのデバッグ行はカンマ区切りで4つに分かれないので自動的に無視される
    if (values.length >= 4) {
      try {
        // 1. 周波数(Hz)・ミリ秒・強さ・BPMを、それぞれ数値として読み込む
        float freq = float(values[0]) / 2.0f; // 周波数を半分にして1オクターブ下げる
        float durationMs = float(values[1]);
        // values[2] = velocity（強さ）。今回は音量に反映しないため未使用
        int bpmValue = int(float(values[3]));

        // BPMをクライアントに同期（表示と拍数計算用）
        if (bpmValue > 0) {
          currentBpm = bpmValue;
          out.setTempo(currentBpm);
        }

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

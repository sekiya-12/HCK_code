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

// --- 周波数計測用 ---
float measuredFreq = 0.0f;   // 自己相関法で計測した基本周波数(Hz)
float displayedFreq = 0.0f;  // 表示用に平滑化した自己相関周波数(Hz)
float measuredZc = 0.0f;     // ゼロ交差法で計測した基本周波数(Hz)
float displayedZc = 0.0f;    // 表示用に平滑化したゼロ交差周波数(Hz)
float displayedPeak = 0.0f;  // 表示用に平滑化したピーク振幅
float AMP_GATE = 0.01f;      // この音量(RMS)未満は無音とみなし計測しない

// 目標周波数：C4(261.63Hz)を1オクターブ下げた値（本システムはfreq/2で発音）
float TARGET_FREQ = 261.63f / 2.0f; // = 130.815 Hz（C3相当）

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
  // 日本語が「□（豆腐）」にならないよう、日本語対応フォントを設定する
  textFont(pickJapaneseFont(40));
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

  // --- 周波数を計測して画面に表示 ---
  measuredFreq = detectFrequency(out.mix, out.sampleRate());        // 自己相関法
  measuredZc   = detectFreqZeroCross(out.mix, out.sampleRate());    // ゼロ交差法
  float peak   = peakAmplitude(out.mix);                            // ピーク振幅
  if (measuredFreq > 0) {
    // 急な変動を抑えるため指数移動平均で平滑化
    displayedFreq = (displayedFreq <= 0) ? measuredFreq
                                         : displayedFreq * 0.8f + measuredFreq * 0.2f;
    displayedZc   = (displayedZc <= 0 || measuredZc <= 0) ? measuredZc
                                         : displayedZc * 0.8f + measuredZc * 0.2f;
    displayedPeak = displayedPeak * 0.8f + peak * 0.2f;
  } else {
    displayedFreq = 0;
    displayedZc = 0;
    displayedPeak = 0; // 無音
  }
  drawFreqReadout(displayedFreq, displayedZc, displayedPeak);

  // 保存リクエストがあれば、ヒント等を描く前のきれいな画面を保存する
  if (saveRequested) {
    String fn = "capture_" + timestamp() + ".png";
    save(fn); // スケッチフォルダ直下に保存（波形＋周波数のみ）
    lastSavedName = fn;
    savedFlashUntil = millis() + 1500;
    println("画像を保存しました: " + sketchPath(fn));
    saveRequested = false;
  }

  // 操作ヒント（右下）
  pushStyle();
  textAlign(RIGHT, BOTTOM);
  fill(120);
  textSize(14);
  text("S: 画像を保存", width - 10, height - 8);

  // 保存直後のフラッシュ表示
  if (millis() < savedFlashUntil) {
    textAlign(RIGHT, TOP);
    fill(255, 230, 0);
    textSize(16);
    text("保存しました: " + lastSavedName, width - 10, 12);
  }
  popStyle();
}

// 画面左上に各計測値（自己相関・ゼロ交差・ピーク振幅・目標誤差）を表示する
void drawFreqReadout(float fAuto, float fZc, float peak) {
  pushStyle();
  textAlign(LEFT, TOP);

  // 文字が波形と重なっても読めるよう、背面に半透明の暗いパネルを敷く
  noStroke();
  fill(0, 170); // 黒・不透明度170/255（波形がうっすら透ける程度）
  rect(0, 0, 300, (fAuto > 0) ? 104 : 50);

  if (fAuto > 0) {
    // 自己相関法（メイン）
    fill(0, 255, 120);
    textSize(26);
    text(nf(fAuto, 0, 2) + " Hz", 10, 5);

    String name = nearestNoteName(fAuto);
    float cents = centsOff(fAuto);
    fill(200);
    textSize(13);
    text("自己相関  " + name + " (" + sign(cents) + nf(cents, 0, 1) + " cent)", 10, 36);

    // ゼロ交差法
    if (fZc > 0) {
      text("ゼロ交差  " + nf(fZc, 0, 2) + " Hz", 10, 52);
    } else {
      text("ゼロ交差  -- Hz", 10, 52);
    }

    // ピーク振幅（1.0超でクリップ）
    text("ピーク振幅  " + nf(peak, 0, 3) + (peak >= 1.0f ? "（クリップ）" : "（クリップなし）"), 10, 68);

    // 目標値に対する誤差（自己相関法ベース）
    float errHz = fAuto - TARGET_FREQ;
    float errPct = errHz / TARGET_FREQ * 100.0f;
    fill(errPct >= -1.0f && errPct <= 1.0f ? color(0, 255, 120) : color(255, 120, 120));
    text("目標 " + nf(TARGET_FREQ, 0, 2) + "Hz  誤差 "
         + sign(errHz) + nf(errHz, 0, 2) + "Hz / " + sign(errPct) + nf(errPct, 0, 2) + "%",
         10, 84);
  } else {
    fill(120);
    textSize(26);
    text("-- Hz", 10, 5);
    textSize(13);
    text("（無音）", 10, 36);
  }
  popStyle();
}

// 符号付き表示用（+を明示）
String sign(float v) {
  return v >= 0 ? "+" : "";
}

// 自己相関による基本周波数の推定（低音域でも精度が出る）
float detectFrequency(AudioBuffer buf, float sampleRate) {
  int n = buf.size();

  // 1) 音量(RMS)を見て、無音なら計測しない
  float rms = 0;
  for (int i = 0; i < n; i++) {
    float v = buf.get(i);
    rms += v * v;
  }
  rms = sqrt(rms / n);
  if (rms < AMP_GATE) return 0;

  // 2) 探索する周期(ラグ)の範囲：約 50Hz 〜 1500Hz
  int minLag = (int)(sampleRate / 1500.0f);
  int maxLag = (int)(sampleRate / 50.0f);
  if (maxLag > n - 1) maxLag = n - 1;
  if (minLag < 2) minLag = 2;

  // 3) 自己相関が最大になるラグを探す
  float bestCorr = 0;
  int bestLag = -1;
  for (int lag = minLag; lag <= maxLag; lag++) {
    float corr = 0;
    for (int i = 0; i < n - lag; i++) {
      corr += buf.get(i) * buf.get(i + lag);
    }
    if (corr > bestCorr) {
      bestCorr = corr;
      bestLag = lag;
    }
  }
  if (bestLag <= 0) return 0;

  // 4) 放物線補間でサブサンプル精度に補正
  float lag = bestLag;
  if (bestLag > minLag && bestLag < maxLag) {
    float c0 = autocorrAt(buf, bestLag - 1);
    float c1 = autocorrAt(buf, bestLag);
    float c2 = autocorrAt(buf, bestLag + 1);
    float denom = (c0 - 2 * c1 + c2);
    if (abs(denom) > 1e-9) {
      lag = bestLag + 0.5f * (c0 - c2) / denom;
    }
  }
  return sampleRate / lag;
}

// 指定ラグでの自己相関値（補間用ヘルパー）
float autocorrAt(AudioBuffer buf, int lag) {
  int n = buf.size();
  float corr = 0;
  for (int i = 0; i < n - lag; i++) {
    corr += buf.get(i) * buf.get(i + lag);
  }
  return corr;
}

// ゼロ交差法による基本周波数の推定
// 倍音のさざ波を誤検出しないよう、ピーク振幅の±25%のヒステリシス（シュミットトリガ）で
// 立ち上がりエッジのみを数え、ゼロ点を線形補間して最初と最後の交差から平均周期を求める
float detectFreqZeroCross(AudioBuffer buf, float sampleRate) {
  int n = buf.size();
  float peak = peakAmplitude(buf);
  if (peak < AMP_GATE) return 0;

  float hi = peak * 0.25f;   // 上側しきい値
  float lo = -peak * 0.25f;  // 下側しきい値
  boolean wasLow = false;    // 直近で下側しきい値を下回ったか
  float firstCross = -1, lastCross = -1;
  int cycles = 0;

  for (int i = 1; i < n; i++) {
    float v = buf.get(i);
    if (v < lo) wasLow = true;
    if (wasLow && v > hi) {
      // 立ち上がり確定。直前サンプルとの間でゼロ点(=0)を線形補間
      float v0 = buf.get(i - 1);
      float pos = i;
      if (v != v0) pos = (i - 1) + (0 - v0) / (v - v0);
      if (firstCross < 0) {
        firstCross = pos;
      } else {
        lastCross = pos;
        cycles++;
      }
      wasLow = false;
    }
  }
  if (cycles < 1 || lastCross < 0) return 0;
  float periodSamples = (lastCross - firstCross) / cycles;
  if (periodSamples <= 0) return 0;
  return sampleRate / periodSamples;
}

// バッファ内の最大振幅（絶対値）
float peakAmplitude(AudioBuffer buf) {
  int n = buf.size();
  float peak = 0;
  for (int i = 0; i < n; i++) {
    float a = abs(buf.get(i));
    if (a > peak) peak = a;
  }
  return peak;
}

// 周波数に最も近い音名（A4=440の12平均律）
String nearestNoteName(float f) {
  String[] names = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int midi = round(69 + 12 * (log(f / 440.0f) / log(2)));
  int octave = midi / 12 - 1;
  return names[(midi % 12 + 12) % 12] + octave;
}

// 最寄り音からのズレ（セント, ±50が境目）
float centsOff(float f) {
  float midi = 69 + 12 * (log(f / 440.0f) / log(2));
  float nearest = round(midi);
  return (midi - nearest) * 100.0f;
}

// --- 画面（波形＋周波数表示）を画像として保存 ---
// S キー: 現在の画面を PNG でスケッチフォルダに保存する
int savedFlashUntil = 0;        // 「保存しました」表示を消す時刻(millis)
String lastSavedName = "";
boolean saveRequested = false;  // S キーで立てる保存フラグ（実際の保存は draw 内で行う）

void keyPressed() {
  if (key == 's' || key == 'S') {
    saveRequested = true; // 描画タイミングに合わせて draw() 内で保存する
  }
}

// 日本語を表示できるフォントを、この環境にあるものから自動で選ぶ
// （見つからなければ既定フォントにフォールバック）
PFont pickJapaneseFont(int sz) {
  String[] avail = PFont.list();
  // Mac/Win/Linux の代表的な日本語フォント名のキーワード（部分一致で探す）
  String[] keys = {
    "Hiragino", "YuGothic", "Yu Gothic", "Noto Sans CJK", "Noto Sans JP",
    "Noto Serif CJK", "MS Gothic", "ＭＳ", "Meiryo", "メイリオ", "Osaka", "IPAGothic", "TakaoGothic"
  };
  for (String k : keys) {
    for (String a : avail) {
      if (a.toLowerCase().indexOf(k.toLowerCase()) >= 0) {
        println("日本語フォントを使用: " + a);
        return createFont(a, sz, true);
      }
    }
  }
  println("日本語フォントが見つかりませんでした。既定フォントを使用します。");
  return createFont(avail.length > 0 ? avail[0] : "SansSerif", sz, true);
}

// ファイル名用のタイムスタンプ（例: 20260630_071530）
String timestamp() {
  return year()
       + nf(month(), 2) + nf(day(), 2) + "_"
       + nf(hour(), 2) + nf(minute(), 2) + nf(second(), 2);
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

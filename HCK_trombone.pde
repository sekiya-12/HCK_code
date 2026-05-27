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

// 基準音符長（デフォルト: 1.0 = 1拍単位）
float ToneLength = 1.0f;

float[] maxAmp = new float[29];
// 再生中の情報トラッキング
//boolean isPlayingSequence = false;
//float[] scheduledStartTimes = null;
//float playStartTimeSec = 0.0f;
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
  // --- ここから差し替え ---
  println("=== 【確認】PCが認識しているポート一覧 ===");
  printArray(Serial.list());
  println("======================================");
  
  if (Serial.list().length > 0) {
    int portIndex = 2;
    
    String targetPort = Serial.list()[portIndex];
    println("▶️ 現在、次のポートに接続しています: [" + portIndex + "] " + targetPort);
    
    try {
      myPort = new Serial(this, targetPort, 115200);
      myPort.bufferUntil('\n');
      println("成功: ポートを開くことに成功しました。データを待機しています。");
    } catch (Exception e) {
      println("エラー: ポートを開けませんでした。シリアルモニターが開いたままになっていないか確認してください。");
    }
  } else {
    println("エラー: シリアルポートが1つも見つかりません。USBケーブルの接続を確認してください。");
  }
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
    if (values.length == 4) {
      currentNote = values[0];
      float noteDuration = float(values[1]); // ★追加: 音の長さを取得
      currentBpm = int(values[2]);          // ★変更: インデックスを2に変更
      int rawVelocity = int(values[3]);     // ★追加: 音の強さを取得
      float velocity = rawVelocity / 100.0f * 0.15f; // ★追加: Minim用に音量を0.0〜0.15fに調整

      out.setTempo(currentBpm);
      
      if (!currentNote.equals("REST")) {
        float beatSec = 60.0f / max(1, currentBpm);
        float dsec = noteDuration * ToneLength * beatSec;
        out.playNote(0, dsec, new HackInstrument(Frequency.ofPitch(currentNote).asHz() * 0.5f, velocity, currentWaveform));
        // 表示用の更新
        currentToneLengthSec = dsec;
        currentToneLengthBeats = noteDuration * ToneLength;
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
  }
}

import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.analysis.*; 

Minim minim;
AudioOutput out;
Serial port;
FFT fft; 

Wavetable customViolinWave;

void setup() {
  // 画面の縦幅を400に広げ、上下に2つのグラフを表示しやすくします
  size(800, 400); 
  minim = new Minim(this);
  out = minim.getLineOut();
  fft = new FFT(out.bufferSize(), out.sampleRate());

  println("【表情表現モード】時間領域＆周波数領域の同時描画");

  float[] harmonics = {0.8729, 1.0000, 0.1919, 0.1815, 0.1828, 0.0367, 0.0338, 0.0533, 0.0968, 0.0600};
  
  float[] waveData = new float[1024];
  float maxVal = 0;
  for (int i = 0; i < 1024; i++) {
    float t = (float)i / 1024.0 * TWO_PI;
    float val = 0;
    for (int h = 0; h < harmonics.length; h++) {
      val += harmonics[h] * sin((h + 1) * t); 
    }
    waveData[i] = val;
    if (abs(val) > maxVal) maxVal = abs(val);
  }
  for (int i = 0; i < 1024; i++) {
    waveData[i] /= maxVal;
  }
  customViolinWave = new Wavetable(waveData);

  String portName = "/dev/cu.usbmodem34B7DA6377FC2"; 
  port = new Serial(this, portName, 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(20, 30, 40); 
  
  // ------------------------------------------------
  // 1. 時間領域（オシロスコープ）の描画：画面上半分
  // ------------------------------------------------
  stroke(150, 255, 150); // 波形は見やすい明るい緑色
  strokeWeight(2);       // 線を少し太くする
  for(int i = 0; i < out.bufferSize() - 1; i++) {
    // バッファサイズを画面の横幅(width)にマッピングして全体を描画
    float x1 = map(i, 0, out.bufferSize(), 0, width);
    float x2 = map(i + 1, 0, out.bufferSize(), 0, width);
    // Y座標100を中心にして描画
    line(x1, 100 + out.left.get(i) * 100, x2, 100 + out.left.get(i + 1) * 100);
  }

  // ------------------------------------------------
  // 2. 周波数領域（スペクトラムアナライザ）の描画：画面下半分
  // ------------------------------------------------
  fft.forward(out.mix);
  stroke(255, 100, 100); // 棒グラフは赤色
  strokeWeight(1);       // 線の太さを戻す
  for(int i = 0; i < fft.specSize(); i++) {
    float bandHeight = fft.getBand(i) * 10; 
    // 画面の一番下(height)から上に向かって描画
    line(i * 2, height, i * 2, height - bandHeight);
  }
}

// ==========================================
// 【シリアル通信：3つのデータを受け取る】
// ==========================================
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);
    String[] data = split(inString, ',');

    if (data.length == 3) {
      float freq = float(data[0]);
      float durationMs = float(data[1]);
      int articulation = int(data[2]); 
      
      float durationSec = durationMs / 1000.0;
      
      if (freq > 0) {
        float playDuration = durationSec * 0.85;
        if (playDuration < 0.05) playDuration = 0.05; 
        
        out.playNote(0, playDuration, new ViolinInstrument(freq, 0.8, articulation));
      }
    }
  }
}

// ==========================================
// 【完全版：表情豊かなヴァイオリンクラス】
// ==========================================
class ViolinInstrument implements Instrument {
  Oscil waveBody;      
  Oscil waveString1;   
  Oscil waveString2;   
  Oscil vibrato;       
  Noise bowAttack;     
  ADSR attackEnv;      
  Noise bowFriction;   
  ADSR frictionEnv;    
  Summer mix;          
  MoogFilter filter;   
  Line filterSweep;    
  ADSR ampEnv;         
  Line pitchSlide;     
  
  ViolinInstrument(float frequency, float amplitude, int art) {
    
    waveBody = new Oscil(frequency, amplitude * 0.4, customViolinWave);
    waveString1 = new Oscil(frequency * 1.002, amplitude * 0.25, Waves.SAW);
    waveString2 = new Oscil(frequency * 0.998, amplitude * 0.25, Waves.SAW);
    
    pitchSlide = new Line(0.12, frequency - 15, frequency);
    vibrato = new Oscil(6.0, frequency * 0.015, Waves.SINE); 
    pitchSlide.patch(vibrato.offset); 
    vibrato.patch(waveBody.frequency);
    vibrato.patch(waveString1.frequency);
    vibrato.patch(waveString2.frequency);
    
    bowAttack = new Noise(amplitude * 0.6, Noise.Tint.WHITE);
    bowFriction = new Noise(amplitude * 0.08, Noise.Tint.WHITE); 
    
    mix = new Summer();
    waveBody.patch(mix);
    waveString1.patch(mix);
    waveString2.patch(mix);
    
    filter = new MoogFilter(1500, 0.2, MoogFilter.Type.LP);
    mix.patch(filter);
    
    // 【弾き方によるパラメーター分岐】
    if (art == 1) { 
      // STACCATO（短く鋭く）
      attackEnv = new ADSR(1.0, 0.005, 0.05, 0.0, 0.05); 
      frictionEnv = new ADSR(1.0, 0.05, 0.1, 1.0, 0.05);
      ampEnv = new ADSR(1.0, 0.02, 0.1, 0.8, 0.05);      
      filterSweep = new Line(0.1, 4000, 1000);           
    } 
    else if (art == 2) { 
      // DETACHE（細かく連続）
      attackEnv = new ADSR(1.0, 0.01, 0.05, 0.0, 0.08);
      frictionEnv = new ADSR(1.0, 0.08, 0.1, 1.0, 0.1);
      ampEnv = new ADSR(1.0, 0.04, 0.1, 0.8, 0.1);       
      filterSweep = new Line(0.15, 4000, 1500);
    } 
    else { 
      // LEGATO（通常）
      attackEnv = new ADSR(1.0, 0.02, 0.05, 0.0, 0.1);   
      frictionEnv = new ADSR(1.0, 0.1, 0.1, 1.0, 0.2);
      ampEnv = new ADSR(1.0, 0.08, 0.1, 0.8, 0.2);       
      filterSweep = new Line(0.2, 4000, 1500);
    }

    bowAttack.patch(attackEnv);
    attackEnv.patch(mix);
    bowFriction.patch(frictionEnv);
    frictionEnv.patch(mix);
    filterSweep.patch(filter.frequency); 
    filter.patch(ampEnv);
  }

  void noteOn(float duration) {
    ampEnv.noteOn();
    attackEnv.noteOn();  
    frictionEnv.noteOn(); 
    pitchSlide.activate();
    filterSweep.activate(); 
    ampEnv.patch(out);
  }

  void noteOff() {
    ampEnv.noteOff();
    attackEnv.noteOff();
    frictionEnv.noteOff(); 
    ampEnv.unpatchAfterRelease(out);
  }
}

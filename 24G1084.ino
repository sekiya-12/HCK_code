#include <arduinoFFT.h>

const uint16_t FFT_SIZE      = 128; 
const uint8_t  NUM_BANDS     = 32;  
const float    SAMPLING_FREQ = 7350.0; 

float vReal[FFT_SIZE];
float vImag[FFT_SIZE];
ArduinoFFT<float> FFT(vReal, vImag, FFT_SIZE, SAMPLING_FREQ, true);

uint16_t sampleIndex = 0;

void setup() {
  Serial.begin(1000000); 
}

void loop() {
  while (Serial.available() >= 2) {
    uint8_t lowB  = Serial.read();
    uint8_t highB = Serial.read();
    int16_t pcm = (highB << 8) | lowB;
    vReal[sampleIndex] = (float)pcm / 32768.0; 
    vImag[sampleIndex] = 0.0;
    sampleIndex++;

    if (sampleIndex >= FFT_SIZE) {
      FFT.windowing(FFTWindow::Hann, FFTDirection::Forward, true);
      FFT.compute(FFTDirection::Forward);
      FFT.complexToMagnitude();
      sendBands();
      sampleIndex = 0;
    }
  }
}

void sendBands() {
  // 128/2 = 64個のデータを32バンドに分けるので、1バンドあたり2つ
  uint16_t binsPerBand = (FFT_SIZE / 2) / NUM_BANDS;
  float gain = 7.0; 

  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    float sum = 0.0;
    for (uint16_t i = 0; i < binsPerBand; i++) {
      float mag = vReal[band * binsPerBand + i];
      sum += mag * mag;
    }
    float rms = sqrt(sum / binsPerBand) * gain; 
    float db = 20*log(rms)/log(10); 

    uint16_t outVal = constrain((uint16_t)((db + 80.0) * 750.0), 0, 60000);
    Serial.write(lowByte(outVal));
    Serial.write(highByte(outVal));
  }
}
#include <WiFiS3.h>
#include <WiFiUdp.h>

// Wi-Fi設定
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// UDP設定
WiFiUDP Udp;
unsigned int localPort = 50000;  // サーバ側と同じポート番号にする

int beatNumber = 0;
int bpm = 120;

// ピアノ用の譜面配列
// Processing側の testScore と同じ順番にする
const char* pianoScore[] = {
  "C4", "D4", "E4", "F4",
  "E4", "D4", "C4",
  "E4", "F4", "G4", "A4",
  "G4", "F4", "E4",
  "C4", "C4", "C4", "C4",
  "C4", "C4", "D4", "D4", "E4", "E4", "F4", "F4",
  "E4", "D4", "C4", "REST"
};

int scoreLength = sizeof(pianoScore) / sizeof(pianoScore[0]);

void setup() {
  Serial.begin(9600);

  // Wi-Fi接続開始
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(1000);
  }

  // UDP受信開始
  Udp.begin(localPort);
}

void loop() {
  int packetSize = Udp.parsePacket();

  if (packetSize > 0) {
    int data = Udp.read();

    // 通信設計：
    // 40未満 → 拍番号
    // 40以上 → BPM
    if (data >= 40) {
      bpm = data;
    } else {
      beatNumber = data;

      // 拍番号をもとに譜面配列を読む
      int index = beatNumber % scoreLength;
      const char* note = pianoScore[index];

      // Processingへ送信
      // 送信形式：音名,BPM
      Serial.print(note);
      Serial.print(",");
      Serial.println(bpm);
    }
  }
}
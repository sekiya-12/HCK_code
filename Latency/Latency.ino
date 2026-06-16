#include <Arduino.h>

uint32_t pushT[10] = {0};
uint32_t receiveT[10] = {0};
uint8_t p = 0;
uint8_t r = 0;
int p_sf = HIGH;
int r_sf = HIGH;
int p_df = LOW;
int r_df = LOW;

const int server = 2;
const int client = 3;
const int s_out = 4;
const int c_out = 5;

void setup() {
  pinMode(server, INPUT);
  pinMode(client, INPUT);
  Serial.begin(115200);
}

void loop() {
  //読み取って時間取得
  p_df = digitalRead(server);
  r_df = digitalRead(client);
  if (p_df == p_sf && p < 10) {
    pushT[p] = millis();
    p_sf = p_sf == HIGH ? LOW : HIGH;
    p++;
  }
  if (r_df == r_sf && r < 10) {
    receiveT[r] = millis();
    r_sf = r_sf == HIGH ? LOW : HIGH;
    r++;
  }
  //結果表示
  if (p == 9 && r == 9) {
    for (int i = 0; i < 10; i++) {
      Serial.print(pushT[i]);
      Serial.print(",");
      Serial.print(receiveT[i]);
      Serial.print(",");
      Serial.println(receiveT[i] - pushT[i]);
    }
    exit(0);
  }
}

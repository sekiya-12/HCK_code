#include "myLEDMatrix.h"
ArduinoLEDMatrix matrix;
int c, r;
byte frame[8][12] = {
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
byte digits[5][30]{
{ 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
{ 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1 },
{ 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1 },
{ 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1 },
{ 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1 },
};

void initLEDMatrix(){
matrix.begin(); // LEDマトリクスを有効化
matrix.renderBitmap(frame, 8, 12); // 表示
}
void displayDigit(int d, int s_r, int s_c){
// この部分を考える
// 配列frameのs_r行目からs_r+5行目,s_c列目からs_c+3列目を
// 配列digitsの0行目から5行目,d*3列目からd*3+3列目までの値で書き換える
  for(int r=0; r<5; r++){
    for(int c=0; c<3; c++){
      frame[s_r+r][s_c+c] = digits[r][d*3+c];
    }
  }
matrix.renderBitmap(frame, 8, 12); // 表示
}

void updateDisplay(int pulse){
displayDigit( pulse%10, 2, 9); // 一の位の表示
displayDigit( (pulse/10)%10, 2, 5); // 十の位の表示
displayDigit((pulse/100)%10, 2, 1); // 百の位の表示
}

void displayDigitC1(int d, int s_r, int s_c){
  for(int r=0; r<5; r++){
    for(int c=0; c<3; c++){
      frame[s_r+r][s_c+c] = digits[r][d*3+c];
    }
  }
matrix.renderBitmap(frame, 8, 12);
}
void updateDisplayC1(int pulse){
displayDigitC1( int(pulse*100)%10, 2, 9);
displayDigitC1( (int(pulse*100)/10)%10, 2, 5);
displayDigitC1((int(pulse*100)/100)%10, 2, 1);
}
#ifndef MyLEDMatrix_h
#define MyLEDMatrix_h
#include <Arduino.h>
#include "Arduino_LED_Matrix.h"
void initLEDMatrix();
void displayDigit(int d, int s_r, int s_c);
void updateDisplay(int pulse);
void updateDisplayC1(int pulse);
#endif
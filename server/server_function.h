#ifndef SERVER_FUNCTION_H
#define SERVER_FUNCTION_H
#include <Arduino.h>
#include <WiFiUdp.h>
#include "Arduino_LED_Matrix.h"

void Bar_control(uint32_t *LastBarTime, uint16_t Interval, uint8_t *BarCount, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp);

void BPM_control(uint8_t *BPM, uint16_t *Interval, uint32_t *LastPressTime, bool *Flag, IPAddress BCaddress, uint16_t Port, WiFiUDP &udp);

#endif
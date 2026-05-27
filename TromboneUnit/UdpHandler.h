#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include <Arduino.h>
#include <WiFi.h> //
#include <WiFiUdp.h>

class UdpHandler
{
private:
    WiFiUDP Udp;
    byte packetBuffer[1]; // サーバーから届く1Byteを受信するバッファ

    // ★サーバーから届いた「小節番号」と「BPM」をクライアント側に保存しておく変数
    int savedMeasure;
    int savedBpm;

public:
    UdpHandler();
    void init(unsigned int port);
    // UDPを受信し、値を判定して保存する関数。新しい小節番号が来たらtrueを返す
    bool parseUdp(int &measure, int &bpm);
};

#endif
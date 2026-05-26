#include "UdpHandler.h"

UdpHandler::UdpHandler()
{
    savedBpm = 80;     // 初期値としてのBPMを保存
    savedMeasure = -1; // 初期状態の小節番号を保存
}

void UdpHandler::init(unsigned int port)
{
    Udp.begin(port);
}

// サーバーからのUDP信号を判定・保存するコア機能
bool UdpHandler::parseUdp(int &measure, int &bpm)
{
    int packetSize = Udp.parsePacket();

    if (packetSize > 0)
    {
        Udp.read(packetBuffer, 1);
        byte data = packetBuffer[0]; // サーバーからの1Byteデータ

        // 【判定と保存のロジック】
        if (data >= 40)
        {
            // 40〜255であればBPM情報として変数に上書き保存
            savedBpm = data;
            measure = savedMeasure;
            bpm = savedBpm;
            return false; // BPMの更新だけなので、小節リセットはしない
        }
        else
        {
            // 0〜39であれば小節番号として変数に上書き保存
            savedMeasure = data;
            measure = savedMeasure;
            bpm = savedBpm;
            return true; // 新しい小節が始まった合図としてtrueを返す
        }
    }

    // パケットが届いていない時も、現在保存されている最新の値を返す
    measure = savedMeasure;
    bpm = savedBpm;
    return false;
}
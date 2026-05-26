#ifndef SYNTH_TRIGGER_H
#define SYNTH_TRIGGER_H

#include <Arduino.h>

class SynthTrigger
{
public:
    void init(long baudRate);
    // 複数のデータ（音名、長さ、BPM、強さ）をまとめて送信する関数
    void sendNoteData(String note, float duration, int bpm, int velocity);
};

#endif
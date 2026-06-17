#ifndef ENTERTAINMENT_ROBOT_H
#define ENTERTAINMENT_ROBOT_H

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Servo.h>

// =====================
// 楽器選択定義
// =====================
#define PIANO      1
#define TROMBONE   2
#define VIOLIN     3
#define CASTANET   4

// 使う人形に合わせてここを変更
#define INSTRUMENT PIANO

class EntertainmentRobot {
public:
    EntertainmentRobot();
    void begin(const char* ssid, const char* pass, uint16_t port);
    void update();

private:
    // 通信関連
    WiFiUDP _udp;
    uint16_t _localPort;

    // サーボオブジェクト
    Servo _rightArmServo;
    Servo _leftArmServo;
    Servo _extraServo;

    // ピン配置定数
    static const int RIGHT_ARM_PIN = 9;
    static const int LEFT_ARM_PIN  = 11; // 修正済みの10番ピン
    static const int EXTRA_PIN      = 5;

    // サーボ角度定数
    static const int ARM_REST_ANGLE = 90;
    static const int PIANO_RIGHT_PLAY_ANGLE = 70;
    static const int PIANO_LEFT_PLAY_ANGLE  = 110;
    static const int TROMBONE_FORWARD_ANGLE = 65;
    static const int TROMBONE_BACK_ANGLE    = 115;
    static const int VIOLIN_BOW_LEFT_ANGLE  = 65;
    static const int VIOLIN_BOW_RIGHT_ANGLE = 115;
    static const int CASTANET_PLAY_ANGLE    = 70;
    static const int HEAD_CENTER_ANGLE      = 90;
    static const int HEAD_LEFT_ANGLE        = 70;
    static const int HEAD_RIGHT_ANGLE       = 110;
    static const int MOUTH_CLOSE_ANGLE      = 90;
    static const int MOUTH_OPEN_ANGLE       = 60;

    // 状態管理変数
    uint8_t _currentBPM;
    uint8_t _currentBar;
    unsigned long _beatInterval;
    unsigned long _barStartTime;
    unsigned long _nextBeatTime;
    int _beatInBar;
    static const int BEATS_PER_BAR = 4;

    bool _barActive;
    bool _armDirection;

    // サーボパルス（タイマー）管理
    bool _rightArmReturnActive;
    bool _leftArmReturnActive;
    bool _extraReturnActive;
    unsigned long _rightArmReturnTime;
    unsigned long _leftArmReturnTime;
    unsigned long _extraReturnTime;

    // BPM変更エフェクト管理
    bool _emotionActive;
    unsigned long _emotionStartTime;
    unsigned long _emotionEndTime;
    unsigned long _lastEmotionMoveTime;
    bool _emotionDirection;
    static const int EMOTION_BEATS = 4;

    // 内部メソッド
    void setupServos();
    void receiveUdpData();
    void updateBPM(uint8_t newBPM);
    void calculateBeatInterval();
    void startBarMotion(uint8_t barNumber);
    void updateNormalMotion();
    void triggerBeatMotion(int beat);
    void startEmotion();
    void updateEmotionMotion();
    
    void triggerRightArmPulse(int angle, unsigned long pulseTime);
    void triggerLeftArmPulse(int angle, unsigned long pulseTime);
    void triggerExtraPulse(int angle, unsigned long pulseTime);
    void updateServoReturn();
    void resetMainServos();
    void resetEmotionServo();
};

#endif // ENTERTAINMENT_ROBOT_H
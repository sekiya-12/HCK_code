#include "EntertainmentRobot.h"

EntertainmentRobot::EntertainmentRobot() :
    _currentBPM(120), _currentBar(0), _beatInBar(0),
    _barActive(false), _armDirection(false),
    _rightArmReturnActive(false), _leftArmReturnActive(false), _extraReturnActive(false),
    _rightArmReturnTime(0), _leftArmReturnTime(0), _extraReturnTime(0),
    _emotionActive(false), _emotionStartTime(0), _emotionEndTime(0),
    _lastEmotionMoveTime(0), _emotionDirection(false)
{
    calculateBeatInterval();
}

void EntertainmentRobot::begin(const char* ssid, const char* pass, uint16_t port) {
    _localPort = port;
    
    setupServos();
    calculateBeatInterval();
    
    Serial.println("Connecting to Wi-Fi...");
    while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nWi-Fi Connected!");

    _udp.begin(_localPort);

    Serial.println("Entertainment client started");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("UDP port: ");
    Serial.println(_localPort);
}

void EntertainmentRobot::update() {
    receiveUdpData();
    updateNormalMotion();
    updateEmotionMotion();
    updateServoReturn();
}

void EntertainmentRobot::setupServos() {
#if INSTRUMENT == PIANO
    _rightArmServo.attach(RIGHT_ARM_PIN);
    _leftArmServo.attach(LEFT_ARM_PIN);
    _rightArmServo.write(ARM_REST_ANGLE);
    _leftArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == TROMBONE
    _rightArmServo.attach(RIGHT_ARM_PIN);
    _rightArmServo.write(ARM_REST_ANGLE);

#elif INSTRUMENT == VIOLIN
    _rightArmServo.attach(RIGHT_ARM_PIN);
    _extraServo.attach(EXTRA_PIN);
    _rightArmServo.write(ARM_REST_ANGLE);
    _extraServo.write(HEAD_CENTER_ANGLE);

#elif INSTRUMENT == CASTANET
    _rightArmServo.attach(RIGHT_ARM_PIN);
    _extraServo.attach(EXTRA_PIN);
    _rightArmServo.write(ARM_REST_ANGLE);
    _extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}

void EntertainmentRobot::receiveUdpData() {
    int packetSize = _udp.parsePacket();

    if (packetSize > 0) {
        int data = _udp.read();

        while (_udp.available()) {
            _udp.read();
        }

        if (data < 0) {
            return;
        }

        Serial.print("[UDP Received] Raw Data: ");
        Serial.println(data);

        if (data >= 40) {
            updateBPM((uint8_t)data);
        } else {
            startBarMotion((uint8_t)data);
        }
    }
}

void EntertainmentRobot::updateBPM(uint8_t newBPM) {
    if (newBPM == _currentBPM) {
        return;
    }

    _currentBPM = newBPM;
    calculateBeatInterval();

    Serial.print("BPM updated: ");
    Serial.println(_currentBPM);

    startEmotion();
}

void EntertainmentRobot::calculateBeatInterval() {
    _beatInterval = 60000UL / _currentBPM;
}

void EntertainmentRobot::startBarMotion(uint8_t barNumber) {
    if (_barActive && _currentBar == barNumber) {
        return; 
    }

    _currentBar = barNumber;
    _barStartTime = millis();
    _nextBeatTime = _barStartTime;
    _beatInBar = 0;
    _barActive = true;

    Serial.print("Bar received: ");
    Serial.println(_currentBar);
}

void EntertainmentRobot::updateNormalMotion() {
    if (!_barActive) {
        return;
    }

    unsigned long now = millis();

    if (_beatInBar < BEATS_PER_BAR && now >= _nextBeatTime) {
        triggerBeatMotion(_beatInBar);
        _beatInBar++;
        _nextBeatTime += _beatInterval;
    }

    if (_beatInBar >= BEATS_PER_BAR &&
        now - _barStartTime >= _beatInterval * BEATS_PER_BAR) {
        _barActive = false;
    }
}

void EntertainmentRobot::triggerBeatMotion(int beat) {
    unsigned long pulseTime = _beatInterval / 4;

    if (pulseTime < 80)  pulseTime = 80;
    if (pulseTime > 250) pulseTime = 250;

#if INSTRUMENT == PIANO
    if (beat % 2 == 0) {
        triggerRightArmPulse(PIANO_RIGHT_PLAY_ANGLE, pulseTime);
    } else {
        triggerLeftArmPulse(PIANO_LEFT_PLAY_ANGLE, pulseTime);
    }

#elif INSTRUMENT == TROMBONE
    _armDirection = !_armDirection;
    if (_armDirection) {
        _rightArmServo.write(TROMBONE_FORWARD_ANGLE);
    } else {
        _rightArmServo.write(TROMBONE_BACK_ANGLE);
    }

#elif INSTRUMENT == VIOLIN
    _armDirection = !_armDirection;
    if (_armDirection) {
        _rightArmServo.write(VIOLIN_BOW_LEFT_ANGLE);
    } else {
        _rightArmServo.write(VIOLIN_BOW_RIGHT_ANGLE);
    }

#elif INSTRUMENT == CASTANET
    triggerRightArmPulse(CASTANET_PLAY_ANGLE, pulseTime);
#endif
}

void EntertainmentRobot::startEmotion() {
#if INSTRUMENT == VIOLIN || INSTRUMENT == CASTANET
    _emotionActive = true;
    _emotionStartTime = millis();
    _emotionEndTime = _emotionStartTime + _beatInterval * EMOTION_BEATS;
    _lastEmotionMoveTime = 0;
    _emotionDirection = false;
#endif
}

void EntertainmentRobot::updateEmotionMotion() {
    if (!_emotionActive) {
        return;
    }

    unsigned long now = millis();

    if (now >= _emotionEndTime) {
        resetEmotionServo();
        _emotionActive = false;
        return;
    }

    unsigned long emotionInterval = _beatInterval / 2;
    if (emotionInterval < 100) {
        emotionInterval = 100;
    }

    if (now - _lastEmotionMoveTime >= emotionInterval) {
        _lastEmotionMoveTime = now;
        _emotionDirection = !_emotionDirection;

#if INSTRUMENT == VIOLIN
        if (_emotionDirection) {
            _extraServo.write(HEAD_LEFT_ANGLE);
        } else {
            _extraServo.write(HEAD_RIGHT_ANGLE);
        }
#elif INSTRUMENT == CASTANET
        if (_emotionDirection) {
            _extraServo.write(MOUTH_OPEN_ANGLE);
        } else {
            _extraServo.write(MOUTH_CLOSE_ANGLE);
        }
#endif
    }
}

void EntertainmentRobot::triggerRightArmPulse(int angle, unsigned long pulseTime) {
    _rightArmServo.write(angle);
    _rightArmReturnActive = true;
    _rightArmReturnTime = millis() + pulseTime;
}

void EntertainmentRobot::triggerLeftArmPulse(int angle, unsigned long pulseTime) {
    _leftArmServo.write(angle);
    _leftArmReturnActive = true;
    _leftArmReturnTime = millis() + pulseTime;
}

void EntertainmentRobot::triggerExtraPulse(int angle, unsigned long pulseTime) {
    _extraServo.write(angle);
    _extraReturnActive = true;
    _extraReturnTime = millis() + pulseTime;
}

void EntertainmentRobot::updateServoReturn() {
    unsigned long now = millis();

    if (_rightArmReturnActive && now >= _rightArmReturnTime) {
        _rightArmServo.write(ARM_REST_ANGLE);
        _rightArmReturnActive = false;
    }

    if (_leftArmReturnActive && now >= _leftArmReturnTime) {
        _leftArmServo.write(ARM_REST_ANGLE);
        _leftArmReturnActive = false;
    }

    if (_extraReturnActive && now >= _extraReturnTime) {
        resetEmotionServo();
        _extraReturnActive = false;
    }
}

void EntertainmentRobot::resetMainServos() {
#if INSTRUMENT == PIANO
    _rightArmServo.write(ARM_REST_ANGLE);
    _leftArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == TROMBONE
    _rightArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == VIOLIN
    _rightArmServo.write(ARM_REST_ANGLE);
#elif INSTRUMENT == CASTANET
    _rightArmServo.write(ARM_REST_ANGLE);
#endif
}

void EntertainmentRobot::resetEmotionServo() {
#if INSTRUMENT == VIOLIN
    _extraServo.write(HEAD_CENTER_ANGLE);
#elif INSTRUMENT == CASTANET
    _extraServo.write(MOUTH_CLOSE_ANGLE);
#endif
}
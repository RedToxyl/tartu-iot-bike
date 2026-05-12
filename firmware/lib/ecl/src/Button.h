#pragma once
#include <Arduino.h>
#include <functional>
#include <ECL.h>

namespace ECL
{
    class Button
    {
    private:
        uint8_t _pin;
        bool _lastReading;
        bool _currentState;
        unsigned long _lastDebounceTime;
        unsigned long _pressStartTime;
        bool _longPressTriggered;
        bool _initiated;

        unsigned long _debounceDelay = 50;
        unsigned long _longPressDelay = 1000;

        std::function<void()> _onPress = nullptr;
        std::function<void()> _onRelease = nullptr;
        std::function<void()> _onLongPress = nullptr;

        void _loop()
        {
            bool reading = digitalRead(_pin);
            if (reading != _lastReading)
            {
                _lastDebounceTime = millis();
            }

            if ((millis() - _lastDebounceTime) > _debounceDelay)
            {
                if (reading != _currentState)
                {
                    _currentState = reading;

                    if (_currentState == LOW)
                    {
                        _pressStartTime = millis();
                        _longPressTriggered = false;
                        if (_onPress)
                            _onPress();
                    }
                    else
                    {
                        if (_onRelease)
                            _onRelease();
                    }
                }
            }
            if (_currentState == LOW && !_longPressTriggered)
            {
                if ((millis() - _pressStartTime) > _longPressDelay)
                {
                    _longPressTriggered = true;
                    if (_onLongPress)
                        _onLongPress();
                }
            }
            _lastReading = reading;
        }
        inline void _begin()
        {
            _initiated = true;
            pinMode(_pin, INPUT_PULLUP);
            ECL::addToLoop([this]()
                           { this->_loop(); });
        }

    public:
        Button(uint8_t pin) : _pin(pin)
        {
            _lastReading = HIGH;
            _currentState = HIGH;
            _lastDebounceTime = 0;
            _longPressTriggered = false;
        }

        inline void setOnPress(std::function<void()> cb)
        {
            if (!_initiated)
                _begin();
            _onPress = cb;
        }
        inline void setOnRelease(std::function<void()> cb)
        {
            if (!_initiated)
                _begin();
            _onRelease = cb;
        }
        inline void setOnLongPress(std::function<void()> cb)
        {
            if (!_initiated)
                _begin();
            _onLongPress = cb;
        }

        inline bool isPressed()
        {
            if (!_initiated)
                _begin();
            return _currentState;
        }
    };
}
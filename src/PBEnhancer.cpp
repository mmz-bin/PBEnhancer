/*
    MIT License

    Copyright (c) 2024 MOMIZICH

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <Arduino.h>
#include "PBEnhancer.h"

/* public */

PBEnhancer::PBEnhancer(const uint8_t pin, const uint8_t mode, const uint32_t longThreshold, const uint32_t doubleThreshold, const uint32_t debounceTime)
 : PIN(pin), PIN_MODE(mode),
    LONG_THRESHOLD(longThreshold), DOUBLE_THRESHOLD(doubleThreshold), DEBOUNCE_TIME(debounceTime),
    pressTime_(0), releaseTime_(0), lastTransTime_(0), isPressBak_(false), isHandled_(false), isDoubleClickWait_(false), hasOccurred_(0) {
    pinMode(PIN, PIN_MODE);

    //コールバック配列の初期化
    for (CallbackFunc& func : callbacks_) {
        func = nullptr;
    }
}

uint8_t PBEnhancer::getPin() const { return PIN; }

void PBEnhancer::registerCallback(const Event type, const CallbackFunc func) { callbacks_[static_cast<uint8_t>(type)] = func; }

void PBEnhancer::removeCallback(const Event type) { registerCallback(type, nullptr); }

void PBEnhancer::update() {
    hasOccurred_ = 0;

    bool isPress = (PIN_MODE == INPUT_PULLUP) ? !digitalRead(PIN) : digitalRead(PIN);
    uint32_t now = millis();

    if (now - lastTransTime_ >= DEBOUNCE_TIME) {
        if (isPress) { onPress(now); }
        else { onRelease(now); }
    }

    isPressBak_ = isPress; //前回の値を更新

    invoke();
}

bool PBEnhancer::hasOccurred(const Event type) const { return hasOccurred_ & (1 << static_cast<uint8_t>(type)); }

/* private */

void PBEnhancer::onPress(const uint32_t now) {
    emit(Event::PRESSING);

    //立ち上がりエッジのときの処理
    if (!isPressBak_) { onRisingEdge(now); }

    //長押し判定の時間を過ぎたら
    if ((!isHandled_) && (now - pressTime_ > LONG_THRESHOLD)) {
        emit(Event::LONG);
        isHandled_ = true;
    }
}

void PBEnhancer::onRelease(const uint32_t now) {
    emit(Event::RELEASING);

    //立ち下がりエッジのときの処理
    if (isPressBak_) { onFallingEdge(now); }

    //時間を過ぎた&ダブルクリック待ち(再度押されなかったとき)
    if ((isDoubleClickWait_) && (now - releaseTime_ > DOUBLE_THRESHOLD)) {
        emit(Event::SINGLE);
        isDoubleClickWait_ = false;
    }

    isHandled_ = false;
}

void PBEnhancer::onRisingEdge(const uint32_t now) {
    emit(Event::RISING_EDGE);
    emit(Event::CHANGE_INPUT);

    pressTime_ = now; //押し始めた時間を記録
    lastTransTime_ = now;

    //未処理&ダブルクリック待ちのとき
    if ((isDoubleClickWait_) && (!isHandled_)) {
        emit(Event::DOUBLE);
        isHandled_ = true;
    }

    isDoubleClickWait_ = false;
}

void PBEnhancer::onFallingEdge(const uint32_t now) {
    emit(Event::FALLING_EDGE);
    emit(Event::CHANGE_INPUT);

    releaseTime_ = now; //離し始めた時間を記録
    lastTransTime_ = now;
    isDoubleClickWait_ = !isHandled_; //すでに処理されていれば待たない
}

//コールバック関数を呼び出す
void PBEnhancer::invoke() const {
    for (uint8_t i = 0; i < NUM_OF_EVENTS; i++) {
        if ((hasOccurred(static_cast<Event>(i))) && (callbacks_[i] != nullptr)) {
            callbacks_[i]();
        }
    }
}

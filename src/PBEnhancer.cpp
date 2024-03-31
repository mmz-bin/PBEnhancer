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

//public=============================================================================

PBEnhancer::PBEnhancer(uint8_t pin, uint8_t mode, uint32_t longThreshold, uint32_t doubleThreshold)
 : pin_(pin), mode_(mode), longThreshold_(longThreshold), doubleThreshold_(doubleThreshold), pressTime_(0), releaseTime_(0), isPressBak_(false), isHandled_(false), isDoubleClickWait_(false), hasOccurred_(0) {
    pinMode(pin_, mode_);

    //コールバック配列の初期化
    for (CallbackFunc& func : callbacks_) {
        func = nullptr;
    }
}

uint8_t PBEnhancer::getPin() const { return pin_; }

void PBEnhancer::registerCallback(Event type, CallbackFunc func) { callbacks_[getIndex(type)] = func; }

void PBEnhancer::removeCallback(Event type) { registerCallback(type, nullptr); }

void PBEnhancer::update() {
    hasOccurred_ = 0;

    bool isPress = (mode_ == INPUT_PULLUP) ? !digitalRead(pin_) : digitalRead(pin_);
    uint32_t now = millis();

    if (isPress) { onPress(now); }
    else { onRelease(now); }

    isPressBak_ = isPress; //前回の値を更新
}

bool PBEnhancer::hasOccurred(Event type) const { return hasOccurred_ & (1 << getIndex(type)); }

//private============================================================================

void PBEnhancer::onPress(uint32_t now) {
    invoke(Event::PRESSING);

    //立ち上がりエッジのときの処理
    if (!isPressBak_) { onRisingEdge(now); }

    //長押し判定の時間を過ぎたら
    if ((!isHandled_) && (now - pressTime_ > longThreshold_)) {
        invoke(Event::LONG);
        isHandled_ = true;
    }
}

void PBEnhancer::onRelease(uint32_t now) {
    invoke(Event::RELEASING);

    //立ち下がりエッジのときの処理
    if (isPressBak_) { onFallingEdge(now); }

    //時間を過ぎた&ダブルクリック待ち(再度押されなかったとき)
    if ((now - releaseTime_ > doubleThreshold_) && (isDoubleClickWait_)) {
        invoke(Event::SINGLE);
        isDoubleClickWait_ = false;
    }

    isHandled_ = false;
}

void PBEnhancer::onRisingEdge(uint32_t now) {
    invoke(Event::RISING_EDGE);
    invoke(Event::CHANGE_INPUT);

    pressTime_ = now; //押し始めた時間を記録

    //未処理&ダブルクリック待ちのとき
    if ((!isHandled_) && (isDoubleClickWait_)) {
        invoke(Event::DOUBLE);
        isHandled_ = true;
    }
}

void PBEnhancer::onFallingEdge(uint32_t now) {
    invoke(Event::FALLING_EDGE);
    invoke(Event::CHANGE_INPUT);

    releaseTime_ = now; //離し始めた時間を記録
    isDoubleClickWait_ = !isHandled_; //すでに処理されていれば待たない
}

//コールバック関数を呼び出す
void PBEnhancer::invoke(Event type) {
    uint8_t index = getIndex(type);

    hasOccurred_ |= (1 << index);
    if (callbacks_[index] != nullptr) { callbacks_[index](); }
}

uint8_t PBEnhancer::getIndex(Event type) const { return static_cast<uint8_t>(type); }

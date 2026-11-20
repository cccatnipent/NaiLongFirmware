#pragma once
#include "runtime.h"
#define DEG2RAD(x) ((x) * 3.14159265358979323846 / 180.0)
class Oscillator {
    int position_ = 90;

public:
    void Attach(int) { ++motion_runtime.attaches; }
    void Detach() {}
    void SetTrim(int) {}
    void SetA(int) {}
    void SetO(int) {}
    void SetT(int) {}
    void SetPh(double) {}
    void SetLimiter(int) {}
    void DisableLimiter() {}
    int GetPosition() { return position_; }
    void SetPosition(int position) {
        ++motion_runtime.writes;
        if (motion_runtime.cancelled)
            ++motion_runtime.writes_after_cancel;
        position_ = position;
    }
    void Refresh() { SetPosition(position_); }
};

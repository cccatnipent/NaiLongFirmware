#include <cassert>
#include "otto_movements.h"
#include "runtime.h"

int main() {
    Otto otto;
    otto.Init(17, 39, 18, 38);
    assert(motion_runtime.attaches == 4);
    otto.SetCancelCheck([] { return motion_runtime.cancelled; });
    auto interrupt = [&](auto operation) {
        motion_runtime.cancelled = false;
        motion_runtime.writes_after_cancel = 0;
        motion_runtime.on_delay = [] { motion_runtime.cancelled = true; };
        const auto started = motion_runtime.microseconds;
        operation();
        assert(motion_runtime.cancelled);
        assert(motion_runtime.microseconds - started <= 10000);
        assert(motion_runtime.writes_after_cancel == 0);
        assert(motion_runtime.attaches == 4);
    };
    interrupt([&] { otto.Walk(100, 700, FORWARD); });
    interrupt([&] { otto.Bend(100, 2000, LEFT); });
    interrupt([&] { otto.ShakeLeg(100, 2000, RIGHT); });
    interrupt([&] { otto.DelayTicks(pdMS_TO_TICKS(10000)); });
    int target[SERVO_COUNT] = {180, 180, 180, 180, 90, 90};
    interrupt([&] { otto.MoveServos(1000, target); });
    interrupt([&] { otto.Home(); });
    assert(!otto.GetRestState());  // Interrupted homing must not claim completion.

    // The persistent worker installs a fresh generation before queued HOME.
    motion_runtime.cancelled = false;
    motion_runtime.on_delay = nullptr;
    otto.Home();
    assert(otto.GetRestState() && motion_runtime.attaches == 4);
    assert(motion_runtime.writes_after_cancel == 0);
    const auto started = motion_runtime.microseconds;
    otto.SetCancelCheck({});  // Stock behavior keeps the original delay duration.
    otto.DelayTicks(pdMS_TO_TICKS(1000));
    assert(motion_runtime.microseconds - started == 1000000);
}

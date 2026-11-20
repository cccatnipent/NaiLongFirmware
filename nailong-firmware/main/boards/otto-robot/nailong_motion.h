#pragma once

namespace NailongMotion {
enum class Status { Idle, Pending, Running, Completed, Cancelled };

// Bound motion to the local laugh's duration, with room for returning home.
inline bool ValidRequest(int steps, int speed, int direction) {
    return steps >= 1 && steps <= 7 && speed >= 300 && speed <= 1500 &&
           (direction == 1 || direction == -1) && steps * speed <= 4500;
}
inline bool Active(Status status) { return status == Status::Pending || status == Status::Running; }

// All servo access stays on the existing Otto action task. Start never waits
// for queue space and refuses an already busy controller.
bool TryStart(int steps, int speed, int direction);
Status GetStatus();
void Cancel();
void StopAll();
bool IsMoving();
}  // namespace NailongMotion

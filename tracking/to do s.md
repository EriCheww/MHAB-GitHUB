# To-Do List:

## 1. Motor Control Module (`MotorControls.cpp` / `MotorControls.h`)
- Implement **PD (Proportional–Derivative) control** for smoother motion
- Ensure motor commands are sent at a **lower frequency than `cb_detect`**
- Design a method to **meaningfully utilise high-frequency `cb_detect` output**
  - e.g. filtering, buffering, or averaging inputs
- Maybe Implement **power management**
  - Evaluate if powering off vs holding position is more energy efficient
  - Idle mode vs full stop

---

## 2. Integrate Motor Controls into `Sun_Tracking.cpp`
- Integrate motor control logic into state machine:
  - `SEARCHING` state
  - `TRACKING` state
  - `IDLE` state
- Ensure smooth transition between:
  - Detection → Control → Motion
- Handle edge cases:
  - Lost target → recovery behavior
  - Invalid `targetIndex`

---

## 3. Camera Selection
- Evaluate and decide on camera type:
  - CSI vs USB
  - Hardware encoding support

---

## 4. External Communication for Multi-Target Control
- Find a way to recieve a signal from data storage or scientific camera to controll mutliple target location switching.
- or should we just hardcode timer?

---

## 5. Store temp previous results
- Used for State `RECOVERY` method to determine gradual or sudden tracking loss.
- Need to actually code that method
- Need to actually code the temp storage

---

## 6. Store temp previous results
- Clean up Sun_Tracking.cpp

---

## Maybes? 
- Logging system for debugging (tracking + motor commands)

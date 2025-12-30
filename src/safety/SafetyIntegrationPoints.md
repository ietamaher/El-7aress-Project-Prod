# SafetyInterlock Integration Points

This document identifies all code locations where safety logic currently exists and would need to delegate to the new `SafetyInterlock` class.

## Overview

Current safety logic is distributed across **14 files** with **50+ safety check locations**. The SafetyInterlock class will centralize these into a single authority.

---

## CRITICAL INTEGRATION POINTS (Phase 1)

### 1. GimbalMotionModeBase - Motion Safety Gate

**File:** `src/controllers/motion_modes/gimbalmotionmodebase.cpp:375-391`

**Current Implementation:**
```cpp
bool GimbalMotionModeBase::checkSafetyConditions(GimbalController* controller)
{
    // ...
    SystemStateData data = controller->systemStateModel()->data();

    bool deadManSwitchOk = true;
    if (controller->currentMotionModeType() == MotionMode::Manual ||
        controller->currentMotionModeType() == MotionMode::AutoTrack) {
        deadManSwitchOk = data.deadManSwitchActive;
    }

    return data.stationEnabled &&
           !data.emergencyStopActive && deadManSwitchOk;
}
```

**Target Integration:**
```cpp
bool GimbalMotionModeBase::checkSafetyConditions(GimbalController* controller)
{
    if (!controller || !controller->safetyInterlock()) {
        return false;
    }

    SafetyDenialReason reason;
    // Pass 0,0 velocity for general "can we move at all" check
    bool canMove = controller->safetyInterlock()->canMove(0.0, 0.0, &reason);

    if (!canMove) {
        qDebug() << "[Safety] Motion denied:"
                 << SafetyInterlock::denialReasonToString(reason);
    }

    return canMove;
}
```

**Called From:**
- `manualmotionmode.cpp:45`
- `radarslewmotionmode.cpp:58`
- `trackingmotionmode.cpp:153` (indirectly via deadManSwitch check)

---

### 2. WeaponController - Emergency Stop Handling

**File:** `src/controllers/weaponcontroller.cpp:148-182`

**Current Implementation:**
```cpp
if (newData.emergencyStopActive != m_oldState.emergencyStopActive) {
    if (newData.emergencyStopActive && !m_wasInEmergencyStop) {
        qInfo() << ">>> EMERGENCY STOP ACTIVATED - Aborting all weapon operations";
        abortChargingSequence();
        m_wasInEmergencyStop = true;
        emit emergencyStopActivated();
    } else if (!newData.emergencyStopActive && m_wasInEmergencyStop) {
        qInfo() << ">>> EMERGENCY STOP RELEASED - Resuming normal operation";
        m_wasInEmergencyStop = false;
        emit emergencyStopReleased();
    }
}
```

**Target Integration:**
```cpp
// Connect to SafetyInterlock signal instead of polling SystemStateData
connect(m_safetyInterlock, &SafetyInterlock::emergencyStopChanged,
        this, &WeaponController::onEmergencyStopChanged);

void WeaponController::onEmergencyStopChanged(bool active)
{
    if (active) {
        qInfo() << ">>> EMERGENCY STOP ACTIVATED - Aborting all weapon operations";
        abortChargingSequence();
        emit emergencyStopActivated();
    } else {
        qInfo() << ">>> EMERGENCY STOP RELEASED - Resuming normal operation";
        emit emergencyStopReleased();
    }
}
```

---

### 3. WeaponController - Fire Permission Checks

**File:** `src/controllers/weaponcontroller.cpp:243, 285-286, 313, 465, 524, 924`

**Current Implementation (scattered):**
```cpp
// Line 243
if (newData.emergencyStopActive) {
    // abort charging
}

// Line 285-286
if (m_oldState.deadManSwitchActive != newData.deadManSwitchActive) {
    m_fireReady = newData.deadManSwitchActive;
}

// Line 313
newData.gunArmed && /* other conditions */

// Lines 465, 524, 924 - repeated E-Stop checks
if (m_stateModel && m_stateModel->data().emergencyStopActive) {
    return;
}
```

**Target Integration:**
```cpp
// Single fire permission check
bool WeaponController::attemptFire()
{
    SafetyDenialReason reason;
    if (!m_safetyInterlock->canFire(&reason)) {
        qWarning() << "[Weapon] Fire denied:"
                   << SafetyInterlock::denialReasonToString(reason);
        return false;
    }

    // Proceed with fire sequence...
    return true;
}
```

---

### 4. GimbalController - Emergency Stop Response

**File:** `src/controllers/gimbalcontroller.cpp:346-351, 719, 831`

**Current Implementation:**
```cpp
// Line 346-351
if (newData.emergencyStopActive != m_oldState.emergencyStopActive) {
    // handle E-Stop change
}
if (newData.emergencyStopActive) {
    // stop motion
}

// Line 719
if (data.emergencyStopActive) {
    // abort homing
}

// Line 831
bool emergencyActive = data.emergencyStopActive;
```

**Target Integration:**
```cpp
// Connect to SafetyInterlock signal
connect(m_safetyInterlock, &SafetyInterlock::emergencyStopChanged,
        this, &GimbalController::onEmergencyStopChanged);

void GimbalController::onEmergencyStopChanged(bool active)
{
    if (active) {
        stopAllMotion();
        abortHomingSequence();
    }
}
```

---

## HIGH PRIORITY INTEGRATION POINTS (Phase 2)

### 5. TrackingMotionMode - Dead Man Switch

**File:** `src/controllers/motion_modes/trackingmotionmode.cpp:153`

**Current Implementation:**
```cpp
if (!data.deadManSwitchActive) {
    // stop tracking motion
}
```

**Target:** Delegate to `SafetyInterlock::canMove()`

---

### 6. JoystickController - Station Enable Checks

**File:** `src/controllers/joystickcontroller.cpp:92, 140, 183, 244, 296, 348`

**Current Implementation:**
```cpp
// Line 92
if (!curr.deadManSwitchActive) { /* ... */ }

// Lines 140, 183, 244, 348
if (!curr.stationEnabled) { /* ... */ }

// Line 296
bool isDeadManActive = curr.deadManSwitchActive;
```

**Target:** Consolidate into SafetyInterlock queries before processing joystick commands.

---

### 7. SystemStateModel - Zone State Updates

**File:** `src/models/domain/systemstatemodel.cpp:1932, 1953-1954`

**Current Implementation:**
```cpp
m_currentStateData.isReticleInNoFireZone = inZone;
// ...
m_currentStateData.isReticleInNoTraverseZone = inZone;
```

**Target:** Also notify SafetyInterlock:
```cpp
m_currentStateData.isReticleInNoFireZone = inZone;
if (m_safetyInterlock) {
    m_safetyInterlock->updateZoneState(
        m_currentStateData.isReticleInNoFireZone,
        m_currentStateData.isReticleInNoTraverseZone
    );
}
```

---

### 8. SystemStateModel - State Transitions

**File:** `src/models/domain/systemstatemodel.cpp:2382-2419`

**Current Implementation:**
```cpp
if (newData.emergencyStopActive && !oldData.emergencyStopActive) {
    // E-Stop activated
}
if (!newData.emergencyStopActive && oldData.emergencyStopActive) {
    // E-Stop released
}
if (!newData.stationEnabled && oldData.stationEnabled) {
    // Station disabled
}
```

**Target:** These transitions should be detected and logged by SafetyInterlock for audit trail.

---

## MEDIUM PRIORITY INTEGRATION POINTS (Phase 3)

### 9. Plc42ProtocolParser - E-Stop Derivation

**File:** `src/hardware/protocols/Plc42ProtocolParser.cpp:108-110`

**Current Implementation:**
```cpp
// emergencyStopActive is derived from gimbalOpMode
m_data.emergencyStopActive = (m_data.gimbalOpMode == 1);
```

**Target:** This is the SOURCE of E-Stop state. SafetyInterlock should receive updates from here via Plc42DataModel.

---

### 10. SystemStateModel - PLC21 Data Reception

**File:** `src/models/domain/systemstatemodel.cpp:947-956`

**Current Implementation:**
```cpp
newData.stationEnabled = pData.enableStationSW;
newData.gunArmed = pData.armGunSW;
// ...
newData.emergencyStopActive = !pData.authorizeSw;
```

**Target:** Also update SafetyInterlock:
```cpp
if (m_safetyInterlock) {
    m_safetyInterlock->updateFromPlc21(
        !pData.authorizeSw,      // emergencyStop
        pData.armGunSW,          // gunArmed
        pData.authorizeSw,       // authorized
        pData.deadManSwitch,     // deadManSwitch (if available)
        pData.enableStationSW    // stationEnabled
    );
}
```

---

## LOW PRIORITY INTEGRATION POINTS (Phase 4)

### 11. OSDController - Display State

**File:** `src/controllers/osdcontroller.cpp:449, 563-564`

**Target:** Read-only access to SafetyInterlock state for display purposes.

### 12. CameraVideoStreamDevice - OSD Overlay

**File:** `src/hardware/devices/cameravideostreamdevice.cpp:351-352, 369-371`

**Target:** Read-only access for OSD rendering.

### 13. LEDController - Indicator State

**File:** `src/controllers/ledcontroller.cpp:22-29`

**Target:** Subscribe to SafetyInterlock signals for LED updates.

### 14. SystemStatusController - Status Display

**File:** `src/controllers/systemstatuscontroller.cpp:153-154, 177, 249`

**Target:** Query SafetyInterlock for display status.

---

## Integration Architecture

```
                    ┌─────────────────────┐
                    │   SafetyInterlock   │
                    │  (Single Authority) │
                    └──────────┬──────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌───────────────┐    ┌─────────────────┐    ┌────────────────┐
│ HardwareManager│    │  Controllers    │    │   UI/Display   │
│ (State Source) │    │ (Safety Queries)│    │ (Read-Only)    │
└───────┬───────┘    └────────┬────────┘    └────────────────┘
        │                     │
        │ updateFromPlc21()   │ canFire()
        │ updateFromPlc42()   │ canMove()
        │ updateZoneState()   │ canEngage()
        ▼                     ▼
   ┌─────────┐          ┌──────────────┐
   │  PLC21  │          │WeaponController
   │  PLC42  │          │GimbalController
   │ Devices │          │Motion Modes  │
   └─────────┘          └──────────────┘
```

---

## Summary: Files Requiring Modification

| Priority | File | Changes Required |
|----------|------|------------------|
| P0 | `gimbalmotionmodebase.cpp` | Replace `checkSafetyConditions()` body |
| P0 | `weaponcontroller.cpp` | Add SafetyInterlock dependency, consolidate checks |
| P0 | `gimbalcontroller.cpp` | Add SafetyInterlock dependency, connect signals |
| P1 | `trackingmotionmode.cpp` | Delegate to SafetyInterlock |
| P1 | `manualmotionmode.cpp` | Already calls `checkSafetyConditions()` (inherits fix) |
| P1 | `radarslewmotionmode.cpp` | Already calls `checkSafetyConditions()` (inherits fix) |
| P1 | `joystickcontroller.cpp` | Add SafetyInterlock queries |
| P2 | `systemstatemodel.cpp` | Add SafetyInterlock state updates |
| P2 | `Plc42ProtocolParser.cpp` | Source of E-Stop state (no change needed) |
| P3 | `osdcontroller.cpp` | Read-only SafetyInterlock access |
| P3 | `ledcontroller.cpp` | Subscribe to SafetyInterlock signals |
| P3 | `cameravideostreamdevice.cpp` | Read-only SafetyInterlock access |
| P3 | `systemstatuscontroller.cpp` | Query SafetyInterlock for status |

---

## Verification Checklist

After integration, verify:

- [ ] All fire requests route through `SafetyInterlock::canFire()`
- [ ] All motion commands route through `SafetyInterlock::canMove()`
- [ ] E-Stop response time is < 50ms (measure with instrumentation)
- [ ] All safety state transitions are logged with timestamps
- [ ] No direct access to `emergencyStopActive` from controllers (use SafetyInterlock)
- [ ] Unit tests for SafetyInterlock cover all denial reason paths
- [ ] Integration test: E-Stop during tracking aborts motion
- [ ] Integration test: E-Stop during charging aborts sequence
- [ ] Integration test: No-fire zone prevents weapon fire

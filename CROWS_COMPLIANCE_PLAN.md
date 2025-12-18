# CROWS/RCWS Doctrine Compliance Plan

## Executive Summary

This plan addresses the alignment of our tracking and fire control system with CROWS M153 (TM 9-1090-225-10-2) operational doctrine. The primary issues are:

1. **Tracking PID uses ballistic aim point instead of visual target** (CRITICAL)
2. **LAC uses position steering instead of rate-bias feed-forward**
3. **Palm Switch not integrated with active tracking**
4. **Firing does not implement dead reckoning mode**
5. **LAC not properly latched with manual arm/reset**

---

## Current State Analysis

### What Works Correctly
- Zeroing: Properly shifts reticle for camera-gun offset
- Ballistic calculations: Drop and lead correctly computed
- Tracking phases: State machine exists (Off → Acquisition → LockPending → ActiveLock)
- Dead man switch: Required for TRACK button press

### Critical Issues

| Issue | Current Behavior | CROWS Doctrine |
|-------|------------------|----------------|
| PID Error | `(target + drop + lead) - gimbal` | `visualTarget - gimbal` |
| LAC | Position command to intercept | Rate feed-forward bias |
| Palm Switch | Required only to START tracking | Required to MAINTAIN active tracking |
| Firing | Unknown behavior | Dead reckoning (hold last velocity) |
| LAC Latching | Auto-applied continuously | Manual arm/reset required |
| Manual during track | Blocked (mode cycling disabled) | Allowed within half-screen |

---

## Modification Tasks

### PHASE 1: Core Tracking Fix (CRITICAL)

#### Task 1.1: Fix TrackingMotionMode PID Error Signal
**File:** `src/controllers/motion_modes/trackingmotionmode.cpp`

**Current (WRONG):**
```cpp
// Lines 151-173 - PID targets intercept point
double aimAz = m_smoothedTargetAz;
double aimEl = m_smoothedTargetEl;

if (data.ballisticDropActive) {
    aimAz += data.ballisticDropOffsetAz;
    aimEl -= data.ballisticDropOffsetEl;  // Gimbal physically pitches up!
}

if (data.leadAngleCompensationActive) {
    aimAz += data.motionLeadOffsetAz;
    aimEl += data.motionLeadOffsetEl;  // Gimbal steers to intercept!
}

double errAz = normalizeAngle180(aimAz - data.gimbalAz);  // WRONG!
double errEl = aimEl - data.gimbalEl;                      // WRONG!
```

**CORRECT (CROWS Doctrine):**
```cpp
// PID uses VISUAL target ONLY - no ballistics in gimbal control
double errAz = normalizeAngle180(m_smoothedTargetAz - data.gimbalAz);
double errEl = m_smoothedTargetEl - data.gimbalEl;

// Ballistics used ONLY for:
// 1. CCIP display calculation
// 2. Fire authorization (target overlaps CCIP)
// 3. Optional rate-bias LAC (feed-forward only, NOT position error)
```

#### Task 1.2: Implement CROWS-Style LAC as Rate Bias
**File:** `src/controllers/motion_modes/trackingmotionmode.cpp`

**New LAC Implementation (Rate Feed-Forward):**
```cpp
// LAC as rate-bias feed-forward (CROWS-compliant)
double ffAz = 0.0;
double ffEl = 0.0;

if (data.leadAngleCompensationActive && m_lacLatched) {
    // Use measured target angular velocity as rate bias
    // This helps operator maintain tracking without constant joystick input
    ffAz = m_lacGain * m_smoothedAzVel_dps;
    ffEl = m_lacGain * m_smoothedElVel_dps;
}

// Final velocity = PID output + rate feed-forward
double desiredAzVelocity = pidAzVelocity + ffAz;
double desiredElVelocity = pidElVelocity + ffEl;
```

**Key differences:**
- LAC adds to VELOCITY command, not POSITION error
- Reticle stays on target (not ahead of target)
- Gradual lead through motion, not position jump

---

### PHASE 2: Palm Switch Integration

#### Task 2.1: Add Palm Switch Requirement for Active Tracking
**File:** `src/controllers/motion_modes/trackingmotionmode.cpp`

**Modification to `update()` method:**
```cpp
void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    SystemStateData data = controller->systemStateModel()->data();

    // CROWS: Active tracking requires Palm Switch held
    // Per TM 9-1090-225-10-2 page 39:
    // "Target Tracking actively tracks a target only when the Palm Switch is held.
    //  To stop active tracking at any time, release the Palm Switch."

    if (!data.deadManSwitchActive) {
        // Palm switch released - pause active tracking
        // Gimbal holds current position (or coasts at last velocity)
        stopServos(controller);
        return;
    }

    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    // ... rest of tracking logic ...
}
```

#### Task 2.2: Update Tracking Phase on Palm Switch Release
**File:** `src/models/domain/systemstatemodel.cpp`

Add handler for palm switch release during tracking:
```cpp
void SystemStateModel::setDeadManSwitch(bool pressed) {
    if (m_currentStateData.deadManSwitchActive != pressed) {
        m_currentStateData.deadManSwitchActive = pressed;

        // CROWS: Palm switch release during tracking = pause (not abort)
        if (!pressed && m_currentStateData.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock) {
            // Transition to Coast mode - tracking paused, not aborted
            // System maintains last known target position
            m_currentStateData.currentTrackingPhase = TrackingPhase::Tracking_Coast;
            qInfo() << "[CROWS] Palm switch released - tracking paused (Coast mode)";
        }

        emit dataChanged(m_currentStateData);
    }
}
```

---

### PHASE 3: LAC Latching System

#### Task 3.1: Add LAC Latching State Variables
**File:** `src/models/domain/systemstatedata.h`

```cpp
// Add to BALLISTICS & FIRE CONTROL section:

// LAC Latching State (CROWS-compliant manual arm/reset)
bool lacArmed = false;                      ///< LAC manually armed by operator
float lacLatchedAzRate_dps = 0.0f;          ///< Latched azimuth rate at arm time
float lacLatchedElRate_dps = 0.0f;          ///< Latched elevation rate at arm time
qint64 lacArmTimestampMs = 0;               ///< Timestamp when LAC was armed
```

#### Task 3.2: Implement LAC Arm/Reset Logic
**File:** `src/controllers/joystickcontroller.cpp`

**Modify Button 2 (LAC) handler:**
```cpp
// BUTTON 2: LEAD ANGLE COMPENSATION (CROWS-style latching)
// PDF: "Hold the Palm Switch (2) and press the LEAD button (1)"
case 2:
    if (pressed) {
        bool isDeadManActive = curr.deadManSwitchActive;

        if (!isDeadManActive) {
            qDebug() << "[CROWS] LAC toggle ignored - Palm Switch not active";
            return;
        }

        bool currentLACState = curr.leadAngleCompensationActive;

        if (!currentLACState) {
            // === ARMING LAC ===
            // CROWS: "The computer is constantly monitoring the change in rotation
            //         of the elevation and azimuth axes and measuring the speed."
            // Latch current tracking rate
            m_stateModel->armLAC(
                curr.currentTargetAngularRateAz,
                curr.currentTargetAngularRateEl
            );
            qInfo() << "[CROWS] LAC ARMED - Latched rates: Az="
                    << curr.currentTargetAngularRateAz << "°/s El="
                    << curr.currentTargetAngularRateEl << "°/s";
        } else {
            // === DISARMING LAC ===
            // CROWS: "Cancel Lead Angle Compensation by pressing the Palm Switch
            //         with the CG in the neutral position."
            m_stateModel->disarmLAC();
            qInfo() << "[CROWS] LAC DISARMED";
        }
    }
    break;
```

#### Task 3.3: Add LAC Arm/Disarm Methods to SystemStateModel
**File:** `src/models/domain/systemstatemodel.cpp`

```cpp
void SystemStateModel::armLAC(float azRate_dps, float elRate_dps) {
    SystemStateData& data = m_currentStateData;

    data.leadAngleCompensationActive = true;
    data.lacArmed = true;
    data.lacLatchedAzRate_dps = azRate_dps;
    data.lacLatchedElRate_dps = elRate_dps;
    data.lacArmTimestampMs = QDateTime::currentMSecsSinceEpoch();

    // CROWS WARNING: "If target #2 is not properly acquired, the WS will fire
    //                 outside the desired engagement area by continuing to apply
    //                 the lead angle acquired from target #1"
    qWarning() << "[CROWS WARNING] LAC armed with latched rates. Must reset before engaging new target!";

    emit dataChanged(m_currentStateData);
}

void SystemStateModel::disarmLAC() {
    SystemStateData& data = m_currentStateData;

    data.leadAngleCompensationActive = false;
    data.lacArmed = false;
    data.lacLatchedAzRate_dps = 0.0f;
    data.lacLatchedElRate_dps = 0.0f;
    data.lacArmTimestampMs = 0;

    // Clear motion lead offsets
    data.motionLeadOffsetAz = 0.0f;
    data.motionLeadOffsetEl = 0.0f;
    data.currentLeadAngleStatus = LeadAngleStatus::Off;

    emit dataChanged(m_currentStateData);
}
```

---

### PHASE 4: Firing Behavior (Dead Reckoning)

#### Task 4.1: Add Dead Reckoning State
**File:** `src/models/domain/systemstatedata.h`

```cpp
// Add to Tracking System section:

// Dead Reckoning (firing during tracking)
bool deadReckoningActive = false;           ///< True when firing terminated tracking
float deadReckoningAzVel_dps = 0.0f;        ///< Last Az velocity before firing
float deadReckoningElVel_dps = 0.0f;        ///< Last El velocity before firing
```

#### Task 4.2: Implement Dead Reckoning on Fire
**File:** `src/controllers/weaponcontroller.cpp`

**Modify `startFiring()` method:**
```cpp
void WeaponController::startFiring()
{
    if (!m_systemArmed) {
        qDebug() << "[WeaponController] Cannot fire: system is not armed";
        return;
    }

    // Safety: Prevent firing if in No-Fire zone
    if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (m_stateModel->isPointInNoFireZone(s.gimbalAz, s.gimbalEl)) {
            qWarning() << "[WeaponController] FIRE BLOCKED: Aim point in No-Fire zone";
            return;
        }

        // CROWS: Firing terminates target tracking (TM 9-1090-225-10-2 page 38)
        // "When firing is initiated, CROWS aborts Target Tracking. Instead the system
        //  moves according to the speed and direction of the WS just prior to pulling
        //  the trigger."
        if (s.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock) {
            qInfo() << "[CROWS] FIRING - Entering Dead Reckoning mode";
            m_stateModel->enterDeadReckoning(
                s.currentTargetAngularRateAz,
                s.currentTargetAngularRateEl
            );
        }
    }

    // Fire the weapon
    m_plc42->setSolenoidState(1);
}
```

#### Task 4.3: Add Dead Reckoning Motion Mode Support
**File:** `src/controllers/motion_modes/trackingmotionmode.cpp`

Add dead reckoning handling:
```cpp
void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    SystemStateData data = controller->systemStateModel()->data();

    // CROWS: Dead reckoning during firing
    if (data.deadReckoningActive) {
        // Maintain constant velocity from last tracking moment
        // "CROWS will not automatically compensate for changes in speed or
        //  direction of the tracked target during firing."
        sendStabilizedServoCommands(
            controller,
            data.deadReckoningAzVel_dps,
            data.deadReckoningElVel_dps,
            true, dt
        );
        return;
    }

    // ... rest of tracking logic ...
}
```

---

### PHASE 5: Manual Control During Tracking

#### Task 5.1: Allow Limited Manual Slew During Tracking
**File:** `src/controllers/motion_modes/trackingmotionmode.cpp`

**Add manual override support within FOV limits:**
```cpp
void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    SystemStateData data = controller->systemStateModel()->data();

    // CROWS: "CROWS maintains Target Tracking while the reticle is moved to another
    //         point (no more than half a screen from tracked target)."

    if (m_targetValid && data.deadManSwitchActive) {
        // Check for joystick input
        float joystickAz = data.joystickAzValue;
        float joystickEl = data.joystickElValue;

        const float JOYSTICK_DEADBAND = 0.15f;
        bool hasManualInput = (std::abs(joystickAz) > JOYSTICK_DEADBAND ||
                               std::abs(joystickEl) > JOYSTICK_DEADBAND);

        if (hasManualInput) {
            // Calculate offset from tracked target
            // Allow manual slew within half-screen limit
            float halfScreenAz = data.activeCameraIsDay ?
                                 data.dayCurrentHFOV / 2.0f :
                                 data.nightCurrentHFOV / 2.0f;
            float halfScreenEl = halfScreenAz * 0.75f; // Approximate VFOV

            // Clamp manual offset to half-screen
            // ... (implementation details)

            // Apply manual velocity on top of tracking
            double manualAzVel = joystickAz * MANUAL_OVERRIDE_SCALE;
            double manualElVel = joystickEl * MANUAL_OVERRIDE_SCALE;

            // PID + manual + (optional LAC rate bias)
            desiredAzVelocity = pidAzVelocity + manualAzVel + ffAz;
            desiredElVelocity = pidElVelocity + manualElVel + ffEl;
        }
    }
}
```

---

### PHASE 6: CCIP and Fire Authorization

#### Task 6.1: Ensure CCIP is Calculated Separately from Tracking
**File:** `src/controllers/osdcontroller.cpp` (or wherever CCIP is computed)

CCIP should be:
```cpp
// CCIP = Screen center + Zeroing + Drop + Lead
// This is the predicted bullet impact point

QPointF ccipPosition = screenCenter;

// 1. Apply zeroing (camera-gun alignment)
if (zeroingActive) {
    ccipPosition += zeroingOffset;
}

// 2. Apply ballistic drop (gravity compensation)
if (dropActive) {
    ccipPosition += dropOffset;
}

// 3. Apply motion lead (moving target prediction)
if (lacActive) {
    ccipPosition += leadOffset;
}
```

Reticle should be:
```cpp
// Reticle = Screen center + Zeroing ONLY
// This shows where the gun is pointing

QPointF reticlePosition = screenCenter;

if (zeroingActive) {
    reticlePosition += zeroingOffset;
}

// NO drop, NO lead on reticle!
```

#### Task 6.2: Add Fire Authorization Logic
**File:** `src/controllers/weaponcontroller.cpp`

```cpp
bool WeaponController::isFireSolutionValid() const
{
    if (!m_stateModel) return false;

    SystemStateData s = m_stateModel->data();

    // CROWS: Fire when TARGET overlaps CCIP (not when reticle is on target)

    // Calculate distance between visual target center and CCIP
    float targetX = s.trackedTargetCenterX_px;
    float targetY = s.trackedTargetCenterY_px;
    float ccipX = s.ccipImpactImageX_px;
    float ccipY = s.ccipImpactImageY_px;

    float distance = std::sqrt(
        (targetX - ccipX) * (targetX - ccipX) +
        (targetY - ccipY) * (targetY - ccipY)
    );

    // Tolerance in pixels (adjustable based on weapon accuracy)
    const float FIRE_SOLUTION_TOLERANCE_PX = 20.0f;

    return distance < FIRE_SOLUTION_TOLERANCE_PX;
}
```

---

## Summary: Before/After Comparison

### Tracking Control Loop

**BEFORE (Current - WRONG):**
```
Target Position → Add Drop → Add Lead → PID → Gimbal
                     ↑           ↑
                   WRONG       WRONG
```

**AFTER (CROWS-Compliant):**
```
Target Position → PID → + Rate Bias (optional) → Gimbal
                              ↑
                         LAC Feed-Forward
                         (velocity, not position)

Separately:
Target + Drop + Lead → CCIP Display
```

### Key Behavioral Changes

| Scenario | Before | After (CROWS) |
|----------|--------|---------------|
| Tracking stationary target | Reticle on target | Reticle on target |
| Tracking moving target (no LAC) | Reticle on target | Reticle on target |
| Tracking moving target (LAC on) | Reticle AHEAD of target | Reticle ON target, CCIP shows impact |
| Fire trigger | Immediate fire | Fire when target overlaps CCIP |
| Palm switch release | Continue tracking | Pause tracking (coast) |
| Target switch with LAC | Auto-adapts | Must reset LAC (2 sec minimum) |

---

## Implementation Order

1. **Phase 1** (CRITICAL): Fix PID error signal - this is the core issue
2. **Phase 2**: Palm switch integration
3. **Phase 3**: LAC latching system
4. **Phase 4**: Dead reckoning on fire
5. **Phase 5**: Manual control during tracking
6. **Phase 6**: Fire authorization

---

## Testing Requirements

### Test Case 1: Basic Tracking Without LAC
1. Acquire stationary target
2. Lock on
3. Verify reticle stays on target
4. Verify gimbal tracks target visually

### Test Case 2: Moving Target Without LAC
1. Track moving target
2. Verify reticle follows target
3. Verify CCIP is offset by drop only
4. Verify gimbal physically follows target (not intercept)

### Test Case 3: Moving Target With LAC
1. Track moving target
2. Arm LAC
3. Verify reticle STILL follows target (not ahead)
4. Verify CCIP includes drop + lead
5. Verify LAC provides rate assistance (easier to track)

### Test Case 4: Palm Switch
1. Lock on target
2. Release palm switch
3. Verify tracking pauses (coast mode)
4. Verify gimbal stops moving
5. Re-engage palm switch
6. Verify tracking resumes

### Test Case 5: Fire During Tracking
1. Track moving target
2. Fire weapon
3. Verify tracking aborts
4. Verify gimbal continues at last velocity (dead reckoning)
5. Verify tracking box turns green

### Test Case 6: LAC Target Switch Warning
1. Track target #1 with LAC
2. Slew to target #2
3. Verify system warns about LAC reset
4. Verify minimum 2-second wait enforced

---

## Files to Modify

1. `src/controllers/motion_modes/trackingmotionmode.cpp` - Core PID fix + LAC rate bias
2. `src/controllers/motion_modes/trackingmotionmode.h` - Add LAC state variables
3. `src/models/domain/systemstatedata.h` - Add LAC latching fields
4. `src/models/domain/systemstatemodel.cpp` - Palm switch + LAC arm/disarm
5. `src/controllers/joystickcontroller.cpp` - LAC button handling
6. `src/controllers/weaponcontroller.cpp` - Dead reckoning + fire authorization
7. `src/controllers/osdcontroller.cpp` - Verify CCIP vs reticle separation

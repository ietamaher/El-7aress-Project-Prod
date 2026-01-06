# TrackingMotionMode Class Documentation

**Version:** 2.0
**Last Updated:** 2026-01-06
**Status:** Production

---

## Overview

`TrackingMotionMode` implements vision-based target tracking with P+D control, filtered feedforward, and a Lead Angle Compensation (LAC) state machine for ballistic corrections. Uses a "rigid cradle" model where camera and gun are physically locked together.

**Location:** `src/controllers/motion_modes/trackingmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Autonomous target tracking using visual feedback with lead angle injection for fire control.

---

## Key Design Features (v2.0)

### LAC State Machine
- **TRACK:** Normal P+D+FF control keeping target centered
- **FIRE_LEAD:** Open-loop lead injection (target drifts off-center)
- **RECENTER:** Smooth transition back to tracking control

### Rigid Cradle Physics
Camera and gun are physically locked - when lead is injected, the target visibly drifts off-center in the camera frame. This is expected behavior.

### Filtered Control
- **Derivative filter (τ=100ms):** Reduces dErr/dt noise from tracker jitter
- **Feedforward filter (τ=150ms):** Prevents tracker velocity glitch amplification

### Manual Nudge
Joystick offset during tracking with decay - allows operator adjustment without breaking lock.

---

## LAC State Machine

```
┌─────────────────────────────────────────────────────────────┐
│                    LAC STATE MACHINE                        │
│         (Rigid Cradle: Camera + Gun Locked Together)        │
└─────────────────────────────────────────────────────────────┘

           lacArmed && deadReckoningActive
     ┌─────────┐ ─────────────────────────→ ┌─────────────┐
     │  TRACK  │                            │  FIRE_LEAD  │
     │         │                            │             │
     │ blend=0 │                            │ blend → 1.0 │
     │ P+D+FF  │                            │ Open-loop   │
     │ target  │                            │ lead inject │
     │ centered│                            │ target drift│
     └────┬────┘                            └──────┬──────┘
          ↑                                        │
          │ blend reaches 0                        │ LAC deactivated OR
          │ (333ms ramp)                           │ deadReckoningActive=false
          │                                        ↓
     ┌────┴────┐                            ┌─────────────┐
     │         │←───────────────────────────│  RECENTER   │
     │         │                            │             │
     │         │                            │ blend → 0.0 │
     └─────────┘                            │ Resume P+D  │
                                            └─────────────┘

Blend Factor:
  0.0 = 100% tracking control (target centered)
  1.0 = 100% lead injection (target drifts)
  Ramp in: 5.0/sec (200ms to reach 1.0)
  Ramp out: 3.0/sec (333ms to reach 0.0)
```

---

## Control Architecture

```
               ┌─────────────────────────────────┐
               │    Vision Tracker Input         │
               │   (imageErrAz, imageErrEl)      │
               │   (targetVelAz, targetVelEl)    │
               └─────────────┬───────────────────┘
                             │
                             ▼
               ┌─────────────────────────────────┐
               │     Apply Manual Nudge          │
               │   (joystick offset with decay)  │
               └─────────────┬───────────────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
     ┌────────────────┐           ┌────────────────┐
     │  Raw dErr/dt   │           │  Target Vel    │
     │  (noisy)       │           │  (noisy)       │
     └───────┬────────┘           └───────┬────────┘
             │                            │
             ▼                            ▼
     ┌────────────────┐           ┌────────────────┐
     │  IIR Filter    │           │  IIR Filter    │
     │  τ = 100ms     │           │  τ = 150ms     │
     └───────┬────────┘           └───────┬────────┘
             │                            │
             ▼                            ▼
     ┌────────────────┐           ┌────────────────┐
     │  D-term        │           │  Feedforward   │
     │  Kd × dErr     │           │  (scaled,clamped)│
     │  clamp ±8°/s   │           │  clamp ±3°/s   │
     └───────┬────────┘           └───────┬────────┘
             │                            │
             └──────────────┬─────────────┘
                            │
                            ▼
               ┌────────────────────────┐
               │  P-term: Kp × error    │
               │  + filtered D-term     │
               │  + filtered FF         │
               │  = trackCmd            │
               └───────────┬────────────┘
                           │
                           ▼
         ┌─────────────────────────────────────────┐
         │               BLEND                      │
         │  final = (1-blend)×track + blend×LAC    │
         │                                          │
         │  LAC cmd = latchedRate × LAC_RATE_GAIN  │
         └─────────────────┬───────────────────────┘
                           │
                           ▼
               ┌────────────────────────┐
               │  sendStabilizedServoCmds│
               │  (stabilization OFF)   │
               └────────────────────────┘
```

---

## Member Variables

### Control Parameters
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `m_targetValid` | `bool` | `false` | Track lock status |
| `m_azPid` | `PIDController` | - | Azimuth P+D controller |
| `m_elPid` | `PIDController` | - | Elevation P+D controller |

### LAC State Machine
| Variable | Type | Purpose |
|----------|------|---------|
| `m_lacState` | `LACState` | Current LAC state (TRACK/FIRE_LEAD/RECENTER) |
| `m_lacBlendFactor` | `double` | 0.0-1.0 blend between tracking and lead |

### Filtered Variables
| Variable | Type | Purpose |
|----------|------|---------|
| `m_prevErrAz/El` | `double` | Previous error (for derivative) |
| `m_filteredDErrAz/El` | `double` | Filtered derivative-on-error |
| `m_smoothedAzVel_dps` | `double` | Smoothed target velocity |
| `m_filteredTargetVelAz/El` | `double` | Filtered feedforward velocity |

### Manual Nudge
| Variable | Type | Purpose |
|----------|------|---------|
| `m_nudgeAz/El` | `double` | Current nudge offset [degrees] |
| `m_nudgeDecay` | `double` | Decay rate per cycle |

---

## Constants

```cpp
// Control gains
static constexpr double TRACKING_KP_AZ = 1.0;
static constexpr double TRACKING_KD_AZ = 0.35;
static constexpr double TRACKING_KP_EL = 1.0;
static constexpr double TRACKING_KD_EL = 0.35;

// Filter time constants
static constexpr double DERR_FILTER_TAU = 0.1;    // 100ms D-term filter
static constexpr double FF_FILTER_TAU = 0.15;     // 150ms FF filter

// Feedforward
static constexpr double FF_GAIN = 0.5;
static constexpr double FF_MAX = 3.0;             // deg/s

// Clamps
static constexpr double DERR_MAX = 8.0;           // deg/s
static constexpr double MAX_VEL_AZ = 12.0;        // deg/s
static constexpr double MAX_VEL_EL = 12.0;        // deg/s

// LAC
static constexpr double LAC_RATE_BIAS_GAIN = 1.0;
static constexpr double LAC_BLEND_RAMP_IN = 5.0;  // 0→1 in 200ms
static constexpr double LAC_BLEND_RAMP_OUT = 3.0; // 1→0 in 333ms
```

---

## Public Methods

### Constructor
```cpp
TrackingMotionMode(QObject* parent = nullptr)
```
**Actions:**
- Initialize P+D gains (Kp=1.0, Kd=0.35, Ki=0)
- Clear all filter state
- Set `m_lacState = TRACK`, `m_lacBlendFactor = 0.0`

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```
**Actions:**
1. Start velocity timer
2. Reset PID controllers
3. Clear all filter state
4. Reset LAC state to TRACK, blend to 0.0
5. Clear manual nudge

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```
**Actions:**
- Stop servos
- Reset target validity

### onTargetPositionUpdated(...)
```cpp
void onTargetPositionUpdated(double az, double el,
                            double velocityAz_dps, double velocityEl_dps,
                            bool isValid)
```
**Purpose:** Receive vision tracker updates.

**When `isValid == true`:**
- Store target position and velocity
- Set `m_targetValid = true`

**When `isValid == false`:**
- Set `m_targetValid = false`
- Zero velocities

---

## updateImpl(GimbalController* controller, double dt)

### Step 1: Check Target Validity
```cpp
if (!m_targetValid) {
    stopServos(controller);
    return;
}
```

### Step 2: Get Error from Vision
```cpp
double errAz = data.imageErrAz;  // Error in camera frame
double errEl = data.imageErrEl;
```

### Step 3: Apply Manual Nudge (if any)
```cpp
double effectiveErrAz = errAz + m_nudgeAz;
double effectiveErrEl = errEl + m_nudgeEl;

// Decay nudge toward zero
m_nudgeAz *= (1.0 - NUDGE_DECAY_RATE * dt);
m_nudgeEl *= (1.0 - NUDGE_DECAY_RATE * dt);
```

### Step 4: Compute Filtered Derivative-on-Error
```cpp
// Raw derivative (noisy)
double rawDErrAz = (effectiveErrAz - m_prevErrAz) / dt;
m_prevErrAz = effectiveErrAz;

// Low-pass filter (τ = 100ms)
double alpha = 1.0 - std::exp(-dt / DERR_FILTER_TAU);
m_filteredDErrAz = alpha * rawDErrAz + (1.0 - alpha) * m_filteredDErrAz;

// Clamp to prevent tracker glitch amplification
double dErrAz = qBound(-DERR_MAX, m_filteredDErrAz, DERR_MAX);
```

### Step 5: Compute Filtered Feedforward
```cpp
// Filter target velocity (τ = 150ms)
double ffAlpha = 1.0 - std::exp(-dt / FF_FILTER_TAU);
m_filteredTargetVelAz = ffAlpha * m_smoothedAzVel_dps
                       + (1.0 - ffAlpha) * m_filteredTargetVelAz;

// Apply gain and clamp
double ffAz = qBound(-FF_MAX, m_filteredTargetVelAz * FF_GAIN, FF_MAX);

// Disable FF when error is large (recovery mode)
if (std::abs(effectiveErrAz) > 0.15) ffAz = 0.0;
```

### Step 6: Compute Tracking Command
```cpp
// P + D + FF (no integral - Ki=0)
double pTerm = m_azPid.Kp * effectiveErrAz;
double dTerm = m_azPid.Kd * dErrAz;
double trackAzCmd = pTerm + dTerm + ffAz;
```

### Step 7: LAC State Machine Update
```cpp
updateLACState(data, dt);

// Compute LAC command
double lacAzCmd = data.lacLatchedAzRate_dps * LAC_RATE_BIAS_GAIN;
double lacElCmd = data.lacLatchedElRate_dps * LAC_RATE_BIAS_GAIN;
```

### Step 8: Blend Tracking and LAC
```cpp
double finalAzVel = (1.0 - m_lacBlendFactor) * trackAzCmd
                   + m_lacBlendFactor * lacAzCmd;
double finalElVel = (1.0 - m_lacBlendFactor) * trackElCmd
                   + m_lacBlendFactor * lacElCmd;
```

### Step 9: Clamp and Send
```cpp
finalAzVel = qBound(-MAX_VEL_AZ, finalAzVel, MAX_VEL_AZ);
finalElVel = qBound(-MAX_VEL_EL, finalElVel, MAX_VEL_EL);

// Stabilization OFF during tracking (vision is ground truth)
sendStabilizedServoCommands(controller, finalAzVel, finalElVel, false, dt);
```

---

## LAC State Machine Logic

### updateLACState(SystemStateData& data, double dt)

```cpp
switch (m_lacState) {
case LACState::TRACK:
    if (data.lacArmed && data.deadReckoningActive) {
        // Transition to FIRE_LEAD
        m_lacState = LACState::FIRE_LEAD;
    }
    // Blend stays at 0.0
    break;

case LACState::FIRE_LEAD:
    // Ramp blend toward 1.0
    m_lacBlendFactor += LAC_BLEND_RAMP_IN * dt;
    if (m_lacBlendFactor > 1.0) m_lacBlendFactor = 1.0;

    if (!data.lacArmed || !data.deadReckoningActive) {
        // Transition to RECENTER
        m_lacState = LACState::RECENTER;
    }
    break;

case LACState::RECENTER:
    // Ramp blend toward 0.0
    m_lacBlendFactor -= LAC_BLEND_RAMP_OUT * dt;
    if (m_lacBlendFactor < 0.0) {
        m_lacBlendFactor = 0.0;
        m_lacState = LACState::TRACK;
    }
    break;
}
```

---

## Why No Integral Term?

This implementation uses **P+D control only** (Ki=0):

1. **Vision is Ground Truth:** The tracker provides absolute error, not a setpoint
2. **No Steady-State Error:** If target is centered, error = 0 naturally
3. **Integral Causes Problems:**
   - Windup during target occlusion
   - Overshoot when target reappears
   - Hunting behavior with noisy tracker

The filtered D-term and FF provide sufficient dynamics without integral.

---

## Why Stabilization is OFF?

During tracking, the vision system provides direct error measurement in the camera frame. Platform motion is already compensated by the tracker itself. Enabling stabilization would:

1. **Double-compensate** for platform motion
2. **Fight against** the tracking loop
3. **Cause oscillation** and instability

Stabilization is only enabled in modes without visual feedback (Manual, Scan, etc.).

---

## Tuning Guide

### For Faster Response
```cpp
TRACKING_KP_AZ = 1.5;    // Increase from 1.0
DERR_FILTER_TAU = 0.05;  // Faster D-term (50ms)
FF_GAIN = 0.7;           // More feedforward
```

### For Smoother Motion
```cpp
TRACKING_KP_AZ = 0.8;    // Decrease from 1.0
TRACKING_KD_AZ = 0.5;    // More damping
DERR_FILTER_TAU = 0.15;  // Slower D-term
```

### For Better Lead Injection
```cpp
LAC_BLEND_RAMP_IN = 10.0;   // Faster entry (100ms)
LAC_RATE_BIAS_GAIN = 1.2;   // Overshoot lead slightly
```

---

## Testing Checklist

- [ ] Target lock acquired (m_targetValid = true)
- [ ] Gimbal tracks moving target smoothly
- [ ] D-term filter reduces tracker jitter
- [ ] FF activates for moving targets (< 0.15° error)
- [ ] FF disables during large error recovery
- [ ] Manual nudge shifts aim point temporarily
- [ ] Nudge decays back to zero
- [ ] LAC transitions to FIRE_LEAD when armed
- [ ] Target visibly drifts during lead injection
- [ ] LAC transitions to RECENTER when disarmed
- [ ] Blend ramps are smooth (no step changes)
- [ ] Stabilization is disabled during tracking
- [ ] exitMode stops servos cleanly

---

## Known Issues

### Issue 1: Tracker Jitter Causes Oscillation
**Cause:** D-term amplifying noisy tracker updates.

**Solution:** Increase `DERR_FILTER_TAU`:
```cpp
static constexpr double DERR_FILTER_TAU = 0.15;  // from 0.10
```

### Issue 2: Sluggish Response to Fast Targets
**Cause:** FF disabled or clamped too aggressively.

**Solution:**
```cpp
FF_MAX = 5.0;    // from 3.0
FF_GAIN = 0.7;   // from 0.5
```

### Issue 3: LAC Blend Too Abrupt
**Cause:** Ramp rates too fast.

**Solution:**
```cpp
LAC_BLEND_RAMP_IN = 3.0;   // from 5.0 (333ms)
LAC_BLEND_RAMP_OUT = 2.0;  // from 3.0 (500ms)
```

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Update Rate** | 20 Hz | 50ms cycle |
| **Max Velocity** | 12 deg/s | Per axis |
| **D-term Latency** | ~100ms | Filter settling |
| **FF Latency** | ~150ms | Filter settling |
| **LAC Ramp-In** | 200ms | 0→1 blend |
| **LAC Ramp-Out** | 333ms | 1→0 blend |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with servo control
- **GimbalController:** Mode orchestration
- **SystemStateModel:** Vision error and LAC data
- **SafetyInterlock:** Dead man switch enforcement

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation |
| 2026-01-06 | 2.0 | LAC state machine, filtered D-term/FF, manual nudge, stabilization OFF |

---

**END OF DOCUMENTATION**

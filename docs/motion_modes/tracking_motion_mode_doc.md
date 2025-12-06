# TrackingMotionMode Class Documentation

## Overview

`TrackingMotionMode` is a gimbal motion control class that implements target tracking with PID feedback control and feed-forward velocity compensation. It inherits from `GimbalMotionModeBase` and manages smooth, stable tracking of moving targets by combining proportional-integral-derivative control with target velocity prediction.

**Location:** `trackingmotionmode.h/cpp`

**Dependencies:**
- `GimbalMotionModeBase` (parent class)
- `GimbalController` (gimbal hardware interface)
- `SystemStateModel` (gimbal position/velocity state data)
- Qt Core (`QDebug`, `QtGlobal`)
- Standard C++ (`<cmath>`)

---

## Class Purpose

Enable gimbal to autonomously track a target by:
1. Receiving target position and velocity updates
2. Computing PID feedback to reduce position error
3. Adding feed-forward velocity to anticipate target motion
4. Smoothing commands to prevent motor jerks and overload
5. Limiting accelerations and velocity changes for mechanical safety

---

## Key Member Variables

### Control Parameters
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `m_targetValid` | `bool` | `false` | Whether a valid tracking target exists |
| `m_targetAz` | `double` | 0.0 | Target azimuth angle [degrees] |
| `m_targetEl` | `double` | 0.0 | Target elevation angle [degrees] |
| `m_targetAzVel_dps` | `double` | 0.0 | Target azimuth velocity [deg/s] |
| `m_targetElVel_dps` | `double` | 0.0 | Target elevation velocity [deg/s] |

### Smoothing & Filtering
| Variable | Type | Purpose |
|----------|------|---------|
| `m_smoothedTargetAz` | `double` | Low-pass filtered target azimuth (for PID) |
| `m_smoothedTargetEl` | `double` | Low-pass filtered target elevation (for PID) |
| `m_smoothedAzVel_dps` | `double` | Low-pass filtered azimuth velocity (for feed-forward) |
| `m_smoothedElVel_dps` | `double` | Low-pass filtered elevation velocity (for feed-forward) |

### PID Controllers
| Variable | Type | Purpose |
|----------|------|---------|
| `m_azPid` | `PIDController` | Azimuth position error feedback controller |
| `m_elPid` | `PIDController` | Elevation position error feedback controller |

### Rate Limiting
| Variable | Type | Purpose |
|----------|------|---------|
| `m_previousDesiredAzVel` | `double` | Previous cycle's azimuth velocity (for rate limiting) |
| `m_previousDesiredElVel` | `double` | Previous cycle's elevation velocity (for rate limiting) |

---

## Constants

```cpp
static const double SMOOTHING_ALPHA = 0.3;              // Position smoothing factor
static const double VELOCITY_SMOOTHING_ALPHA = 0.2;     // Velocity smoothing (more aggressive)
static const double MAX_VELOCITY = 15.0;                // Max gimbal velocity [deg/s]
static const double MAX_ACCELERATION = 30.0;            // Max gimbal acceleration [deg/s²]
static const double VELOCITY_CHANGE_LIMIT = 5.0;        // Max velocity change per cycle [deg/s]
```

### PID Gains (Reduced for Stability)
```cpp
Azimuth:   Kp=0.15, Ki=0.005, Kd=0.01, maxIntegral=10.0
Elevation: Kp=0.15, Ki=0.005, Kd=0.01, maxIntegral=10.0
```

---

## Public Methods

### Constructor
```cpp
TrackingMotionMode(QObject* parent = nullptr)
```
**Purpose:** Initialize tracking mode with conservative PID gains and reset state variables.

**Actions:**
- Sets up azimuth and elevation PID controllers
- Initializes all member variables to default/zero values
- Configures gains to prevent motor overload

---

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller)
```

**Purpose:** Called when gimbal switches INTO tracking mode.

**Actions:**
- Logs mode entry
- Invalidates current target (waits for fresh command)
- Resets both PID controllers (clears integral windup)
- Resets previous velocity values for rate limiting
- Sets servo acceleration to 50,000 [units] for moderate response

**Important:** Clears PID integral to prevent artifacts from previous mode.

---

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller)
```

**Purpose:** Called when gimbal switches OUT of tracking mode.

**Actions:**
- Logs mode exit
- Stops all servos (zeroes velocity commands)

---

### onTargetPositionUpdated(...)
```cpp
void onTargetPositionUpdated(double az, double el, 
                            double velocityAz_dps, double velocityEl_dps, 
                            bool isValid)
```

**Purpose:** Receive new target position and velocity data from tracking system.

**Parameters:**
- `az`: Target azimuth angle [degrees]
- `el`: Target elevation angle [degrees]
- `velocityAz_dps`: Target azimuth velocity [deg/s]
- `velocityEl_dps`: Target elevation velocity [deg/s]
- `isValid`: Whether target is currently valid/tracked

**Behavior:**
- **If `isValid == true`:**
  - First valid target: resets PIDs, initializes smoothed values with current data
  - Subsequent updates: stores target data for `update()` to use
  - Sets `m_targetValid = true`
  
- **If `isValid == false`:**
  - Clears `m_targetValid` flag
  - Zeros all velocity values to stop motion
  - Logs target loss

---

### update(GimbalController* controller)
```cpp
void update(GimbalController* controller)
```

**Purpose:** Main control loop—called every update cycle to compute and send servo commands.

**Control Flow:**

1. **Check target validity** → If invalid, stop servos and return

2. **Get elapsed time** → Restarts velocity timer for accurate dt

3. **Get system state** → Reads current gimbal position from `SystemStateModel`

4. **Smooth target position** 
   ```cpp
   m_smoothedTargetAz = (0.3 * m_targetAz) + (0.7 * m_smoothedTargetAz)
   ```

5. **Smooth target velocity** (more aggressive smoothing)
   ```cpp
   m_smoothedAzVel_dps = (0.2 * m_targetAzVel_dps) + (0.8 * m_smoothedAzVel_dps)
   ```

6. **Calculate position error**
   ```cpp
   errAz = data.gimbalAz - m_smoothedTargetAz
   errEl = data.gimbalEl - m_smoothedTargetEl
   ```
   - Azimuth error normalized to [-180°, 180°]

7. **Compute PID output (feedback control)**
   ```cpp
   pidAzVelocity = PID(errAz, dt)
   pidElVelocity = PID(errEl, dt)
   ```

8. **Add feed-forward term (velocity compensation)**
   ```cpp
   desiredAzVelocity = pidAzVelocity + (0.3 * smoothedAzVel)
   desiredElVelocity = pidElVelocity + (0.3 * smoothedElVel)
   ```

9. **Apply velocity scaling** (reduce command when close to target)

10. **Clamp to system limits** → Enforce MAX_VELOCITY ±15 deg/s

11. **Apply rate limiting** → Prevent velocity jumps > 5 deg/s per cycle

12. **Send commands** → Call `sendStabilizedServoCommands()`

13. **Debug logging** → Every 10th cycle, print tracking info

---

## Private Helper Methods

### applyRateLimit(...)
```cpp
double applyRateLimit(double newVelocity, double previousVelocity, double maxChange)
```

**Purpose:** Limit velocity change to prevent sudden jerks.

**Logic:**
- If `|newVel - prevVel| > maxChange`, clamp the change to ±maxChange
- Otherwise, return newVel as-is

**Example:** With VELOCITY_CHANGE_LIMIT=5.0, velocity cannot jump more than 5 deg/s between cycles.

---

### applyVelocityScaling(...)
```cpp
double applyVelocityScaling(double velocity, double error)
```

**Purpose:** Reduce velocity when gimbal is close to target to improve accuracy.

**Constants:**
- ERROR_THRESHOLD = 0.20° (scale becomes active below this)
- MIN_SCALE = 0.30 (minimum 30% of commanded velocity)

**Behavior:**
- If `|error| >= 0.20°`: return velocity unchanged (100%)
- If `|error| < 0.20°`: scale down linearly from 100% → 30%
- Prevents overshoot when converging to target

**Known Issue:** MIN_SCALE of 30% is too conservative—prevents final convergence. Recommended: increase to 50%.

---

## Known Issues & Solutions

### Issue 1: Azimuth Motor Rotates Wrong Direction
**Symptom:** Motor spins opposite to target direction initially, then corrects.

**Cause:** PID integral term accumulated error in wrong direction before sign reversal.

**Solution:**
```cpp
m_azPid.integral = 0.0;  // Clear windup after changing error sign
```

Or ensure `pidReset()` is called in `enterMode()`.

---

### Issue 2: Elevation Tracking Too Slow, Doesn't Reach Center
**Symptoms:** Gimbal moves toward target but slowly, final position doesn't converge to exact center.

**Root Causes:**
- PID gains too conservative (Kp=0.15 very low)
- Velocity scaling too aggressive (MIN_SCALE=0.30)
- Feed-forward gain too low (0.30 = only 30% of target velocity)

**Tuning Recommendations:**
```cpp
// INCREASE PID GAINS
m_elPid.Kp = 0.4;      // from 0.15
m_elPid.Ki = 0.02;     // from 0.005
m_elPid.Kd = 0.05;     // from 0.01

// IMPROVE VELOCITY SCALING
ERROR_THRESHOLD = 0.50°;    // from 0.20° (more time to converge)
MIN_SCALE = 0.50;           // from 0.30 (allow 50% velocity at target)

// INCREASE FEED-FORWARD
FEEDFORWARD_GAIN = 0.6;     // from 0.3

// RELAX RATE LIMITING
VELOCITY_CHANGE_LIMIT = 10.0;  // from 5.0
```

---

## Data Flow Diagram

```
Target Tracker System
    ↓
onTargetPositionUpdated(az, el, vel_az, vel_el, isValid)
    ↓
[Store in m_target* and m_smoothedTarget*]
    ↓
update() called each cycle
    ├→ Apply low-pass filters to position/velocity
    ├→ Calculate error (gimbal position - target position)
    ├→ PID feedback controller → pidVelocity
    ├→ Add feed-forward velocity term
    ├→ Apply velocity scaling
    ├→ Apply rate limiting & max velocity clamps
    └→ sendStabilizedServoCommands(az_vel, el_vel)
        ↓
    GimbalController Servo Hardware
```

---

## Tuning Guidelines

### For Faster Tracking
1. Increase `Kp` (0.15 → 0.3-0.4)
2. Increase `FEEDFORWARD_GAIN` (0.3 → 0.6)
3. Increase `VELOCITY_CHANGE_LIMIT` (5 → 10)

### For Smoother/More Stable Tracking
1. Increase `SMOOTHING_ALPHA` damping (0.3 → 0.5)
2. Increase `Kd` slightly (0.01 → 0.05)
3. Decrease `VELOCITY_CHANGE_LIMIT` (5 → 2)

### For Precise Convergence
1. Increase `ERROR_THRESHOLD` (0.20 → 0.50)
2. Increase `MIN_SCALE` (0.30 → 0.50-0.75)
3. Add `Ki` integral term to eliminate steady-state error

---

## Testing Checklist

- [ ] Target acquisition works (m_targetValid becomes true)
- [ ] Azimuth rotates in correct direction toward target
- [ ] Elevation rotates in correct direction toward target
- [ ] Gimbal converges to target center (not hovering around it)
- [ ] Gimbal smoothly follows moving targets without jerking
- [ ] Gimbal stops when target is lost (isValid=false)
- [ ] PID integral is cleared when switching modes
- [ ] No servo overload or stalling (acceleration limits respected)
- [ ] Debug output shows consistent dt values

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with common servo control methods
- **GimbalController:** Manages gimbal hardware, servo objects, state model
- **SystemStateModel:** Provides current gimbal position/velocity state
- **PIDController:** (assumed to exist) Handles PID computation with anti-windup

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified integral windup and slow elevation tracking issues |

---

## Contact & Notes

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Active Development - Known tuning issues being addressed
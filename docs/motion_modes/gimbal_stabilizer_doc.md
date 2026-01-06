# GimbalStabilizer Class Documentation

**Version:** 1.0
**Last Updated:** 2026-01-06
**Status:** Production

---

## Overview

`GimbalStabilizer` implements AHRS-based gimbal stabilization using a velocity-domain control approach. It computes correction velocities that compensate for platform motion, allowing the gimbal to hold a world-frame pointing direction regardless of vehicle roll, pitch, and yaw.

**Location:** `src/controllers/motion_modes/GimbalStabilizer.h/cpp`

**Purpose:** Platform motion compensation for stabilized gimbal pointing using position (AHRS) and rate (gyro) feedback.

**Usage:** Shared static instance in `GimbalMotionModeBase`, called via `sendStabilizedServoCommands()`.

---

## Key Design Features

### Velocity-Domain Control
Unlike position-domain controllers that compute target angles, GimbalStabilizer outputs correction **velocities** that are added to the motion mode's desired velocity.

### Dual-Path Compensation
1. **Position Correction (AHRS):** PD control to correct drift from target world-frame direction
2. **Rate Feed-Forward (Gyro):** Kinematic cancellation of platform angular rates

### Matrix-Based Rotation
Uses Eigen matrices for proper 3D rotation, avoiding gimbal-lock issues near vertical pointing.

### Stateless Design
The stabilizer maintains minimal state (filtered values, previous errors) and can be reset cleanly when stabilization is disabled/enabled.

---

## Control Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    GimbalStabilizer                          │
└──────────────────────────────────────────────────────────────┘
                            │
            ┌───────────────┴───────────────┐
            │                               │
            ▼                               ▼
   ┌─────────────────┐            ┌─────────────────┐
   │ POSITION PATH   │            │ RATE PATH       │
   │ (AHRS-based)    │            │ (Gyro-based)    │
   └────────┬────────┘            └────────┬────────┘
            │                               │
            ▼                               ▼
   ┌─────────────────┐            ┌─────────────────┐
   │ World Target    │            │ Platform Rates  │
   │ (Az_w, El_w)    │            │ (p, q, r)       │
   └────────┬────────┘            └────────┬────────┘
            │                               │
            ▼                               ▼
   ┌─────────────────┐            ┌─────────────────┐
   │ calculateRequired│           │ Filter + Scale  │
   │ GimbalAngles()   │           │ (smooth ramp)   │
   └────────┬────────┘            └────────┬────────┘
            │                               │
            ▼                               ▼
   ┌─────────────────┐            ┌─────────────────┐
   │ Error:          │            │ Kinematic       │
   │ req - current   │            │ Transform:      │
   │                 │            │ platform→gimbal │
   └────────┬────────┘            └────────┬────────┘
            │                               │
            ▼                               ▼
   ┌─────────────────┐            ┌─────────────────┐
   │ PD Controller   │            │ Negative        │
   │ Kp×err + Kd×dErr│            │ Feed-Forward    │
   └────────┬────────┘            └────────┬────────┘
            │                               │
            └───────────────┬───────────────┘
                            │
                            ▼
               ┌────────────────────────┐
               │ Position Correction    │
               │ + Rate Feed-Forward    │
               │ = Total Correction     │
               └───────────┬────────────┘
                           │
                           ▼
               ┌────────────────────────┐
               │ Acceleration Limit     │
               │ (35 deg/s²)            │
               └───────────┬────────────┘
                           │
                           ▼
               ┌────────────────────────┐
               │ Output: (corrAz, corrEl)│
               │ Added to mode velocity │
               └────────────────────────┘
```

---

## Control Law

### Overall Equation
```
ω_correction = ω_position + ω_rate

where:
  ω_position = Kp × position_error + Kd × error_rate
  ω_rate = -f(p, q, r, Az_gimbal, El_gimbal)
```

### Position Correction (PD Control)

```cpp
// Calculate required gimbal angles for world-frame target
(reqAz, reqEl) = calculateRequiredGimbalAngles(
    targetAz_world, targetEl_world,
    roll, pitch, yaw
);

// Position error
errAz = reqAz - currentGimbalAz;
errEl = reqEl - currentGimbalEl;

// Error rate (filtered)
dErrAz = (errAz - prevErrAz) / dt;  // Then low-pass filtered

// PD output
positionCorrAz = Kp * errAz + Kd * dErrAz;
positionCorrEl = Kp * errEl + Kd * dErrEl;
```

### Rate Feed-Forward (Kinematic Cancellation)

```cpp
// Platform angular rates in body frame
double p = gyroX;  // Roll rate
double q = gyroY;  // Pitch rate
double r = gyroZ;  // Yaw rate

// Transform to gimbal frame
double az_rad = gimbalAz * DEG2RAD;
double el_rad = gimbalEl * DEG2RAD;

// Kinematic coupling equations
double platformEffectOnEl = q * cos(az_rad) - p * sin(az_rad);
double platformEffectOnAz = r + tan(el_rad) * (q * sin(az_rad) + p * cos(az_rad));

// Negative for cancellation
rateCorrAz = -platformEffectOnAz;
rateCorrEl = -platformEffectOnEl;
```

---

## Member Variables

### Filter State
| Variable | Type | Purpose |
|----------|------|---------|
| `m_filteredRoll` | `double` | Low-pass filtered AHRS roll |
| `m_filteredPitch` | `double` | Low-pass filtered AHRS pitch |
| `m_filteredYaw` | `double` | Low-pass filtered AHRS yaw |
| `m_prevErrAz` | `double` | Previous Az error (for D-term) |
| `m_prevErrEl` | `double` | Previous El error (for D-term) |
| `m_filteredDErrAz` | `double` | Filtered Az error rate |
| `m_filteredDErrEl` | `double` | Filtered El error rate |

### Output State
| Variable | Type | Purpose |
|----------|------|---------|
| `m_prevCorrAz` | `double` | Previous Az correction (for accel limit) |
| `m_prevCorrEl` | `double` | Previous El correction (for accel limit) |

### Configuration (from MotionTuningConfig)
| Parameter | Default | Purpose |
|-----------|---------|---------|
| `kpPosition` | 2.0 | Position error gain |
| `kdPosition` | 0.5 | Error rate damping |
| `ahrsFilterTau` | 0.1s | AHRS angle filter τ |
| `positionDeadbandDeg` | 0.02 | Error deadband |
| `maxVelocityCorr` | 5.0 | Max FF velocity |
| `maxTotalVel` | 12.0 | Max total correction |
| `maxErrorRate` | 10.0 | Max dErr clamp |

---

## Public Methods

### computeStabilizedVelocity(...)
```cpp
std::pair<double, double> computeStabilizedVelocity(
    const SystemStateData& data,
    double desiredAzVelocity,
    double desiredElVelocity,
    double dt,
    SystemStateModel* stateModel = nullptr  // For debug output
);
```

**Purpose:** Compute stabilization correction velocities.

**Parameters:**
- `data`: Current system state (gimbal angles, AHRS, gyro, world target)
- `desiredAzVelocity`: Motion mode's desired Az velocity [deg/s]
- `desiredElVelocity`: Motion mode's desired El velocity [deg/s]
- `dt`: Time since last call [seconds]
- `stateModel`: Optional, for updating debug display

**Returns:** Pair of (correctionAz, correctionEl) velocities [deg/s]

**Usage:**
```cpp
auto [corrAz, corrEl] = stabilizer.computeStabilizedVelocity(
    data, desiredAz, desiredEl, dt, stateModel
);
double finalAz = desiredAz + corrAz;
double finalEl = desiredEl + corrEl;
```

### reset()
```cpp
void reset();
```
**Purpose:** Clear all filter state and previous values.

**Call:** When stabilization is enabled or mode changes.

---

## Key Algorithms

### calculateRequiredGimbalAngles(...)
```cpp
std::pair<double, double> calculateRequiredGimbalAngles(
    double worldAz, double worldEl,
    double roll, double pitch, double yaw
);
```

**Purpose:** Convert world-frame target to gimbal-frame angles.

**Algorithm:**
```cpp
// Step 1: World target → unit vector
double cosEl = cos(worldEl * DEG2RAD);
double sinEl = sin(worldEl * DEG2RAD);
double cosAz = cos(worldAz * DEG2RAD);
double sinAz = sin(worldAz * DEG2RAD);

Eigen::Vector3d v_world;
v_world << cosEl * cosAz,
           cosEl * sinAz,
           sinEl;

// Step 2: Build platform rotation matrix (Rz * Ry * Rx)
Eigen::Matrix3d Rx, Ry, Rz;
// ... rotation matrices for roll, pitch, yaw ...
Eigen::Matrix3d R_platform = Rz * Ry * Rx;

// Step 3: Transform to platform frame
Eigen::Vector3d v_platform = R_platform.transpose() * v_world;

// Step 4: Extract gimbal angles
double gimbalAz = atan2(v_platform.y(), v_platform.x()) * RAD2DEG;
double gimbalEl = atan2(v_platform.z(),
    sqrt(v_platform.x()*v_platform.x() + v_platform.y()*v_platform.y())
) * RAD2DEG;

return {gimbalAz, gimbalEl};
```

### Rate Feed-Forward Scaling

```cpp
// Gyro rate magnitude
double gyroMag = sqrt(p*p + q*q + r*r);

// Smooth ramp from GYRO_DB (0.3) to GYRO_FULL (1.2) deg/s
double ffScale;
if (gyroMag < GYRO_DB) {
    ffScale = 0.0;  // Deadband
} else if (gyroMag > GYRO_FULL) {
    ffScale = 1.0;  // Full FF
} else {
    ffScale = (gyroMag - GYRO_DB) / (GYRO_FULL - GYRO_DB);
}

// Apply scaling
rateCorrAz *= ffScale;
rateCorrEl *= ffScale;
```

**Why Ramping?** Prevents noise amplification at low rates while providing full compensation at high rates.

---

## Position Deadband

```cpp
// Apply deadband to prevent chatter
if (std::abs(errAz) < POSITION_DEADBAND_DEG) errAz = 0.0;
if (std::abs(errEl) < POSITION_DEADBAND_DEG) errEl = 0.0;
```

**Default:** 0.02° (20 millidegrees)

**Purpose:** Prevents micro-corrections that cause servo heating and jitter.

---

## Acceleration Limiting

```cpp
static constexpr double MAX_ACCEL_DPS2 = 35.0;  // deg/s²

double maxChange = MAX_ACCEL_DPS2 * dt;
double changAz = totalCorrAz - m_prevCorrAz;

if (std::abs(changAz) > maxChange) {
    totalCorrAz = m_prevCorrAz + (changAz > 0 ? maxChange : -maxChange);
}
m_prevCorrAz = totalCorrAz;
```

**Purpose:** Prevents sudden velocity jumps that cause mechanical stress and overshoot.

---

## Debug Output

When `stateModel` is provided, stabilizer updates debug display:

```cpp
struct StabilizationDebug {
    double reqAz, reqEl;        // Required gimbal angles
    double errAz, errEl;        // Position errors
    double posCorrAz, posCorrEl;// Position correction
    double rateCorrAz, rateCorrEl; // Rate FF
    double totalCorrAz, totalCorrEl; // Final output
};

stateModel->setStabilizationDebug(debug);
```

Available in QML for OSD display.

---

## Tuning Guide

### For Tighter Holding (Less Drift)
```cpp
kpPosition = 3.0;     // Increase from 2.0
kdPosition = 0.8;     // Increase from 0.5
```

### For Smoother Motion (Less Jitter)
```cpp
ahrsFilterTau = 0.15; // Increase from 0.1
positionDeadbandDeg = 0.05; // Increase from 0.02
```

### For Faster Response to Platform Motion
```cpp
kpPosition = 2.5;
GYRO_DB = 0.2;        // Lower deadband
GYRO_FULL = 0.8;      // Earlier full FF
```

### For Heavy Gimbal (More Inertia)
```cpp
MAX_ACCEL_DPS2 = 25.0;  // Lower from 35.0
kdPosition = 0.7;       // More damping
```

---

## Integration with Motion Modes

### In GimbalMotionModeBase::sendStabilizedServoCommands()

```cpp
void sendStabilizedServoCommands(
    GimbalController* controller,
    double desiredAz, double desiredEl,
    bool enableStabilization,
    double dt
) {
    const SystemStateData& data = controller->systemStateModel()->data();

    double finalAz = desiredAz;
    double finalEl = desiredEl;

    if (enableStabilization && data.useWorldFrameTarget) {
        auto [corrAz, corrEl] = s_stabilizer.computeStabilizedVelocity(
            data, desiredAz, desiredEl, dt,
            controller->systemStateModel()
        );
        finalAz += corrAz;
        finalEl += corrEl;
    }

    // Clamp and send to servos
    finalAz = qBound(-MAX_VELOCITY, finalAz, MAX_VELOCITY);
    finalEl = qBound(-MAX_VELOCITY, finalEl, MAX_VELOCITY);

    writeVelocityCommandOptimized(azServo, GimbalAxis::Azimuth, finalAz, ...);
    writeVelocityCommandOptimized(elServo, GimbalAxis::Elevation, finalEl, ...);
}
```

### When Stabilization is Enabled

1. **Manual Mode:** Captures world-frame target when joystick centered
2. **Scan Modes:** Uses world-frame endpoints for stabilized scanning
3. **Tracking Mode:** Stabilization OFF (vision provides feedback)

---

## Testing Checklist

- [ ] `reset()` clears all state
- [ ] `calculateRequiredGimbalAngles()` correct for various platform orientations
- [ ] Position error computed correctly (req - current)
- [ ] D-term filtered properly (no noise amplification)
- [ ] Rate FF scales smoothly from deadband to full
- [ ] Kinematic transform correct for various gimbal angles
- [ ] Acceleration limiting prevents velocity jumps
- [ ] Total correction clamped to `maxTotalVel`
- [ ] Debug output updates correctly
- [ ] Works with platform tilted 30°+ roll/pitch
- [ ] Handles gimbal-lock region (near vertical) gracefully
- [ ] No oscillation at rest (deadband working)

---

## Known Issues

### Issue 1: Drift When Platform Yaws
**Cause:** Yaw has no absolute reference, only rate cancellation.

**Mitigation:** Rate FF cancels yaw motion; residual drift is compass-dependent.

### Issue 2: Oscillation Near Gimbal Lock
**Cause:** tan(el) → ∞ as el → 90°.

**Solution:** Clamp elevation-dependent terms:
```cpp
double tanEl = std::clamp(tan(el_rad), -10.0, 10.0);
```

### Issue 3: Jerky Motion at Mode Entry
**Cause:** Filter state not initialized.

**Solution:** Call `reset()` in `enterMode()`.

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Computation Time** | < 1ms | Per call |
| **Filter Latency** | ~100ms | AHRS filter settling |
| **Position Bandwidth** | ~2 Hz | Kp-dependent |
| **Rate FF Bandwidth** | ~10 Hz | Limited by gyro filter |
| **Max Correction Rate** | 12 deg/s | `maxTotalVel` |
| **Acceleration Limit** | 35 deg/s² | Smooth transitions |

---

## Related Classes

- **GimbalMotionModeBase:** Uses GimbalStabilizer via static instance
- **SystemStateModel:** Provides AHRS, gyro, world target data
- **MotionTuningConfig:** Runtime parameters

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-06 | 1.0 | Initial documentation |

---

**END OF DOCUMENTATION**

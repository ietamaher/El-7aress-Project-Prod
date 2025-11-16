# Motion Modes - Technical Reference Guide
## El 7arress RCWS - Gimbal Control System

**Version:** 1.0
**Author:** Motion Control Engineering Team
**Last Updated:** 2025-01-16
**Classification:** Technical Reference

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Motion Mode Fundamentals](#motion-mode-fundamentals)
3. [Manual Motion Mode](#manual-motion-mode)
4. [Tracking Motion Mode](#tracking-motion-mode)
5. [AutoSectorScan Motion Mode](#autosectorscan-motion-mode)
6. [TRP Scan Motion Mode](#trp-scan-motion-mode)
7. [Radar Slew Motion Mode](#radar-slew-motion-mode)
8. [AHRS Stabilization System](#ahrs-stabilization-system)
9. [Motion Tuning Parameters](#motion-tuning-parameters)
10. [Advanced Topics](#advanced-topics)

---

## System Architecture

### Gimbal Control Stack

```
┌──────────────────────────────────────────────────────────┐
│                  QML User Interface                       │
│            (Joystick, Mode Selection, Status)             │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│                  GimbalController                         │
│  - Mode Management                                        │
│  - System State Integration                               │
│  - Safety Interlocks                                      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│              GimbalMotionModeBase (Abstract)              │
│  - AHRS Stabilization                                     │
│  - Servo Command Writing                                  │
│  - PID Controllers                                        │
│  - Helper Functions                                       │
└────────────────────┬─────────────────────────────────────┘
                     │
       ┌─────────────┼─────────────┬─────────────┬─────────┐
       ▼             ▼             ▼             ▼         ▼
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ Manual   │  │ Tracking │  │AutoSector│  │TRP Scan  │  │Radar Slew│
│  Mode    │  │  Mode    │  │  Scan    │  │  Mode    │  │  Mode    │
└────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘
     │             │              │              │              │
     └─────────────┴──────────────┴──────────────┴──────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────┐
│            ServoDriverDevice (AZD-KD)                     │
│  - Modbus RTU Communication                               │
│  - Position/Velocity Control                              │
│  - Status Monitoring                                      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│         Physical Servo Motors & Encoders                  │
│  - Azimuth: 222500 steps/rev                              │
│  - Elevation: 200000 steps/rev                            │
└──────────────────────────────────────────────────────────┘
```

### Control Loop Timing

```
Main Control Loop (20 Hz - 50ms period)
├── Update IMU/AHRS (40 Hz interpolated to 20 Hz)
├── Update Vision Tracking (30 Hz interpolated to 20 Hz)
├── Motion Mode update()
│   ├── Compute desired velocities
│   ├── Apply AHRS stabilization
│   └── Send velocity commands to servos
└── Update System State Model (for QML UI @ 10 Hz throttled)
```

**Critical timing:**
- **Control loop:** 20 Hz (50ms) - **DO NOT CHANGE** `updateIntervalS = 0.05`
- **IMU sampling:** 40 Hz (native 3DM-GX3-25 rate)
- **Vision updates:** 30 Hz (camera frame rate)
- **UI updates:** 10 Hz (throttled to reduce QML overhead)

---

## Motion Mode Fundamentals

### Base Class: GimbalMotionModeBase

All motion modes inherit from `GimbalMotionModeBase` which provides:

#### 1. **Lifecycle Methods** (Virtual, Overridden by Derived Classes)

```cpp
virtual void enterMode(GimbalController* controller);  // Called on mode transition
virtual void exitMode(GimbalController* controller);   // Called on mode exit
virtual void update(GimbalController* controller);     // Called every 50ms
```

**Purpose:**
- `enterMode()`: Initialize mode state, reset PIDs, configure servos
- `update()`: Compute desired velocities, send commands
- `exitMode()`: Stop servos, cleanup

#### 2. **AHRS Stabilization** (Common to All Modes)

```cpp
void sendStabilizedServoCommands(
    GimbalController* controller,
    double desiredAzVelocity,      // World-frame desired velocity
    double desiredElVelocity,
    bool enableStabilization
);
```

**How it works:**
1. Motion mode computes desired world-frame velocity
2. AHRS stabilization calculates platform motion compensation
3. Final servo command = desired + compensation
4. Servo receives corrected velocity command

**Mathematical model:**
```
v_servo = v_desired + v_compensation

Where:
v_compensation = -K × (ω_platform × R)
  K = stabilization gain
  ω_platform = platform angular velocity (from gyros)
  R = rotation matrix (from Euler angles)
```

#### 3. **PID Controllers** (Shared Infrastructure)

```cpp
struct PIDController {
    double Kp, Ki, Kd;
    double integral;
    double maxIntegral;
    double previousError;
    double previousMeasurement;
};

double pidCompute(PIDController& pid, double error, double dt);
```

**PID equation (derivative-on-measurement):**
```
P = Kp × error
I = Ki × ∫(error) dt    [clamped to maxIntegral]
D = Kd × d(measurement)/dt  [NOT d(error)/dt to avoid derivative kick]

output = P + I + D
```

**Why derivative-on-measurement?**
- Avoids "derivative kick" when setpoint changes
- Smoother response to target jumps
- Better for tracking modes

#### 4. **Helper Functions** (Static Utilities)

```cpp
// Time-aware filter coefficient
static double alphaFromTauDt(double tau, double dt);

// Physics-based rate limiting
static double applyRateLimitTimeBased(double desired, double prev, double maxDelta);

// Safe time step clamping
static double clampDt(double dt);
```

---

## Manual Motion Mode

### Purpose
Direct joystick control of gimbal with smooth acceleration ramping and world-frame stabilization.

### State Diagram

```
┌─────────────────────────────────────────────────────┐
│                  MANUAL MODE                        │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │  INITIALIZE   │
               │ - Reset speeds│
               │ - Start timer │
               └───────┬───────┘
                       │
        ┌──────────────┴──────────────┐
        │       update() loop         │
        │      (every 50ms)           │
        └──────────────┬──────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Read Joystick Input        │
        │   (azimuth & elevation)      │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Apply Joystick Filter      │
        │   (exponential smoothing)    │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Apply Power Curve          │
        │   (for fine control)         │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Convert to Target Speed    │
        │   (Hz domain)                │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Apply Acceleration Limit   │
        │   (time-based rate limiting) │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Convert Hz → deg/s         │
        │   (using steps_per_degree)   │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Update World-Frame Target  │
        │   (for stabilization hold)   │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Send Stabilized Commands   │
        │   (AHRS compensation)        │
        └──────────────┬───────────────┘
                       │
                       └──────────┐
                                  │
                       exitMode() │
                                  ▼
                       ┌──────────────┐
                       │  STOP SERVOS │
                       └──────────────┘
```

### Algorithm Details

#### Step 1: Joystick Filtering

```cpp
// Runtime-configurable time constant (from motion_tuning.json)
double tau = cfg.filters.manualJoystickTau;  // Default: 0.08s

// Compute filter coefficient using measured dt
double alpha = alphaFromTauDt(tau, dt);

// Apply exponential filter
filteredJoystick = alpha × rawJoystick + (1 - alpha) × filteredJoystick_prev;
```

**Purpose:** Smooth out noise and quantization from joystick ADC

**Effect of tau:**
- **Low tau (0.05s):** Fast, responsive, may feel twitchy
- **High tau (0.15s):** Smooth, damped, may feel sluggish

#### Step 2: Power Curve (Fine Control)

```cpp
double shapedInput = std::copysign(
    std::pow(std::abs(filteredJoystick), POWER_CURVE_EXPONENT),
    filteredJoystick
);
```

**Purpose:** Provide finer control near center, full power at extremes

**Typical exponent:** 1.5-2.0
- Exponent = 1.0 → Linear response
- Exponent = 2.0 → Quadratic (very fine center, aggressive edges)

#### Step 3: Speed Calculation

```cpp
static constexpr double MAX_SPEED_HZ = 25000.0;  // Servo max speed

double speedPercent = systemState.gimbalSpeed / 100.0;  // From UI slider
double maxCurrentSpeedHz = speedPercent × MAX_SPEED_HZ;

double targetSpeedHz = shapedInput × maxCurrentSpeedHz;
```

**Why Hz domain?**
- Servo drivers operate in Hz (steps/second)
- Direct control without multiple conversions
- Matches hardware native units

#### Step 4: Acceleration Limiting (Critical!)

```cpp
// From motion_tuning.json
double maxAccelHz = cfg.manualLimits.maxAccelHzPerSec;  // e.g., 500000 Hz/s

// Time-based limit (physics-correct)
double maxChangeHz = maxAccelHz × dt;  // dt ≈ 0.05s

// Clamp velocity change
currentSpeedHz = applyRateLimitTimeBased(targetSpeedHz, prevSpeedHz, maxChangeHz);
```

**Why time-based?**
- Fixed per-cycle limits fail with variable dt
- Ensures consistent acceleration regardless of timer jitter
- Physical units: Hz/s = acceleration

#### Step 5: Unit Conversion

```cpp
// From motion_tuning.json
double azStepsPerDeg = cfg.servo.azStepsPerDegree;  // 618.0556

// Convert servo Hz → gimbal deg/s
double velocityDegS = currentSpeedHz / azStepsPerDeg;
```

#### Step 6: World-Frame Target Management

```cpp
if (joystickActive) {
    // Joystick moving - update world-frame target to current direction
    // This ensures gimbal holds current pointing when joystick released
    convertGimbalToWorldFrame(gimbalAz, gimbalEl, roll, pitch, yaw,
                              worldAz, worldEl);

    systemStateModel->setWorldFrameTarget(worldAz, worldEl);
} else {
    // Joystick released - stabilization holds last world-frame target
    // (automatic hold, no additional logic needed)
}
```

**Key insight:**
Continuous world-frame target updates create "hold-on-release" behavior

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `joystickTau` | `filters.manual` | 0.08s | Joystick smoothing |
| `maxAccelHzPerSec` | `accelLimits` | 500000 Hz/s | Acceleration limit |

### Common Issues & Solutions

**Problem:** Joystick feels jerky
- **Cause:** Low joystick tau or high acceleration
- **Fix:** Increase `joystickTau` to 0.10-0.12s

**Problem:** Joystick feels sluggish
- **Cause:** High joystick tau or low acceleration
- **Fix:** Decrease `joystickTau` to 0.06s, increase `maxAccelHzPerSec`

---

## Tracking Motion Mode

### Purpose
Vision-based automatic target following with feedforward velocity compensation and lead angle support.

### State Diagram

```
┌─────────────────────────────────────────────────────┐
│                 TRACKING MODE                       │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │  INITIALIZE   │
               │ - Reset PIDs  │
               │ - Invalid     │
               └───────┬───────┘
                       │
        ┌──────────────┴──────────────┐
        │  onTargetPositionUpdated()  │
        │  (Qt::QueuedConnection)     │
        │  From vision thread         │
        └──────────────┬──────────────┘
                       │
                       ▼
                ┌──────────────┐
                │ Target Valid?│
                └──────┬───────┘
                       │
            ┌──────────┴──────────┐
            │ NO                  │ YES
            ▼                     ▼
    ┌───────────────┐     ┌────────────────┐
    │  WAITING      │     │  TRACKING      │
    │ (Stop servos) │     │  (Active)      │
    └───────────────┘     └───────┬────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │       update() loop       │
                    │      (every 50ms)         │
                    └─────────────┬─────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Smooth Target Position      │
                    │  (exponential filter)        │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Smooth Target Velocity      │
                    │  (exponential filter)        │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Apply Lead Angle            │
                    │  (ballistic compensation)    │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Compute Position Error      │
                    │  (aim_point - gimbal_pos)    │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  PID Control (Feedback)      │
                    │  (derivative on measurement) │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Add Feedforward Velocity    │
                    │  (target motion prediction)  │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Apply Velocity Limit        │
                    │  (maxVelocityDegS)           │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Apply Acceleration Limit    │
                    │  (time-based rate limiting)  │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Convert to World Frame      │
                    │  (for AHRS stabilization)    │
                    └──────────────┬───────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────────┐
                    │  Send Stabilized Commands    │
                    └──────────────────────────────┘
```

### Algorithm Details

#### Thread Safety (Critical!)

```cpp
// Vision thread (30 Hz) → Controller thread (20 Hz)
// Uses Qt::QueuedConnection to prevent race conditions

// In GimbalController:
connect(this, &GimbalController::trackingTargetUpdated,
        trackingMode, &TrackingMotionMode::onTargetPositionUpdated,
        Qt::QueuedConnection);  // ← CRITICAL: Queued, not direct!

// Vision thread emits (safe):
emit trackingTargetUpdated(az, el, velAz, velEl, valid);

// Controller thread receives (safe):
void TrackingMotionMode::onTargetPositionUpdated(...) {
    m_targetAz = az;  // Thread-safe update
    m_targetValid = valid;
}
```

**Why queued connection?**
- Vision processing runs in separate thread
- Direct call would cause race condition
- Queued ensures thread-safe delivery via event loop

#### Step 1: Target Smoothing (Noise Rejection)

```cpp
// Runtime-configurable time constants
double tau_pos = cfg.filters.trackingPositionTau;  // 0.12s
double tau_vel = cfg.filters.trackingVelocityTau;  // 0.08s

// Compute adaptive filter coefficients
double alphaPos = alphaFromTauDt(tau_pos, dt);
double alphaVel = alphaFromTauDt(tau_vel, dt);

// Exponential smoothing
smoothedAz = alphaPos × rawAz + (1 - alphaPos) × smoothedAz_prev;
smoothedVel = alphaVel × rawVel + (1 - alphaVel) × smoothedVel_prev;
```

**Why smooth position AND velocity separately?**
- Position: Remove vision jitter (box jumping)
- Velocity: Remove numerical differentiation noise
- Different time constants optimize each

#### Step 2: Lead Angle Compensation (Ballistic Solution)

```cpp
double aimPointAz = smoothedTargetAz;
double aimPointEl = smoothedTargetEl;

if (leadAngleActive && leadAngleStatus == On) {
    // Add ballistic lead from fire control computer
    aimPointAz += leadAngleOffsetAz;  // Calculated by FCS
    aimPointEl += leadAngleOffsetEl;
}
```

**Critical insight:**
- Vision gives **where target IS**
- Lead angle gives **where target WILL BE** when bullet arrives
- Gimbal aims at **aim point** (with lead), not visual target

**Lead angle calculation** (done by separate fire control system):
```
lead = (target_velocity × time_of_flight) + gravity_drop + wind_drift
```

#### Step 3: PID Control (Feedback)

```cpp
// Position error (aim point, not visual target!)
double errorAz = aimPointAz - currentGimbalAz;
double errorEl = aimPointEl - currentGimbalEl;

// Normalize azimuth to [-180, 180]
while (errorAz > 180.0) errorAz -= 360.0;
while (errorAz < -180.0) errorAz += 360.0;

// PID with derivative-on-measurement
double pidAz = pidCompute(m_azPid, errorAz, aimPointAz, currentAz, true, dt);
double pidEl = pidCompute(m_elPid, errorEl, aimPointEl, currentEl, true, dt);
```

**Why derivative-on-measurement?**
```
Standard PID:  D = Kd × d(error)/dt
Problem: When aim point jumps (new target), derivative spikes

Derivative-on-measurement:  D = -Kd × d(measurement)/dt
Benefit: Smooth response to aim point changes
```

#### Step 4: Feedforward Velocity (Prediction)

```cpp
static const double FEEDFORWARD_GAIN = 0.5;  // Scale factor

double desiredAzVel = pidAz + (FEEDFORWARD_GAIN × smoothedAzVel);
double desiredElVel = pidEl + (FEEDFORWARD_GAIN × smoothedElVel);
```

**Control architecture:**
```
        ┌─────────────────┐
        │  Target Motion  │
        └────────┬────────┘
                 │
         ┌───────┴────────┐
         │                │
         ▼                ▼
    ┌────────┐      ┌──────────┐
    │ PID    │      │Feedforward│
    │(Error) │      │(Velocity) │
    └────┬───┘      └─────┬────┘
         │                │
         └────────┬───────┘
                  │
                  ▼
           ┌────────────┐
           │ Servo Cmd  │
           └────────────┘
```

**Benefits:**
- PID handles position error (feedback)
- Feedforward predicts target motion (proactive)
- Faster tracking, reduced lag

**Why 0.5 gain?**
- Full feedforward (1.0) can amplify velocity estimation noise
- Half gain (0.5) balances response vs. stability

#### Step 5: Rate Limiting

```cpp
// From motion_tuning.json
double maxAccel = cfg.motion.maxAccelerationDegS2;  // 50 deg/s²
double maxVel = cfg.motion.maxVelocityDegS;         // 30 deg/s

// Velocity limit
desiredAzVel = qBound(-maxVel, desiredAzVel, maxVel);

// Acceleration limit (physics-based)
double maxDelta = maxAccel × dt;
desiredAzVel = applyRateLimitTimeBased(desiredAzVel, prevVel, maxDelta);
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `positionTau` | `filters.tracking` | 0.12s | Target position smoothing |
| `velocityTau` | `filters.tracking` | 0.08s | Target velocity smoothing |
| `tracking.azimuth.kp` | `pid.tracking` | 1.0 | Position error gain |
| `tracking.azimuth.ki` | `pid.tracking` | 0.005 | Integral gain |
| `tracking.azimuth.kd` | `pid.tracking` | 0.01 | Damping gain |
| `maxAccelerationDegS2` | `motion` | 50.0 | Acceleration limit |

### Common Issues & Solutions

**Problem:** Tracking oscillates on stationary target
- **Cause:** Kp too high or position filter too low
- **Fix:** Reduce Kp to 0.8, increase positionTau to 0.15s

**Problem:** Tracking lags behind moving target
- **Cause:** Position/velocity tau too high or Kp too low
- **Fix:** Decrease velocityTau to 0.05s, increase Kp to 1.5

**Problem:** Jerky tracking motion
- **Cause:** Kd too high amplifying velocity noise
- **Fix:** Reduce Kd to 0.005, increase velocityTau

---

## AutoSectorScan Motion Mode

### Purpose
Automated surveillance scanning between two waypoints with smooth constant-velocity cruise and PID-controlled deceleration.

### State Diagram

```
┌─────────────────────────────────────────────────────┐
│            AUTO SECTOR SCAN MODE                    │
└─────────────────────────────────────────────────────┘
                       │
                       │ setActiveScanZone()
                       │ (zone parameters loaded)
                       ▼
               ┌───────────────┐
               │  INITIALIZE   │
               │ - Point1 → 2  │
               │ - Reset PIDs  │
               └───────┬───────┘
                       │
                       │ enterMode()
                       ▼
        ┌──────────────────────────────┐
        │   MOVING TO POINT 2          │
        │   (or Point 1 if reversed)   │
        └──────────────┬───────────────┘
                       │
        ┌──────────────┴──────────────┐
        │       update() loop         │
        │      (every 50ms)           │
        └──────────────┬──────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Compute Distance to Target │
        │   (2D Euclidean)             │
        └──────────────┬───────────────┘
                       │
                       ▼
              ┌────────────────┐
              │ Distance Check │
              └────────┬───────┘
                       │
         ┌─────────────┼─────────────┐
         │             │             │
         │ < arrival   │ < decel     │ > decel
         │ threshold   │ distance    │ distance
         ▼             ▼             ▼
   ┌─────────┐  ┌──────────┐  ┌──────────────┐
   │ ARRIVED │  │   DECEL  │  │   CRUISE     │
   │(Switch  │  │ (PID     │  │ (Const vel)  │
   │ target) │  │  control)│  │              │
   └────┬────┘  └────┬─────┘  └──────┬───────┘
        │            │               │
        │            │               └──→ Reset PID integrators
        │            │
        └────────────┴────────────────┐
                                      │
                                      ▼
                        ┌──────────────────────────┐
                        │  Apply Rate Limiting     │
                        │  (time-based accel)      │
                        └──────────┬───────────────┘
                                  │
                                  ▼
                        ┌──────────────────────────┐
                        │  Send Stabilized Cmd     │
                        └──────────────────────────┘
                                  │
                                  └──→ Loop back to update()
```

### Algorithm Details

#### Scan Zone Configuration

```cpp
struct AutoSectorScanZone {
    int id;
    bool isEnabled;
    float az1, el1;        // Waypoint 1 (degrees)
    float az2, el2;        // Waypoint 2 (degrees)
    float scanSpeed;       // Cruise speed (deg/s)
    QString name;
};
```

#### Motion Profile (Cruise + Deceleration)

```
Velocity
   │
   │    ┌──────────────────────────┐  ← Cruise phase (const velocity)
   │    │                          │
   │    │                          │
   │   ╱│                          │╲   ← Deceleration phase (PID control)
   │  ╱ │                          │ ╲
   │ ╱  │                          │  ╲
   │╱   │                          │   ╲
   └────┴──────────────────────────┴────┴──→ Distance from target
        │                          │    │
     Start                      Decel  Arrival
                              Distance Threshold
```

**Key insight:**
Two-phase control provides:
- **Cruise:** Smooth, energy-efficient, predictable motion
- **Deceleration:** Precise stopping without overshoot

#### Step 1: Deceleration Distance Calculation (Kinematics)

```cpp
// From motion_tuning.json
double accel = cfg.motion.scanMaxAccelDegS2;  // 20 deg/s²
double speed = activeScanZone.scanSpeed;       // e.g., 20 deg/s

// Physics: d = v² / (2a)
double decelDist = (speed × speed) / (2.0 × accel);

// Example: 20 deg/s @ 20 deg/s² → 10 degrees deceleration distance
```

**Can override from config:**
```json
"autoSectorScan": {
  "decelerationDistanceDeg": 5.0  // Manual override if needed
}
```

#### Step 2: Distance to Target

```cpp
// Current target (toggles between Point1 and Point2)
double targetAz = movingToPoint2 ? zone.az2 : zone.az1;
double targetEl = movingToPoint2 ? zone.el2 : zone.el1;

// Position error
double errAz = targetAz - currentGimbalAz;
double errEl = targetEl - currentGimbalEl;  // Uses IMU pitch!

// 2D Euclidean distance
double distance = std::hypot(errAz, errEl);
```

**Important:** Elevation uses **IMU pitch**, not gimbal encoder
- Compensates for platform tilt
- Maintains level horizon scanning

#### Step 3: Motion State Machine

```cpp
if (distance < arrivalThreshold) {
    // ARRIVED - Switch to opposite waypoint
    movingToPoint2 = !movingToPoint2;
    targetAz = movingToPoint2 ? zone.az2 : zone.az1;
    targetEl = movingToPoint2 ? zone.el2 : zone.el1;

} else if (zone.scanSpeed <= 0) {
    // HOLD POSITION (scanSpeed = 0)
    desiredAzVel = pidCompute(m_azPid, errAz, dt);
    desiredElVel = pidCompute(m_elPid, errEl, dt);

} else if (distance < decelDist) {
    // DECELERATION ZONE
    desiredAzVel = pidCompute(m_azPid, errAz, dt);
    desiredElVel = pidCompute(m_elPid, errEl, dt);

} else {
    // CRUISE ZONE
    double dirAz = errAz / distance;  // Unit vector
    double dirEl = errEl / distance;

    desiredAzVel = dirAz × zone.scanSpeed;
    desiredElVel = dirEl × zone.scanSpeed;

    // CRITICAL: Reset PID integrators to prevent windup
    m_azPid.integral = 0.0;
    m_elPid.integral = 0.0;
}
```

**Why reset integrators in cruise?**
- PID not active during cruise
- Integral would wind up (accumulate error)
- Causes overshoot when entering decel zone
- Solution: Reset integral = 0 during cruise

#### Step 4: Critical Bug Fix - Magic 0.1 Multiplier

```cpp
// ❌ OLD CODE (BUG):
desiredAzVel = dirAz × zone.scanSpeed × 0.1;  // 10x too slow!

// ✅ FIXED CODE:
desiredAzVel = dirAz × zone.scanSpeed;  // Correct
```

**Impact of bug:**
- User sets scanSpeed = 20 deg/s
- Actual scan speed = 2 deg/s (10x slower!)
- Caused confusion during field trials

#### Step 5: Acceleration Limiting

```cpp
double maxDelta = cfg.motion.scanMaxAccelDegS2 × dt;

desiredAzVel = applyRateLimitTimeBased(desiredAzVel, prevVel, maxDelta);
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `scanMaxAccelDegS2` | `motion` | 20.0 | Deceleration rate |
| `decelerationDistanceDeg` | `autoSectorScan` | (auto) | Start decel distance |
| `arrivalThresholdDeg` | `autoSectorScan` | 0.2 | Waypoint tolerance |
| `autoScanAz.kp/ki/kd` | `pid.autoSectorScan` | 1.0/0.01/0.05 | Decel PID gains |

### Common Issues & Solutions

**Problem:** Never reaches waypoints, oscillates near endpoint
- **Cause:** Arrival threshold too tight
- **Fix:** Increase `arrivalThresholdDeg` to 0.5 deg

**Problem:** Overshoots waypoints
- **Cause:** Deceleration distance too short or Kd too low
- **Fix:** Increase `decelerationDistanceDeg` or increase Kd

**Problem:** Scan speed is 10x slower than expected
- **Cause:** Old codebase with 0.1 multiplier bug
- **Fix:** Ensure using latest code without multiplier

---

## TRP Scan Motion Mode

### Purpose
Sequential point-to-point navigation through user-defined Target Reference Points (TRPs) with configurable halt times.

### State Diagram

```
┌─────────────────────────────────────────────────────┐
│              TRP SCAN MODE                          │
└─────────────────────────────────────────────────────┘
                       │
                       │ setActiveTRPPage()
                       │ (load TRP waypoints)
                       ▼
               ┌───────────────┐
               │  INITIALIZE   │
               │ - Index = 0   │
               │ - State:Moving│
               └───────┬───────┘
                       │
                       │ enterMode()
                       ▼
        ┌──────────────────────────────┐
        │        STATE: MOVING         │
        │  (Navigate to current TRP)   │
        └──────────────┬───────────────┘
                       │
        ┌──────────────┴──────────────┐
        │       update() loop         │
        │      (every 50ms)           │
        └──────────────┬──────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Compute Distance to TRP    │
        │   (2D Euclidean)             │
        └──────────────┬───────────────┘
                       │
                       ▼
              ┌────────────────┐
              │ Distance Check │
              └────────┬───────┘
                       │
         ┌─────────────┼─────────────┐
         │             │             │
         │ < arrival   │ < decel     │ > decel
         │             │             │
         ▼             ▼             ▼
   ┌─────────────┐ ┌──────────┐ ┌──────────────┐
   │   ARRIVED   │ │  DECEL   │ │   CRUISE     │
   │ (Stop &     │ │ (PID)    │ │ (Const vel)  │
   │  Halt)      │ │          │ │              │
   └──────┬──────┘ └──────────┘ └──────────────┘
          │
          ▼
   ┌──────────────────┐
   │ STATE: HALTED    │
   │ (Wait haltTime)  │
   └────────┬─────────┘
            │
            │ Timer expires
            ▼
   ┌─────────────────────┐
   │ Index++             │
   │ (Next TRP)          │
   └────────┬────────────┘
            │
            ▼
   ┌─────────────────────┐
   │ More TRPs?          │
   └────────┬────────────┘
            │
     ┌──────┴──────┐
     │ YES         │ NO
     ▼             ▼
 ┌────────┐  ┌──────────┐
 │ MOVING │  │   IDLE   │
 │(to next│  │(Scan done│
 │  TRP)  │  │ Stop)    │
 └────────┘  └──────────┘
```

### TRP Data Structure

```cpp
struct TargetReferencePoint {
    int id;                // Unique identifier
    int locationPage;      // Page number (organizational)
    int trpInPage;         // TRP number within page
    float azimuth;         // Position (degrees)
    float elevation;       // Position (degrees)
    float haltTime;        // Dwell time at TRP (seconds)
};
```

**Note:** No per-point speed - uses global `TRP_DEFAULT_TRAVEL_SPEED`

### Algorithm Details

#### TRP Page Loading

```cpp
// User selects "Location Page 3"
// GimbalController filters all TRPs for that page:

std::vector<TargetReferencePoint> pageToScan;
for (const auto& trp : allTRPs) {
    if (trp.locationPage == activePageNum) {
        pageToScan.push_back(trp);
    }
}

trpMode->setActiveTRPPage(pageToScan);
```

**Result:** Sequential list of TRPs to visit in order

#### Motion Profile (Same as AutoSectorScan)

```cpp
// From motion_tuning.json
double travelSpeed = cfg.motion.trpDefaultTravelSpeed;  // 15 deg/s
double accel = cfg.motion.trpMaxAccelDegS2;             // 50 deg/s²

// Compute deceleration distance
double decelDist = (travelSpeed × travelSpeed) / (2.0 × accel);
// Example: 15²/(2×50) = 2.25 degrees

// Can override:
if (cfg.trpScanParams.decelerationDistanceDeg > 0) {
    decelDist = cfg.trpScanParams.decelerationDistanceDeg;  // e.g., 3.0
}
```

#### Critical Bug Fix - Hard-Coded Speed

```cpp
// ❌ OLD CODE (BUG):
double travelSpeed = 15;  // Hard-coded, ignored user config!

// ✅ FIXED CODE:
double travelSpeed = cfg.motion.trpDefaultTravelSpeed;  // Runtime-configurable
```

**Impact:**
- Users couldn't adjust TRP travel speed
- Always stuck at 15 deg/s
- Now configurable via JSON without rebuild

#### Halt State Logic

```cpp
if (distance < arrivalThreshold) {
    qDebug() << "Arrived at TRP" << currentTrpIndex;

    // Stop servos
    stopServos(controller);

    // Transition to HALTED state
    currentState = State::Halted;

    // Start halt timer
    haltTimer.start();

    return;  // Exit update() until timer expires
}

// In next update() call:
if (currentState == State::Halted) {
    if (haltTimer.elapsed() >= targetTrp.haltTime × 1000) {
        // Halt complete - move to next TRP
        currentTrpIndex++;

        if (currentTrpIndex >= trpPage.size()) {
            // All TRPs complete
            currentState = State::Idle;
            stopServos(controller);
        } else {
            // More TRPs remaining
            currentState = State::Moving;
            resetPIDs();
        }
    }
    return;  // Still halting, don't send servo commands
}
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `trpDefaultTravelSpeed` | `motion` | 15.0 | Point-to-point speed |
| `trpMaxAccelDegS2` | `motion` | 50.0 | Acceleration |
| `decelerationDistanceDeg` | `trpScanParams` | (auto) | Decel distance |
| `arrivalThresholdDeg` | `trpScanParams` | 0.1 | Waypoint tolerance |
| `trpScanAz.kp/ki/kd` | `pid.trpScan` | 1.2/0.015/0.08 | PID gains |

### Common Issues & Solutions

**Problem:** TRP scan never advances to next point
- **Cause:** Never reaches "arrived" condition
- **Fix:** Increase `arrivalThresholdDeg` to 0.15-0.2 deg

**Problem:** Overshoots TRPs
- **Cause:** Too fast or insufficient deceleration
- **Fix:** Decrease `trpDefaultTravelSpeed` or increase `trpMaxAccelDegS2`

**Problem:** Halt time not working
- **Cause:** `haltTime` in TRP data is 0
- **Fix:** Set haltTime in zone definition UI

---

## Radar Slew Motion Mode

### Purpose
Fast slewing to radar-detected target coordinates for immediate engagement.

### Characteristics
- **Fastest mode** (highest PID gains)
- **Point-and-shoot** (no dwell, no scanning)
- **Single-shot** (slew once, then idle)

### Algorithm

Very similar to TRP scan, but:
- **Higher PID gains** (Kp=1.5, Ki=0.08, Kd=0.15)
- **No halt state** (arrives and stops)
- **Single target** (not a sequence)

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `radarSlewAz.kp/ki/kd` | `pid.radarSlew` | 1.5/0.08/0.15 | Aggressive PID |

---

## AHRS Stabilization System

### Purpose
Compensate for platform motion (ship roll/pitch/yaw) to maintain world-frame pointing direction.

### Physical Principle

```
When platform rotates, gimbal must counter-rotate to maintain world-frame direction

Example:
- Ship rolls 5° right
- Gimbal must roll 5° left
- Result: Camera maintains horizon level
```

### Implementation

#### Gyro-Based Velocity Compensation

```cpp
void calculateHybridStabilizationCorrection(
    const SystemStateData& state,
    double& velocityCorrectionAz_dps,
    double& velocityCorrectionEl_dps
) {
    // 1. Measure dt
    double dt = clampDt(velocityTimer.restart() / 1000.0);

    // 2. Get bias-corrected gyro readings (deg/s)
    double gx = state.GyroX - gyroBiasX;
    double gy = state.GyroY - gyroBiasY;
    double gz = state.GyroZ - gyroBiasZ;

    // 3. Low-pass filter (runtime cutoff from config)
    double gx_f = gyroXFilter.updateWithDt(gx, dt);
    double gy_f = gyroYFilter.updateWithDt(gy, dt);
    double gz_f = gyroZFilter.updateWithDt(gz, dt);

    // 4. Map IMU frame → body frame
    // IMU: X=forward, Y=right, Z=down
    // Body: p=roll, q=pitch, r=yaw
    double p_dps = gx_f;  // Roll rate
    double q_dps = gy_f;  // Pitch rate
    double r_dps = gz_f;  // Yaw rate

    // 5. Compute rotation matrix derivatives
    // (Complex math - see code for details)

    // 6. Negate to get compensation
    // Platform rotates +5°/s right → gimbal commands -5°/s left
    velocityCorrectionAz_dps = -correctionAz;
    velocityCorrectionEl_dps = -correctionEl;
}
```

### Gyro Filter Cutoff Tuning

```json
"filters": {
  "gyro": {
    "cutoffFreqHz": 5.0  // Configurable!
  }
}
```

**Effect:**
- **Lower (2-3 Hz):** Smoother stabilization, slower response to platform motion
- **Higher (10-20 Hz):** Faster response, more noise

**When to adjust:**
- Platform has high-frequency vibration → **Lower** to 3 Hz
- Stabilization feels laggy on fast maneuvers → **Raise** to 7-10 Hz

---

## Motion Tuning Parameters

### Complete Parameter Reference

See `config/motion_tuning.json` for full structure.

**Key sections:**
1. **filters:** Smoothing time constants
2. **motion:** Acceleration/velocity limits
3. **servo:** Hardware conversion constants (**DO NOT CHANGE**)
4. **pid:** Controller gains per mode
5. **accelLimits:** Manual mode acceleration

### Validation on Startup

```cpp
// In main.cpp:
MotionTuningConfig::load("./config/motion_tuning.json");
ConfigurationValidator::validateAll();

// If validation fails, app exits with errors shown
```

**Range checks:**
- Kp: (0, 10]
- Ki: [0, 1.0]
- Kd: [0, 2.0]
- Acceleration: [1, 200] deg/s²
- Velocity: [1, 120] deg/s

---

## Advanced Topics

### Time-Based vs. Cycle-Based Control

**OLD (broken):**
```cpp
// Fixed per-cycle limit
const double ACCEL_LIMIT = 2.0;  // deg/s per cycle
velocity = prev + ACCEL_LIMIT;
```

**Problem:** Assumes fixed 50ms update
- If update takes 55ms → under-accelerates
- If update takes 45ms → over-accelerates

**NEW (correct):**
```cpp
// Time-based limit
const double ACCEL = 50.0;  // deg/s²
double maxChange = ACCEL × dt;  // Physics-correct
velocity = applyRateLimitTimeBased(target, prev, maxChange);
```

**Benefit:** Consistent acceleration regardless of timer jitter

### Derivative-on-Measurement

**Standard PID:**
```cpp
double error = setpoint - measurement;
double d_term = Kd × (error - prev_error) / dt;
```

**Problem:** Setpoint step causes derivative spike

**Derivative-on-Measurement:**
```cpp
double error = setpoint - measurement;
double d_term = -Kd × (measurement - prev_measurement) / dt;
```

**Benefit:** Smooth response to setpoint changes

### Integral Windup Protection

**Problem:**
```
Large error (target far away)
→ Integral accumulates
→ Integral = 500
→ Target reached, but integral still huge
→ Massive overshoot
```

**Solution:**
```cpp
integral += error × dt;
integral = qBound(-maxIntegral, integral, maxIntegral);  // Clamp!
```

**Effect:** Limits integral contribution, prevents windup

---

## Conclusion

This guide provides deep technical insight into the El 7arress RCWS motion control system. For field tuning procedures, see the Motion Tuning Guide.

**For support:**
- Email: support@el7aress.mil
- Docs: ./docs/motion_modes/

---

**END OF TECHNICAL REFERENCE**

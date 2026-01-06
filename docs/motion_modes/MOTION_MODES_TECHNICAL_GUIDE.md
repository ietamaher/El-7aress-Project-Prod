# Motion Modes - Technical Reference Guide
## El 7arress RCWS - Gimbal Control System

**Version:** 2.0
**Author:** Motion Control Engineering Team
**Last Updated:** 2026-01-06
**Classification:** Technical Reference

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Motion Mode Fundamentals](#motion-mode-fundamentals)
3. [Safety Architecture](#safety-architecture)
4. [Manual Motion Mode](#manual-motion-mode)
5. [Tracking Motion Mode](#tracking-motion-mode)
6. [AutoSectorScan Motion Mode](#autosectorscan-motion-mode)
7. [TRP Scan Motion Mode](#trp-scan-motion-mode)
8. [GimbalStabilizer System](#gimbalstabilizer-system)
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
│  - dt Measurement (centralized)                           │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│              GimbalMotionModeBase (Abstract)              │
│  - Template Method Pattern (Safety Enforcement)           │
│  - GimbalStabilizer Integration                           │
│  - Axis-Specific Servo Command Optimization               │
│  - PID Controllers                                        │
└────────────────────┬─────────────────────────────────────┘
                     │
       ┌─────────────┼─────────────┬─────────────┐
       ▼             ▼             ▼             ▼
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ Manual   │  │ Tracking │  │AutoSector│  │TRP Scan  │
│  Mode    │  │  Mode    │  │  Scan    │  │  Mode    │
└────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘
     │             │              │              │
     └─────────────┴──────────────┴──────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────┐
│                   GimbalStabilizer                        │
│  - AHRS Position Correction (PD Control)                  │
│  - Gyro Rate Feed-Forward                                 │
│  - World-Frame Target Holding                             │
│  - Matrix-Based Rotation (Gimbal Lock Avoidance)          │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│            ServoDriverDevice (AZD-KD)                     │
│  - Optimized Single Modbus Write (10 registers)           │
│  - Axis-Specific Accel/Decel/Current                      │
│  - Position/Velocity Control                              │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│         Physical Servo Motors & Encoders                  │
│  - Azimuth: 222500 steps/rev (~618 steps/deg)             │
│  - Elevation: 200000 steps/rev (~556 steps/deg)           │
└──────────────────────────────────────────────────────────┘
```

### Control Loop Timing

```
Main Control Loop (20 Hz - 50ms period)
├── Update IMU/AHRS (40 Hz interpolated to 20 Hz)
├── Update Vision Tracking (30 Hz interpolated to 20 Hz)
├── Motion Mode updateWithSafety()
│   ├── SafetyInterlock.canMove() check
│   ├── updateImpl() - compute desired velocities
│   ├── GimbalStabilizer.computeStabilizedVelocity()
│   └── Send axis-optimized velocity commands to servos
└── Update System State Model (for QML UI @ 10 Hz throttled)
```

**Critical timing:**
- **Control loop:** 20 Hz (50ms) - `updateIntervalS = 0.05`
- **IMU sampling:** 40 Hz (native 3DM-GX3-25 rate)
- **Vision updates:** 30 Hz (camera frame rate)
- **UI updates:** 10 Hz (throttled to reduce QML overhead)

---

## Motion Mode Fundamentals

### Base Class: GimbalMotionModeBase

All motion modes inherit from `GimbalMotionModeBase` which provides:

#### 1. **Template Method Pattern for Safety** (NEW in v2.0)

```cpp
// PUBLIC - Called by GimbalController
void updateWithSafety(GimbalController* controller, double dt);  // FINAL

// PROTECTED - Override in derived classes
virtual void updateImpl(GimbalController* controller, double dt);
```

**Safety Flow:**
```
updateWithSafety()
    ├── checkSafetyConditions() via SafetyInterlock
    │   ├── E-Stop check
    │   ├── Station enabled check
    │   └── Dead man switch check (mode-dependent)
    ├── IF safety denied → stopServos() and return
    └── IF safety passed → updateImpl() [derived class logic]
```

**Key Benefit:** Derived classes cannot bypass safety checks - they only implement `updateImpl()`.

#### 2. **Lifecycle Methods**

```cpp
virtual void enterMode(GimbalController* controller);  // Called on mode transition
virtual void exitMode(GimbalController* controller);   // Called on mode exit
```

**Purpose:**
- `enterMode()`: Initialize mode state, reset PIDs, start velocity timer
- `exitMode()`: Stop servos, cleanup

#### 3. **GimbalStabilizer Integration** (NEW in v2.0)

```cpp
void sendStabilizedServoCommands(
    GimbalController* controller,
    double desiredAzVelocity,      // World-frame desired velocity (deg/s)
    double desiredElVelocity,
    bool enableStabilization,       // Enable/disable stabilization
    double dt                       // Time delta for filtering
);
```

**How it works:**
1. Motion mode computes desired world-frame velocity
2. GimbalStabilizer computes stabilization correction:
   - Position correction (AHRS-based drift compensation)
   - Rate feed-forward (gyro-based motion compensation)
3. Final servo command = desired + position_correction + rate_feedforward
4. Axis-optimized Modbus packet sent to servo

**Control Law:**
```
ω_cmd = ω_user + Kp×(angle_error) + Kd×(angle_error_rate) + ω_feedforward
```

#### 4. **Axis-Optimized Servo Commands** (NEW in v2.0)

```cpp
void writeVelocityCommandOptimized(
    ServoDriverDevice* driver,
    GimbalAxis axis,              // Azimuth or Elevation
    double velocityDegS,
    double stepsPerDegree,
    qint32& lastSpeedHz
);
```

**Optimization:**
- Single Modbus write (10 registers) instead of multiple writes
- Pre-built packet templates with axis-specific parameters:
  - **Azimuth:** Slow decel (100kHz/s) to prevent overvoltage on heavy turret
  - **Elevation:** Fast decel (300kHz/s) for crisp stops, 70% current limit

#### 5. **PID Controllers** (Shared Infrastructure)

```cpp
struct PIDController {
    double Kp, Ki, Kd;
    double integral;
    double maxIntegral;
    double previousError;
    double previousMeasurement;
    void reset();
};

// Full version with derivative-on-measurement option
double pidCompute(PIDController& pid, double error, double setpoint,
                  double measurement, bool derivativeOnMeasurement, double dt);

// Simple version (derivative on error)
double pidCompute(PIDController& pid, double error, double dt);
```

#### 6. **Helper Functions**

```cpp
// Time-aware filter coefficient
static double alphaFromTauDt(double tau, double dt);

// Physics-based rate limiting
static double applyRateLimitTimeBased(double desired, double prev, double maxDelta);

// Unit conversions (from MotionTuningConfig)
static double AZ_STEPS_PER_DEGREE();  // ~618
static double EL_STEPS_PER_DEGREE();  // ~556

// Angle normalization
static double normalizeAngle180(double angle);
```

---

## Safety Architecture

### SafetyInterlock Integration (NEW in v2.0)

The base class delegates all safety decisions to a centralized `SafetyInterlock` authority:

```cpp
bool checkSafetyConditions(GimbalController* controller) {
    SafetyDenialReason reason;
    int motionMode = static_cast<int>(controller->currentMotionModeType());
    bool allowed = controller->safetyInterlock()->canMove(motionMode, &reason);

    if (!allowed) {
        qDebug() << "Motion denied:" << SafetyInterlock::denialReasonToString(reason);
    }
    return allowed;
}
```

**Safety Checks:**
- E-Stop status
- Station enabled
- Dead man switch (required for Manual, AutoTrack, ManualTrack modes)

**Benefit:** All safety logic in one auditable location.

---

## Manual Motion Mode

### Purpose
Direct joystick control of gimbal with world-frame stabilization and smooth acceleration.

### Key Features (v2.0)

1. **Hz-Domain Control:** Operates in servo Hz units, then converts to deg/s
2. **World-Frame Target Holding:** Captures pointing direction when joystick released
3. **Time-Based Rate Limiting:** Physics-correct acceleration limiting
4. **Joystick Shaping:** Power curve (exponent 1.5) for fine control

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
        │       updateImpl() loop     │
        │      (every 50ms)           │
        └──────────────┬──────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Read Joystick Input        │
        │   Apply dt-aware IIR filter  │
        │   (tau from config)          │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Apply Power Curve          │
        │   (exponent = 1.5)           │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Calculate Target Speed     │
        │   (Hz domain, max 35kHz)     │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Apply Rate Limit           │
        │   (maxAccelHzPerSec × dt)    │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   Convert Hz → deg/s         │
        │   (÷ AZ_STEPS_PER_DEGREE)    │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   World-Frame Target Mgmt    │
        │   - Active: Update target    │
        │   - Centered: Enable hold    │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   sendStabilizedServoCommands│
        │   (with stabilization ON)    │
        └──────────────────────────────┘
```

### Algorithm Details

#### Joystick Filtering
```cpp
double alpha = alphaFromTauDt(cfg.filters.manualJoystickTau, dt);
m_filteredAzJoystick = alpha * rawAz + (1.0 - alpha) * m_filteredAzJoystick;
```

#### Power Curve Shaping
```cpp
double shaped = std::pow(std::abs(filteredInput), 1.5);
return (filteredInput < 0) ? -shaped : shaped;
```

#### World-Frame Target Management
```cpp
if (joystickActive) {
    // Update world-frame target to current direction (10 Hz throttled)
    convertGimbalToWorldFrame(gimbalAz, gimbalEl, roll, pitch, yaw, worldAz, worldEl);
    updatedState.targetAzimuth_world = worldAz;
    updatedState.useWorldFrameTarget = false;  // Disable hold while moving
} else {
    // Enable world-frame hold when joystick centered
    updatedState.useWorldFrameTarget = true;
}
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `manualJoystickTau` | `filters` | 0.08s | Joystick smoothing |
| `maxAccelHzPerSec` | `manualLimits` | 500000 Hz/s | Acceleration limit |

---

## Tracking Motion Mode

### Purpose
Vision-based target tracking with P+D control, filtered feedforward, and Lead Angle Compensation (LAC) state machine.

### Key Features (v2.0)

1. **LAC State Machine:** TRACK → FIRE_LEAD → RECENTER for lead angle compensation
2. **Filtered Derivative-on-Error:** Noise-resistant D-term using IIR filter
3. **Filtered Feedforward:** Prevents tracker glitch amplification
4. **Manual Nudge:** Joystick offset during tracking with decay
5. **Rigid Cradle Physics:** Target drifts off-center during lead injection

### LAC State Machine

```
┌─────────────────────────────────────────────────────────┐
│                 LAC STATE MACHINE                       │
│  (Rigid Cradle: Camera + Gun locked together)           │
└─────────────────────────────────────────────────────────┘

     ┌─────────┐  lacArmed && deadReckoningActive  ┌─────────────┐
     │  TRACK  │ ─────────────────────────────────→│  FIRE_LEAD  │
     │         │                                    │             │
     │ blend=0 │                                    │  blend→1.0  │
     │ P+D+FF  │                                    │ Open-loop   │
     │ centered│                                    │ lead inject │
     └────┬────┘                                    └──────┬──────┘
          ↑                                                │
          │ blend reaches 0                                │ LAC deactivated
          │                                                ↓
     ┌────┴────┐                                    ┌─────────────┐
     │         │←───────────────────────────────────│  RECENTER   │
     │         │        blend ramp-out (333ms)      │             │
     └─────────┘                                    │  blend→0.0  │
                                                    └─────────────┘
```

**Blend Factor:**
- `lacBlendFactor = 0.0`: 100% tracking control (target centered)
- `lacBlendFactor = 1.0`: 100% lead injection (target drifts)
- Ramp in: 5.0/sec (200ms to reach 1.0)
- Ramp out: 3.0/sec (333ms to reach 0.0)

### Control Architecture

```
               ┌─────────────────────────────────┐
               │  Vision Tracker Input           │
               │  (imageErrAz, imageErrEl)       │
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
     │  Kd × dErr     │           │  (scaled, clamped)│
     └───────┬────────┘           └───────┬────────┘
             │                            │
             └──────────────┬─────────────┘
                            │
                            ▼
               ┌────────────────────────┐
               │  P-term: Kp × error    │
               │  + D-term + FF         │
               │  = trackCmd            │
               └───────────┬────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         │              BLEND                 │
         │  final = (1-blend)×track + blend×LAC│
         └─────────────────┬─────────────────┘
                           │
                           ▼
               ┌────────────────────────┐
               │  sendStabilizedServoCmds│
               │  (stabilization OFF)   │
               └────────────────────────┘
```

### Algorithm Details

#### Filtered Derivative-on-Error
```cpp
// Raw dErr (noisy)
double rawDErrAz = (errAz - m_prevErrAz) / dt;

// Low-pass filter (τ = 100ms, ~10Hz cutoff)
double alpha = 1.0 - std::exp(-dt / 0.1);
m_filteredDErrAz = alpha * rawDErrAz + (1.0 - alpha) * m_filteredDErrAz;

// Hard clamp to prevent tracker glitches
double dErrAz = qBound(-8.0, m_filteredDErrAz, 8.0);
```

#### Filtered Feedforward
```cpp
// Filter target velocity (τ = 150ms)
double ffAlpha = 1.0 - std::exp(-dt / 0.15);
m_filteredTargetVelAz = ffAlpha * m_smoothedAzVel_dps + (1.0 - ffAlpha) * m_filteredTargetVelAz;

// Apply gain and clamp
double ffAz = qBound(-3.0, m_filteredTargetVelAz * 0.5, 3.0);

// Disable FF when error is large (recovery mode)
if (std::abs(effectiveErrAz) > 0.15) ffAz = 0.0;
```

#### Lead Injection (Open-Loop)
```cpp
double lacAzCmd = data.lacLatchedAzRate_dps * LAC_RATE_BIAS_GAIN;
double lacElCmd = data.lacLatchedElRate_dps * LAC_RATE_BIAS_GAIN;
```

### Tunable Parameters

| Parameter | Default | Effect |
|-----------|---------|--------|
| `m_azPid.Kp` | 1.0 | Position error gain |
| `m_azPid.Kd` | 0.35 | Damping (filtered dErr) |
| `DERR_FILTER_TAU` | 0.1s | dErr filter time constant |
| `FF_FILTER_TAU` | 0.15s | FF filter time constant |
| `FF_GAIN` | 0.5 | Feedforward scaling |
| `MAX_VEL_AZ` | 12.0 deg/s | Max azimuth velocity |

---

## AutoSectorScan Motion Mode

### Purpose
Automated surveillance scanning between two waypoints with two-phase motion: elevation alignment followed by azimuth scanning.

### Key Features (v2.0)

1. **Two-Phase Motion:** Align elevation first, then scan azimuth
2. **Trapezoidal Motion Profile:** Cruise + deceleration with smoothing
3. **Turn-Around Delay:** 0.5 second pause at endpoints
4. **Closest Point Start:** Begins at nearest endpoint to current position

### State Machine

```
┌─────────────────────────────────────────────────────┐
│            AUTO SECTOR SCAN MODE                    │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │ Find Closest  │
               │ Endpoint      │
               └───────┬───────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │     STATE: AlignElevation    │
        │  (Move only elevation axis)  │
        └──────────────┬───────────────┘
                       │
                       │ |elErr| < 0.5°
                       ▼
        ┌──────────────────────────────┐
        │      STATE: ScanAzimuth      │
        │  (Trapezoidal motion)        │
        └──────────────┬───────────────┘
                       │
           ┌───────────┴───────────┐
           │ Check Distance         │
           └───────────┬───────────┘
                       │
         ┌─────────────┼─────────────┐
         │ < 2° (arrived)            │ > decelDist
         ▼                           ▼
   ┌─────────────┐          ┌──────────────┐
   │ Wait 0.5s   │          │   CRUISE     │
   │ Reverse Dir │          │   v = vmax   │
   │             │          │              │
   └─────────────┘          └──────────────┘
                   \                /
                    \              /
                     \            /
                      ▼          ▼
               ┌──────────────────────┐
               │    DECELERATION      │
               │  v = sqrt(2·a·dist)  │
               └──────────────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │  sendStabilizedServoCommands │
        │  (stabilization ON)          │
        └──────────────────────────────┘
```

### Algorithm Details

#### Closest Point Selection
```cpp
double d1 = std::abs(shortestDiff(zone.az1, currentAz));
double d2 = std::abs(shortestDiff(zone.az2, currentAz));

if (d1 < d2) {
    m_movingToPoint2 = false;
    m_targetAz = zone.az1;
} else {
    m_movingToPoint2 = true;
    m_targetAz = zone.az2;
}
```

#### Trapezoidal Motion Profile
```cpp
double decelDist = (v_max * v_max) / (2.0 * accel);

if (distAz <= decelDist) {
    // Deceleration zone: v = sqrt(2·a·d)
    double v_dec = std::sqrt(2.0 * accel * distAz);
    desiredVel = direction * std::min(v_max, v_dec);
} else {
    // Cruise zone: constant velocity
    desiredVel = direction * v_max;
}
```

#### Smoothing (Adaptive Alpha)
```cpp
double alpha = dt / (SMOOTHING_TAU_S + dt);  // τ = 60ms
double smoothedVel = alpha * desiredVel + (1.0 - alpha) * m_previousDesiredAzVel;
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `scanMaxAccelDegS2` | `motion` | 15.0 | Acceleration rate |
| `arrivalThresholdDeg` | constant | 2.0 | Waypoint tolerance |
| `turnAroundDelay` | constant | 0.5s | Pause at endpoints |
| `autoScanAz.kp` | `pid` | (from config) | Elevation Kp |

---

## TRP Scan Motion Mode

### Purpose
Sequential point-to-point navigation through Target Reference Points with configurable hold times.

### Key Features (v2.0)

1. **Page-Based Navigation:** TRPs organized by location pages
2. **Hybrid Motion Profile:** Trapezoidal cruise + PID for fine approach
3. **Hold Phase:** Configurable dwell time at each TRP
4. **Auto-Loop:** Automatically cycles through page continuously

### State Machine

```
┌─────────────────────────────────────────────────────┐
│              TRP SCAN MODE                          │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │  Build Page   │
               │  Order Index  │
               └───────┬───────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │     STATE: SlewToPoint       │
        └──────────────┬───────────────┘
                       │
           ┌───────────┴───────────┐
           │ Check Arrival         │
           │ (Az < 0.2° & El < 0.2°)│
           └───────────┬───────────┘
                       │ Arrived
                       ▼
        ┌──────────────────────────────┐
        │      STATE: HoldPoint        │
        │  (Wait for haltTime)         │
        └──────────────┬───────────────┘
                       │
                       │ Timer expires
                       ▼
               ┌───────────────┐
               │  Advance to   │
               │  Next TRP     │
               │  (loop to 0)  │
               └───────┬───────┘
                       │
                       └───────→ SlewToPoint
```

### Algorithm Details

#### Hybrid Motion Profile
```cpp
if (distAz <= FINE_APPROACH_DEG) {  // 8°
    // Fine approach: PID control for precise convergence
    desiredAz = m_azPid.Kp * errAz;
} else if (distAz <= decelDist) {
    // Deceleration: Trapezoidal ramp-down
    double v_req = std::sqrt(2 * accel * distAz);
    desiredAz = direction * std::min(v_max, v_req);
} else {
    // Cruise: Full speed
    desiredAz = direction * v_max;
}
```

#### Page-Based TRP Loading
```cpp
void selectPage(int locationPage) {
    m_pageOrder.clear();
    for (int i = 0; i < m_trps.size(); ++i) {
        if (m_trps[i].locationPage == locationPage)
            m_pageOrder.push_back(i);
    }
    // Sort by trpInPage order
    std::sort(m_pageOrder.begin(), m_pageOrder.end(), ...);
}
```

### Tunable Parameters

| Parameter | Location | Default | Effect |
|-----------|----------|---------|--------|
| `trpDefaultTravelSpeed` | `motion` | 12.0 | Point-to-point speed |
| `trpMaxAccelDegS2` | `motion` | 50.0 | Acceleration |
| `trpScanAz.kp` | `pid` | (from config) | Fine approach Kp |

---

## GimbalStabilizer System

### Purpose
Platform motion compensation for AHRS-stabilized gimbal pointing using matrix-based rotation and velocity-domain control.

### Architecture

```
┌──────────────────────────────────────────────────────────┐
│                   GimbalStabilizer                        │
│  (Stateless, shared across all motion modes)              │
└────────────────────┬─────────────────────────────────────┘
                     │
     ┌───────────────┴───────────────┐
     │                               │
     ▼                               ▼
┌─────────────────┐         ┌─────────────────┐
│ Position        │         │ Rate            │
│ Correction      │         │ Feed-Forward    │
│ (AHRS-based)    │         │ (Gyro-based)    │
└────────┬────────┘         └────────┬────────┘
         │                           │
         │ PD Control                │ Kinematic Transform
         │ Kp×error + Kd×error_rate  │ ω_gimbal = f(p,q,r,az,el)
         │                           │
         └───────────────┬───────────┘
                         │
                         ▼
               ┌─────────────────┐
               │ Velocity        │
               │ Composition     │
               │                 │
               │ ω_cmd = ω_user  │
               │  + pos_corr     │
               │  + rate_ff      │
               └────────┬────────┘
                        │
                        ▼
               ┌─────────────────┐
               │ Acceleration    │
               │ Limiting        │
               │ (35 deg/s²)     │
               └─────────────────┘
```

### Control Law

```
ω_cmd = ω_user + ω_positionCorrection + ω_rateFeedforward

Where:
  ω_positionCorrection = Kp × (requiredAngle - currentAngle) + Kd × errorRate
  ω_rateFeedforward = -f(gyroP, gyroQ, gyroR, azimuth, elevation)
```

### Key Features

1. **AHRS Filtering:** Low-pass filter on roll/pitch/yaw (configurable τ)
2. **Position Deadband:** Prevents chatter around target (0.02°)
3. **Smooth FF Ramp:** Gradual activation based on gyro rate (0.3-1.2 deg/s)
4. **Acceleration Limiting:** Max 35 deg/s² for smooth motion
5. **Matrix-Based Rotation:** Proper gimbal-lock avoidance using Eigen

### Required Gimbal Angles Calculation

```cpp
// Step 1: Build platform rotation matrix
Eigen::Matrix3d Rplat = Rz(yaw) * Ry(pitch) * Rx(roll);

// Step 2: World target to unit vector
Eigen::Vector3d v_world = {cos_el * cos_az, cos_el * sin_az, sin_el};

// Step 3: Transform to platform frame
Eigen::Vector3d v_platform = Rplat.transpose() * v_world;

// Step 4: Extract gimbal angles
gimbalAz = atan2(v_platform.y(), v_platform.x());
gimbalEl = atan2(v_platform.z(), sqrt(x² + y²));
```

### Rate Feed-Forward

```cpp
// Kinematic coupling: platform rates → gimbal frame
double platformEffectOnEl = q*cos(az) - p*sin(az);
double platformEffectOnAz = r + tan(el)*(q*sin(az) + p*cos(az));

// Return negative for compensation
return {-platformEffectOnAz, -platformEffectOnEl};
```

### Tunable Parameters (motion_tuning.json)

| Parameter | Default | Effect |
|-----------|---------|--------|
| `kpPosition` | 2.0 | Position error gain |
| `kdPosition` | 0.5 | Error rate damping |
| `ahrsFilterTau` | 0.1s | AHRS angle filter |
| `positionDeadbandDeg` | 0.02 | Error deadband |
| `maxVelocityCorr` | 5.0 | Max FF velocity |
| `maxTotalVel` | 12.0 | Max total correction |
| `maxErrorRate` | 10.0 | Max error rate clamp |

---

## Motion Tuning Parameters

### Complete Parameter Reference

All parameters loaded from `config/motion_tuning.json` at startup.

**Key Sections:**
1. **servo:** Hardware constants (AZ/EL steps per degree, axis-specific accel/decel)
2. **motion:** Velocity/acceleration limits
3. **filters:** Time constants for smoothing
4. **pid:** Per-mode PID gains
5. **stabilizer:** GimbalStabilizer parameters
6. **manualLimits:** Manual mode acceleration
7. **axisServo:** Per-axis Modbus packet parameters

### Runtime Configuration Access

```cpp
const auto& cfg = MotionTuningConfig::instance();

// Servo parameters
double azSteps = cfg.servo.azStepsPerDegree;

// Motion limits
double maxVel = cfg.motion.maxVelocityDegS;

// Stabilizer
double kp = cfg.stabilizer.kpPosition;

// Axis-specific
quint32 azAccel = cfg.axisServo.azimuth.accelHz;
```

---

## Advanced Topics

### Template Method Safety Pattern

```cpp
// Base class (FINAL - cannot be overridden)
void GimbalMotionModeBase::updateWithSafety(GimbalController* controller, double dt) {
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);
        return;  // Safety denied
    }
    updateImpl(controller, dt);  // Delegate to derived class
}

// Derived class implements updateImpl()
void TrackingMotionMode::updateImpl(GimbalController* controller, double dt) {
    // Safety already checked - implement motion logic here
}
```

### Time-Based vs. Cycle-Based Control

**Physics-correct rate limiting:**
```cpp
double maxChange = maxAccelDegS2 * dt;  // deg/s² × s = deg/s
velocity = applyRateLimitTimeBased(target, prev, maxChange);
```

**IIR filter with actual dt:**
```cpp
double alpha = 1.0 - std::exp(-dt / tau);  // Correct for any dt
filtered = alpha * raw + (1.0 - alpha) * filtered;
```

### Shutdown Safety

All motion mode code checks for shutdown conditions:
```cpp
void sendStabilizedServoCommands(...) {
    // Shutdown safety: systemStateModel may be destroyed before controller
    if (!controller || !controller->systemStateModel()) {
        return;  // Silent return during cleanup
    }
    // ... normal operation
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-01-16 | 1.0 | Initial documentation |
| 2026-01-06 | 2.0 | Major update: Template Method safety pattern, GimbalStabilizer, axis-optimized servo commands, LAC state machine, filtered D-term/FF |

---

**END OF TECHNICAL REFERENCE**

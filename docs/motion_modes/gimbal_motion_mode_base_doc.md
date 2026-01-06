# GimbalMotionModeBase Class Documentation

**Version:** 2.0
**Last Updated:** 2026-01-06
**Status:** Production

---

## Overview

`GimbalMotionModeBase` is the abstract base class for all gimbal motion control modes. It provides shared functionality for servo control, GimbalStabilizer integration, axis-optimized command writing, and safety enforcement through the Template Method pattern.

**Location:** `src/controllers/motion_modes/gimbalmotionmodebase.h/cpp`

**Key Responsibility:** Provide low-level servo command infrastructure, safety enforcement, and stabilization integration for derived motion modes.

**Design Pattern:** Template Method Pattern - `updateWithSafety()` enforces safety checks, then delegates to `updateImpl()` which subclasses override.

---

## Dependencies

- `GimbalController` - manages gimbal hardware and system state
- `SystemStateModel` - provides current gimbal angles, IMU orientation, gyro data
- `SystemStateData` - container for all sensor/state information
- `ServoDriverDevice` - low-level communication with AZD-KD motor drivers
- `SafetyInterlock` - centralized safety decision authority
- `MotionTuningConfig` - runtime configuration parameters
- `GimbalStabilizer` - AHRS-based stabilization engine (shared static instance)
- Qt Core (`QObject`, `QElapsedTimer`, `QtMath`)

---

## Class Architecture (v2.0)

```
GimbalMotionModeBase (QObject)
├── Template Method Pattern
│   ├── updateWithSafety() [FINAL - safety enforcement]
│   └── updateImpl() [VIRTUAL - override in derived]
│
├── Lifecycle Methods
│   ├── enterMode()
│   ├── exitMode()
│   └── startVelocityTimer()
│
├── Servo Command Methods
│   ├── writeVelocityCommandOptimized() [NEW - axis-specific]
│   ├── sendStabilizedServoCommands() [with GimbalStabilizer]
│   └── stopServos()
│
├── PID Controllers
│   ├── PIDController struct
│   ├── pidCompute() (with derivative-on-measurement)
│   └── pidCompute() (simple overload)
│
├── Static Utilities
│   ├── alphaFromTauDt() [time-aware filter coefficient]
│   ├── applyRateLimitTimeBased() [physics-based limiting]
│   ├── normalizeAngle180()
│   ├── AZ_STEPS_PER_DEGREE()
│   └── EL_STEPS_PER_DEGREE()
│
├── Safety Integration
│   └── checkSafetyConditions() [via SafetyInterlock]
│
└── Static Shared State
    ├── s_azVelocityPacketTemplate [Azimuth Modbus template]
    ├── s_elVelocityPacketTemplate [Elevation Modbus template]
    └── s_stabilizer [GimbalStabilizer instance]
```

---

## Template Method Safety Pattern (NEW in v2.0)

### The Problem
Previously, derived classes could (accidentally or intentionally) bypass safety checks by not calling base class methods correctly.

### The Solution
Use the Template Method pattern where `updateWithSafety()` is **final** and **non-virtual**:

```cpp
// PUBLIC - Called by GimbalController (FINAL - cannot be overridden)
void updateWithSafety(GimbalController* controller, double dt) {
    // Shutdown safety check
    if (!controller || !controller->systemStateModel()) {
        return;
    }

    // Safety interlock check
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);
        return;
    }

    // Delegate to derived class
    updateImpl(controller, dt);
}

// PROTECTED - Override in derived classes
virtual void updateImpl(GimbalController* controller, double dt) = 0;
```

### Benefits
1. **Safety cannot be bypassed** - GimbalController calls `updateWithSafety()`
2. **Clean separation** - Derived classes only implement motion logic
3. **Centralized safety** - All checks in one auditable location

---

## Member Variables

### Velocity Smoothing State
| Variable | Type | Purpose |
|----------|------|---------|
| `m_previousDesiredAzVel` | `double` | Previous cycle's azimuth velocity [deg/s] |
| `m_previousDesiredElVel` | `double` | Previous cycle's elevation velocity [deg/s] |
| `m_lastAzSpeedHz` | `qint32` | Previous azimuth command [Hz] - for change detection |
| `m_lastElSpeedHz` | `qint32` | Previous elevation command [Hz] - for change detection |

### Timing
| Variable | Type | Purpose |
|----------|------|---------|
| `m_velocityTimer` | `QElapsedTimer` | Measures elapsed time for dt calculation |

### Static Shared State (NEW in v2.0)
| Variable | Type | Purpose |
|----------|------|---------|
| `s_azVelocityPacketTemplate` | `std::vector<quint16>` | Pre-built Modbus packet for azimuth |
| `s_elVelocityPacketTemplate` | `std::vector<quint16>` | Pre-built Modbus packet for elevation |
| `s_stabilizer` | `GimbalStabilizer` | Shared stabilization engine |

---

## Axis-Specific Packet Templates (NEW in v2.0)

### Purpose
Reduce Modbus transaction overhead by sending all velocity-related registers in a single write.

### Structure (10 registers starting at 0x005E)
```
Offset  Register    Azimuth Default    Elevation Default
------  --------    ---------------    -----------------
0x00    OpSpeed_L   (runtime)          (runtime)
0x01    OpSpeed_H   (runtime)          (runtime)
0x02    OpAccel_L   100,000            300,000
0x03    OpAccel_H   (Hz/s)             (Hz/s)
0x04    OpDecel_L   100,000            300,000
0x05    OpDecel_H   (Hz/s)             (Hz/s)
0x06    OpCurrent_L 1000               700
0x07    OpCurrent_H (100%)             (70%)
0x08    OpTrigger_L 0xFFFC             0xFFFC
0x09    OpTrigger_H 0xFFFF             0xFFFF
```

### Why Different Settings?
- **Azimuth (100kHz/s decel):** Slow deceleration prevents regenerative overvoltage on heavy turret
- **Elevation (300kHz/s decel):** Fast stops for crisp, responsive elevation control
- **Elevation (70% current):** Lighter axis, reduced power consumption

---

## Key Public Methods

### Lifecycle Methods

#### enterMode(GimbalController* controller)
```cpp
virtual void enterMode(GimbalController* controller);
```
**Purpose:** Called when gimbal switches INTO this motion mode.

**Typical Implementation:**
```cpp
void MyMode::enterMode(GimbalController* controller) {
    startVelocityTimer();  // Reset timing
    m_myPid.reset();       // Clear integral windup
    // Mode-specific initialization
}
```

#### exitMode(GimbalController* controller)
```cpp
virtual void exitMode(GimbalController* controller);
```
**Purpose:** Called when gimbal switches OUT of this motion mode.

**Typical Implementation:**
```cpp
void MyMode::exitMode(GimbalController* controller) {
    stopServos(controller);  // Zero velocity
}
```

#### startVelocityTimer()
```cpp
void startVelocityTimer();
```
**Purpose:** Restart the velocity timer for accurate dt calculation.

**Call:** In `enterMode()` of derived classes.

---

### Safety Methods

#### checkSafetyConditions(GimbalController* controller)
```cpp
bool checkSafetyConditions(GimbalController* controller);
```
**Purpose:** Query SafetyInterlock for permission to move.

**Returns:** `true` if motion is allowed, `false` otherwise.

**Implementation:**
```cpp
bool GimbalMotionModeBase::checkSafetyConditions(GimbalController* controller) {
    SafetyDenialReason reason;
    int motionMode = static_cast<int>(controller->currentMotionModeType());
    bool allowed = controller->safetyInterlock()->canMove(motionMode, &reason);

    if (!allowed) {
        qDebug() << "Motion denied:" << SafetyInterlock::denialReasonToString(reason);
    }
    return allowed;
}
```

**Safety Checks (via SafetyInterlock):**
- E-Stop status
- Station enabled
- Dead man switch (mode-dependent)

---

### Servo Command Methods

#### sendStabilizedServoCommands(...)
```cpp
void sendStabilizedServoCommands(
    GimbalController* controller,
    double desiredAzVelocity,     // World-frame velocity [deg/s]
    double desiredElVelocity,
    bool enableStabilization,      // true = apply stabilization
    double dt                      // Time delta for filtering
);
```

**Purpose:** Main output function. Combines motion velocity with stabilization correction.

**Control Flow:**
1. Read system state (IMU, gimbal angles, stabilization flag)
2. If stabilization enabled AND world-frame target set:
   - Call `GimbalStabilizer::computeStabilizedVelocity()`
   - Get correction velocities (Az/El)
3. Compose final velocity: `final = desired + correction`
4. Clamp to MAX_VELOCITY
5. Call `writeVelocityCommandOptimized()` for each axis

**Example:**
```cpp
// In updateImpl():
double desiredAz = m_azPid.Kp * errAz;
double desiredEl = m_elPid.Kp * errEl;
sendStabilizedServoCommands(controller, desiredAz, desiredEl, true, dt);
```

#### writeVelocityCommandOptimized(...)
```cpp
void writeVelocityCommandOptimized(
    ServoDriverDevice* driver,
    GimbalAxis axis,           // GimbalAxis::Azimuth or Elevation
    double velocityDegS,
    double stepsPerDegree,
    qint32& lastSpeedHz
);
```

**Purpose:** Send optimized velocity command using axis-specific packet template.

**Optimization:**
- Single Modbus write (10 registers) vs. multiple individual writes
- Skip write if velocity unchanged (uses `lastSpeedHz` tracking)
- Pre-built templates with axis-specific accel/decel/current

**Register Mapping:**
- OpSpeed (0x005E): Signed 32-bit velocity [Hz]
- OpAccel (0x0060): Acceleration rate [Hz/s]
- OpDecel (0x0062): Deceleration rate [Hz/s]
- OpCurrent (0x0064): Current limit [0.1%]
- OpTrigger (0x0066): Trigger value (0xFFFFFFFC)

#### stopServos(GimbalController* controller)
```cpp
void stopServos(GimbalController* controller);
```
**Purpose:** Send zero-velocity commands to all servos.

**Implementation:** Calls `writeVelocityCommandOptimized()` with velocity = 0.

---

### PID Methods

#### PIDController Structure
```cpp
struct PIDController {
    double Kp = 0.0;              // Proportional gain
    double Ki = 0.0;              // Integral gain
    double Kd = 0.0;              // Derivative gain
    double integral = 0.0;        // Accumulated integral error
    double maxIntegral = 1.0;     // Integral windup limit
    double previousError = 0.0;   // Error from last cycle
    double previousMeasurement = 0.0;  // Measurement from last cycle

    void reset() {
        integral = 0.0;
        previousError = 0.0;
        previousMeasurement = 0.0;
    }
};
```

#### pidCompute(...) - Full Version
```cpp
double pidCompute(PIDController& pid, double error, double setpoint,
                  double measurement, bool derivativeOnMeasurement, double dt);
```

**Parameters:**
- `error`: setpoint - measurement
- `setpoint`: Desired value
- `measurement`: Current measured value
- `derivativeOnMeasurement`: If true, D-term uses measurement change (prevents setpoint kick)
- `dt`: Time elapsed [seconds]

**Why Derivative on Measurement?** Prevents aggressive jerks when setpoint changes suddenly. Recommended for tracking and stabilization.

#### pidCompute(...) - Simple Version
```cpp
double pidCompute(PIDController& pid, double error, double dt);
```
Convenience overload using derivative on error (classic PID).

---

### Static Utility Methods

#### alphaFromTauDt(double tau, double dt)
```cpp
static double alphaFromTauDt(double tau, double dt);
```
**Purpose:** Compute time-correct IIR filter coefficient.

**Formula:**
```cpp
return 1.0 - std::exp(-dt / tau);
```

**Usage:**
```cpp
double alpha = alphaFromTauDt(cfg.filters.manualJoystickTau, dt);
m_filtered = alpha * raw + (1.0 - alpha) * m_filtered;
```

#### applyRateLimitTimeBased(...)
```cpp
static double applyRateLimitTimeBased(double desired, double prev, double maxDelta);
```
**Purpose:** Physics-correct rate limiting.

**Usage:**
```cpp
double maxChange = maxAccelDegS2 * dt;  // deg/s² × s = deg/s
velocity = applyRateLimitTimeBased(target, prev, maxChange);
```

#### normalizeAngle180(double angle)
```cpp
static double normalizeAngle180(double angle);
```
**Purpose:** Normalize angle to [-180, 180] range.

**Implementation:**
```cpp
while (angle > 180.0) angle -= 360.0;
while (angle < -180.0) angle += 360.0;
return angle;
```

#### AZ_STEPS_PER_DEGREE() / EL_STEPS_PER_DEGREE()
```cpp
static double AZ_STEPS_PER_DEGREE();  // ~618.06
static double EL_STEPS_PER_DEGREE();  // ~555.56
```
**Purpose:** Get axis scaling factors from MotionTuningConfig.

---

## Constants

### Motion Limits
```cpp
static constexpr double MAX_VELOCITY = 30.0;  // deg/s
```

### Axis Enumeration
```cpp
enum class GimbalAxis {
    Azimuth,
    Elevation
};
```

---

## GimbalStabilizer Integration

### Shared Instance
```cpp
static GimbalStabilizer s_stabilizer;
```
All motion modes share a single stabilizer instance for consistent state.

### Usage in sendStabilizedServoCommands()
```cpp
if (enableStabilization && data.useWorldFrameTarget) {
    auto [corrAz, corrEl] = s_stabilizer.computeStabilizedVelocity(
        data,                    // System state
        desiredAzVelocity,       // User velocity
        desiredElVelocity,
        dt,
        controller->systemStateModel()  // For debug output
    );
    finalAz = desiredAzVelocity + corrAz;
    finalEl = desiredElVelocity + corrEl;
}
```

See `GimbalStabilizer` documentation for details.

---

## Creating a Custom Motion Mode

```cpp
class MyCustomMode : public GimbalMotionModeBase {
public:
    explicit MyCustomMode(QObject* parent = nullptr)
        : GimbalMotionModeBase(parent)
    {
        // Initialize PID gains from config
        const auto& cfg = MotionTuningConfig::instance();
        m_azPid.Kp = cfg.pid.myModeAz.kp;
        m_azPid.Ki = cfg.pid.myModeAz.ki;
        m_azPid.Kd = cfg.pid.myModeAz.kd;
    }

    void enterMode(GimbalController* controller) override {
        qDebug() << "Entering MyCustomMode";
        startVelocityTimer();
        m_azPid.reset();
        m_elPid.reset();
    }

    void exitMode(GimbalController* controller) override {
        qDebug() << "Exiting MyCustomMode";
        stopServos(controller);
    }

protected:
    // updateImpl() - Safety already checked by updateWithSafety()
    void updateImpl(GimbalController* controller, double dt) override {
        const SystemStateData& data = controller->systemStateModel()->data();

        // Calculate error
        double errAz = data.targetAz - data.gimbalAz;
        errAz = normalizeAngle180(errAz);
        double errEl = data.targetEl - data.gimbalEl;

        // PID control
        double velAz = pidCompute(m_azPid, errAz, dt);
        double velEl = pidCompute(m_elPid, errEl, dt);

        // Send with stabilization
        sendStabilizedServoCommands(controller, velAz, velEl, true, dt);
    }

private:
    PIDController m_azPid;
    PIDController m_elPid;
};
```

---

## Testing Checklist

- [ ] `enterMode()` properly initializes state and calls `startVelocityTimer()`
- [ ] `exitMode()` cleanly stops servos
- [ ] `updateWithSafety()` blocks motion when safety fails
- [ ] `updateImpl()` is never called when safety conditions fail
- [ ] Stabilization targets latch correctly when enabled
- [ ] Axis-specific packets use correct accel/decel/current values
- [ ] Velocity commands scale correctly to motor Hz
- [ ] PID integral windup is prevented
- [ ] No servo stalling or overload warnings
- [ ] Shutdown sequence handles null pointers gracefully

---

## Migration Guide (v1.0 → v2.0)

### Breaking Changes

1. **`update()` renamed to `updateImpl()`**
```cpp
// OLD
void update(GimbalController* controller) override;

// NEW
void updateImpl(GimbalController* controller, double dt) override;
```

2. **dt passed as parameter** (no longer computed internally)
```cpp
// OLD
void MyMode::update(GimbalController* controller) {
    double dt = m_velocityTimer.elapsed() / 1000.0;
    m_velocityTimer.restart();
    // ...
}

// NEW
void MyMode::updateImpl(GimbalController* controller, double dt) {
    // dt already provided, just use it
}
```

3. **Safety checks automatic** (remove from updateImpl)
```cpp
// OLD
void MyMode::update(GimbalController* controller) {
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);
        return;
    }
    // ...
}

// NEW
void MyMode::updateImpl(GimbalController* controller, double dt) {
    // Safety already checked - just implement logic
}
```

4. **`sendStabilizedServoCommands()` signature changed**
```cpp
// OLD
sendStabilizedServoCommands(controller, azVel, elVel);

// NEW (with dt for time-aware filtering)
sendStabilizedServoCommands(controller, azVel, elVel, enableStab, dt);
```

---

## Related Classes

- **TrackingMotionMode:** Vision-based target tracking with LAC
- **ManualMotionMode:** Joystick control with world-frame holding
- **AutoSectorScanMotionMode:** Two-phase surveillance scanning
- **TRPScanMotionMode:** Waypoint navigation
- **GimbalStabilizer:** AHRS-based stabilization engine
- **GimbalController:** Mode orchestration
- **SafetyInterlock:** Centralized safety authority

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation |
| 2026-01-06 | 2.0 | Template Method pattern, GimbalStabilizer integration, axis-optimized packets, dt as parameter |

---

**END OF DOCUMENTATION**

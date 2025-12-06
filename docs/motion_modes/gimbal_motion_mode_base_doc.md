# GimbalMotionModeBase Class Documentation

## Overview

`GimbalMotionModeBase` is the abstract base class for all gimbal motion control modes. It provides shared functionality for servo control, PID computation, gimbal stabilization, and safety checks. All motion modes (Tracking, Manual, Scanning, etc.) inherit from this class and override virtual methods to implement specific behavior.

**Location:** `gimbalmotionmodebase.h/cpp`

**Key Responsibility:** Provide low-level servo command infrastructure and gimbal stabilization logic for derived classes.

**Design Pattern:** Template Method Pattern - subclasses implement `enterMode()`, `exitMode()`, and `update()` to define specific motion behavior.

---

## Dependencies

- `GimbalController` (forward declared) - manages gimbal hardware and system state
- `SystemStateModel` - provides current gimbal angles, IMU orientation, gyro data
- `SystemStateData` - container for all sensor/state information
- `ServoDriverDevice` - low-level communication with AZD-KX motor drivers
- Qt Core (`QObject`, `QtMath`)

---

## Class Architecture

```
GimbalMotionModeBase (QObject)
├── PIDController struct (x2: pitch, roll stabilization)
├── GyroLowPassFilter (x3: X, Y, Z axes)
├── Virtual Methods (overridden by subclasses)
│   ├── enterMode()
│   ├── exitMode()
│   └── update()
├── Servo Command Methods
│   ├── writeVelocityCommand()
│   ├── writeServoCommands()
│   ├── setAcceleration()
│   └── configureVelocityMode()
├── PID Computation
│   ├── pidCompute() (full version with derivative on measurement)
│   └── pidCompute() (simple overload)
├── Stabilization
│   ├── calculateStabilizationCorrection()
│   ├── sendStabilizedServoCommands()
│   └── resetStabilization()
└── Utility Methods
    ├── updateGyroBias()
    ├── stopServos()
    ├── checkSafetyConditions()
    └── checkElevationLimits()
```

---

## Member Variables

### Stabilization State
| Variable | Type | Purpose |
|----------|------|---------|
| `m_stabilizationTargetSet` | `bool` | Flag indicating whether stabilization target angles are latched |
| `m_targetStabilizedPitch` | `double` | Target pitch angle to hold [degrees] |
| `m_targetStabilizedRoll` | `double` | Target roll angle to hold [degrees] |
| `m_stabilizationPitchPid` | `PIDController` | PID controller for pitch stabilization |
| `m_stabilizationRollPid` | `PIDController` | PID controller for roll stabilization |

### Gyroscope Calibration
| Variable | Type | Purpose |
|----------|------|---------|
| `m_gyroBiasX` | `double` | Calibrated gyro bias for X-axis [deg/s] |
| `m_gyroBiasY` | `double` | Calibrated gyro bias for Y-axis [deg/s] |
| `m_gyroBiasZ` | `double` | Calibrated gyro bias for Z-axis [deg/s] |
| `m_gyroXFilter` | `GyroLowPassFilter` | Low-pass filter for X-axis gyro data |
| `m_gyroYFilter` | `GyroLowPassFilter` | Low-pass filter for Y-axis gyro data |
| `m_gyroZFilter` | `GyroLowPassFilter` | Low-pass filter for Z-axis gyro data |

---

## Nested Structures

### PIDController
```cpp
struct PIDController {
    double Kp = 0.0;              // Proportional gain
    double Ki = 0.0;              // Integral gain
    double Kd = 0.0;              // Derivative gain
    double integral = 0.0;        // Accumulated integral error
    double maxIntegral = 1.0;     // Integral windup limit
    double previousError = 0.0;   // Error from last cycle (for derivative)
    double previousMeasurement = 0.0;  // Measurement from last cycle
    
    void reset() {
        integral = 0.0;
        previousError = 0.0;
    }
};
```

### GyroLowPassFilter
Embedded class for filtering noisy gyroscope data.

```cpp
GyroLowPassFilter(double cutoffFreq = 10.0, double sampleRate = 100.0)
// Implements: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
// cutoffFreq: filter cutoff frequency [Hz]
// sampleRate: sensor sample rate [Hz]
```

---

## Public Methods

### Virtual Interface (Implemented by Subclasses)

#### enterMode(GimbalController* controller)
```cpp
virtual void enterMode(GimbalController* controller)
```
**Purpose:** Called when gimbal switches INTO this motion mode.

**Subclass Responsibilities:**
- Initialize mode-specific PID controllers
- Configure servo drivers for the desired operation type
- Set acceleration/deceleration limits
- Initialize smoothing filters if used

**Example (from TrackingMotionMode):**
```cpp
void TrackingMotionMode::enterMode(GimbalController* controller) {
    m_targetValid = false;
    m_azPid.reset();
    m_elPid.reset();
    if (auto servo = controller->azimuthServo()) 
        setAcceleration(servo, 50000);
}
```

---

#### exitMode(GimbalController* controller)
```cpp
virtual void exitMode(GimbalController* controller)
```
**Purpose:** Called when gimbal switches OUT of this motion mode.

**Typical Implementation:** Stop all servos and cleanup resources.

---

#### update(GimbalController* controller)
```cpp
virtual void update(GimbalController* controller)
```
**Purpose:** Called every control cycle (~50ms) to compute and send servo commands.

**Typical Flow:**
1. Read current gimbal position from `SystemStateModel`
2. Calculate desired motion (varies by mode)
3. Call `sendStabilizedServoCommands()` with desired velocities
4. Return to caller

---

### Public Utility Methods

#### stopServos(GimbalController* controller)
```cpp
void stopServos(GimbalController* controller)
```
Sends zero-velocity commands to all servos. If stabilization is enabled, gimbal will actively hold its current position.

---

#### checkSafetyConditions(GimbalController* controller)
```cpp
bool checkSafetyConditions(GimbalController* controller)
```
**Returns:** `true` if all safety conditions are satisfied, `false` otherwise.

**Safety Checks:**
- Station/Power enabled (`stationEnabled`)
- Emergency stop NOT active (`!emergencyStopActive`)
- Dead-man switch active for manual/tracking modes (`deadManSwitchActive`)

**Usage:** Call at the beginning of `update()` to prevent motion if unsafe:
```cpp
void MyMotionMode::update(GimbalController* controller) {
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);
        return;
    }
    // ... proceed with motion logic
}
```

---

#### resetStabilization()
```cpp
void resetStabilization()
```
**Purpose:** Clear stabilization targets and reset PID controllers.

**When to Call:**
- When stabilization is first enabled
- When switching to a new motion mode
- When recovering from a disturbance

**Effect:**
- Sets `m_stabilizationTargetSet = false` (forces re-latching of target angles)
- Calls `reset()` on pitch and roll PIDs
- Next call to `calculateStabilizationCorrection()` will latch current IMU angles

---

#### updateGyroBias(const SystemStateData& systemState)
```cpp
void updateGyroBias(const SystemStateData& systemState)
```
**Purpose:** Continuously calibrate gyroscope bias when vehicle is stationary.

**Behavior:**
- Accumulates gyro readings while `systemState.isVehicleStationary == true`
- After 50 samples (~2.5s at 20Hz), computes average bias
- Stores result in `m_gyroBiasX/Y/Z`
- Used by `calculateStabilizationCorrection()` for rate cancellation

**Integration:** Call from main gimbal controller loop:
```cpp
updateGyroBias(controller->systemStateModel()->data());
```

---

## Protected Methods

### Servo Command Methods

#### configureVelocityMode(ServoDriverDevice* driverInterface)
```cpp
void configureVelocityMode(ServoDriverDevice* driverInterface)
```
**Purpose:** One-time setup to configure AZD-KX driver for continuous velocity control.

**What It Does:**
1. Sets Operation Type register to 0x10 (continuous speed mode)
2. Sets acceleration/deceleration rate to 150,000 kHz/s
3. Prepares driver for real-time velocity commands

**Call Once:** In your `enterMode()` implementation:
```cpp
if (auto servo = controller->azimuthServo())
    configureVelocityMode(servo);
```

---

#### writeVelocityCommand(ServoDriverDevice* driverInterface, double finalVelocity, double scalingFactor)
```cpp
void writeVelocityCommand(ServoDriverDevice* driverInterface,
                         double finalVelocity,
                         double scalingFactor)
```
**Purpose:** Send real-time velocity command to motor driver.

**Parameters:**
- `finalVelocity`: Desired gimbal velocity [deg/s]
- `scalingFactor`: Conversion factor (Hz per deg/s). For azimuth: `222500/360 ≈ 618.06`. For elevation: `200000/360 ≈ 555.56`

**How It Works:**
1. Converts `finalVelocity` (deg/s) to motor speed (Hz) using `scalingFactor`
2. Splits 32-bit signed speed into two 16-bit registers
3. Writes to AZD-KX `OpSpeed` register (0x005E)
4. Triggers update with "magic value" 0xFFFFFFFC to `OpTrigger` register (0x0066)

**AZD-KX Register Reference:**
```
OpSpeed (0x005E): Signed 32-bit speed in Hz
  - Positive: clockwise
  - Negative: counter-clockwise
  - Range: ±4,000,000 Hz
OpTrigger (0x0066): Write 0xFFFFFFFC to apply new speed
```

---

#### setAcceleration(ServoDriverDevice* driverInterface, quint32 acceleration)
```cpp
void setAcceleration(ServoDriverDevice* driverInterface, quint32 acceleration)
```
**Purpose:** Set motor acceleration/deceleration rate.

**Parameters:**
- `acceleration`: Ramp rate in [kHz/s]. Example: 150000 = 150 kHz/s

**Behavior:**
- Clamps value to MAX_ACCELERATION (1 billion)
- Splits 32-bit value into two 16-bit registers
- Writes to all acceleration registers defined in `ACCEL_REGISTERS[]`

---

#### sendStabilizedServoCommands(GimbalController* controller, double desiredAzVelocity, double desiredElVelocity)
```cpp
void sendStabilizedServoCommands(GimbalController* controller,
                                 double desiredAzVelocity,
                                 double desiredElVelocity)
```
**Purpose:** Main command output function. Combines motion-mode velocity with stabilization correction.

**Control Flow:**
1. Reads system state (IMU angles, gimbal angles, stabilization flag)
2. Detects stabilization state changes and resets accordingly
3. **If stabilization enabled:**
   - Calls `calculateStabilizationCorrection()` 
   - Adds correction velocity to desired motion velocity
4. Clamps final velocity to `MAX_VELOCITY` (±30 deg/s)
5. Converts to motor units and sends commands:
   - Azimuth: `finalAzVelocity * 618.06` Hz
   - Elevation: `-finalElVelocity * 555.56` Hz (note: negation for inverted axis)

**Critical Design:** All motion modes send their desired velocities through this function, ensuring stabilization is consistently applied.

---

### PID Computation

#### pidCompute(...) - Full Version
```cpp
double pidCompute(PIDController& pid, double error, double setpoint, 
                  double measurement, bool derivativeOnMeasurement, double dt)
```
**Purpose:** Compute full PID output with advanced options.

**Parameters:**
- `error`: Current error (setpoint - measurement or target - current)
- `setpoint`: Desired position/value
- `measurement`: Current measured position/value
- `derivativeOnMeasurement`: If true, derivative acts on measurement change (prevents setpoint kick); if false, acts on error change
- `dt`: Time elapsed since last call [seconds]

**Computation:**
```
P = Kp * error
I = Ki * (integral += error * dt), clamped to ±maxIntegral
D = Kd * d(measurement)/dt  [if derivativeOnMeasurement=true]
  or Kd * d(error)/dt       [if derivativeOnMeasurement=false]
Output = P + I + D
```

**Why Derivative on Measurement?** Prevents aggressive jerks when setpoint changes suddenly. Recommended for tracking and stabilization.

**Example (Tracking Mode):**
```cpp
double pidAzVelocity = pidCompute(m_azPid, errAz, m_smoothedTargetAz, 
                                  data.gimbalAz, true, dt_s);
```

---

#### pidCompute(...) - Simple Overload
```cpp
double pidCompute(PIDController& pid, double error, double dt)
```
**Purpose:** Convenience overload for "derivative on error" (classic PID).

**Equivalent To:**
```cpp
pidCompute(pid, error, 0.0, 0.0, false, dt)
```

**Usage:** In scanning modes where derivative on error is acceptable:
```cpp
double pidVel = pidCompute(m_scanPid, error, dt);
```

---

### Stabilization

#### calculateStabilizationCorrection(const SystemStateData& systemState, double& azCorrection_dps, double& elCorrection_dps)
```cpp
void calculateStabilizationCorrection(const SystemStateData& systemState,
                                      double& azCorrection_dps,
                                      double& elCorrection_dps)
```
**Purpose:** Compute gimbal stabilization correction velocities based on IMU disturbances.

**How It Works:**

**Step 1: Latch Target Angles**
- On first call (when stabilization enabled), locks in current IMU pitch/roll as the "hold" target
- Resets PID controllers for clean start
- Subsequent calls maintain this target until stabilization is disabled

**Step 2: Calculate Pitch/Roll Correction**
- Computes pitch/roll error: `error = targetAngle - currentIMUAngle`
- Uses simple P-controller: `correction_dps = Kp * error`
- Result: gimbal actively opposes pitch/roll disturbances

**Step 3: Calculate Yaw Correction**
- Yaw angle drifts (no absolute reference), so use rate cancellation
- Measures gyro Z-rate, corrects for bias: `corrected_rate = GyroZ - biasZ`
- Applies low-pass filter to smooth noise
- Applies negative feedback: `yawCorrection = -filteredGyroZ_dps`
- Result: gimbal actively cancels platform yaw motion

**Step 4: Kinematic Transformation**
- Converts body-frame corrections (p, q, r) to gimbal motor commands (Az, El)
- Accounts for gimbal angle orientation: uses `sin/cos` of current azimuth/elevation
- Handles singularity when elevation near 90° (vertical)

```cpp
elCorrection = (q * cos(Az)) - (p * sin(Az))
azCorrection = r + tan(El) * (q * sin(Az) + p * cos(Az))
```

**Step 5: Limit Correction**
- Clamps output to ±25 deg/s to prevent aggressive oscillation

**Integration:** Called by `sendStabilizedServoCommands()` when `enableStabilization == true`.

---

### Utility Methods

#### checkElevationLimits(double currentEl, double targetVelocity, bool upperLimit, bool lowerLimit)
```cpp
bool checkElevationLimits(double currentEl, double targetVelocity,
                         bool upperLimit, bool lowerLimit)
```
**Returns:** `true` if motion is allowed, `false` if it would violate mechanical limits.

**Limits:**
- MIN_ELEVATION_ANGLE = -10°
- MAX_ELEVATION_ANGLE = +50°

**Blocks Motion When:**
- `currentEl >= 50°` AND `targetVelocity > 0` (trying to go higher than max)
- `currentEl <= -10°` AND `targetVelocity < 0` (trying to go lower than min)
- External limit sensors indicate boundary reached

**Usage:**
```cpp
if (!checkElevationLimits(currentEl, desiredVel, data.upperLimitSwitch, data.lowerLimitSwitch)) {
    desiredVel = 0.0; // Stop motion
}
```

---

## Constants

### Servo Control
```cpp
static constexpr quint32 DEFAULT_ACCELERATION = 100000;      // kHz/s
static constexpr quint32 MAX_ACCELERATION = 1000000000;      // kHz/s
static constexpr quint32 MAX_SPEED = 30000;                  // Motor units
```

### Servo Registers (AZD-KX Motor Driver)
```cpp
static constexpr quint16 SPEED_REGISTER = 0x0480;
static constexpr quint16 DIRECTION_REGISTER = 0x007D;
static constexpr quint16 ACCEL_REGISTERS[] = {0x2A4, 0x282, 0x600, 0x680};
```

### Direction Commands
```cpp
static constexpr quint16 DIRECTION_FORWARD = 0x4000;
static constexpr quint16 DIRECTION_REVERSE = 0x8000;
static constexpr quint16 DIRECTION_STOP = 0x0000;
```

### Motion Limits
```cpp
static constexpr double MIN_ELEVATION_ANGLE = -10.0;    // degrees
static constexpr double MAX_ELEVATION_ANGLE = 50.0;     // degrees
static constexpr double MAX_VELOCITY = 30.0;            // deg/s
```

### Scaling Factors
```cpp
static constexpr float SPEED_SCALING_FACTOR_SCAN = 250.0f;
static constexpr float SPEED_SCALING_FACTOR_TRP_SCAN = 250.0f;

// Derived (for velocity mode):
static constexpr double AZ_SCALING = 222500.0 / 360.0;   // ≈ 618.06 Hz/(deg/s)
static constexpr double EL_SCALING = 200000.0 / 360.0;   // ≈ 555.56 Hz/(deg/s)
```

### Common Parameters
```cpp
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.5;    // How close = "reached"
static constexpr double UPDATE_INTERVAL_S = 0.05;       // 50ms cycles
```

---

## PID Stabilization Gains (Default)

```cpp
// Pitch Stabilization
Kp = 2.5   // Proportional: responds to angle error
Ki = 0.2   // Integral: eliminates steady-state error
Kd = 0.05  // Derivative: damps oscillation
maxIntegral = 5.0

// Roll Stabilization
Kp = 2.5
Ki = 0.2
Kd = 0.05
maxIntegral = 5.0
```

**Note:** These gains are conservative. Tune based on your gimbal's inertia and motor responsiveness. Higher Kp = faster response, but risk of oscillation.

---

## Usage Example: Creating a Custom Motion Mode

```cpp
class MyCustomMode : public GimbalMotionModeBase {
public:
    explicit MyCustomMode(QObject* parent = nullptr) : GimbalMotionModeBase(parent) {
        m_myPid.Kp = 0.2;
        m_myPid.Ki = 0.01;
        m_myPid.Kd = 0.05;
    }
    
    void enterMode(GimbalController* controller) override {
        qDebug() << "Entering custom mode";
        m_myPid.reset();
        if (auto servo = controller->azimuthServo()) {
            configureVelocityMode(servo);
            setAcceleration(servo, 50000);
        }
    }
    
    void exitMode(GimbalController* controller) override {
        qDebug() << "Exiting custom mode";
        stopServos(controller);
    }
    
    void update(GimbalController* controller) override {
        if (!checkSafetyConditions(controller)) {
            stopServos(controller);
            return;
        }
        
        SystemStateData data = controller->systemStateModel()->data();
        double error = data.targetAz - data.gimbalAz;
        
        // Normalize azimuth error
        while (error > 180.0) error -= 360.0;
        while (error < -180.0) error += 360.0;
        
        double desiredVel = pidCompute(m_myPid, error, dt_s);
        
        // Safety check
        if (!checkElevationLimits(data.gimbalEl, 0.0, false, false)) {
            desiredVel = 0.0;
        }
        
        sendStabilizedServoCommands(controller, desiredVel, 0.0);
    }

private:
    PIDController m_myPid;
};
```

---

## Testing Checklist

- [ ] `enterMode()` properly initializes PID controllers and servo configuration
- [ ] `exitMode()` cleanly stops servos without causing jerks
- [ ] `update()` is called at consistent intervals (check dt values)
- [ ] Stabilization targets latch correctly when enabled
- [ ] Gyro bias calibration completes after 2-3 seconds of stationary time
- [ ] Gimbal actively holds position against disturbances when stabilization on
- [ ] Elevation limits prevent over-travel in both directions
- [ ] Safety conditions stop gimbal when unsafe
- [ ] PID integral windup is prevented (max integral limits respected)
- [ ] Velocity commands scale correctly to motor Hz (verify against datasheet)
- [ ] No servo stalling or overload warnings

---

## Tuning Guide: PID Stabilization

### If Gimbal Oscillates Around Target
- **Decrease Kp** (0.2 → 0.1)
- **Increase Kd** (0.05 → 0.1)

### If Gimbal Doesn't Reach Target (Slow Response)
- **Increase Kp** (2.5 → 4.0)
- **Increase Ki** (0.2 → 0.5)

### If Gimbal Has Steady-State Error
- **Increase Ki** (0.2 → 0.5)
- **Ensure maxIntegral is sufficient** (5.0 → 10.0)

### General Tuning Procedure
1. Set Ki = 0, Kd = 0, start with Kp = 1.0
2. Increase Kp until gimbal oscillates (unstable)
3. Reduce Kp by ~50% to find stable sweet spot
4. Add small Ki to eliminate steady-state error
5. Add Kd if oscillation remains

---

## Related Classes

- **TrackingMotionMode:** Inherits from this; implements autonomous target tracking
- **ManualMotionMode:** Inherits from this; implements joystick-controlled gimbal
- **ScanMotionMode:** Inherits from this; implements automated scanning patterns
- **GimbalController:** Manages mode switching and calls virtual methods
- **SystemStateModel:** Provides sensor data to all motion modes

---

## Known Issues & Troubleshooting

### Issue: Gimbal Doesn't Stabilize Against Disturbances
**Possible Causes:**
- Stabilization PID gains too low
- Gyro bias not calibrated (updateGyroBias not being called)
- IMU orientation transform incorrect

**Solution:**
- Increase Kp in stabilization PIDs
- Ensure vehicle is stationary for 3 seconds for bias calibration
- Verify IMU coordinate frame matches gimbal implementation

---

### Issue: Velocity Commands Don't Scale Correctly
**Check:**
- Verify AZD-KX manual for current register addresses
- Confirm scalingFactor calculation: `Hz_per_deg_s = motor_steps_per_rev / 360`
- Test with manual register writes to confirm motor response

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; added full API reference and tuning guide |

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Active Development
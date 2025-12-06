# AutoSectorScanMotionMode Class Documentation

## Overview

`AutoSectorScanMotionMode` implements automated back-and-forth scanning between two points in a defined sector. The gimbal scans continuously from point 1 to point 2 and back, using a configurable scan speed. When close to endpoints, the gimbal smoothly decelerates using PID control before reversing direction.

**Location:** `autosectorscanmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Enable operator-defined rectangular scan regions with automatic endpoint transitions and motion profiling.

---

## Key Design Features

### Motion Profile
The gimbal uses a **two-state motion profile** for smooth, continuous scanning:

1. **Cruising State:** Moves at constant `scanSpeed` toward target endpoint
2. **Deceleration State:** Uses PID feedback to smoothly approach and stop at endpoint

This prevents jerky reversals and allows smooth back-and-forth motion.

### Endpoint Handling
- Detects arrival at target using 2D distance threshold
- Automatically toggles between point 1 and point 2
- Resets PID controllers at reversals to prevent integral windup
- Seamlessly transitions to next sweep

---

## Class Purpose

Provide automated surveillance or inspection scanning of predefined rectangular areas by:
1. Accepting a scan zone configuration (two corner points, speed, enablement)
2. Moving gimbal back and forth between endpoints at constant speed
3. Smoothly decelerating as it approaches each endpoint
4. Reversing direction automatically when target reached
5. Supporting real-time enable/disable of scan zones

---

## Data Structures

### AutoSectorScanZone
```cpp
struct AutoSectorScanZone {
    int id;                    // Unique zone identifier
    double az1, el1;           // Azimuth/Elevation of point 1 [degrees]
    double az2, el2;           // Azimuth/Elevation of point 2 [degrees]
    double scanSpeed;          // Scanning velocity [deg/s]
    bool isEnabled;            // Whether this zone is currently active
};
```

**Purpose:** Defines a rectangular scan region with two endpoints and motion parameters.

**Typical Values:**
```
id: 1
az1: -30°, el1: 10°         (Lower-left corner)
az2: +30°, el2: 50°         (Upper-right corner)
scanSpeed: 5.0 deg/s        (Speed between endpoints)
isEnabled: true
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_activeScanZone` | `AutoSectorScanZone` | Currently active scan zone configuration |
| `m_scanZoneSet` | `bool` | Flag: whether a valid scan zone has been set |
| `m_movingToPoint2` | `bool` | Flag: currently moving toward point 2 (else point 1) |
| `m_targetAz` | `double` | Current target azimuth [degrees] |
| `m_targetEl` | `double` | Current target elevation [degrees] |
| `m_azPid` | `PIDController` | PID for azimuth position control (from base class) |
| `m_elPid` | `PIDController` | PID for elevation position control (from base class) |
| `m_velocityTimer` | `QElapsedTimer` | (Inherited) Timer for computing dt between updates |

---

## Constants

```cpp
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.5;    // Distance to consider endpoint "reached"
static constexpr double DECELERATION_DISTANCE_DEG = 5.0; // Distance at which to start decelerating
static constexpr double UPDATE_INTERVAL_S = 0.05;        // 50ms control cycles
```

**DECELERATION_DISTANCE_DEG:** Critical parameter that controls when deceleration begins.
- **Too small (< 1°):** Late braking, may overshoot targets
- **Too large (> 10°):** Early braking, inefficient scanning (spends too much time decelerating)
- **Recommended (3-5°):** Smooth transitions with good scanning efficiency

---

## PID Gains (Default)

```cpp
Azimuth:
  Kp = 1.0    // Proportional: responds to position error
  Ki = 0.01   // Integral: eliminates steady-state error
  Kd = 0.05   // Derivative: smooth deceleration
  maxIntegral = 20.0

Elevation:
  Kp = 1.0
  Ki = 0.01
  Kd = 0.05
  maxIntegral = 20.0
```

**Why These Values?**
- Lower Kp (1.0) than tracking mode: scanning is not time-critical, smoothness preferred
- Low Ki: scanning error tolerance is moderate
- Small Kd: mostly used to smooth the final approach

---

## Public Methods

### Constructor
```cpp
AutoSectorScanMotionMode(QObject* parent = nullptr)
```
**Purpose:** Initialize the scan mode and configure PID gains for smooth scanning.

**Actions:**
- Sets `m_scanZoneSet = false` (no zone active initially)
- Sets `m_movingToPoint2 = true` (start by targeting point 2)
- Configures PID gains for smooth, continuous motion
- Initializes all state variables

---

### setActiveScanZone(const AutoSectorScanZone& scanZone)
```cpp
void setActiveScanZone(const AutoSectorScanZone& scanZone)
```

**Purpose:** Configure a new scan zone or update the active zone parameters.

**Parameters:**
- `scanZone`: AutoSectorScanZone struct with endpoint coordinates and scan speed

**Behavior:**
- Copies scanZone to `m_activeScanZone`
- Sets `m_scanZoneSet = true` (validates zone is available)
- Logs the zone ID for debugging
- Takes effect on next `update()` call

**Example:**
```cpp
AutoSectorScanZone myZone;
myZone.id = 1;
myZone.az1 = -30.0;  myZone.el1 = 10.0;
myZone.az2 = +30.0;  myZone.el2 = 50.0;
myZone.scanSpeed = 5.0;
myZone.isEnabled = true;

scanMode->setActiveScanZone(myZone);
```

---

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches INTO scan mode.

**Actions:**
1. Validates that a scan zone is set and enabled
   - If not, logs warning and returns to Idle mode
2. Resets both PID controllers (clears integral windup)
3. Sets initial target to point 2: `m_targetAz = az2, m_targetEl = el2`
4. Sets direction flag: `m_movingToPoint2 = true`
5. Configures servo acceleration to 1,000,000 kHz/s (smooth scanning motion)

**Safety Check:** Exits immediately if `!m_scanZoneSet || !m_activeScanZone.isEnabled`. This prevents hanging in scan mode if the zone is disabled remotely.

---

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches OUT of scan mode.

**Actions:**
- Logs mode exit
- Stops all servos (zero velocity)
- Resets `m_scanZoneSet = false` for clean re-entry

---

### update(GimbalController* controller)
```cpp
void update(GimbalController* controller) override
```

**Purpose:** Main control loop called every cycle (~50ms) to execute scanning motion.

**Control Flow:**

#### Step 1: Guard Clauses
```
Check if controller valid?
Check if scan zone is set?
Check if scan zone is enabled?
  → If ANY fail: stop servos and switch to Idle mode
```

#### Step 2: Read System State
```cpp
data = systemStateModel()->data()
errAz = m_targetAz - data.gimbalAz
errEl = m_targetEl - data.imuPitchDeg    // Uses IMU for elevation (not encoder)
distanceToTarget = sqrt(errAz² + errEl²)  // 2D distance to target
```

**Important:** Elevation error uses `imuPitchDeg` (not encoder) to be consistent with stabilization.

#### Step 3: Endpoint Detection
```
if (distanceToTarget < ARRIVAL_THRESHOLD_DEG) {
    // Arrived at current target
    Toggle m_movingToPoint2
    Update m_targetAz and m_targetEl to other endpoint
    Reset both PID controllers (prevent integral windup)
    Recalculate error for new target
}
```

This seamless toggling creates continuous back-and-forth motion.

#### Step 4: Motion Profile Selection

**Case A: Scan Speed ≤ 0**
```
Use PID to hold position at target.
desiredAzVelocity = PID_Az(errAz)
desiredElVelocity = PID_El(errEl)
```
Allows "pausing" at endpoints if scanSpeed=0.

**Case B: In Deceleration Zone** (`distanceToTarget < DECELERATION_DISTANCE_DEG`)
```
Use PID feedback to smoothly approach target.
desiredAzVelocity = PID_Az(errAz)
desiredElVelocity = PID_El(errEl)
```
PID naturally reduces velocity as error decreases, creating smooth braking.

**Case C: Cruising** (far from target)
```
Calculate direction vector to target:
  dirAz = errAz / distanceToTarget
  dirEl = errEl / distanceToTarget

Move at constant scan speed in that direction:
  desiredAzVelocity = dirAz * scanSpeed * 0.1
  desiredElVelocity = dirEl * scanSpeed * 0.1

Reset PIDs to prevent integral windup during cruise.
```

**Important:** The `* 0.1` scaling factor appears to be a tuning parameter. This reduces effective scanning speed to 10% of the configured value.

#### Step 5: Send Commands
```cpp
sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity)
```
Passes velocities to base class, which applies stabilization if enabled.

---

## Control State Diagram

```
┌─────────────────────────────────────────────────────┐
│           MODE ACTIVE & VALID ZONE                  │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
          ┌───────────────────────────────┐
          │  CALCULATE 2D DISTANCE TO     │
          │  TARGET (Az & El)             │
          └───────────────────────────────┘
                          │
                          ▼
          ┌───────────────────────────────┐
          │  DISTANCE < ARRIVAL?          │  NO
          │                               ├───────────┐
          │  YES                          │           │
          │                               │           ▼
          └──────────┬────────────────────┘   ┌──────────────┐
                     │                        │ IN DECEL     │
                     ▼                        │ ZONE?        │
            ┌─────────────────────┐          └──────────────┘
            │ TOGGLE DIRECTION    │               │ YES  │ NO
            │ RESET PIDs          │              │      │
            │ SET NEW TARGET      │        PID   │      │ CRUISE
            │ RECALC DISTANCE     │        MODE  │      │ at scan
            │                     │              │      │ speed
            └────────┬────────────┘              │      │
                     │                          │      │
                     └──────────────┬───────────┘      │
                                    │                  │
                                    ▼                  │
                          ┌─────────────────┐         │
                          │ SEND COMMAND    │◄────────┘
                          │ (PID or Cruise) │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │ STABILIZATION   │
                          │ (if enabled)    │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │ MOTOR COMMANDS  │
                          └─────────────────┘
```

---

## Motion Profile Example

**Scenario:** Scanning from point 1 (-30°, 10°) to point 2 (+30°, 50°) at 5 deg/s.

```
TIME (s)  | DISTANCE (°) | STATE      | VELOCITY (deg/s)
──────────┼──────────────┼────────────┼──────────────────
0.0       | 70.0         | Cruising   | 5.0 (constant)
5.0       | 25.0         | Cruising   | 5.0 (constant)
14.0      | 5.0          | Cruising→  | 5.0 → 3.5 (PID kicking in)
          |              | Decel      |
16.0      | 2.0          | Decel      | 1.5 (slowing down)
17.0      | 0.5          | Decel      | 0.2 (almost stopped)
17.5      | 0.0          | Arrived    | 0.0 (toggle to point 1)
```

**Why This Works:**
- Cruising phase: gimbal scans quickly across large distances
- Deceleration phase: PID smoothly slows gimbal as it approaches target
- Endpoint: gimbal stops precisely before reversing
- No jerks: velocity changes are continuous, not abrupt

---

## Tuning Parameters

### Scan Speed
```cpp
scanSpeed: 2.0 deg/s  (slow, careful surveillance)
scanSpeed: 5.0 deg/s  (typical scanning speed)
scanSpeed: 15.0 deg/s (fast, time-critical scanning)
```
**Higher speed** = faster coverage but less precise endpoint control

### Deceleration Distance
```cpp
DECELERATION_DISTANCE_DEG = 2.0;  // Aggressive braking, late onset
DECELERATION_DISTANCE_DEG = 5.0;  // Balanced (recommended)
DECELERATION_DISTANCE_DEG = 10.0; // Gradual braking, early onset
```
**Larger distance** = more time to decelerate smoothly, less efficient coverage

### PID Gains for Smooth Deceleration
```cpp
// SMOOTH APPROACH
Kp = 0.5    // Less aggressive
Kd = 0.1    // More damping

// FASTER RESPONSE
Kp = 1.5    // More aggressive
Kd = 0.03   // Less damping
```

---

## Known Issues & Fixes

### Issue 1: Gimbal Doesn't Reverse at Endpoints
**Cause:** `ARRIVAL_THRESHOLD_DEG` too large, gimbal never reaches target.

**Solution:** Reduce threshold to 0.3-0.5°:
```cpp
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.3;
```

---

### Issue 2: Gimbal Oscillates Around Endpoint
**Cause:** PID gains too high (Kp or Kd aggressive).

**Solution:**
```cpp
// Reduce proportional gain
m_azPid.Kp = 0.5;  // from 1.0
m_elPid.Kp = 0.5;

// Increase derivative damping
m_azPid.Kd = 0.15; // from 0.05
m_elPid.Kd = 0.15;
```

---

### Issue 3: Gimbal Overshoots Target, Then Corrects
**Cause:** Deceleration begins too late, gimbal has too much momentum.

**Solution:** Increase deceleration distance:
```cpp
static constexpr double DECELERATION_DISTANCE_DEG = 10.0; // from 5.0
```

---

### Issue 4: Scanning Too Slow (Effective Speed 10% of Config)
**Cause:** Mysterious `* 0.1` scaling factor in cruising velocity calculation.

**Current Code:**
```cpp
desiredAzVelocity = dirAz * m_activeScanZone.scanSpeed * 0.1;
desiredElVelocity = dirEl * m_activeScanZone.scanSpeed * 0.1;
```

**Fix:** Remove the `* 0.1` scaling (or adjust to desired value):
```cpp
// For full configured speed:
desiredAzVelocity = dirAz * m_activeScanZone.scanSpeed;
desiredElVelocity = dirEl * m_activeScanZone.scanSpeed;

// Or adjust to 50% of configured speed:
desiredAzVelocity = dirAz * m_activeScanZone.scanSpeed * 0.5;
desiredElVelocity = dirEl * m_activeScanZone.scanSpeed * 0.5;
```

---

### Issue 5: Integral Windup During Cruise
**Cause:** PID error is large while cruising; integral term builds up uncontrollably.

**Solution:** Reset PID while cruising (ALREADY IMPLEMENTED):
```cpp
else {
    // Cruising state
    // ... set constant velocity ...
    m_azPid.reset();  // Reset integral and error history
    m_elPid.reset();
}
```
This prevents massive velocity spike when switching to deceleration state.

---

## Integration with GimbalController

### Entering Scan Mode
```cpp
GimbalController* controller = ...;

AutoSectorScanZone zone;
zone.id = 1;
zone.az1 = -30.0;  zone.el1 = 10.0;
zone.az2 = +30.0;  zone.el2 = 50.0;
zone.scanSpeed = 5.0;
zone.isEnabled = true;

// Get the scan mode instance from controller
auto scanMode = qobject_cast<AutoSectorScanMotionMode*>(controller->motionMode(MotionMode::AutoSectorScan));
if (scanMode) {
    scanMode->setActiveScanZone(zone);
    controller->setMotionMode(MotionMode::AutoSectorScan);
}
```

### Disabling Scan Zone Remotely
```cpp
// From anywhere in your application:
zone.isEnabled = false;
scanMode->setActiveScanZone(zone);

// AutoSectorScanMotionMode will detect this in next update()
// and automatically switch to Idle mode
```

---

## Testing Checklist

- [ ] Gimbal moves from point 1 to point 2 at configured scan speed
- [ ] Gimbal decelerates smoothly as it approaches each endpoint
- [ ] Gimbal stops precisely at each endpoint (within 0.5°)
- [ ] Gimbal reverses direction cleanly without hesitation
- [ ] PID controllers reset at each endpoint (check debug output)
- [ ] Gimbal does not overshoot targets
- [ ] Gimbal does not oscillate around endpoints
- [ ] Disabling scan zone causes immediate return to Idle
- [ ] Enabling/disabling scan zone mid-scan works correctly
- [ ] Stabilization (if enabled) works during scan motion
- [ ] No jerks or velocity discontinuities during transitions
- [ ] 2D distance calculation accounts for both Az and El errors
- [ ] Elevation uses IMU pitch (not encoder) consistently

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Update Frequency** | 20 Hz (50ms) | From base class UPDATE_INTERVAL_S |
| **Max Scan Speed** | ~30 deg/s | Limited by gimbal hardware (MAX_VELOCITY) |
| **Typical Scan Speed** | 2-8 deg/s | Balanced coverage and precision |
| **Endpoint Accuracy** | ±0.5° | Set by ARRIVAL_THRESHOLD_DEG |
| **Deceleration Smoothness** | ~5-8° decel zone | Prevents overshoot |
| **Latency (command→motion)** | ~50ms | One control cycle |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class providing servo control and stabilization
- **GimbalController:** Manages mode switching and gimbal state
- **SystemStateModel:** Provides gimbal angles and IMU data
- **TrackingMotionMode:** Alternative motion mode for target tracking
- **ManualMotionMode:** Alternative mode for joystick control

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified 0.1x scaling factor issue and motion profiling logic |

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Active Development - Tuning recommended for production use
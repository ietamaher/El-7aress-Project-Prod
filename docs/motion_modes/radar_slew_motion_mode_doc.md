# RadarSlewMotionMode Class Documentation

## Overview

`RadarSlewMotionMode` implements radar-guided gimbal slewing to automatically point at radar-detected targets. The gimbal receives radar track data (range, azimuth) from the system state model, calculates elevation from range geometry, and autonomously slews to point at the target with smooth motion profiling.

**Location:** `radarslewmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Automated target slewing based on radar detection data with smooth acceleration and deceleration.

---

## Key Design Features

### Radar-to-Gimbal Conversion
- Receives radar plot data: azimuth and range
- Calculates elevation from range and known system height
- Converts polar coordinates to gimbal angles

### Single-Target Slewing
- Slews to one target at a time
- Waits for new target command
- Can switch targets mid-slew (aborts current, starts new)

### Smooth Motion Profiling
- Cruising at constant speed (12 deg/s)
- Deceleration zone (5° away) with PID braking
- Arrival detection with configurable threshold

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_currentTargetId` | `uint32_t` | ID of currently targeted radar track |
| `m_targetAz` | `double` | Target azimuth [degrees] |
| `m_targetEl` | `double` | Target elevation [degrees] (calculated from range) |
| `m_isSlewInProgress` | `bool` | Flag: currently slewing to target |
| `m_previousDesiredAzVel` | `double` | Previous cycle's azimuth velocity (rate limiting) |
| `m_previousDesiredElVel` | `double` | Previous cycle's elevation velocity (rate limiting) |
| `m_azPid` | `PIDController` | Azimuth position control (from base class) |
| `m_elPid` | `PIDController` | Elevation position control (from base class) |

---

## Constants

```cpp
// Radar geometry (system-specific)
static const double SYSTEM_HEIGHT_METERS = 10.0;  // Height above sea level

// Motion profiling
static const double DECELERATION_DISTANCE_DEG = 5.0;   // Start braking zone
static const double CRUISE_SPEED_DEGS = 12.0;          // Constant cruise velocity
static const double MAX_SLEW_SPEED_DEGS = 30.0;        // Hard velocity limit
static const double MAX_VELOCITY_CHANGE = 3.0;         // Rate limiting (deg/s per cycle)
static const double ARRIVAL_THRESHOLD_DEG = 0.5;       // Distance = "arrived"

// Control
static const double UPDATE_INTERVAL_S = 0.05;          // 50ms cycles
```

---

## PID Gains (Default)

```cpp
Azimuth:
  Kp = 1.5    // Aggressive for fast slewing
  Ki = 0.08   // Moderate integral for steady-state
  Kd = 0.15   // Good damping for smooth deceleration
  maxIntegral = 30.0

Elevation:
  Kp = 1.5
  Ki = 0.08
  Kd = 0.15
  maxIntegral = 30.0
```

**Characteristics:** More aggressive than TRP scan (Kp=1.2) for fast target acquisition.

---

## Data Structures

### SimpleRadarPlot
```cpp
struct SimpleRadarPlot {
    uint32_t id;          // Unique track identifier
    double azimuth;       // Azimuth [degrees]
    double range;         // Distance from system [meters]
    // (May have additional fields: velocity, signal strength, etc.)
};
```

**Typical Values:**
```
id: 42
azimuth: 123.5°
range: 5000m (5 km)
```

---

## Public Methods

### Constructor
```cpp
RadarSlewMotionMode(QObject* parent = nullptr)
```

**Purpose:** Initialize radar slew mode.

**Actions:**
- Calls base class constructor
- Initializes state to idle (no slew in progress)
- Configures aggressive PID gains for fast acquisition
- Sets current target ID to 0

---

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches INTO radar slew mode.

**Actions:**
1. Resets slew state to idle
2. Clears current target ID
3. Resets velocity smoothing variables
4. Configures servo acceleration to 100,000 kHz/s (medium speed)
5. Awaits slew command from state model

**Note:** Does NOT call `stopServos()` (commented out in code). Relies on previous mode's `exitMode()` to have stopped servos.

---

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches OUT of radar slew mode.

**Actions:**
- Stops all servos (zero velocity)
- Aborts any in-progress slew

---

### update(GimbalController* controller)
```cpp
void update(GimbalController* controller) override
```

**Purpose:** Main control loop executed every 50ms.

**Control Flow:**

#### Step 1: Safety Checks
```cpp
if (!controller || !controller->systemStateModel()) return;
if (!checkSafetyConditions(controller)) {
    if (m_isSlewInProgress) {
        stopServos(controller);
        m_isSlewInProgress = false;
    }
    return;
}
```

If safety conditions fail (E-stop, power loss, etc.), abort slew immediately.

---

#### Step 2: Check for New Slew Command
```cpp
if (data.selectedRadarTrackId != 0 && 
    data.selectedRadarTrackId != m_currentTargetId) {
    
    // New target commanded
    m_currentTargetId = data.selectedRadarTrackId;
    
    // Find the radar plot with matching ID
    auto it = std::find_if(data.radarPlots.begin(), data.radarPlots.end(),
                           [&](const SimpleRadarPlot& p){ 
                               return p.id == m_currentTargetId; 
                           });
    
    if (it != data.radarPlots.end()) {
        // Found target plot
        m_targetAz = it->azimuth;
        
        // CRITICAL: Calculate elevation from range and system height
        m_targetEl = atan2(-SYSTEM_HEIGHT_METERS, it->range) * (180.0 / M_PI);
        
        m_isSlewInProgress = true;
        m_azPid.reset();
        m_elPid.reset();
        m_previousDesiredAzVel = 0.0;
        m_previousDesiredElVel = 0.0;
    } else {
        // Target not found in radar data
        qWarning() << "Target ID not found, slew aborted";
        m_isSlewInProgress = false;
        m_currentTargetId = 0;
    }
}
```

**Key Transformation:**
```
Radar coordinates (range, az) 
    ↓
    └─→ m_targetAz = azimuth (direct)
    └─→ m_targetEl = atan2(-height, range) × (180/π)
        (converts slant range to depression angle)
```

---

#### Step 3: Execute Movement (if slew in progress)
```cpp
if (!m_isSlewInProgress) {
    stopServos(controller);
    return;
}
```

If no active slew, stop and return.

---

#### Step 4: Calculate Error
```cpp
double errAz = m_targetAz - data.gimbalAz;
double errEl = m_targetEl - data.imuPitchDeg;

// Normalize azimuth for shortest path
while (errAz > 180.0)  errAz -= 360.0;
while (errAz < -180.0) errAz += 360.0;
```

---

#### Step 5: Check Arrival
```cpp
if (std::abs(errAz) < ARRIVAL_THRESHOLD_DEG && 
    std::abs(errEl) < ARRIVAL_THRESHOLD_DEG) {
    // Arrived at target
    stopServos(controller);
    m_isSlewInProgress = false;
    return;
}
```

Uses **independent threshold per axis** (not 2D distance). Both axes must be within 0.5°.

---

#### Step 6: Motion Profiling
```cpp
double distanceToTarget = std::sqrt(errAz * errAz + errEl * errEl);

if (distanceToTarget < DECELERATION_DISTANCE_DEG) {
    // DECELERATION: Use PID to slow down
    desiredAzVelocity = pidCompute(m_azPid, errAz, UPDATE_INTERVAL_S);
    desiredElVelocity = pidCompute(m_elPid, errEl, UPDATE_INTERVAL_S);
} else {
    // CRUISING: Constant speed toward target
    double dirAz = errAz / distanceToTarget;
    double dirEl = errEl / distanceToTarget;
    desiredAzVelocity = dirAz * CRUISE_SPEED_DEGS;  // 12 deg/s
    desiredElVelocity = dirEl * CRUISE_SPEED_DEGS;
    
    // Reset PIDs during cruise to prevent windup
    m_azPid.reset();
    m_elPid.reset();
}
```

---

#### Step 7: Velocity Constraints and Rate Limiting
```cpp
// Hard velocity limits
desiredAzVelocity = qBound(-MAX_SLEW_SPEED_DEGS, desiredAzVelocity, MAX_SLEW_SPEED_DEGS);
desiredElVelocity = qBound(-MAX_SLEW_SPEED_DEGS, desiredElVelocity, MAX_SLEW_SPEED_DEGS);

// Rate limiting (prevent sudden jumps)
double velocityChangeAz = desiredAzVelocity - m_previousDesiredAzVel;
if (std::abs(velocityChangeAz) > MAX_VELOCITY_CHANGE) {
    desiredAzVelocity = m_previousDesiredAzVel + 
        (velocityChangeAz > 0 ? MAX_VELOCITY_CHANGE : -MAX_VELOCITY_CHANGE);
}

// Same for elevation...

// Store for next cycle
m_previousDesiredAzVel = desiredAzVelocity;
m_previousDesiredElVel = desiredElVelocity;
```

**Purpose:** Prevents smooth gimbal motion from jerking when velocity changes rapidly.

---

#### Step 8: Send Commands
```cpp
sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity);
```

---

## Radar Elevation Calculation

### The Math

**Given:**
- System height above sea level: `H` meters
- Target range (slant distance): `R` meters

**Find:** Depression angle (negative = below horizon)

**Formula:**
```
elevation = atan2(-H, R) * (180/π)
```

**Example:**
```
H = 10m (antenna at 10m height)
R = 5000m (target 5km away)

elevation = atan2(-10, 5000) * (180/π)
          = atan(-0.002) * (180/π)
          ≈ -0.1146°  (target is slightly below horizon)
```

### Why Negative for Below Horizon?
- The formula uses `atan2(-H, R)` (negative height)
- This produces negative angles for targets below antenna level
- Matches gimbal convention: negative elevation = looking down

### Accuracy Considerations

**Assumptions:**
- Target is on ground level (sea level)
- No atmospheric refraction
- Range is slant range (direct line-of-sight distance)
- Radar uses same reference frame as gimbal

**Limitations:**
- If target is elevated, formula underestimates depression angle
- If range is measured differently (ground range vs slant), result will be wrong

---

## Data Flow: Slewing to Radar Target

```
1. Radar System detects target
   └─ Track ID: 42, Azimuth: 123.5°, Range: 5000m
   └─ Data sent to SystemStateModel

2. Operator or AI selects target
   └─ systemStateModel->selectedRadarTrackId = 42
   └─ Emits dataChanged signal

3. GimbalController detects mode is RadarSlew
   └─ Calls RadarSlewMotionMode::update()

4. RadarSlewMotionMode::update() executes
   ├─ Detects: selectedRadarTrackId (42) != m_currentTargetId (0)
   ├─ Searches radarPlots for ID 42 → Found
   ├─ Extracts: azimuth = 123.5°, range = 5000m
   ├─ Calculates: elevation = atan2(-10, 5000) ≈ -0.11°
   ├─ Sets: m_isSlewInProgress = true
   └─ Resets: PIDs and velocity smoothing

5. Main control loop (50ms intervals)
   ├─ Calculate error: errAz = 123.5° - 0° = 123.5°
   ├─ Calculate error: errEl = -0.11° - 0° = -0.11°
   ├─ Distance = sqrt(123.5² + 0.11²) ≈ 123.5°
   ├─ Far from target (>5°) → Cruising state
   ├─ Direction vector: dirAz = 123.5/123.5 = 1.0, dirEl ≈ 0
   ├─ Velocity: azVel = 1.0 × 12 = 12 deg/s, elVel ≈ 0
   ├─ Apply constraints and rate limiting
   └─ Send to servos: "Move azimuth at 12 deg/s, elevation at ~0"

6. Gimbal physically rotates
   └─ Azimuth increases from 0° → 10° → 20° → ... → 120°

7. Approaching target (within 5° of 123.5°)
   ├─ Enter deceleration zone
   ├─ Use PID feedback: velocity decreases smoothly
   ├─ Velocity: 12 → 8 → 4 → 1 deg/s

8. Very close (within 0.5° on both axes)
   ├─ Arrival detected
   ├─ Stop servos
   ├─ m_isSlewInProgress = false
   └─ Gimbal now pointed at radar target

9. Gimbal holds position (stabilization if enabled)
   └─ Awaits new slew command
```

---

## Known Issues

### Issue 1: Arrival Detection Uses Independent Thresholds
**Current Code:**
```cpp
if (std::abs(errAz) < ARRIVAL_THRESHOLD_DEG && 
    std::abs(errEl) < ARRIVAL_THRESHOLD_DEG) {
```

**Problem:** Requires BOTH axes within 0.5°. A diagonal approach (0.4° azimuth + 0.4° elevation offset) still satisfies arrival even though actual 2D distance is 0.57°.

**Alternative (Better):**
```cpp
double distanceToTarget = std::sqrt(errAz * errAz + errEl * errEl);
if (distanceToTarget < ARRIVAL_THRESHOLD_DEG) {
```

This ensures true 2D distance-based arrival.

---

### Issue 2: No Current Target Validation
**Current Code:**
```cpp
m_targetAz = it->azimuth;
m_targetEl = atan2(-SYSTEM_HEIGHT_METERS, it->range) * (180.0 / M_PI);
m_isSlewInProgress = true;
```

**Problem:** If radar plot is stale or invalid, gimbal slews to bad data.

**Fix:**
```cpp
// Validate plot data before using
if (it->range < MIN_RANGE || it->range > MAX_RANGE) {
    qWarning() << "Target out of valid range:" << it->range;
    m_isSlewInProgress = false;
    return;
}

// Check if plot is fresh (not > 1 second old)
if ((QDateTime::currentMSecsSinceEpoch() - it->lastUpdateMs) > 1000) {
    qWarning() << "Target plot is stale";
    m_isSlewInProgress = false;
    return;
}
```

---

### Issue 3: No Slew Timeout
**Current Code:** Mode assumes target stays valid indefinitely.

**Problem:** If radar loses track mid-slew, gimbal keeps slewing to stale coordinates.

**Fix:**
```cpp
static const double SLEW_TIMEOUT_SECONDS = 30.0;
if (m_isSlewInProgress && m_slewTimer.elapsed() > SLEW_TIMEOUT_SECONDS * 1000) {
    qWarning() << "Slew timeout - target lost";
    stopServos(controller);
    m_isSlewInProgress = false;
}
```

---

### Issue 4: Elevation Calculation Assumes Sea Level Target
**Current Code:**
```cpp
m_targetEl = atan2(-SYSTEM_HEIGHT_METERS, it->range) * (180.0 / M_PI);
```

**Assumes:** Target is at elevation = system height (sea level).

**Problem:** If target is on a hill (higher than system), calculation is wrong.

**Better:**
```cpp
// If target elevation available in radar data:
if (it->hasElevation) {
    double heightDiff = SYSTEM_HEIGHT_METERS - it->elevation;
    m_targetEl = atan2(-heightDiff, it->range) * (180.0 / M_PI);
} else {
    m_targetEl = atan2(-SYSTEM_HEIGHT_METERS, it->range) * (180.0 / M_PI);
}
```

---

## Tuning Guide

### For Faster Slewing
```cpp
CRUISE_SPEED_DEGS = 20.0;        // Increase from 12
DECELERATION_DISTANCE_DEG = 3.0; // Brake later
m_azPid.Kp = 2.0;                // More aggressive
m_elPid.Kp = 2.0;
```

### For Smoother Approach
```cpp
CRUISE_SPEED_DEGS = 8.0;         // Slower cruise
DECELERATION_DISTANCE_DEG = 8.0; // Brake earlier
m_azPid.Kd = 0.25;               // More damping
m_elPid.Kd = 0.25;
```

### For Precise Arrival
```cpp
ARRIVAL_THRESHOLD_DEG = 0.2;     // Stricter threshold
m_azPid.Ki = 0.15;               // More integral correction
m_elPid.Ki = 0.15;
```

---

## Testing Checklist

- [ ] Mode enters without errors
- [ ] Mode exits cleanly on command
- [ ] Radar plot data loaded into system state correctly
- [ ] New slew command detected when track ID changes
- [ ] Elevation calculated correctly from range/height
- [ ] Gimbal slews toward correct azimuth
- [ ] Gimbal slews toward correct elevation
- [ ] Azimuth path takes shortest route (wraps 350° → 10° not 0° → 360° → 10°)
- [ ] Cruise phase holds constant speed
- [ ] Deceleration phase reduces speed smoothly
- [ ] Arrival detected within threshold
- [ ] Gimbal stops at arrival
- [ ] Can switch to new target mid-slew
- [ ] Safety conditions stop slew immediately
- [ ] Rate limiting prevents velocity jerks
- [ ] Stabilization works during slew
- [ ] No integral windup during cruise
- [ ] Velocity logging shows expected values

---

## Integration with System

### Input: Radar Tracks
```cpp
systemState.selectedRadarTrackId      // Which track to slew to
systemState.radarPlots                // All radar tracks with pos/range
systemState.gimbalAz, gimbalEl        // Current gimbal position
systemState.imuPitchDeg               // Current elevation (IMU)
```

### Output: Servo Commands
```cpp
sendStabilizedServoCommands(azVel, elVel)
// Velocity commands to servo drivers
```

### Flow: Operator Selects Radar Target
```
Radar Display (UI)
    ├─ Operator clicks on radar blip
    └─ Sends command: "Slew to track 42"

SystemStateModel::setSelectedRadarTrack(42)
    └─ Updates: selectedRadarTrackId = 42
    └─ Emits: dataChanged signal

GimbalController detects mode is RadarSlew
    └─ Calls: update()

RadarSlewMotionMode detects track 42 selected
    ├─ Finds track in radarPlots
    ├─ Calculates gimbal angles
    └─ Begins slew motion

Gimbal autonomously points at target
    ├─ Camera frame shows target
    └─ Operator can now visually confirm or switch to tracking
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified arrival detection bug, missing validation, no timeout handling |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with servo control
- **GimbalController:** Mode orchestration
- **SystemStateModel:** Radar track data source
- **TRPScanMotionMode:** Alternative scanning mode (waypoint-based)

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Production - Radar data validation recommended before deployment
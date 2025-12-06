# TRPScanMotionMode Class Documentation

## Overview

`TRPScanMotionMode` implements waypoint-based gimbal scanning through a sequence of Target Reference Points (TRPs). The gimbal visits each TRP in order, optionally pauses at each point, then loops back to the start. This enables systematic surveillance of predefined locations (checkpoints, watch positions, perimeter points).

**Location:** `trpscanmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Autonomous surveillance by visiting a scripted sequence of geographical positions with configurable dwell times.

---

## Key Design Features

### Waypoint-Based Path Following
- Visits TRPs in sequential order
- Executes motion-profiling (cruise → decelerate → halt) between waypoints
- Automatically loops from last point back to first

### Halt-Dwell Pattern
- Pauses at each TRP for configurable duration
- Holds position actively (not just stops)
- Resumes path after dwell time expires

### Two-State Control
```
Moving → (cruise or decelerate) → Halted → (wait) → Moving → ...
```

---

## Data Structures

### TargetReferencePoint
```cpp
struct TargetReferencePoint {
    int locationPage;      // Page/group identifier
    double azimuth;        // Target azimuth [degrees]
    double elevation;      // Target elevation [degrees]
    double haltTime;       // Pause duration at this point [seconds]
    // (Other fields may exist depending on application)
};
```

**Typical Values:**
```
locationPage: 1
azimuth: -45°
elevation: 15°
haltTime: 5.0 seconds
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_trpPage` | `std::vector<TargetReferencePoint>` | Current TRP list/page |
| `m_currentTrpIndex` | `size_t` | Index of current target TRP (0-based) |
| `m_currentState` | `State` | Current state machine state |
| `m_haltTimer` | `QElapsedTimer` | Timer for dwell time at waypoints |
| `m_azPid` | `PIDController` | PID for azimuth position control |
| `m_elPid` | `PIDController` | PID for elevation position control |

---

## State Enumeration

```cpp
enum class State {
    Idle,       // No path active
    Moving,     // Traveling between waypoints
    Halted      // Paused at waypoint, waiting for dwell time
};
```

**Transitions:**
```
Idle
  ↓ (enterMode)
Moving
  ├─ (arrival at waypoint)
  ↓
Halted
  ├─ (dwell time expires)
  ↓
Moving (to next waypoint)
  ├─ (last waypoint → next=first)
  ↓
Halted (at first waypoint again)
  └─ (loop continues...)
```

---

## Constants

```cpp
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.5;    // Distance to "reach" waypoint
static constexpr double DECELERATION_DISTANCE_DEG = 5.0; // Start braking distance
static constexpr double UPDATE_INTERVAL_S = 0.05;        // 50ms control cycles
```

---

## PID Gains (Default)

```cpp
Azimuth:
  Kp = 1.2    // Responsive position control
  Ki = 0.1    // Eliminate steady-state error
  Kd = 0.1    // Smooth deceleration
  maxIntegral = 20.0

Elevation:
  Kp = 1.2
  Ki = 0.1
  Kd = 0.1
  maxIntegral = 20.0
```

**Characteristics:** Faster response than scanning modes (Kp=1.2 vs 1.0), focused on precise waypoint arrival.

---

## Public Methods

### Constructor
```cpp
TRPScanMotionMode()
```

**Purpose:** Initialize TRP scanning mode.

**Actions:**
- Sets initial state to `Idle`
- Initializes TRP index to 0
- Configures PID gains for responsive waypoint arrival
- Clears TRP page

---

### setActiveTRPPage(const std::vector<TargetReferencePoint>& trpPage)
```cpp
void setActiveTRPPage(const std::vector<TargetReferencePoint>& trpPage)
```

**Purpose:** Load a new TRP page (list of waypoints).

**Parameters:**
- `trpPage`: Vector of TRP structures defining the path

**Actions:**
1. Stores TRP page in `m_trpPage`
2. Resets current index to 0
3. Sets state to `Idle` (if page empty) or `Moving` (if page has waypoints)
4. Logs page size for debugging

**Example:**
```cpp
std::vector<TargetReferencePoint> perimeter = {
    {1, -45.0, 15.0, 5.0},   // Point 0: NW corner, 5s dwell
    {1,  45.0, 15.0, 5.0},   // Point 1: NE corner, 5s dwell
    {1,  45.0, 50.0, 5.0},   // Point 2: NE high, 5s dwell
    {1, -45.0, 50.0, 5.0},   // Point 3: NW high, 5s dwell
};
trpScanMode->setActiveTRPPage(perimeter);
```

---

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches INTO TRP scan mode.

**Actions:**

1. **Validate:** Check if TRP page is loaded
   - If empty, log warning, stop servos, return to Idle
   
2. **Reset State:**
   - `m_currentTrpIndex = 0`
   - `m_currentState = State::Moving`
   - Reset both PID controllers

3. **Configure Servos:**
   - Set acceleration to 200,000 kHz/s (faster than sector scan for point-to-point)

**Safety Check:** Exits mode if no TRP page set.

---

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```

**Purpose:** Called when gimbal switches OUT of TRP scan mode.

**Actions:**
- Stops all servos (zero velocity)
- Sets state to `Idle`

---

### update(GimbalController* controller)
```cpp
void update(GimbalController* controller) override
```

**Purpose:** Main control loop executed every 50ms.

**State Machine Logic:**

#### State: Idle
```cpp
case State::Idle:
    stopServos(controller);
    return;
```
No motion. Waits for `enterMode()` or page change.

---

#### State: Halted
```cpp
case State::Halted:
    if (haltTimer.elapsed() >= currentTrp.haltTime * 1000.0) {
        // Dwell time complete
        m_currentTrpIndex++;
        
        // Loop: if reached end, restart from beginning
        if (m_currentTrpIndex >= m_trpPage.size()) {
            m_currentTrpIndex = 0;
            qDebug() << "Path loop finished. Returning to point 0.";
        }
        
        // Transition to Moving state
        m_currentState = State::Moving;
        m_azPid.reset();
        m_elPid.reset();
    }
    return;  // Servos remain stopped while halted
```

**Key Behavior:**
- Measures elapsed time from `m_haltTimer.start()`
- When dwell time expires, advances to next waypoint
- **Automatically loops:** Last point → First point
- Resets PIDs before moving to prevent integral windup

---

#### State: Moving (Main Motion Logic)

**Step 1: Calculate Error**
```cpp
double errAz = targetTrp.azimuth - data.gimbalAz;
double errEl = targetTrp.elevation - data.imuPitchDeg;

// Normalize azimuth for shortest path
while (errAz > 180.0)  errAz -= 360.0;
while (errAz < -180.0) errAz += 360.0;

double distanceToTarget = std::sqrt(errAz * errAz + errEl * errEl);
```

**Note:** Code currently computes distance using only elevation (`std::sqrt(errEl * errEl)`). This should likely use both axes: `std::sqrt(errAz * errAz + errEl * errEl)`.

**Step 2: Check Arrival**
```cpp
if (distanceToTarget < ARRIVAL_THRESHOLD_DEG) {
    stopServos(controller);
    m_currentState = State::Halted;
    m_haltTimer.start();  // Begin countdown for dwell time
    return;
}
```

When within 0.5°, waypoint is considered "reached."

**Step 3: Motion Profile**

**Case A: Zero or Negative Speed**
```cpp
if (travelSpeed <= 0) {
    desiredAzVelocity = pidCompute(m_azPid, errAz, UPDATE_INTERVAL_S);
    desiredElVelocity = pidCompute(m_elPid, errEl, UPDATE_INTERVAL_S);
}
```
Use PID feedback to go to waypoint (similar to position-hold).

**Case B: In Deceleration Zone** (< 5° away)
```cpp
else if (distanceToTarget < DECELERATION_DISTANCE_DEG) {
    desiredAzVelocity = pidCompute(m_azPid, errAz, UPDATE_INTERVAL_S);
    desiredElVelocity = pidCompute(m_elPid, errEl, UPDATE_INTERVAL_S);
}
```
Use PID to smoothly slow down as waypoint approaches.

**Case C: Cruising** (far from waypoint)
```cpp
else {
    double dirAz = errAz / distanceToTarget;
    double dirEl = errEl / distanceToTarget;
    desiredAzVelocity = dirAz * travelSpeed;
    desiredElVelocity = dirEl * travelSpeed;
    
    // CRITICAL: Reset PIDs to prevent integral windup during cruise
    m_azPid.reset();
    m_elPid.reset();
}
```
Move at constant speed toward waypoint. Reset PIDs to prevent accumulating error.

**Step 4: Send Commands**
```cpp
sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity);
```

---

## Motion Profile Example

**Scenario:** Visiting 3 waypoints in a triangle, 15 deg/s travel speed.

```
TIME   | STATE   | POSITION | ACTIVITY
───────┼─────────┼──────────┼──────────────────────
0.0s   | Moving  | Pt 0     | Starting toward Pt 1
2.0s   | Moving  | 60% → Pt 1 | Cruising at 15 deg/s
9.0s   | Moving  | 5° away  | Entering decel zone
11.0s  | Halted  | Pt 1     | Arrived, dwell=5s
16.0s  | Moving  | Pt 1     | Dwell complete, moving to Pt 2
...    | ...     | ...      | ...
```

---

## Path Looping Behavior

**Current Implementation:**
```cpp
if (m_currentTrpIndex >= m_trpPage.size()) {
    m_currentTrpIndex = 0;  // Restart from first point
    qDebug() << "Path loop finished. Returning to point 0.";
}
```

**Behavior:**
1. Visit waypoints 0, 1, 2, ..., N-1
2. After halting at waypoint N-1, dwell time expires
3. Automatically set target to waypoint 0
4. Resume moving
5. Loop continues indefinitely (or until mode changed)

**Alternative Designs (Not Implemented):**
- **One-shot:** Stop after last waypoint (requires mode change to resume)
- **Ping-pong:** Reverse direction (go 0→1→2→...→N-1→N-2→...→0)
- **Conditional loop:** Only loop if enabled flag set

---

## Known Issues & Improvements

### Issue 1: Distance Calculation Only Uses Elevation
**Current Code:**
```cpp
double distanceToTarget = std::sqrt(errEl * errEl);  // Only elevation!
```

**Problem:** Arrival detection ignores azimuth error. If azimuth is off by 10° but elevation correct, gimbal still "arrives" and halts.

**Fix:**
```cpp
double distanceToTarget = std::sqrt(errAz * errAz + errEl * errEl);  // 2D distance
```

**Impact:** More accurate waypoint arrival detection, better convergence to waypoints.

---

### Issue 2: Travel Speed Hardcoded
**Current Code:**
```cpp
double travelSpeed = 15;  // Hardcoded!
// targetTrp.scanSpeed;  // This line is commented out
```

**Problem:** All waypoints traverse at 15 deg/s, ignoring any per-waypoint speed setting.

**Fix:**
```cpp
double travelSpeed = targetTrp.scanSpeed;  // Use TRP-specific speed
// Add bounds checking:
travelSpeed = std::clamp(travelSpeed, 0.0, 30.0);  // 0-30 deg/s
```

**Benefits:** Different waypoints can have different transit speeds (e.g., slow through dense area, fast through open area).

---

### Issue 3: No Halt Time Validation
**Current Code:**
```cpp
if (m_haltTimer.elapsed() >= static_cast<qint64>(currentTrp.haltTime * 1000.0))
```

**Problem:** If `haltTime` is negative or NaN, unpredictable behavior.

**Fix:**
```cpp
double haltMs = std::max(0.0, currentTrp.haltTime * 1000.0);  // Clamp to 0+
if (m_haltTimer.elapsed() >= static_cast<qint64>(haltMs)) {
```

---

### Issue 4: No Arrival Threshold Configuration
**Current Code:**
```cpp
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.5;  // Fixed
```

**Problem:** 0.5° might be too loose for precision tracking or too strict for fast scanning.

**Solution:** Make configurable per-waypoint or mode-wide:
```cpp
struct TargetReferencePoint {
    // ... existing fields ...
    double arrivalThreshold = 0.5;  // Allow override per waypoint
};
```

---

## Tuning Guide

### For Faster Waypoint Traversal
```cpp
m_azPid.Kp = 1.5;    // Increase responsiveness
m_elPid.Kp = 1.5;
DECELERATION_DISTANCE_DEG = 3.0;  // Brake later
travelSpeed = 20;  // Faster transit
```

### For Precise Waypoint Arrival
```cpp
m_azPid.Kp = 0.8;    // Gentler response
m_elPid.Kp = 0.8;
m_azPid.Ki = 0.2;    // More integral (fine-tune to target)
m_elPid.Ki = 0.2;
ARRIVAL_THRESHOLD_DEG = 0.2;  // Stricter arrival
DECELERATION_DISTANCE_DEG = 8.0;  // Brake early, gently
```

### For Smooth Transitions Between Waypoints
```cpp
m_azPid.Kd = 0.2;    // Increase damping
m_elPid.Kd = 0.2;
travelSpeed = 10;  // Slower, smoother
```

---

## Integration with GimbalController

### Loading TRP Page
```cpp
std::vector<TargetReferencePoint> perimeter = {
    // ... define waypoints ...
};

auto trpMode = qobject_cast<TRPScanMotionMode*>(
    controller->motionMode(MotionMode::TRPScan)
);
if (trpMode) {
    trpMode->setActiveTRPPage(perimeter);
    controller->setMotionMode(MotionMode::TRPScan);
}
```

### Switching TRP Pages Mid-Scan
```cpp
// Load new page (will reset to waypoint 0)
trpMode->setActiveTRPPage(newPerimeter);

// If you want to resume from current point on old page,
// need to save/restore m_currentTrpIndex
```

---

## Testing Checklist

- [ ] Constructor initializes state correctly
- [ ] `setActiveTRPPage()` loads waypoints and resets index
- [ ] Empty TRP page handled gracefully (mode exits to Idle)
- [ ] `enterMode()` configures servos and resets PIDs
- [ ] Gimbal moves toward first waypoint after entering mode
- [ ] Distance calculation uses both azimuth and elevation
- [ ] Arrival detection triggers within ARRIVAL_THRESHOLD_DEG
- [ ] Gimbal transitions from cruising to deceleration correctly
- [ ] Gimbal halts precisely at waypoint
- [ ] Halt timer counts down for dwell duration
- [ ] Gimbal resumes after dwell time expires
- [ ] Path loops correctly (last → first waypoint)
- [ ] All waypoints visited in order
- [ ] Azimuth normalization prevents long circular paths
- [ ] No PID integral windup during cruise
- [ ] Gimbal stops on mode exit
- [ ] Multiple TRP pages can be loaded sequentially
- [ ] Stabilization works during TRP scanning (if enabled)
- [ ] Emergency stop halts gimbal immediately

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Update Frequency** | 20 Hz (50ms) | From base class |
| **Travel Speed Range** | 0-30 deg/s | Configurable per-waypoint |
| **Waypoint Arrival Accuracy** | ±0.5° | Configurable threshold |
| **Deceleration Distance** | ~5° | Smooth approach zone |
| **Dwell Time Range** | 0-∞ seconds | Per-waypoint configuration |
| **Path Loop Latency** | ~50-100ms | One control cycle + PID reset |
| **Max Waypoints per Page** | Unlimited | Memory dependent |

---

## Data Flow: Visiting Three Waypoints

```
1. GimbalController sets mode to TRPScan
   └─ TRPScanMotionMode::enterMode()
   └─ Load waypoint 0 as target
   └─ m_currentState = Moving

2. Main control loop (50ms)
   └─ TRPScanMotionMode::update()
   └─ Calculate error to waypoint 0
   └─ Cruise at constant speed
   └─ Send servo commands
   
3. Approaching waypoint (5° away)
   └─ Enter deceleration zone
   └─ Use PID to slow down smoothly
   
4. Reach waypoint (<0.5° away)
   └─ Stop servos
   └─ m_currentState = Halted
   └─ m_haltTimer.start()
   
5. Wait for dwell time (e.g., 5 seconds)
   └─ Gimbal holds position actively
   └─ Servos receive zero-velocity commands
   
6. Dwell time expires
   └─ m_currentTrpIndex++  (advance to waypoint 1)
   └─ m_currentState = Moving
   └─ Reset PIDs
   
7. Repeat steps 2-6 for waypoint 1, 2, etc.

8. After last waypoint
   └─ m_currentTrpIndex = 0
   └─ Loop back to waypoint 0
   └─ Cycle continues indefinitely
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified distance calc bug (elevation only), hardcoded speed, missing halt validation |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with servo control
- **GimbalController:** Mode orchestration
- **SystemStateModel:** Gimbal position and state
- **AutoSectorScanMotionMode:** Alternative scanning mode (two points, continuous)

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Production - Tuning recommended for specific deployment
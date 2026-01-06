# AutoSectorScanMotionMode Class Documentation

**Version:** 2.0
**Last Updated:** 2026-01-06
**Status:** Production

---

## Overview

`AutoSectorScanMotionMode` implements automated surveillance scanning between two waypoints with a two-phase motion profile: first align elevation, then perform continuous azimuth scanning with trapezoidal motion.

**Location:** `src/controllers/motion_modes/autosectorscanmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Enable operator-defined surveillance zones with smooth, predictable scanning motion for threat detection and area monitoring.

---

## Key Design Features (v2.0)

### Two-Phase Motion Profile
1. **AlignElevation Phase:** Move only elevation axis to target elevation
2. **ScanAzimuth Phase:** Continuous back-and-forth azimuth scanning

### Trapezoidal Motion with Smoothing
- Physics-based deceleration: `v = sqrt(2 × a × d)`
- Adaptive smoothing filter: `τ = 60ms`
- Turn-around delay: `0.5 seconds` at endpoints

### Closest Point Start
- Automatically begins at nearest endpoint to current position
- Reduces initial slew time

### Hardware Polarity Support
- `m_azHardwareSign`: Configurable motor direction polarity

---

## State Machine

```
┌─────────────────────────────────────────────────────┐
│            AUTO SECTOR SCAN MODE                    │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │ Validate Zone │
               │ (isEnabled?)  │
               └───────┬───────┘
                       │ YES
                       ▼
               ┌───────────────┐
               │ Find Closest  │
               │ Endpoint      │
               └───────┬───────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │   STATE: AlignElevation      │
        │   - Move only elevation      │
        │   - Az velocity = 0          │
        │   - Use PID for El           │
        └──────────────┬───────────────┘
                       │
                       │ |elErr| < 0.5°
                       ▼
        ┌──────────────────────────────┐
        │   STATE: ScanAzimuth         │
        │   - Trapezoidal Az motion    │
        │   - El holds at target       │
        └──────────────┬───────────────┘
                       │
           ┌───────────┴───────────┐
           │ Calculate distAz       │
           │ = |targetAz - currentAz|│
           └───────────┬───────────┘
                       │
         ┌─────────────┼─────────────┐
         │ < 2°        │             │ > decelDist
         │ (arrived)   │             │
         ▼             ▼             ▼
   ┌──────────┐  ┌──────────┐  ┌──────────┐
   │TURN-AROUND│ │DECEL    │  │CRUISE    │
   │Wait 0.5s │  │v=sqrt.. │  │v=vmax    │
   │Toggle dir│  │         │  │          │
   └──────────┘  └──────────┘  └──────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │  Apply Smoothing Filter      │
        │  alpha = dt / (τ + dt)       │
        └──────────────┬───────────────┘
                       │
                       ▼
        ┌──────────────────────────────┐
        │  sendStabilizedServoCommands │
        │  (stabilization ON, dt)      │
        └──────────────────────────────┘
```

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

**Typical Values:**
```
id: 1
az1: -30°, el1: 15°         (Left endpoint)
az2: +30°, el2: 15°         (Right endpoint)
scanSpeed: 8.0 deg/s        (Cruise velocity)
isEnabled: true
```

### State Enumeration
```cpp
enum class ScanState {
    AlignElevation,  // Phase 1: Move to target elevation
    ScanAzimuth      // Phase 2: Continuous azimuth scanning
};
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_activeScanZone` | `AutoSectorScanZone` | Currently active scan zone |
| `m_scanZoneSet` | `bool` | Flag: valid zone configured |
| `m_movingToPoint2` | `bool` | Direction flag: true = toward az2 |
| `m_targetAz` | `double` | Current azimuth target [degrees] |
| `m_targetEl` | `double` | Current elevation target [degrees] |
| `m_scanState` | `ScanState` | Current state machine state |
| `m_previousDesiredAzVel` | `double` | Previous Az velocity (smoothing) |
| `m_previousDesiredElVel` | `double` | Previous El velocity (smoothing) |
| `m_turnAroundTimer` | `QElapsedTimer` | Timer for endpoint delay |
| `m_waitingAtEndpoint` | `bool` | Flag: in turn-around delay |
| `m_azHardwareSign` | `double` | Motor polarity (+1 or -1) |
| `m_azPid` | `PIDController` | Azimuth PID (for fine approach) |
| `m_elPid` | `PIDController` | Elevation PID (for alignment) |

---

## Constants

```cpp
// Motion profile
static constexpr double SMOOTHING_TAU_S = 0.06;           // 60ms smoothing
static constexpr double ARRIVAL_THRESHOLD_DEG = 2.0;      // Endpoint tolerance
static constexpr double EL_ALIGNMENT_THRESHOLD_DEG = 0.5; // Elevation tolerance
static constexpr double TURN_AROUND_DELAY_S = 0.5;        // Pause at endpoints

// From MotionTuningConfig
double scanMaxAccelDegS2 = cfg.motion.scanMaxAccelDegS2;  // ~15 deg/s²
double maxVelocityDegS = cfg.motion.maxVelocityDegS;      // ~12 deg/s
```

---

## Public Methods

### Constructor
```cpp
AutoSectorScanMotionMode(QObject* parent = nullptr)
```

**Purpose:** Initialize scan mode with PID gains from config.

**Actions:**
- Loads PID gains from `MotionTuningConfig::instance().pid.autoScanAz`
- Sets initial state to `AlignElevation`
- Clears zone configuration

---

### setActiveScanZone(const AutoSectorScanZone& scanZone)
```cpp
void setActiveScanZone(const AutoSectorScanZone& scanZone)
```

**Purpose:** Configure scan zone parameters.

**Parameters:**
- `scanZone`: Zone definition with endpoints and speed

**Actions:**
1. Copies zone to `m_activeScanZone`
2. Sets `m_scanZoneSet = true`
3. Takes effect on next `enterMode()` or immediately if already active

**Example:**
```cpp
AutoSectorScanZone zone;
zone.id = 1;
zone.az1 = -30.0; zone.el1 = 15.0;
zone.az2 = +30.0; zone.el2 = 15.0;
zone.scanSpeed = 8.0;
zone.isEnabled = true;

scanMode->setActiveScanZone(zone);
controller->setMotionMode(MotionMode::AutoSectorScan);
```

---

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```

**Purpose:** Initialize scanning when mode becomes active.

**Actions:**
1. Validate zone is set and enabled
2. Start velocity timer
3. Reset both PID controllers
4. **Find closest endpoint:**
   ```cpp
   double d1 = std::abs(shortestDiff(zone.az1, currentAz));
   double d2 = std::abs(shortestDiff(zone.az2, currentAz));
   m_movingToPoint2 = (d2 < d1);  // Start toward farther point
   ```
5. Set initial targets and state to `AlignElevation`
6. Clear smoothing variables

---

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```

**Purpose:** Clean shutdown.

**Actions:**
- Stop servos
- Reset state to `AlignElevation`

---

### updateImpl(GimbalController* controller, double dt)
```cpp
void updateImpl(GimbalController* controller, double dt) override
```

**Purpose:** Main control loop (called by base class after safety check).

**Control Flow:**

#### Phase 1: AlignElevation
```cpp
case ScanState::AlignElevation:
    double elErr = m_targetEl - data.gimbalEl;

    if (std::abs(elErr) < EL_ALIGNMENT_THRESHOLD_DEG) {
        // Elevation aligned, transition to scanning
        m_scanState = ScanState::ScanAzimuth;
        m_waitingAtEndpoint = false;
        return;
    }

    // Move only elevation
    double elVel = m_elPid.Kp * elErr;
    sendStabilizedServoCommands(controller, 0.0, elVel, true, dt);
```

#### Phase 2: ScanAzimuth

**Step 1: Handle Turn-Around Delay**
```cpp
if (m_waitingAtEndpoint) {
    if (m_turnAroundTimer.elapsed() < TURN_AROUND_DELAY_S * 1000) {
        // Still waiting
        sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt);
        return;
    }
    // Delay complete, toggle direction
    m_movingToPoint2 = !m_movingToPoint2;
    m_targetAz = m_movingToPoint2 ? m_activeScanZone.az2 : m_activeScanZone.az1;
    m_waitingAtEndpoint = false;
}
```

**Step 2: Calculate Azimuth Error**
```cpp
double errAz = shortestDiff(m_targetAz, data.gimbalAz);
double distAz = std::abs(errAz);
double direction = (errAz > 0) ? 1.0 : -1.0;
```

**Step 3: Check Arrival**
```cpp
if (distAz < ARRIVAL_THRESHOLD_DEG) {
    // Start turn-around delay
    m_waitingAtEndpoint = true;
    m_turnAroundTimer.start();
    sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt);
    return;
}
```

**Step 4: Trapezoidal Motion Profile**
```cpp
double v_max = m_activeScanZone.scanSpeed;
double accel = MotionTuningConfig::instance().motion.scanMaxAccelDegS2;
double decelDist = (v_max * v_max) / (2.0 * accel);

double desiredVel;
if (distAz <= decelDist) {
    // Deceleration zone: physics-based ramp-down
    double v_dec = std::sqrt(2.0 * accel * distAz);
    desiredVel = direction * std::min(v_max, v_dec);
} else {
    // Cruise zone: constant velocity
    desiredVel = direction * v_max;
}
```

**Step 5: Apply Smoothing**
```cpp
double alpha = dt / (SMOOTHING_TAU_S + dt);
double smoothedAzVel = alpha * desiredVel + (1.0 - alpha) * m_previousDesiredAzVel;
m_previousDesiredAzVel = smoothedAzVel;

// Apply hardware polarity
smoothedAzVel *= m_azHardwareSign;
```

**Step 6: Send Commands**
```cpp
sendStabilizedServoCommands(controller, smoothedAzVel, 0.0, true, dt);
```

---

## Helper Functions

### shortestDiff(double target, double current)
```cpp
static double shortestDiff(double target, double current) {
    double diff = target - current;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    return diff;
}
```
**Purpose:** Calculate shortest angular path (handles 360° wraparound).

---

## Motion Profile Visualization

```
Velocity
  ^
  |        ┌────────────────────────┐
  |        │                        │
v_max ─────│        CRUISE          │
  |        │                        │
  |       /│                        │\
  |      / │                        │ \
  |     /  │                        │  \
  |    /   │                        │   \
  |   / DECEL                 DECEL  \   |
  |  /     │                        │   \|
  | /      │                        │    \
  |/       │                        │     \
──┴────────┴────────────────────────┴──────┴──→ Position
  az1   decelDist              decelDist   az2

decelDist = v_max² / (2 × accel)

Example: v_max = 8 deg/s, accel = 15 deg/s²
         decelDist = 64 / 30 ≈ 2.1°
```

---

## Turn-Around Timing

```
Time →
     │          SCAN                │ WAIT │           SCAN
     │────────────────────────────→ │ 0.5s │ ←────────────────────────
     │        az1 → az2             │      │        az2 → az1
     │                              │      │
Position
  az2 ─────────────────────────────────┬────────────────────────────────
                                       │
  az1 ──────────────────┬──────────────┼───────────────────────────────
                        ↑              ↑
                    Arrival      Delay Complete
                                   (toggle dir)
```

---

## Configuration Parameters

### From MotionTuningConfig

| Parameter | Path | Default | Effect |
|-----------|------|---------|--------|
| `scanMaxAccelDegS2` | `motion` | 15.0 | Trapezoidal acceleration |
| `maxVelocityDegS` | `motion` | 12.0 | Max cruise speed |
| `autoScanAz.kp` | `pid` | 1.5 | Elevation alignment Kp |
| `autoScanAz.ki` | `pid` | 0.0 | Not used |
| `autoScanAz.kd` | `pid` | 0.0 | Not used |

### Zone Parameters

| Parameter | Type | Effect |
|-----------|------|--------|
| `scanSpeed` | `double` | Cruise velocity [deg/s] |
| `az1, el1` | `double` | First endpoint |
| `az2, el2` | `double` | Second endpoint |
| `isEnabled` | `bool` | Zone active flag |

---

## Testing Checklist

- [ ] Zone validation rejects disabled zones
- [ ] Closest endpoint selection works correctly
- [ ] AlignElevation phase moves only elevation
- [ ] Transition to ScanAzimuth occurs at 0.5° el threshold
- [ ] Trapezoidal profile decelerates smoothly
- [ ] Deceleration distance calculated correctly
- [ ] Turn-around delay is exactly 0.5 seconds
- [ ] Direction toggles correctly at each endpoint
- [ ] Smoothing filter prevents velocity jumps
- [ ] Hardware polarity sign applied correctly
- [ ] Stabilization active during scan
- [ ] Zone changes take effect immediately
- [ ] exitMode() stops servos cleanly
- [ ] Works correctly across 0°/360° boundary

---

## Known Issues & Solutions

### Issue 1: Jerky Motion at High Speeds
**Cause:** Smoothing tau too short for high scan speeds.

**Solution:** Increase `SMOOTHING_TAU_S`:
```cpp
static constexpr double SMOOTHING_TAU_S = 0.10;  // from 0.06
```

### Issue 2: Overshoot at Endpoints
**Cause:** Deceleration distance too small for configured acceleration.

**Solution:** Ensure physics is correct:
```cpp
decelDist = v_max² / (2 × accel)
// For v_max=10, accel=15: decelDist = 100/30 = 3.3°
// If overshooting, reduce v_max or increase accel
```

### Issue 3: Stuck in AlignElevation
**Cause:** Elevation threshold too tight or elevation axis not responding.

**Solution:** Check:
1. `EL_ALIGNMENT_THRESHOLD_DEG` is reasonable (0.5°)
2. Elevation servo is connected and enabled
3. Elevation PID gains are non-zero

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Update Rate** | 20 Hz | 50ms cycle |
| **Scan Speed Range** | 2-12 deg/s | Limited by servo |
| **Endpoint Tolerance** | 2.0° | ARRIVAL_THRESHOLD_DEG |
| **Turn-Around Delay** | 0.5s | Fixed |
| **Smoothing Time Constant** | 60ms | SMOOTHING_TAU_S |
| **El Alignment Tolerance** | 0.5° | EL_ALIGNMENT_THRESHOLD_DEG |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with servo control
- **GimbalController:** Mode orchestration
- **SystemStateModel:** Current gimbal position
- **TRPScanMotionMode:** Alternative for discrete waypoints

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation |
| 2026-01-06 | 2.0 | Two-phase motion, trapezoidal profile, turn-around delay, hardware polarity |

---

**END OF DOCUMENTATION**

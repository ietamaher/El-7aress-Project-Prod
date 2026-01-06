# TRPScanMotionMode Class Documentation

**Version:** 2.0
**Last Updated:** 2026-01-06
**Status:** Production

---

## Overview

`TRPScanMotionMode` implements waypoint-based gimbal scanning through a sequence of Target Reference Points (TRPs). Organized by location pages, TRPs are visited sequentially with configurable hold times at each point, enabling systematic surveillance of predefined locations.

**Location:** `src/controllers/motion_modes/trpscanmotionmode.h/cpp`

**Inherits From:** `GimbalMotionModeBase`

**Purpose:** Autonomous surveillance by visiting scripted sequences of geographical positions with configurable dwell times.

---

## Key Design Features (v2.0)

### Page-Based Navigation
- TRPs organized into location pages
- Select page to filter visible TRPs
- Automatic page order indexing

### Hybrid Motion Profile
1. **Cruise Phase:** Constant velocity toward waypoint
2. **Deceleration Phase:** Trapezoidal ramp-down
3. **Fine Approach Phase:** PID control for precise convergence

### Hold Phase
- Configurable dwell time at each TRP
- Active position hold during dwell
- Timer-based progression

### Auto-Loop
- Automatically cycles through all page TRPs
- Continuous surveillance pattern

---

## State Machine

```
┌─────────────────────────────────────────────────────┐
│                TRP SCAN MODE                        │
└─────────────────────────────────────────────────────┘
                       │
                       │ enterMode()
                       ▼
               ┌───────────────┐
               │ Build m_pageOrder│
               │ from current page│
               └───────┬───────┘
                       │
                       │ Page has TRPs?
                       ▼
        ┌──────────────────────────────┐
        │   STATE: SlewToPoint         │
        │   - Hybrid motion profile    │
        │   - Cruise → Decel → Fine    │
        └──────────────┬───────────────┘
                       │
           ┌───────────┴───────────┐
           │ Check Arrival         │
           │ (|errAz| < 0.2° AND   │
           │  |errEl| < 0.2°)      │
           └───────────┬───────────┘
                       │ Arrived
                       ▼
        ┌──────────────────────────────┐
        │   STATE: HoldPoint           │
        │   - Zero velocity commands   │
        │   - Wait for haltTime        │
        │   - Stabilization active     │
        └──────────────┬───────────────┘
                       │
                       │ Timer expires
                       ▼
               ┌───────────────┐
               │ Advance to    │
               │ Next TRP      │
               │ (wrap to 0)   │
               └───────┬───────┘
                       │
                       └───────→ SlewToPoint
```

---

## Data Structures

### TargetReferencePoint
```cpp
struct TargetReferencePoint {
    int id;                // Unique TRP identifier
    int locationPage;      // Page/group this TRP belongs to
    int trpInPage;         // Order within the page (0-based)
    double azimuth;        // Target azimuth [degrees]
    double elevation;      // Target elevation [degrees]
    double haltTime;       // Pause duration at this point [seconds]
    QString name;          // Human-readable name
};
```

**Example:**
```
id: 5
locationPage: 1
trpInPage: 2
azimuth: -45.0°
elevation: 15.0°
haltTime: 5.0s
name: "NW Checkpoint"
```

### State Enumeration
```cpp
enum class State {
    Idle,          // No active scan
    SlewToPoint,   // Moving toward TRP
    HoldPoint      // Dwelling at TRP
};
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_trps` | `QVector<TargetReferencePoint>` | All loaded TRPs |
| `m_pageOrder` | `std::vector<int>` | Indices of TRPs in current page |
| `m_currentOrderIndex` | `size_t` | Current position in m_pageOrder |
| `m_currentState` | `State` | State machine state |
| `m_targetAz` | `double` | Current target azimuth |
| `m_targetEl` | `double` | Current target elevation |
| `m_haltTimer` | `QElapsedTimer` | Dwell time countdown |
| `m_azPid` | `PIDController` | Fine approach PID (azimuth) |
| `m_elPid` | `PIDController` | Fine approach PID (elevation) |
| `m_previousDesiredAzVel` | `double` | Smoothing state |
| `m_previousDesiredElVel` | `double` | Smoothing state |

---

## Constants

```cpp
// Arrival detection
static constexpr double ARRIVAL_THRESHOLD_DEG = 0.2;    // Per-axis tolerance
static constexpr double FINE_APPROACH_DIST_DEG = 8.0;   // PID takes over

// Motion profile (from MotionTuningConfig)
double trpDefaultTravelSpeed = cfg.motion.trpDefaultTravelSpeed;  // ~12 deg/s
double trpMaxAccelDegS2 = cfg.motion.trpMaxAccelDegS2;            // ~50 deg/s²

// Update rate
static constexpr double UPDATE_INTERVAL_S = 0.05;       // 50ms
```

---

## Public Methods

### Constructor
```cpp
TRPScanMotionMode(QObject* parent = nullptr)
```
**Actions:**
- Load PID gains from `MotionTuningConfig::instance().pid.trpScanAz`
- Initialize state to `Idle`
- Clear page order

### setTRPs(const QVector<TargetReferencePoint>& trps)
```cpp
void setTRPs(const QVector<TargetReferencePoint>& trps)
```
**Purpose:** Load all TRP definitions.

**Actions:**
1. Store TRPs in `m_trps`
2. Clear `m_pageOrder` (will be rebuilt on page select)

### selectPage(int locationPage)
```cpp
void selectPage(int locationPage)
```
**Purpose:** Build navigation order for specified page.

**Actions:**
```cpp
m_pageOrder.clear();
for (int i = 0; i < m_trps.size(); ++i) {
    if (m_trps[i].locationPage == locationPage)
        m_pageOrder.push_back(i);
}
// Sort by trpInPage order
std::sort(m_pageOrder.begin(), m_pageOrder.end(),
    [this](int a, int b) {
        return m_trps[a].trpInPage < m_trps[b].trpInPage;
    });
```

### enterMode(GimbalController* controller)
```cpp
void enterMode(GimbalController* controller) override
```
**Actions:**
1. Validate `m_pageOrder` is not empty
   - If empty: log warning, return to Idle
2. Start velocity timer
3. Reset PID controllers
4. Set `m_currentOrderIndex = 0`
5. Load first TRP target
6. Set state to `SlewToPoint`

### exitMode(GimbalController* controller)
```cpp
void exitMode(GimbalController* controller) override
```
**Actions:**
- Stop servos
- Set state to `Idle`

---

## updateImpl(GimbalController* controller, double dt)

### State: Idle
```cpp
case State::Idle:
    stopServos(controller);
    return;
```
No motion. Waits for `enterMode()`.

### State: HoldPoint
```cpp
case State::HoldPoint:
    const auto& currentTrp = m_trps[m_pageOrder[m_currentOrderIndex]];
    qint64 haltMs = std::max(0.0, currentTrp.haltTime * 1000.0);

    if (m_haltTimer.elapsed() >= haltMs) {
        // Dwell complete, advance to next TRP
        m_currentOrderIndex++;

        // Loop back to start
        if (m_currentOrderIndex >= m_pageOrder.size()) {
            m_currentOrderIndex = 0;
            qDebug() << "TRP page loop complete, restarting";
        }

        // Load next target
        const auto& nextTrp = m_trps[m_pageOrder[m_currentOrderIndex]];
        m_targetAz = nextTrp.azimuth;
        m_targetEl = nextTrp.elevation;

        // Reset PIDs and transition
        m_azPid.reset();
        m_elPid.reset();
        m_currentState = State::SlewToPoint;
    }
    // During hold: zero velocity, stabilization active
    sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt);
    return;
```

### State: SlewToPoint

#### Step 1: Calculate Error
```cpp
double errAz = shortestAngleDiff(m_targetAz, data.gimbalAz);
double errEl = -(m_targetEl - data.gimbalEl);  // Note: sign convention

double distAz = std::abs(errAz);
double distEl = std::abs(errEl);
```

#### Step 2: Check Arrival
```cpp
if (distAz < ARRIVAL_THRESHOLD_DEG && distEl < ARRIVAL_THRESHOLD_DEG) {
    // Arrived at TRP
    m_currentState = State::HoldPoint;
    m_haltTimer.start();
    sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt);
    qDebug() << "Arrived at TRP:" << currentTrp.name;
    return;
}
```

#### Step 3: Hybrid Motion Profile
```cpp
const auto& cfg = MotionTuningConfig::instance();
double v_max = cfg.motion.trpDefaultTravelSpeed;
double accel = cfg.motion.trpMaxAccelDegS2;

double desiredAzVel, desiredElVel;

// === AZIMUTH ===
if (distAz < FINE_APPROACH_DIST_DEG) {
    // Fine approach: PID control
    desiredAzVel = m_azPid.Kp * errAz;
} else {
    // Trapezoidal: cruise or decel
    double decelDist = (v_max * v_max) / (2.0 * accel);
    if (distAz < decelDist) {
        double v_req = std::sqrt(2.0 * accel * distAz);
        desiredAzVel = (errAz > 0 ? 1.0 : -1.0) * std::min(v_max, v_req);
    } else {
        desiredAzVel = (errAz > 0 ? 1.0 : -1.0) * v_max;
    }
}

// === ELEVATION (same logic) ===
if (distEl < FINE_APPROACH_DIST_DEG) {
    desiredElVel = m_elPid.Kp * errEl;
} else {
    double decelDist = (v_max * v_max) / (2.0 * accel);
    if (distEl < decelDist) {
        double v_req = std::sqrt(2.0 * accel * distEl);
        desiredElVel = (errEl > 0 ? 1.0 : -1.0) * std::min(v_max, v_req);
    } else {
        desiredElVel = (errEl > 0 ? 1.0 : -1.0) * v_max;
    }
}
```

#### Step 4: Send Commands
```cpp
sendStabilizedServoCommands(controller, desiredAzVel, desiredElVel, true, dt);
```

---

## Motion Profile Visualization

```
Velocity
  ^
  |         ┌───────────────────┐
  |         │                   │
v_max ──────│      CRUISE       │
  |         │                   │
  |        /│                   │\
  |       / │                   │ \
  |      /  │     DECEL         │  \
  |     /   │                   │   \
  |    /    │           ┌───────┤    \
  |   /     │           │ FINE  │     \
  |  /      │           │ (PID) │      \
  | /       │           │       │       \
──┴─────────┴───────────┴───────┴────────┴──→ Distance
  start                 8°     0.2°   target

FINE_APPROACH_DIST_DEG = 8°
ARRIVAL_THRESHOLD_DEG = 0.2°
```

---

## Page-Based Navigation Example

```
m_trps = [
    {id=0, page=1, order=0, az=-45, el=10, name="P1-A"},
    {id=1, page=1, order=1, az=0,   el=15, name="P1-B"},
    {id=2, page=1, order=2, az=45,  el=10, name="P1-C"},
    {id=3, page=2, order=0, az=-30, el=20, name="P2-A"},
    {id=4, page=2, order=1, az=30,  el=20, name="P2-B"},
]

selectPage(1):
  m_pageOrder = [0, 1, 2]  // Indices of page 1 TRPs
  Navigation: P1-A → P1-B → P1-C → P1-A → ...

selectPage(2):
  m_pageOrder = [3, 4]     // Indices of page 2 TRPs
  Navigation: P2-A → P2-B → P2-A → ...
```

---

## Configuration Parameters

### From MotionTuningConfig

| Parameter | Path | Default | Effect |
|-----------|------|---------|--------|
| `trpDefaultTravelSpeed` | `motion` | 12.0 | Cruise velocity [deg/s] |
| `trpMaxAccelDegS2` | `motion` | 50.0 | Trapezoidal accel [deg/s²] |
| `trpScanAz.kp` | `pid` | 1.2 | Fine approach P-gain |
| `trpScanAz.ki` | `pid` | 0.0 | Not used |
| `trpScanAz.kd` | `pid` | 0.0 | Not used |

### Per-TRP Parameters

| Parameter | Type | Effect |
|-----------|------|--------|
| `azimuth` | `double` | Target azimuth [degrees] |
| `elevation` | `double` | Target elevation [degrees] |
| `haltTime` | `double` | Dwell duration [seconds] |
| `locationPage` | `int` | Page grouping |
| `trpInPage` | `int` | Visit order within page |

---

## Testing Checklist

- [ ] `setTRPs()` loads all TRP definitions
- [ ] `selectPage()` filters and orders correctly
- [ ] Empty page handled gracefully (stays in Idle)
- [ ] `enterMode()` starts at first TRP of page
- [ ] Hybrid motion profile: cruise → decel → fine
- [ ] Fine approach uses PID (8° threshold)
- [ ] Arrival detection per-axis (0.2° each)
- [ ] HoldPoint timer counts correctly
- [ ] Page loops to first TRP after last
- [ ] Stabilization active during all phases
- [ ] Elevation sign convention correct
- [ ] Azimuth shortest path (handles wrap)
- [ ] exitMode() stops servos cleanly

---

## Known Issues & Solutions

### Issue 1: TRPs Visited Out of Order
**Cause:** `trpInPage` values not set correctly.

**Solution:** Ensure TRPs have sequential `trpInPage` values:
```cpp
trp[0].trpInPage = 0;
trp[1].trpInPage = 1;
trp[2].trpInPage = 2;
```

### Issue 2: Slow Convergence to TRP
**Cause:** `FINE_APPROACH_DIST_DEG` too large or PID Kp too low.

**Solution:**
```cpp
FINE_APPROACH_DIST_DEG = 5.0;  // from 8.0
m_azPid.Kp = 1.5;              // from 1.2
```

### Issue 3: Gimbal Oscillates at TRP
**Cause:** PID Kp too high, no damping.

**Solution:** Add D-term:
```cpp
m_azPid.Kd = 0.1;
m_elPid.Kd = 0.1;
```

### Issue 4: Elevation Goes Wrong Direction
**Cause:** Sign convention mismatch.

**Current Code:**
```cpp
double errEl = -(m_targetEl - data.gimbalEl);
```

**Check:** Verify positive elevation = gimbal tilting up.

---

## Performance Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Update Rate** | 20 Hz | 50ms cycle |
| **Travel Speed** | 12 deg/s | Configurable |
| **Max Acceleration** | 50 deg/s² | Configurable |
| **Arrival Tolerance** | 0.2° | Per-axis |
| **Fine Approach Zone** | 8° | PID active |
| **Min Dwell Time** | 0 sec | Can skip |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class with servo control
- **GimbalController:** Mode orchestration
- **SystemStateModel:** Current gimbal position
- **AutoSectorScanMotionMode:** Alternative (continuous two-point scan)

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation |
| 2026-01-06 | 2.0 | Page-based navigation, hybrid motion profile, fine approach PID |

---

**END OF DOCUMENTATION**

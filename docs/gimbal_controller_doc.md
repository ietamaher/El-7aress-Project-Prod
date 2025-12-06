# GimbalController Class Documentation

## Overview

`GimbalController` is the central orchestrator for the gimbal system. It manages all motion modes, coordinates hardware communication with servo drivers, updates system state, and ensures safety. It acts as the main "brain" of the gimbal, delegating specific motion behaviors to specialized motion mode classes while handling cross-cutting concerns like state management, mode switching, and alarm handling.

**Location:** `gimbalcontroller.h/cpp`

**Responsibility:** High-level gimbal control logic, mode management, and hardware coordination

**Pattern:** Controller/Facade pattern - centralizes all gimbal operations and abstracts complexity of multiple motion modes and hardware devices

---

## Dependencies

- **Motion Mode Classes:** ManualMotionMode, TrackingMotionMode, AutoSectorScanMotionMode, RadarSlewMotionMode, TRPScanMotionMode
- **Hardware Interfaces:**
  - `ServoDriverDevice` (×2) - Azimuth and elevation motor drivers
  - `Plc42Device` - PLC interface for system control
- **State Management:** `SystemStateModel` - centralized system state data source
- **Qt Core:** QObject, QTimer, Qt signals/slots

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  GimbalController                       │
│                  (Central Orchestrator)                 │
└──────────────┬──────────────────────────┬───────────────┘
               │                          │
    ┌──────────┴─────────┬────────────────┴──────────┐
    │                    │                           │
    ▼                    ▼                           ▼
┌─────────────┐  ┌─────────────────┐  ┌──────────────────┐
│ Motion Mode │  │  Hardware I/O   │  │ State Management │
│ Dispatcher  │  │                 │  │                  │
│             │  ├─ ServoDriver Az │  ├─ SystemStateData │
├─ Manual     │  ├─ ServoDriver El │  ├─ Mode Switching  │
├─ Tracking   │  ├─ PLC42          │  ├─ Tracking Input  │
├─ Scanning   │  └─────────────────┘  ├─ Alarm Status    │
└─────────────┘                        └──────────────────┘
     │
     ├─ ManualMotionMode
     ├─ TrackingMotionMode
     ├─ AutoSectorScanMotionMode
     ├─ RadarSlewMotionMode
     └─ TRPScanMotionMode
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_azServo` | `ServoDriverDevice*` | Hardware driver for azimuth motor |
| `m_elServo` | `ServoDriverDevice*` | Hardware driver for elevation motor |
| `m_plc42` | `Plc42Device*` | PLC interface for system control |
| `m_stateModel` | `SystemStateModel*` | Central state data repository |
| `m_currentMode` | `std::unique_ptr<GimbalMotionModeBase>` | Currently active motion mode instance |
| `m_currentMotionModeType` | `MotionMode` | Enum identifying current mode type |
| `m_updateTimer` | `QTimer*` | 50ms periodic update timer |
| `m_oldState` | `SystemStateData` | Previous state snapshot for change detection |

---

## Public Methods

### Constructor
```cpp
GimbalController(ServoDriverDevice* azServo,
                 ServoDriverDevice* elServo,
                 Plc42Device* plc42,
                 SystemStateModel* stateModel,
                 QObject* parent = nullptr)
```

**Purpose:** Initialize the gimbal controller with hardware interfaces and state model.

**Parameters:**
- `azServo`: Motor driver for azimuth axis
- `elServo`: Motor driver for elevation axis
- `plc42`: PLC device for system control
- `stateModel`: System state repository
- `parent`: Qt parent object

**Actions:**
1. Stores hardware interface pointers
2. Connects state model signals (detects state changes)
3. Connects servo alarm signals (detects motor faults)
4. Creates 50ms update timer for control loop
5. Sets initial motion mode to `Idle`

**Signal Connections:**
- `SystemStateModel::dataChanged` → `onSystemStateChanged()`
- `ServoDriverDevice::alarmDetected` → `onAzAlarmDetected()` / `onElAlarmDetected()`
- `ServoDriverDevice::alarmCleared` → `onAzAlarmCleared()` / `onElAlarmCleared()`
- `QTimer::timeout` → `update()`

---

### shutdown()
```cpp
void shutdown()
```

**Purpose:** Gracefully shutdown the gimbal controller.

**Actions:**
1. Calls `exitMode()` on current motion mode
2. Stops the update timer
3. Prevents further control loop iterations

**When to Call:** Application shutdown or error recovery

---

### setMotionMode(MotionMode newMode)
```cpp
void setMotionMode(MotionMode newMode)
```

**Purpose:** Switch gimbal to a different motion mode (Manual, Tracking, Scanning, etc.).

**Parameters:**
- `newMode`: Target motion mode enum value

**Mode Switching Flow:**

**Step 1: Exit Current Mode**
```cpp
if (m_currentMode) {
    m_currentMode->exitMode(this);  // Cleanup, stop servos
}
```

**Step 2: Create New Mode Instance**
```cpp
switch (newMode) {
    case MotionMode::Manual:
        m_currentMode = std::make_unique<ManualMotionMode>();
        break;
    case MotionMode::AutoTrack:
    case MotionMode::ManualTrack:
        m_currentMode = std::make_unique<TrackingMotionMode>();
        break;
    case MotionMode::AutoSectorScan:
        auto scanMode = std::make_unique<AutoSectorScanMotionMode>();
        // Configure with active zone from system state
        int activeId = m_stateModel->data().activeAutoSectorScanZoneId;
        // Find and set the corresponding scan zone...
        break;
    // ... more modes
}
```

**Step 3: Configure Mode-Specific Parameters**

For **AutoSectorScan:**
- Retrieves `activeAutoSectorScanZoneId` from state
- Searches `m_stateModel->data().sectorScanZones` for matching zone
- Calls `scanMode->setActiveScanZone()` with the zone
- If zone not found or disabled, falls back to Idle

For **TRPScan:**
- Retrieves `activeTRPLocationPage` from state
- Filters `m_stateModel->data().targetReferencePoints` by page number
- Calls `trpMode->setActiveTRPPage()` with filtered points
- If no matching points, falls back to Idle

**Step 4: Enter New Mode**
```cpp
m_currentMode->enterMode(this);  // Initialize servos, PID controllers
```

**Step 5: Update Mode Type**
```cpp
m_currentMotionModeType = newMode;
```

**Important:** New mode instance is created fresh each time (not reused), ensuring clean state.

---

### update()
```cpp
void update()
```

**Purpose:** Main control loop called every 50ms by the update timer.

**Called:** Automatically via `m_updateTimer` (20 Hz frequency)

**Control Flow:**

```cpp
if (!m_currentMode) return;

// 1. Update gyro bias calibration
m_currentMode->updateGyroBias(m_stateModel->data());

// 2. Check safety conditions
if (m_currentMode->checkSafetyConditions(this)) {
    // Safe: execute motion mode logic
    m_currentMode->update(this);
} else {
    // Unsafe: stop immediately
    m_currentMode->stopServos(this);
}
```

**Safety Priority:** Even if a mode is active, if safety conditions fail (E-stop, power loss, etc.), servos are stopped before returning control to the mode.

**Frequency:** ~20 Hz (50ms interval) provides responsive control while managing CPU load

---

### readAlarms()
```cpp
void readAlarms()
```
**Purpose:** Trigger alarm status read from both servo drivers.

**Actions:**
- Calls `m_azServo->readAlarmStatus()`
- Calls `m_elServo->readAlarmStatus()`

**Result:** Servo drivers query their status and emit `alarmDetected` or `alarmCleared` signals

---

### clearAlarms()
```cpp
void clearAlarms()
```
**Purpose:** Clear motor alarms and reset system state.

**Implementation:**
1. Sends `setResetAlarm(0)` to PLC42
2. Waits 1 second
3. Sends `setResetAlarm(1)` to PLC42

**Why Two Commands?** Some PLC systems require a low-high pulse sequence for reliable reset.

**When to Use:** After resolving an alarm condition (e.g., removing obstacle, clearing fault)

---

## Protected Methods

### onSystemStateChanged(const SystemStateData& newData)
```cpp
void onSystemStateChanged(const SystemStateData& newData)
```

**Purpose:** Handle state changes from SystemStateModel (slot function).

**Called:** When `SystemStateModel::dataChanged` signal is emitted

**Responsibilities:**

#### 1. Detect Motion Mode Changes
```cpp
if (m_oldState.motionMode != newData.motionMode) {
    setMotionMode(newData.motionMode);
}
```
If user changes mode via UI or remote command, automatically switch to new mode.

#### 2. Detect Scan Parameter Changes
```cpp
if (newData.motionMode == MotionMode::AutoSectorScan &&
    m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId) {
    // Active zone ID changed while in scan mode
    setMotionMode(newData.motionMode);  // Re-initialize with new zone
}
```
If operator changes which zone is active during an ongoing scan, reconfigure the mode.

#### 3. Update Tracking Target
```cpp
if (newData.motionMode == MotionMode::AutoTrack && m_currentMode) {
    if (auto* trackingMode = dynamic_cast<TrackingMotionMode*>(m_currentMode.get())) {
        if (newData.trackerHasValidTarget) {
            // Calculate pixel error from camera frame
            float errorPxX = newData.trackedTargetCenterX_px - screenCenterX_px;
            float errorPxY = newData.trackedTargetCenterY_px - screenCenterY_px;
            
            // Convert pixel error to angular offset
            QPointF angularOffset = GimbalUtils::calculateAngularOffsetFromPixelError(
                errorPxX, errorPxY,
                newData.currentImageWidthPx, newData.currentImageHeightPx,
                activeHfov
            );
            
            // Calculate gimbal target position
            double targetGimbalAz = newData.gimbalAz + angularOffset.x();
            double targetGimbalEl = newData.gimbalEl + angularOffset.y();
            
            // Calculate target velocity from pixel velocity
            QPointF angularVelocity = GimbalUtils::calculateAngularOffsetFromPixelError(
                newData.trackedTargetVelocityX_px_s,
                newData.trackedTargetVelocityY_px_s,
                ...
            );
            
            // Send updated target to tracking mode
            trackingMode->onTargetPositionUpdated(
                targetGimbalAz, targetGimbalEl,
                angularVelocity.x(), angularVelocity.y(),
                true  // Valid target
            );
        } else {
            // Target lost
            trackingMode->onTargetPositionUpdated(0, 0, 0, 0, false);
        }
    }
}
```

**Key Transformation:** Converts camera tracker output (pixel coordinates) into gimbal commands.

#### 4. Update No-Traverse Zone Status
```cpp
float aimAz = newData.gimbalAz;
float aimEl = newData.gimbalEl;
bool inNTZ = m_stateModel->isPointInNoTraverseZone(aimAz, aimEl);
if (newData.isReticleInNoTraverseZone != inNTZ) {
    m_stateModel->setPointInNoTraverseZone(inNTZ);
}
```
Checks if current gimbal position is in a no-traverse zone and updates state.

#### 5. Update Old State Snapshot
```cpp
m_oldState = newData;
```
Saves current state for next cycle's change detection.

---

### Alarm Signal Handlers

#### onAzAlarmDetected(uint16_t alarmCode, const QString& description)
```cpp
void onAzAlarmDetected(uint16_t alarmCode, const QString& description)
{
    emit azAlarmDetected(alarmCode, description);
}
```
Azimuth servo detected a fault. Re-emits signal for UI/logging.

#### onAzAlarmCleared()
```cpp
void onAzAlarmCleared()
{
    emit azAlarmCleared();
}
```
Azimuth servo fault resolved.

#### onElAlarmDetected(...) / onElAlarmCleared()
Same as azimuth, but for elevation axis.

---

## Utility Functions (GimbalUtils Namespace)

### calculateAngularOffsetFromPixelError(...)
```cpp
QPointF calculateAngularOffsetFromPixelError(
    double errorPxX, double errorPxY,
    int imageWidthPx, int imageHeightPx,
    float cameraHfovDegrees)
```

**Purpose:** Convert pixel-space tracking error into gimbal angle offsets.

**Parameters:**
- `errorPxX, errorPxY`: Pixel error (tracked_pos - screen_center)
- `imageWidthPx, imageHeightPx`: Camera image resolution
- `cameraHfovDegrees`: Horizontal field of view

**Computation:**

**Azimuth (X) Offset:**
```
degreesPerPixelAz = cameraHfovDegrees / imageWidthPx
angularOffsetXDeg = errorPxX * degreesPerPixelAz
```
Maps horizontal pixel error to azimuth adjustment.

**Elevation (Y) Offset:**
```
aspectRatio = imageWidthPx / imageHeightPx
vfov_deg = arctan(tan(hfov/2) / aspectRatio) * 2
degreesPerPixelEl = vfov_deg / imageHeightPx
angularOffsetYDeg = -errorPxY * degreesPerPixelEl  // Negative for inverted Y axis
```
Maps vertical pixel error to elevation adjustment, accounting for aspect ratio.

**Example:**
```
errorPxX = +10 px (target is 10 pixels right of center)
imageWidthPx = 1920
hfov = 60°
→ degreesPerPixelAz = 60 / 1920 = 0.03125 deg/px
→ angularOffsetXDeg = +10 * 0.03125 = +0.3125°
(Gimbal should move 0.3125° clockwise/right)
```

**Returns:** QPointF with x = azimuth offset, y = elevation offset [degrees]

---

## Data Flow: Tracking Mode Example

```
1. Camera Tracker finds target in image frame
   └─→ Tracker outputs pixel coordinates & velocity

2. SystemStateModel receives tracker output
   └─→ Stores in: trackedTargetCenterX_px, trackedTargetCenterY_px,
                  trackedTargetVelocityX_px_s, trackedTargetVelocityY_px_s

3. SystemStateModel emits dataChanged signal
   └─→ GimbalController::onSystemStateChanged() is called

4. GimbalController checks if in AutoTrack mode
   └─→ Gets TrackingMotionMode instance

5. GimbalController calculates angular offset
   └─→ Calls GimbalUtils::calculateAngularOffsetFromPixelError()
   └─→ Result: desired gimbal angle = current + offset

6. GimbalController sends target to TrackingMotionMode
   └─→ trackingMode->onTargetPositionUpdated(az, el, vel_az, vel_el, true)

7. TrackingMotionMode::update() runs in next 50ms cycle
   └─→ Computes PID to move gimbal to target
   └─→ Sends servo commands

8. Gimbal physically moves toward target in image
   └─→ Loop repeats every 50ms
```

---

## Mode Switching Sequence

**User Request:** Switch from Manual to AutoTrack

```
Time   | Action
──────┼────────────────────────────────────────────
T0    | GimbalController::setMotionMode(AutoTrack)
      |
T0+1  | Exit Manual mode
      | └─ ManualMotionMode::exitMode()
      | └─ stopServos(gimbal)
      |
T0+2  | Create Tracking mode
      | └─ m_currentMode = make_unique<TrackingMotionMode>()
      |
T0+3  | Enter Tracking mode
      | └─ TrackingMotionMode::enterMode()
      | └─ Reset PIDs, configure acceleration
      |
T0+4  | Update mode type
      | └─ m_currentMotionModeType = MotionMode::AutoTrack
      |
T0+5  | Ready for next update()
      | └─ TrackingMotionMode::update() will run in 50ms
```

**Duration:** < 5ms (typically)

---

## State Management

### Old State vs New State
```cpp
// Each cycle:
onSystemStateChanged(newData) {
    // Detect what changed
    bool modeChanged = (m_oldState.motionMode != newData.motionMode);
    bool zoneChanged = (m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId);
    
    // React to changes
    if (modeChanged) setMotionMode(newData.motionMode);
    if (zoneChanged) reconfigureMode();
    
    // Update tracking target if needed
    // ...
    
    // Save for next cycle
    m_oldState = newData;
}
```

This pattern prevents unnecessary reconfigurations and detects when operators change parameters remotely.

---

## Control Loop Timing

```
┌─────────────┐
│   50ms      │
│  interval   │
└──────┬──────┘
       │
       ▼
┌──────────────────────────────────────┐
│ GimbalController::update()           │
├──────────────────────────────────────┤
│ 1. Check safety conditions    ~1ms   │
│ 2. Update gyro bias           ~2ms   │
│ 3. Call motion mode update    ~5ms   │
│    └─ PID computation         ~2ms   │
│    └─ Servo commands          ~1ms   │
│ 4. Return                            │
└──────────────────────────────────────┘
       │
       │ ~9ms total latency
       │
       ▼
   ┌─ 41ms idle ─┐
   │             │
   └─────────────┘
       │
       ▼ (repeat every 50ms)
```

**Effective Control Frequency:** 20 Hz (50ms cycle)

**Latency:** ~9ms from state update to servo command

---

## Integration Points

### Input: SystemStateModel
```cpp
// GimbalController reads from system state
systemState.gimbalAz, gimbalEl           // Current gimbal angles
systemState.imuPitchDeg, imuRollDeg      // IMU orientation
systemState.trackedTargetCenterX_px      // Tracking target
systemState.sectorScanZones              // Scan zone list
systemState.targetReferencePoints        // TRP list
systemState.enableStabilization          // Stabilization flag
systemState.stationEnabled               // Power flag
systemState.emergencyStopActive          // Safety flag
```

### Output: Servo Drivers
```cpp
// GimbalController commands servo drivers via base class
writeVelocityCommand(azServo, velocityAz_dps, scaling)
writeVelocityCommand(elServo, velocityEl_dps, scaling)
setAcceleration(servo, accelRate)
```

### Signals Emitted
```cpp
signal azAlarmDetected(uint16_t code, QString desc)
signal azAlarmCleared()
signal elAlarmDetected(uint16_t code, QString desc)
signal elAlarmCleared()
```

---

## Safety & Error Handling

### Centralized Safety Check
```cpp
void update() {
    // ALWAYS check safety first
    if (!m_currentMode->checkSafetyConditions(this)) {
        m_currentMode->stopServos(this);
        return;  // Skip motion mode logic
    }
    m_currentMode->update(this);  // Only if safe
}
```

**Prevents:** Gimbal motion during E-stop, power loss, or emergency

### Mode Validation
```cpp
// When switching to scan mode
if (zone not found OR zone disabled) {
    qWarning() << "Invalid zone, switching to Idle";
    m_currentMode = nullptr;
    newMode = MotionMode::Idle;
}
```

**Prevents:** Hanging in invalid modes

### Alarm Detection
```cpp
// Servo drivers emit alarms
onAzAlarmDetected(code, desc) {
    // Log alarm
    // Update UI
    // Could trigger mode switch to Idle if critical
}
```

---

## Testing Checklist

- [ ] Mode switching works (all 5 modes)
- [ ] Mode parameters (zones, TRPs) are correctly loaded
- [ ] Tracking target updates correctly from camera
- [ ] Safety conditions stop gimbal when violated
- [ ] Alarms are detected and reported
- [ ] State changes trigger appropriate reconfigurations
- [ ] Gimbal returns to Idle if scan zone becomes disabled
- [ ] No-traverse zone detection works
- [ ] 50ms update cycle runs consistently
- [ ] Mode teardown properly stops servos
- [ ] Mode teardown prevents residual motion

---

## Common Pitfalls

### Pitfall 1: Mode Created But Not Entered
```cpp
// WRONG: Mode exists but not initialized
m_currentMode = std::make_unique<ScanMode>();
// enterMode() not called → PIDs not reset, servos not configured
```

**Fix:** Always call `enterMode()` after creating mode instance.

---

### Pitfall 2: State Update But Mode Not Switched
```cpp
// WRONG: Zone changed but mode not reconfigured
m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId
// Detected change but forgot to call setMotionMode()
```

**Fix:** Always call `setMotionMode()` when scanning parameters change.

---

### Pitfall 3: Pixel Error Direction Wrong
```cpp
// WRONG: Camera coordinate frame mismatch
errorPxX = screenCenterX - trackedTargetCenterX;  // Inverted sign
// Gimbal moves opposite to target
```

**Fix:** Verify: `errorPxX = trackedTargetCenterX - screenCenterX` (target relative to center).

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; added mode switching flow, tracking data pipeline, safety model |

---

## Related Classes

- **GimbalMotionModeBase:** Parent class for all motion modes
- **TrackingMotionMode:** Target tracking implementation
- **ManualMotionMode:** Joystick control implementation
- **AutoSectorScanMotionMode:** Back-and-forth scanning
- **SystemStateModel:** State data repository
- **ServoDriverDevice:** Hardware motor driver interface

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Production - Core gimbal orchestration
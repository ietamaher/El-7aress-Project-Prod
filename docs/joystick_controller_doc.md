# JoystickController Class Documentation

## Overview

`JoystickController` translates raw joystick input (buttons, axes, hats) into gimbal, camera, and weapon system commands. It acts as an input translator between the hardware joystick and the high-level system controllers, enforcing safety rules (deadman switch, acquisition phase restrictions) and managing complex multi-press gestures (double-click for tracking abort).

**Location:** `joystickcontroller.h/cpp`

**Responsibility:** Input handling, mode transitions, safety enforcement for operator commands

**Design Pattern:** Input-to-Command translator with safety constraints

---

## Dependencies

- **Input Model:** `JoystickDataModel` - processes raw joystick events
- **System State:** `SystemStateModel` - reads/writes operational state
- **Gimbal Control:** `GimbalController` - motion mode management
- **Camera Control:** `CameraController` - zoom and thermal imaging
- **Weapon Control:** `WeaponController` - firing and ballistic compensation
- Qt Core: QObject, QDateTime (for debouncing)

---

## Architecture

```
Physical Joystick Hardware
    ↓
JoystickDevice (SDL layer)
    ↓
JoystickDataModel (coalesces events)
    ├→ axisMoved(axis, value)
    ├→ buttonPressed(button, state)
    └→ hatMoved(hat, value)
         ↓
    JoystickController (enforces rules)
    ├→ onAxisChanged()
    ├→ onButtonChanged()
    └→ onHatChanged()
         ↓
    System Controllers
    ├→ GimbalController (motion modes)
    ├→ CameraController (zoom, LUT)
    └→ WeaponController (fire control)
         ↓
    Hardware Output
    ├→ Servo Motors (gimbal)
    ├→ Camera Pan/Tilt/Zoom
    └→ Weapon Fire Signal
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_joystickModel` | `JoystickDataModel*` | Source of joystick input events |
| `m_stateModel` | `SystemStateModel*` | Operational state (modes, tracking phase) |
| `m_gimbalController` | `GimbalController*` | Gimbal motion mode control |
| `m_cameraController` | `CameraController*` | Camera zoom and imaging |
| `m_weaponController` | `WeaponController*` | Weapon firing and ballistics |
| `m_activeCameraIndex` | `int` | Currently selected camera (0=day, 1=night) |
| `m_lastTrackButtonPressTime` | `qint64` | Timestamp of last TRACK button press [ms] |
| `videoLUT` | `int` | Current thermal LUT index (0-12) |

---

## Constants

```cpp
static constexpr int DOUBLE_CLICK_INTERVAL_MS = 300;  // Max time between presses for double-click
```

---

## Public Methods

### Constructor
```cpp
JoystickController(JoystickDataModel *joystickModel,
                   SystemStateModel *stateModel,
                   GimbalController *gimbalCtrl,
                   CameraController *cameraCtrl,
                   WeaponController *weaponCtrl,
                   QObject *parent = nullptr)
```

**Purpose:** Initialize joystick input handler with all system controller dependencies.

**Actions:**
1. Stores pointers to all controller dependencies
2. Connects joystick model signals to slot handlers:
   - `JoystickDataModel::axisMoved` → `onAxisChanged()`
   - `JoystickDataModel::buttonPressed` → `onButtonChanged()`
   - `JoystickDataModel::hatMoved` → `onHatChanged()`
3. Initializes state variables (active camera = day, LUT = 0, etc.)

---

## Protected Methods (Slots)

### onAxisChanged(int axis, float value)
```cpp
void onAxisChanged(int axis, float value)
```

**Purpose:** Handle continuous joystick axis input (analog sticks).

**Parameters:**
- `axis`: Axis identifier (0=azimuth, 1=elevation, etc.)
- `value`: Normalized axis value [-1.0 to +1.0]

**Current Implementation:**
```
Axis 0 (X): Azimuth control
Axis 1 (Y): Elevation control (inverted sign: -value)
   Scaling: value * 10.0 deg/s
```

**Typical Usage:** Maps to gimbal manual control or velocity commands.

**Notes:**
- Currently commented out (not actively used)
- Velocity mapping would be: `velocityAz = value * 10.0f`
- Elevation is inverted: `velocityEl = -value * 10.0f`

---

### onHatChanged(int hat, int value)
```cpp
void onHatChanged(int hat, int value)
```

**Purpose:** Handle D-pad / hat switch input (directional buttons).

**Parameters:**
- `hat`: Hat switch identifier (usually 0 for D-pad)
- `value`: Direction constant (SDL_HAT_UP, SDL_HAT_DOWN, etc.)

**Control Logic:**

#### During Tracking Acquisition Phase
When in `TrackingPhase::Acquisition`, hat control resizes the acquisition gate:

```
D-Pad UP:    Decrease gate height    (sizeStep = -4 pixels)
D-Pad DOWN:  Increase gate height    (sizeStep = +4 pixels)
D-Pad LEFT:  Decrease gate width     (sizeStep = -4 pixels)
D-Pad RIGHT: Increase gate width     (sizeStep = +4 pixels)

Command: m_stateModel->adjustAcquisitionBoxSize(dW, dH)
```

**Rationale:** During target acquisition, operator quickly adjusts tracking gate to fit target.

#### Outside Acquisition Phase
Currently no action (fallback logic commented out).

---

### onButtonChanged(int button, bool pressed)
```cpp
void onButtonChanged(int button, bool pressed)
```

**Purpose:** Handle discrete button input with complex multi-press logic and safety checks.

**Control Flow:**

```
Button Pressed?
    ↓
┌─────────────────────┐
│ Safety Checks       │
├─────────────────────┤
│ • Station enabled?  │
│ • Deadman switch?   │
│ • Acquisition phase?│
└─────────────────────┘
    ↓
┌─────────────────────┐
│ Context-Specific    │
│ Button Action       │
├─────────────────────┤
│ Mode: Manual        │
│ Mode: Tracking      │
│ Mode: Surveillance  │
│ Mode: Scanning      │
└─────────────────────┘
```

---

## Button Mapping

### Button 0: Engagement Momentary Switch
```cpp
case 0:
    if (pressed) {
        m_stateModel->commandEngagement(true);   // On press
    } else {
        m_stateModel->commandEngagement(false);  // On release
    }
```
**Purpose:** Momentary switch for system engagement/disengagement.

---

### Button 2: Lead Angle Compensation (LAC) Toggle
```cpp
case 2:
    if (pressed) {
        if (!deadManSwitchActive) {
            qDebug() << "Cannot toggle LAC without deadman";
            return;
        }
        bool currentLAC = m_stateModel->data().leadAngleCompensationActive;
        m_stateModel->setLeadAngleCompensationActive(!currentLAC);
        
        if (!currentLAC) {  // Was off, now on
            m_weaponController->updateFireControlSolution();
        }
    }
```
**Purpose:** Enable/disable ballistic lead angle compensation.

**Safety:** Requires deadman switch to be active.

**Action:** When enabling, triggers fire control solution update.

---

### Button 3: Deadman Switch
```cpp
case 3:
    m_stateModel->setDeadManSwitch(pressed);
```
**Purpose:** Enable/disable entire control system via deadman switch.

**Impact:** All safety-critical operations require this to be active.

---

### Button 4: Tracking Control (Complex Multi-Press)
```cpp
case 4:
    if (!deadManSwitchActive) {
        qDebug() << "TRACK ignored, deadman not active";
        return;
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool isDoubleClick = (now - m_lastTrackButtonPressTime) < DOUBLE_CLICK_INTERVAL_MS;
    m_lastTrackButtonPressTime = now;
```

**Multi-Click State Machine:**

```
Off (initial)
    ↓
Single Press (within 300ms of previous)
    ↓
Acquisition Phase
    │
    └─→ Single Press
        ↓
    Lock-On Requested (Tracking_LockPending)
        ↓
    Active Track / Coast / Firing
        │
        └─→ Double-Click (within 300ms)
            ↓
        Abort Tracking (back to Off)
```

**Detailed State Transitions:**

| Current State | Single Press | Double Click |
|---------------|--------------|--------------|
| `Off` | → `Acquisition` | N/A |
| `Acquisition` | → `Tracking_LockPending` | (ignored) |
| `Tracking_LockPending` | Log "already tracking" | → `Off` |
| `Tracking_ActiveLock` | Log "already tracking" | → `Off` |
| `Tracking_Coast` | Log "already tracking" | → `Off` |
| `Tracking_Firing` | Log "already tracking" | → `Off` |

**Implementation:**
```cpp
if (isDoubleClick) {
    qDebug() << "Double-click detected. Aborting track.";
    m_stateModel->stopTracking();
    return;
}

// Single press logic
switch (currentPhase) {
    case TrackingPhase::Off:
        m_stateModel->startTrackingAcquisition();
        break;
    case TrackingPhase::Acquisition:
        m_stateModel->requestTrackerLockOn();
        break;
    default:
        qDebug() << "Already tracking, double-click to abort.";
        break;
}
```

---

### Button 5: Fire Weapon
```cpp
case 5:
    if (!curr.stationEnabled) {
        qDebug() << "Cannot fire, station off";
        return;
    }
    if (pressed) {
        m_weaponController->startFiring();
    } else {
        m_weaponController->stopFiring();
    }
```
**Purpose:** Trigger weapon firing on button press, stop on release.

**Safety:** Station must be enabled.

**Behavior:** Momentary control (fire starts on press, stops on release).

---

### Button 6 & 8: Camera Zoom In/Out
```cpp
case 6:  // Zoom In
    if (pressed) {
        m_cameraController->zoomIn();
    } else {
        m_cameraController->zoomStop();
    }

case 8:  // Zoom Out
    if (pressed) {
        m_cameraController->zoomOut();
    } else {
        m_cameraController->zoomStop();
    }
```
**Purpose:** Continuous zoom control with auto-stop on release.

**Behavior:** Momentary (zoom continues while pressed, stops on release).

---

### Button 7 & 9: Thermal LUT Cycle (Thermal Camera Only)
```cpp
case 7:  // Next LUT
    if (pressed && !curr.activeCameraIsDay) {
        videoLUT = std::min(videoLUT + 1, 12);
        m_cameraController->nextVideoLUT();
    }

case 9:  // Previous LUT
    if (pressed && !curr.activeCameraIsDay) {
        videoLUT = std::max(videoLUT - 1, 0);
        m_cameraController->prevVideoLUT();
    }
```
**Purpose:** Cycle through thermal imaging lookup tables (LUTs).

**Constraints:**
- Only active when thermal camera is selected (`!activeCameraIsDay`)
- Valid range: 0-12 (13 LUT modes)

**Usage:** Enhances contrast and visibility for different target types.

---

### Button 11 & 13: Motion Mode Cycle
```cpp
case 11:  // or 13
    if (pressed) {
        if (!curr.stationEnabled) {
            qWarning() << "Cannot cycle modes, station off";
            return;
        }
        
        // Safety: Cannot cycle during acquisition
        if (curr.currentTrackingPhase == TrackingPhase::Acquisition) {
            qDebug() << "Cannot cycle modes during acquisition";
            return;
        }
        
        // If actively tracking, stop first
        if (curr.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock) {
            qDebug() << "Stopping active track";
            m_stateModel->stopTracking();
            curr = m_stateModel->data();  // Re-fetch state
        }
        
        // Cycle surveillance modes
        switch (curr.motionMode) {
            case MotionMode::Manual:
                m_stateModel->setMotionMode(MotionMode::AutoSectorScan);
                break;
            case MotionMode::AutoSectorScan:
                m_stateModel->setMotionMode(MotionMode::TRPScan);
                break;
            case MotionMode::TRPScan:
                m_stateModel->setMotionMode(MotionMode::RadarSlew);
                break;
            case MotionMode::RadarSlew:
                m_stateModel->setMotionMode(MotionMode::Manual);
                break;
            default:
                m_stateModel->setMotionMode(MotionMode::Manual);
                break;
        }
    }
```

**Purpose:** Cycle through surveillance motion modes in sequence.

**Mode Cycle:**
```
Manual → AutoSectorScan → TRPScan → RadarSlew → Manual (repeat)
```

**Safety Checks:**
1. **Station Enabled:** Cannot cycle if power is off
2. **Not in Acquisition:** Prevents mode change while building tracking gate
3. **Stop Active Track:** If locked on target, automatically stop tracking first

**Rationale:** Prevents operator from accidentally escaping acquisition mode mid-gesture.

---

### Button 14: Up/Next Selector
```cpp
case 14:
    if (pressed) {
        switch (curr.opMode) {
            case OperationalMode::Idle:
                m_stateModel->setUpSw(pressed);
                break;
            case OperationalMode::Tracking:
                m_stateModel->setUpTrack(pressed);
                break;
            case OperationalMode::Surveillance:
                if (curr.motionMode == MotionMode::TRPScan) {
                    m_stateModel->selectNextTRPLocationPage();
                    qDebug() << "Next TRP page:" << m_stateModel->data().activeAutoSectorScanZoneId;
                } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                    m_stateModel->selectNextAutoSectorScanZone();
                    qDebug() << "Next sector zone:" << m_stateModel->data().activeAutoSectorScanZoneId;
                }
                break;
        }
    }
```

**Purpose:** Context-sensitive "up" or "next" command depending on operational mode.

**Behavior:**
- **Idle Mode:** Calls `setUpSw()`
- **Tracking Mode:** Calls `setUpTrack()`
- **Surveillance + TRP Scan:** Select next TRP page
- **Surveillance + Sector Scan:** Select next sector scan zone

---

### Button 16: Down/Previous Selector
```cpp
case 16:
    if (pressed) {
        switch (curr.opMode) {
            case OperationalMode::Idle:
                m_stateModel->setDownSw(pressed);
                break;
            case OperationalMode::Tracking:
                m_stateModel->setDownTrack(pressed);
                break;
            case OperationalMode::Surveillance:
                if (curr.motionMode == MotionMode::TRPScan) {
                    m_stateModel->selectPreviousTRPLocationPage();
                } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                    m_stateModel->selectPreviousAutoSectorScanZone();
                }
                break;
        }
    }
```

**Purpose:** Context-sensitive "down" or "previous" command.

**Analogous to Button 14** but for previous/down direction.

---

## Safety Constraints

### Deadman Switch Required For:
- Button 2 (Lead Angle Compensation toggle)
- Button 4 (Tracking control)

### Station Enabled Required For:
- Button 0 (Engagement)
- Button 5 (Fire weapon)
- Button 11/13 (Mode cycling)
- All mode-dependent controls (14, 16)

### Acquisition Phase Blocks:
- Mode cycling (prevents accidental escape)
- But allows D-pad resize of tracking gate

### Active Lock Blocks:
- Mode cycling requires stopping track first
- D-pad behaves differently (acquisition-specific)

---

## State Machine: Tracking Control

```
                    ┌──────────────┐
                    │      OFF     │
                    └──────┬───────┘
                           │
                    Single Press (Button 4)
                           │
                           ▼
                  ┌─────────────────────┐
                  │   ACQUISITION       │
                  │ (resize gate)       │
                  │ D-Pad: Resize Box   │
                  └─────────┬───────────┘
                            │
                     Single Press
                            │
                            ▼
                  ┌─────────────────────┐
                  │  LOCK_PENDING       │
                  │ (awaiting lock)     │
                  └─────────┬───────────┘
                            │
                    Lock Success
                            │
                            ▼
                  ┌─────────────────────┐
                  │ ACTIVE_LOCK / COAST │
                  │ (actively tracking) │
                  └──────────┬──────────┘
                             │
                      Double Press
                      (within 300ms)
                             │
                             ▼
                  ┌─────────────────────┐
                  │  ABORT → OFF        │
                  └─────────────────────┘
```

---

## Data Flow Example: Zoom Control

```
Operator presses Button 6 (Zoom In)
    ↓
onButtonChanged(6, true)
    ├─ Check: station enabled? ✓
    └─ Call: cameraController->zoomIn()
        ↓
    CameraController starts zoom motor
        ↓
Camera focal length decreases (field of view increases)
        ↓
Operator releases Button 6
    ↓
onButtonChanged(6, false)
    ├─ Call: cameraController->zoomStop()
    └─ Zoom motor stops
        ↓
Camera holds at current zoom level
```

---

## Data Flow Example: Mode Cycling

```
Operator presses Button 11 (Cycle Mode)
    ↓
onButtonChanged(11, true)
    ├─ Check: station enabled? ✓
    ├─ Check: not in acquisition? ✓
    ├─ Check: read current mode from state model
    │          (currently Manual)
    │
    ├─ Active lock? (currently no)
    │
    ├─ Determine next mode: AutoSectorScan
    │
    └─ Call: stateModel->setMotionMode(AutoSectorScan)
        ↓
    SystemStateModel updates state
        ↓
    GimbalController detects mode change
        ↓
    GimbalController creates AutoSectorScanMotionMode
        ├─ Retrieves active scan zone
        ├─ Calls enterMode()
        └─ Gimbal begins scanning
```

---

## Input Validation Patterns

### Pattern 1: Safety Check Then Action
```cpp
if (!m_stateModel->data().stationEnabled) {
    qDebug() << "Cannot fire, station is off";
    return;  // Prevent action
}
// Only reaches here if safe
m_weaponController->startFiring();
```

### Pattern 2: Context-Specific Behavior
```cpp
switch (m_stateModel->data().opMode) {
    case OperationalMode::Idle:
        // Idle behavior
        break;
    case OperationalMode::Tracking:
        // Tracking behavior
        break;
    case OperationalMode::Surveillance:
        // Surveillance behavior
        break;
}
```

### Pattern 3: Double-Click Detection
```cpp
qint64 now = QDateTime::currentMSecsSinceEpoch();
bool isDoubleClick = (now - m_lastTrackButtonPressTime) < DOUBLE_CLICK_INTERVAL_MS;
m_lastTrackButtonPressTime = now;

if (isDoubleClick) {
    // Abort tracking
} else {
    // Start/continue tracking
}
```

---

## Known Issues & Considerations

### Issue 1: Axis Control Currently Disabled
**Current State:** `onAxisChanged()` is mostly commented out.

**Impact:** Gimbal manual control via joystick analog sticks not active.

**Fix:**
```cpp
void JoystickController::onAxisChanged(int axis, float value) {
    if (!m_gimbalController) return;
    
    if (axis == 0) {
        // X-axis → Azimuth
        double velocityAz = value * 10.0f;
        m_gimbalController->setManualVelocity(velocityAz, 0.0);
    } else if (axis == 1) {
        // Y-axis → Elevation
        double velocityEl = -value * 10.0f;
        m_gimbalController->setManualVelocity(0.0, velocityEl);
    }
}
```

---

### Issue 2: Tracking Acquisition Gate Resize Not Implemented
**Current State:** `onHatChanged()` calls `m_stateModel->adjustAcquisitionBoxSize()` but this method may not exist.

**Impact:** D-pad doesn't actually resize acquisition gate during tracking.

**Fix Needed:** Implement `SystemStateModel::adjustAcquisitionBoxSize(float dW, float dH)`.

---

### Issue 3: Hat Control Fallback Logic Removed
**Current State:** Fallback gimbal control via D-pad is commented out.

**Impact:** D-pad only works in acquisition phase; no fallback for other modes.

**Decision:** Re-enable if gimbal analog stick isn't being used, or leave disabled if stick is primary.

---

## Testing Checklist

- [ ] Button press/release events detected correctly
- [ ] Deadman switch blocks tracking control
- [ ] Deadman switch blocks LAC toggle
- [ ] Double-click abort within 300ms works
- [ ] Single press advances tracking phase
- [ ] Mode cycling works in all operational modes
- [ ] Mode cycling blocks in acquisition phase
- [ ] Mode cycling stops active track first
- [ ] Zoom In/Out buttons work
- [ ] Zoom buttons auto-stop on release
- [ ] Thermal LUT cycling works (thermal camera only)
- [ ] LUT clamped to 0-12 range
- [ ] D-pad resizes acquisition gate (if implemented)
- [ ] Fire weapon button works
- [ ] Station disabled blocks weapon fire
- [ ] LAC toggle updates fire control solution
- [ ] Button 14/16 context-sensitive behavior works
- [ ] No rapid toggle issues (debounce if needed)

---

## Integration with System

### Input Path
```
Physical Joystick → JoystickDevice (SDL)
    ↓
JoystickDataModel (coalesces events)
    ↓
JoystickController (enforces rules)
    ↓
System Controllers (gimbal, camera, weapon)
    ↓
Hardware Output
```

### State Dependencies
```cpp
// JoystickController reads:
m_stateModel->data().stationEnabled
m_stateModel->data().deadManSwitchActive
m_stateModel->data().currentTrackingPhase
m_stateModel->data().motionMode
m_stateModel->data().opMode
m_stateModel->data().activeCameraIsDay

// JoystickController writes:
m_stateModel->setMotionMode()
m_stateModel->setDeadManSwitch()
m_stateModel->startTrackingAcquisition()
m_stateModel->requestTrackerLockOn()
m_stateModel->stopTracking()
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified disabled axis control and missing acquisition resize method |

---

## Related Classes

- **JoystickDataModel:** Raw joystick input events
- **SystemStateModel:** Operational state
- **GimbalController:** Gimbal motion control
- **CameraController:** Camera and imaging control
- **WeaponController:** Weapon firing and ballistics

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Production - Input handler for operator control
# CameraController Class Documentation

## Overview

`CameraController` manages dual-camera (day/night) operation, video streaming processors, thermal imaging features, and real-time tracking coordination. It abstracts camera hardware complexity behind a unified interface and synchronizes camera state with the central system state model.

**Location:** `cameracontroller.h/cpp`

**Responsibility:** Camera hardware abstraction, camera switching, thermal control, tracking enable/disable, zoom/focus management

**Design Pattern:** Facade/Adapter pattern - unified interface for heterogeneous camera hardware

---

## Dependencies

- **Hardware Interfaces:**
  - `DayCameraControlDevice` - Day camera control (zoom, focus)
  - `NightCameraControlDevice` - Night camera control (thermal, digital zoom, LUT)
  - `LensDevice` - Lens/zoom actuator
  - `CameraVideoStreamDevice` (×2) - Video processors with tracking capability
- **State Management:** `SystemStateModel` - camera state and tracking flags
- **Qt Core:** QObject, QMutex, QMetaObject (thread-safe signal invocation)

---

## Architecture

```
Operator Commands
    ↓
JoystickController
    ├─ Camera Switch
    ├─ Zoom In/Out
    ├─ LUT Cycle
    └─ Focus Control
         ↓
    CameraController (coordination layer)
    ├─ State Synchronization
    ├─ Active Camera Selection
    ├─ Tracking Coordination
    └─ Command Routing
         ↓
    ┌────────────────┬──────────────────┬──────────────┐
    ↓                ↓                  ↓              ↓
Day Camera      Night Camera      Video Processors  Lens Device
Control         Control           (Day & Night)
    ├─ Zoom         ├─ Digital Zoom    ├─ Tracking    └─ Focus
    ├─ Focus        ├─ LUT             ├─ Detection
    └─ Auto-Focus   ├─ FFC             └─ Output
                    └─ Thermal
```

---

## Member Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `m_dayControl` | `DayCameraControlDevice*` | Day camera hardware interface |
| `m_dayProcessor` | `CameraVideoStreamDevice*` | Day camera video processor with tracking |
| `m_nightControl` | `NightCameraControlDevice*` | Night camera hardware interface |
| `m_nightProcessor` | `CameraVideoStreamDevice*` | Night camera video processor with tracking |
| `m_lensDevice` | `LensDevice*` | Lens/zoom actuator |
| `m_stateModel` | `SystemStateModel*` | Central state repository |
| `m_isDayCameraActive` | `bool` | Cached flag: which camera is active |
| `m_lutIndex` | `int` | Current thermal LUT index (0-12) |
| `m_cachedState` | `SystemStateData` | Previous state snapshot for change detection |
| `m_mutex` | `QMutex` | Thread synchronization for state access |
| `statusMessage` | `QString` | Last status message (avoid redundant updates) |

---

## Public Methods

### Constructor
```cpp
CameraController(DayCameraControlDevice* dayControl,
                 CameraVideoStreamDevice* dayProcessor,
                 NightCameraControlDevice* nightControl,
                 CameraVideoStreamDevice* nightProcessor,
                 LensDevice* lensDevice,
                 SystemStateModel* stateModel,
                 QObject* parent = nullptr)
```

**Purpose:** Initialize camera controller with all hardware interfaces.

**Parameters:**
- `dayControl`: Day camera control device
- `dayProcessor`: Day camera video processor
- `nightControl`: Night camera control device
- `nightProcessor`: Night camera video processor
- `lensDevice`: Lens/zoom actuator
- `stateModel`: Central system state
- `parent`: Qt parent object

**Actions:**
1. Stores all hardware device pointers
2. Initializes default state (day camera active, LUT=0)
3. Connects to `SystemStateModel::dataChanged` signal (QueuedConnection for thread safety)
4. Caches initial camera selection from state model

**Important:** Uses `Qt::QueuedConnection` to ensure slots execute on correct thread.

---

### initialize()
```cpp
bool initialize()
```

**Purpose:** Perform initialization after all components are ready.

**Returns:** `true` if initialization succeeded, `false` otherwise

**Actions:**
1. Validates all required components are present
2. Returns error if any device pointers are null
3. Performs any initial configuration needed
4. Updates status message

**Typical Usage:**
```cpp
CameraController* camCtrl = new CameraController(...);
if (!camCtrl->initialize()) {
    qWarning() << "Camera initialization failed";
    return false;
}
```

---

### getActiveCameraProcessor()
```cpp
CameraVideoStreamDevice* getActiveCameraProcessor() const
```

**Returns:** Pointer to currently active camera's video processor (day or night)

**Purpose:** Get active processor for tracking/detection commands.

**Uses:** Internal `m_isDayCameraActive` flag for immediate (non-blocking) response.

---

### getDayCameraProcessor() / getNightCameraProcessor()
```cpp
CameraVideoStreamDevice* getDayCameraProcessor() const
CameraVideoStreamDevice* getNightCameraProcessor() const
```

**Returns:** Day or night processor, regardless of which is active.

**Purpose:** Direct access to specific processors (e.g., for testing or multi-camera tracking).

---

### isDayCameraActive()
```cpp
bool isDayCameraActive() const
```

**Returns:** `true` if day camera is active, `false` if night camera is active.

---

## Tracking Control

### startTracking()
```cpp
bool startTracking()
```

**Purpose:** Enable tracking on the active camera.

**Control Flow:**

```cpp
1. Lock mutex (thread-safe access to state model)

2. Check preconditions:
   - State model exists? ✓
   - Active processor exists? ✓
   - Tracking not already active? ✓

3. Unlock mutex (before long operation)

4. Invoke setTrackingEnabled(true) on active processor
   Via: QMetaObject::invokeMethod(..., Qt::QueuedConnection)
   
5. Update state model:
   m_stateModel->setTrackingStarted(true)
   
6. Wait for state model signal back via onSystemStateChanged()
   (ensures consistency across system)

7. Return success status
```

**Returns:** `true` if command sent successfully, `false` otherwise

**Important:** This is an **asynchronous request**, not synchronous action:
- Command is queued to video processor thread
- Actual tracking starts on processor thread
- State model signal confirms completion

**Thread Safety:** Uses mutex to prevent race conditions between UI thread and state model updates.

---

### stopTracking()
```cpp
void stopTracking()
```

**Purpose:** Disable tracking on the active camera.

**Control Flow:** Similar to `startTracking()` but invokes `setTrackingEnabled(false)`.

**Checks:**
- State model exists
- Active processor exists
- Tracking is currently active

---

## Camera Switching

### setActiveCamera(bool isDay)
```cpp
void setActiveCamera(bool isDay)
```

**Purpose:** Internal helper to update active camera flag.

**Parameters:**
- `isDay`: `true` to select day camera, `false` for night camera

**Called From:** `onSystemStateChanged()` when camera preference changes

**Actions:**
- Updates `m_isDayCameraActive` flag
- Logs camera selection
- Does NOT emit signals (caller handles that)

---

## Zoom & Focus Control

### zoomIn() / zoomOut() / zoomStop()
```cpp
void zoomIn()
void zoomOut()
void zoomStop()
```

**Purpose:** Control camera zoom (optical for day, digital for night).

**Behavior:**

**Day Camera:**
- Delegates to `m_dayControl->zoomIn()` / `zoomOut()`
- Calls `zoomStop()` on button release

**Night Camera:**
- Calls `m_nightControl->setDigitalZoom()` with increment/decrement values
- `zoomStop()` no-op (digital zoom doesn't require stopping)

**Typical Flow:**
```
Operator presses Zoom In button
    ↓
JoystickController::onButtonChanged(6, true)
    ↓
CameraController::zoomIn()
    ├─ If day camera: m_dayControl->zoomIn()
    └─ If night camera: m_nightControl->setDigitalZoom(4)
    
Operator releases button
    ↓
CameraController::zoomStop()
    ├─ If day camera: m_dayControl->zoomStop()
    └─ If night camera: (no-op)
```

---

### focusNear() / focusFar() / focusStop()
```cpp
void focusNear()
void focusFar()
void focusStop()
```

**Purpose:** Manual focus control (day camera only).

**Availability:** Only on day camera (`m_dayControl`)

**Night camera:** No manual focus (thermal imaging typically has fixed focus)

---

### setFocusAuto(bool enabled)
```cpp
void setFocusAuto(bool enabled)
```

**Purpose:** Enable/disable autofocus on day camera.

**Behavior:** Only works if day camera is active.

---

## Thermal Imaging Control

### nextVideoLUT() / prevVideoLUT()
```cpp
void nextVideoLUT()
void prevVideoLUT()
```

**Purpose:** Cycle through thermal image lookup tables (LUTs).

**Availability:** Night (thermal) camera only

**Implementation:**
```cpp
void nextVideoLUT() {
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex++;
        if (m_lutIndex > 12) m_lutIndex = 12;  // Clamp to 12
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}
```

**Valid Range:** 0-12 (13 different thermal color palettes)

**State:** Index stored in `m_lutIndex` and persists across calls

**LUT Examples:**
- 0: Grayscale (default)
- 4: Hot/Cold (red/blue)
- 8: Rainbow
- 12: Arctic (cool colors)

(Exact mapping depends on thermal camera model)

---

### performFFC()
```cpp
void performFFC()
```

**Purpose:** Trigger Flat Field Correction on thermal camera.

**What is FFC?** Thermal imaging correction that accounts for camera temperature drift and non-uniformity.

**Behavior:**
- Typically takes 2-5 seconds
- Momentarily blanks thermal image during correction
- Should be performed every 5-10 minutes during operation

**Availability:** Night (thermal) camera only

---

## State Synchronization

### onSystemStateChanged(const SystemStateData& newData)
```cpp
void onSystemStateChanged(const SystemStateData& newData)
```

**Purpose:** React to state model changes (slot function).

**Called:** When `SystemStateModel::dataChanged` signal emitted

**Thread Safety:** Uses `QMutexLocker` for safe state access

**Reaction Logic:**

#### 1. Detect Camera Switch
```cpp
bool cameraChanged = (m_cachedState.activeCameraIsDay != newData.activeCameraIsDay);
```

If camera changed:
- Update internal flag: `setActiveCamera(newData.activeCameraIsDay)`
- Stop tracking on inactive camera (if it was active):
  ```cpp
  CameraVideoStreamDevice* oldProcessor = 
      oldStateBeforeUpdate.activeCameraIsDay ? m_dayProcessor : m_nightProcessor;
  
  if (oldProcessor && oldStateBeforeUpdate.trackingActive) {
      QMetaObject::invokeMethod(oldProcessor, "setTrackingEnabled", 
                                Qt::QueuedConnection,
                                Q_ARG(bool, false));
  }
  ```
- Emit `stateChanged()` signal to notify UI

#### 2. Cache State for Next Cycle
```cpp
m_cachedState = newData;
```

---

## Status Updates

### updateStatus(const QString& message)
```cpp
void updateStatus(const QString& message)
```

**Purpose:** Log status messages and notify UI.

**Avoids Redundancy:** Only updates/emits if message is different from last:

```cpp
if (statusMessage != message) {
    statusMessage = message;
    qDebug() << "CameraController Status:" << message;
    emit statusUpdated(message);  // Signal for status bar/UI
}
```

**Emitted Signals:**
- `statusUpdated(QString)` - for UI status bar display

---

## Signals Emitted

```cpp
signal stateChanged()
// Emitted when active camera changes or tracking state changes

signal statusUpdated(const QString& message)
// Emitted when status message updates (for UI display)
```

---

## Thread Model

### Thread Safety Design

```
Main/UI Thread          Video Processor Thread
─────────────────────────────────────────────────
CameraController        CameraVideoStreamDevice
(Qt slots)              (Processing loop)
    ├─ Has mutex             ├─ Receives Qt signals
    ├─ Protects m_cached*    └─ Invoked via QueuedConnection
    └─ Uses QMetaObject
       invokeMethod()
         ↓ (QueuedConnection)
       [Thread-safe queue]
         ↓
       Video processor slot
       executes on its thread
```

**Why QueuedConnection?**
- `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` posts slot calls to receiver's thread
- Ensures video processor slot runs on processor's thread (where it expects to run)
- Prevents race conditions and thread-related crashes

**Mutex Usage:**
- Protects `m_cachedState` and `m_isDayCameraActive` when accessed from state model signals
- Prevents main thread and state model thread from corrupting shared data

---

## Known Issues & Fixes

### Issue 1: String Concatenation Compilation Error
**Current Code (Line 198 - BROKEN):**
```cpp
updateStatus("Tracking start requested on " + (m_isDayCameraActive ? "Day" : "Night") + " camera.");
```

**Error:** Cannot concatenate `const char*` with `const char` using `+` operator.

**Fix:**
```cpp
updateStatus(QString("Tracking start requested on %1 camera.")
    .arg(m_isDayCameraActive ? "Day" : "Night"));
```

Or:
```cpp
updateStatus(QString("Tracking start requested on ") + 
    (m_isDayCameraActive ? "Day" : "Night") + " camera.");
```

**Why:** `QString` supports `+` operator, but C-string literals don't.

---

### Issue 2: Acquisition Box Resize Not Implemented
**Current State:** `JoystickController::onHatChanged()` calls `m_stateModel->adjustAcquisitionBoxSize()`, but this method may not exist in `SystemStateModel`.

**Impact:** D-pad doesn't resize tracking acquisition gate during tracking acquisition phase.

**Fix Needed:** Implement in `SystemStateModel`:
```cpp
void SystemStateModel::adjustAcquisitionBoxSize(float dW, float dH) {
    // Modify acquisition box dimensions
    // Constrain to image bounds
    // Emit signal for UI update
}
```

---

### Issue 3: Night Camera Zoom Behavior Unclear
**Current Code:**
```cpp
void CameraController::zoomIn() {
    if (!m_isDayCameraActive) {
        if (m_nightControl) m_nightControl->setDigitalZoom(4);
    }
}
```

**Issues:**
- No zoom state tracking for night camera
- `setDigitalZoom(4)` sets absolute level, not relative increment
- Previous zoom level is lost

**Improved Implementation:**
```cpp
void CameraController::zoomIn() {
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomIn();
    } else {
        // Night camera digital zoom
        m_nightZoomLevel = std::min(m_nightZoomLevel + 1, MAX_DIGITAL_ZOOM);
        if (m_nightControl) m_nightControl->setDigitalZoom(m_nightZoomLevel);
    }
}
```

Add member variable:
```cpp
int m_nightZoomLevel = 1;  // Track current digital zoom level
```

---

### Issue 4: Tracking State Inconsistency
**Scenario:** If video processor crashes while tracking is supposed to be active:
- CameraController thinks tracking is active (state model says so)
- But video processor stopped processing

**Impact:** Tracking won't restart (controller thinks it's already running)

**Fix:** Implement heartbeat/watchdog:
```cpp
connect(dayProcessor, &CameraVideoStreamDevice::crashed,
        this, [this]() {
    m_stateModel->setTrackingStarted(false);
    updateStatus("Day camera processor crashed, tracking stopped.");
});
```

---

## Data Flow Example: Zoom In

```
Operator presses Zoom In (Button 6)
    ↓
JoystickController::onButtonChanged(6, true)
    ├─ Check: station enabled? ✓
    ├─ Call: cameraController->zoomIn()
    │   ↓
    │   if (isDayCameraActive)
    │       ├─ dayControl->zoomIn()
    │       │   ├─ Increase zoom step
    │       │   ├─ Send motor command
    │       │   └─ Zoom increases in real-time
    │       │
    │       └─ Processor sees wider FOV
    │           └─ Tracker recalibrates for new zoom
    │
    └─ Return (command sent)

Operator releases Button 6
    ↓
JoystickController::onButtonChanged(6, false)
    ├─ Call: cameraController->zoomStop()
    │   ├─ dayControl->zoomStop()
    │   └─ Motor halts at current position
    │
    └─ Zoom locked at final level
```

---

## Data Flow Example: Camera Switch

```
Operator presses Camera Toggle (Button 2 - LAC button)
    ↓
SystemStateModel detects camera switch request
    └─ Sets activeCameraIsDay = !activeCameraIsDay
    └─ Emits dataChanged signal
    
    ↓
CameraController::onSystemStateChanged()
    ├─ Lock mutex
    ├─ Detect: cameraChanged = true
    │
    ├─ Unlock mutex
    │
    ├─ Call: setActiveCamera(newIsDay)
    │   └─ Update m_isDayCameraActive flag
    │
    ├─ If tracking was active on old camera:
    │   └─ Invoke setTrackingEnabled(false) on old processor
    │       (Via QueuedConnection to old processor's thread)
    │
    ├─ Emit: stateChanged()
    │   └─ UI updates to show new camera
    │
    └─ Done
    
New camera is now active for subsequent commands:
    ├─ Zoom commands go to new camera
    ├─ Tracking starts on new camera if requested
    └─ LUT commands go to new camera
```

---

## Integration Points

### Input From JoystickController
```cpp
// Zoom/Focus
cameraController->zoomIn()
cameraController->zoomOut()
cameraController->zoomStop()
cameraController->focusNear()
cameraController->focusFar()
cameraController->focusStop()
cameraController->setFocusAuto(enabled)

// Thermal
cameraController->nextVideoLUT()
cameraController->prevVideoLUT()
cameraController->performFFC()

// Tracking
cameraController->startTracking()
cameraController->stopTracking()
```

### State Model Input
```cpp
systemStateModel->data().activeCameraIsDay
systemStateModel->data().trackingActive
systemStateModel->data().opMode
```

### Video Processors (Output)
```cpp
dayProcessor->setTrackingEnabled(bool)
nightProcessor->setTrackingEnabled(bool)
dayProcessor->setTrackingEnabled(bool)  // from camera switch logic
```

---

## Testing Checklist

- [ ] Constructor initializes all members correctly
- [ ] `initialize()` validates all required components
- [ ] Day/night camera active state tracked correctly
- [ ] Camera switch stops tracking on inactive camera
- [ ] Zoom In/Out works for day camera (optical)
- [ ] Zoom In/Out works for night camera (digital)
- [ ] Zoom level persists across zoom stop
- [ ] Focus commands only work on day camera
- [ ] Auto-focus toggle works on day camera
- [ ] Thermal LUT cycles 0-12 without underflow/overflow
- [ ] LUT index persists across calls
- [ ] FFC only runs on night camera
- [ ] `startTracking()` invokes processor correctly
- [ ] `stopTracking()` invokes processor correctly
- [ ] Tracking commands only affect active camera
- [ ] State model changes trigger synchronization
- [ ] Status messages don't spam on no-change
- [ ] Thread-safe mutex protects cached state
- [ ] QueuedConnection ensures processor thread safety
- [ ] No crashes on null device pointers
- [ ] Processor crash handled gracefully

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-13 | 1.0 | Initial documentation; identified string concatenation bug, night zoom state tracking issue, and missing acquisition resize method |

---

## Related Classes

- **DayCameraControlDevice:** Hardware interface for day camera
- **NightCameraControlDevice:** Hardware interface for thermal camera
- **CameraVideoStreamDevice:** Video processor with tracking
- **LensDevice:** Zoom/focus actuator interface
- **SystemStateModel:** Central state repository
- **JoystickController:** Input handler (primary caller)

---

## Contact & Maintenance

**Maintainer:** [Your Name/Team]

**Last Updated:** October 13, 2025

**Status:** Production - Requires string concatenation fix before compile
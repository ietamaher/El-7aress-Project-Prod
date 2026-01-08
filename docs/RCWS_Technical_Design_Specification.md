# RCWS Qt6/QML Application - Technical Design Specification (TDS)

**Document Version:** 1.0
**Date:** January 2026
**Classification:** UNCLASSIFIED // FOR OFFICIAL USE ONLY
**Prepared for:** Engineering Team - El Harress RCWS Project

---

## Document Purpose

This Technical Design Specification (TDS) provides engineer-facing technical details for the RCWS Qt6/QML application. It complements the RCWS_Qt6_Design_Document.md by describing implementation-level mechanisms including state machines, timing parameters, threading architecture, safety enforcement flows, error handling, and data flow patterns.

**Intended Audience:** Software engineers, system integrators, and verification/validation teams requiring detailed understanding of internal mechanisms for implementation, debugging, and certification activities.

---

## Table of Contents

1. [State Machines](#1-state-machines)
2. [Control Loop Timing](#2-control-loop-timing)
3. [Threading Model](#3-threading-model)
4. [Safety Enforcement Flow](#4-safety-enforcement-flow)
5. [Error Handling and Recovery](#5-error-handling-and-recovery)
6. [Data Flow](#6-data-flow)

---

## 1. State Machines

### 1.1 Homing State Machine (HomingController)

The homing state machine manages the gimbal zeroing sequence, establishing a known reference position for accurate aiming.

#### States

| State | Description |
|-------|-------------|
| `Idle` | No homing activity; system ready for homing request |
| `Requested` | Homing request received; preparing to send HOME command |
| `InProgress` | HOME command sent; awaiting HOME-END signals from both axes |
| `Completed` | Both azimuth and elevation HOME-END received; success |
| `Failed` | Timeout or error occurred during homing |
| `Aborted` | Operator or system abort (e.g., emergency stop) |

#### State Transition Diagram

```
                  ┌─────────────────────────────────────┐
                  │                                     │
                  v                                     │
    ┌──────┐   button   ┌───────────┐   PLC42     ┌────────────┐
    │ Idle │ ─────────> │ Requested │ ─────────> │ InProgress │
    └──────┘   pressed  └───────────┘   HOME      └────────────┘
       ^                                               │
       │                                    ┌──────────┼──────────┐
       │                                    │          │          │
       │                              HOME-END   Timeout     E-Stop
       │                             (both axes)    │          │
       │                                    v          v          v
       │                              ┌───────────┐ ┌────────┐ ┌─────────┐
       └──────────────────────────────│ Completed │ │ Failed │ │ Aborted │
            (auto-clear after 1 cycle)└───────────┘ └────────┘ └─────────┘
```

#### Timing Parameters

| Parameter | Value | Location |
|-----------|-------|----------|
| `m_homingTimeoutMs` | 30000 ms | `HomingController` configurable |
| Auto-clear delay | 1 control cycle (~50ms) | State clears on next `process()` |

#### Signal Dependencies

- **Input:** `gotoHomePosition` (button rising edge), `azimuthHomeComplete`, `elevationHomeComplete`
- **Output:** `homingStarted`, `homingCompleted`, `homingFailed`, `homingAborted`, `homingStateChanged`

#### Implementation Notes

- Stores `m_modeBeforeHoming` to restore motion mode after completion
- Rising edge detection on button prevents repeated triggering
- Synchronizes with SystemStateModel's own homing state to handle race conditions

---

### 1.2 Charging State Machine (ChargingStateMachine)

The charging (cocking) state machine manages the weapon charging cycle, including multi-cycle support for closed-bolt weapons (M2HB) and jam detection/recovery.

#### States

| State | Description |
|-------|-------------|
| `Idle` | Actuator retracted; ready for charge request |
| `Extending` | Actuator moving toward full extension |
| `Extended` | Actuator at full extension; holding position (continuous hold mode) |
| `Retracting` | Actuator returning to home position |
| `Lockout` | CROWS M153 4-second post-charge lockout period |
| `Fault` | Error state requiring operator acknowledgment |
| `JamDetected` | Mechanical obstruction detected; executing backoff |
| `SafeRetract` | Recovery retraction; transitions to Idle (no auto-cycle) |

#### State Transition Diagram

```
                                    Short Press Mode
                                   ┌──────────────────┐
                                   │                  │
    ┌──────┐  charge  ┌───────────┐│ position ┌────────────┐ position ┌───────────┐
    │ Idle │ ───────> │ Extending │┘ reached  │ Retracting │ <──────  │ Extended  │
    └──────┘  button  └───────────┘ ───────>  └────────────┘ button   └───────────┘
       ^                   │                        │       released      ^
       │                   │                        │                     │
       │                   │ jam                    │ all cycles         position
       │                   │ detected               │ complete           reached
       │                   v                        v                     │
       │              ┌─────────────┐         ┌─────────┐                │
       │              │ JamDetected │         │ Lockout │                │
       │              └─────────────┘         └─────────┘                │
       │                   │                        │                    │
       │                   │ backoff                │ 4s expires         │
       │                   │ complete               │                    │
       │                   v                        │                    │
       │              ┌─────────┐                  │                    │
       │              │  Fault  │                  │                    │
       │              └─────────┘                  │                    │
       │                   │                        │                    │
       │                   │ operator reset         │                    │
       │                   v                        │                    │
       │              ┌────────────┐               │                    │
       └──────────────│ SafeRetract │ <────────────┘                    │
         (position    └────────────┘                                    │
          reached)          ^                                           │
                            │                                           │
                            └───────────────────────────────────────────┘
                                      Continuous Hold Mode
```

#### Timing Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `COCKING_TIMEOUT_MS` | 5000 ms | Watchdog for extend/retract operations |
| `CHARGE_LOCKOUT_MS` | 4000 ms | CROWS M153 mandatory post-charge lockout |
| `BACKOFF_STABILIZE_MS` | 200 ms | Delay before reverse direction after jam |
| Startup retraction timeout | `COCKING_TIMEOUT_MS / 2` | Faster timeout for startup recovery |

#### Position Thresholds

| Parameter | Value | Description |
|-----------|-------|-------------|
| `COCKING_EXTEND_POS` | Configurable (mm) | Full extension position |
| `COCKING_RETRACT_POS` | Configurable (mm) | Home/retracted position |
| `COCKING_POSITION_TOLERANCE` | ±2 mm | Acceptable arrival threshold |
| `ACTUATOR_RETRACTED_THRESHOLD` | 5 mm | Startup retraction trigger |

#### Jam Detection Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `JAM_TORQUE_THRESHOLD_PERCENT` | 80% | High torque detection threshold |
| `POSITION_STALL_TOLERANCE_MM` | 0.5 mm | Stall detection threshold |
| `JAM_CONFIRM_SAMPLES` | 3 | Consecutive samples to confirm jam |

#### Multi-Cycle Support

| Weapon Type | Required Cycles | Description |
|-------------|-----------------|-------------|
| M2HB (.50 cal) | 2 | Closed bolt; cycle 1 picks round, cycle 2 chambers |
| M240B, M249, MK19 | 1 | Open bolt weapons |

#### Operating Modes

1. **Short Press Mode:** Button released before extension complete → auto-cycles through all required extend/retract cycles
2. **Continuous Hold Mode:** Button held through extension → holds at Extended until release, then retracts (counts as one cycle)

---

### 1.3 LAC Tracking State Machine (TrackingMotionMode)

The Lead Angle Compensation (LAC) state machine manages smooth transitions between precision tracking and lead-injected firing modes.

#### States

| State | Description |
|-------|-------------|
| `TRACK` | Normal tracking; target centered; PID control active |
| `FIRE_LEAD` | Lead injection active; target intentionally drifts off-center |
| `RECENTER` | Transitioning from lead back to centered tracking |

#### State Transition Logic

```cpp
if (lacArmed && deadReckoningActive) {
    state = FIRE_LEAD;
} else {
    if (previousState == FIRE_LEAD) {
        state = RECENTER;
    } else if (blendFactor <= 0.001) {
        state = TRACK;
    }
    // else: stay in RECENTER while ramping down
}
```

#### Blend Factor Timing

| Parameter | Value | Resulting Duration |
|-----------|-------|-------------------|
| `LAC_BLEND_RAMP_IN` | 5.0 (1/s) | ~200 ms to reach 1.0 |
| `LAC_BLEND_RAMP_OUT` | 3.0 (1/s) | ~333 ms to reach 0.0 |

#### Rigid Cradle Constraint

**Critical:** Camera and gun are mechanically locked. During lead injection (`FIRE_LEAD`), the target **intentionally drifts off-center** because the gun must aim ahead of the target. This is correct behavior for ballistic lead.

---

### 1.4 Emergency Stop Monitor (EmergencyStopMonitor)

The emergency stop monitor provides debounced state tracking and recovery management for the physical E-Stop button.

#### Debounce State Machine

```
                    state change
    ┌─────────────┐  detected   ┌────────────┐   DEBOUNCE_MS   ┌───────────────┐
    │ Not         │ ──────────> │ Debouncing │ ──────────────> │ State Changed │
    │ Debouncing  │             └────────────┘    elapsed      └───────────────┘
    └─────────────┘                   │
          ^                           │ state reverted
          │                           │ during debounce
          └───────────────────────────┘
```

#### Timing Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `DEBOUNCE_MS` | 50 ms | Contact bounce filtering |
| `RECOVERY_DELAY_MS` | 1000 ms | Safe recovery period after deactivation |
| `MAX_EVENT_HISTORY` | 100 | Audit trail buffer size |

#### Signals

| Signal | Description |
|--------|-------------|
| `activated(event)` | E-Stop activated (includes source, timestamp) |
| `deactivated(event)` | E-Stop deactivated (includes duration) |
| `recoveryStarted()` | Recovery period begun |
| `recoveryComplete()` | Safe to resume normal operation |
| `stateChanged(bool)` | State change notification |

#### Force Activation

The `forceActivate(reason)` method bypasses debounce for software-triggered emergency stops.

---

## 2. Control Loop Timing

### 2.1 Primary Control Loop (GimbalController)

The gimbal controller runs a 20 Hz periodic update loop for motion control.

#### Timer Configuration

```cpp
m_updateTimer = new QTimer(this);
m_updateTimer->start(50);  // 50 ms = 20 Hz
```

#### Centralized dt Computation

```cpp
double dt = m_velocityTimer.restart() / 1000.0;  // Convert ms to seconds
dt = std::clamp(dt, 0.001, 0.050);               // Clamp between 1-50 ms
```

**Rationale:** Clamping prevents numerical instability from extreme dt values caused by system delays or startup conditions.

#### Update Loop Structure

1. **Shutdown safety check** - Return if SystemStateModel destroyed
2. **Startup sanity checks** - Verify IMU units, stabilizer limits (once)
3. **Centralized dt computation** - Measure actual loop interval
4. **LAC velocity calculation** - Gimbal velocity for manual mode LAC
5. **Update loop diagnostics** - Jitter logging every 50 cycles
6. **Motion mode update** - Safety check, then delegate to mode implementation

#### Priority Processing (onSystemStateChanged)

| Priority | Condition | Action |
|----------|-----------|--------|
| 1 | Emergency Stop changed | Process immediately; block all else if active |
| 2 | Homing state changed | Delegate to HomingController |
| 3 | Free mode changed | Log state change |
| 4 | Motion mode changed | Reconfigure motion mode |
| 5 | Tracking data changed | Update target position |

### 2.2 Motion Mode Update Timing

Motion modes receive the centralized `dt` parameter from GimbalController:

```cpp
m_currentMode->update(this, dt);
```

#### Motion Mode Filter Time Constants

| Filter | Tau (τ) | Purpose |
|--------|---------|---------|
| `manualJoystickTau` | 0.05 s | Joystick input smoothing (configurable) |
| `DERR_FILTER_TAU` | 0.1 s | Derivative-on-error noise rejection |
| `FF_FILTER_TAU` | 0.15 s | Feedforward velocity smoothing |
| Gimbal velocity filter | 0.1 s | LAC velocity measurement |

#### Rate Limiting (ManualMotionMode)

```cpp
double maxChangeHz = cfg.manualLimits.maxAccelHzPerSec * dt;  // Hz/s * s = Hz
```

### 2.3 Hardware Polling Intervals

| Device | Polling Rate | Mechanism |
|--------|--------------|-----------|
| Servo Drivers | 110 Hz | Modbus read (implicit via write cycle) |
| PLC21 (Panel) | 50 Hz | Modbus polling timer |
| PLC42 (I/O) | 50 Hz | Modbus polling timer |
| IMU | 100 Hz | Serial streaming (3DM-GX3 internal rate) |
| Joystick | Event-driven | SDL2 event polling |
| LRF | On-demand | Serial request/response |

### 2.4 Video Processing Timing

| Pipeline Stage | Rate | Description |
|----------------|------|-------------|
| Frame capture | 60 fps | GStreamer V4L2 source |
| Frame decode | 60 fps | GStreamer MJPEG decoder |
| OSD overlay | Per-frame | QML Image refresh triggered by signal |
| QML update | Signal-driven | `frameUpdated()` signal → immediate refresh |

**Latency Optimization:** Timer-based polling (33ms jitter) replaced with signal-driven refresh for 0-33ms latency reduction.

---

## 3. Threading Model

### 3.1 Thread Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              Main Thread (Qt Event Loop)                      │
│                                                                              │
│  ┌────────────────┐  ┌──────────────────┐  ┌──────────────────────────────┐ │
│  │ QGuiApplication│  │ QQmlEngine       │  │ SystemStateModel             │ │
│  │ Event Loop     │  │ + QML UI         │  │ + All Controllers            │ │
│  └────────────────┘  └──────────────────┘  │ + Safety Interlock           │ │
│                                            │ + Hardware Devices (Modbus)  │ │
│                                            └──────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
        │                    │                           │
        │                    │                           │
        v                    v                           v
┌───────────────┐    ┌───────────────┐           ┌───────────────┐
│ Day Video     │    │ Night Video   │           │ Servo Threads │
│ Thread        │    │ Thread        │           │ (Az/El)       │
│               │    │               │           │               │
│ ┌───────────┐ │    │ ┌───────────┐ │           │ ┌───────────┐ │
│ │GStreamer  │ │    │ │GStreamer  │ │           │ │Modbus I/O │ │
│ │Pipeline   │ │    │ │Pipeline   │ │           │ │Async      │ │
│ └───────────┘ │    │ └───────────┘ │           │ └───────────┘ │
└───────────────┘    └───────────────┘           └───────────────┘
```

### 3.2 Main Thread Responsibilities

**Why Single-Threaded for Modbus:**
Qt's QModbus requires all operations on the same QModbusClient to occur in the creating thread. Threading violations cause crashes or undefined behavior.

**Main Thread Components:**
- `QGuiApplication` event loop (`app.exec()`)
- `SystemStateModel` (central data hub)
- All controllers (GimbalController, WeaponController, etc.)
- `SafetyInterlock` (safety authority)
- Hardware devices (Modbus-based)
- QML engine and UI

### 3.3 Worker Threads

#### Video Processing Threads

```cpp
// HardwareManager.cpp
m_dayVideoProcessor = new CameraVideoStreamDevice(...);
m_dayVideoProcessor->start();  // Starts internal GStreamer thread
```

**Thread-Safe Communication:**
```cpp
connect(m_hardwareManager->dayVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
        this, [this](const FrameData& data) {
            // Lambda runs in main thread (QueuedConnection implicit)
            m_videoProvider->updateImage(data.baseImage);
        }, Qt::QueuedConnection);  // Explicit for clarity
```

#### Servo Driver Threads

```cpp
m_servoAzThread = new QThread(this);
m_servoAzDevice = new ServoDriverDevice(servoAzConf.name, nullptr);
// Note: Device created on main thread, moved later if needed
```

### 3.4 Signal Connection Types

| Scenario | Connection Type | Rationale |
|----------|-----------------|-----------|
| Same thread, timing-critical | `Qt::DirectConnection` | No event queue delay |
| Cross-thread | `Qt::QueuedConnection` | Thread-safe, copied arguments |
| SystemStateModel → Controllers | `Qt::DirectConnection` | Latency-sensitive control loops |
| Video → SystemStateModel | `Qt::QueuedConnection` | Cross-thread safety |

#### Latency Fix Example (WeaponController)

```cpp
// ORIGINAL (caused latency buildup):
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &WeaponController::onSystemStateChanged,
        Qt::QueuedConnection);  // ❌ Event queue saturation

// FIX (immediate processing):
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &WeaponController::onSystemStateChanged,
        Qt::DirectConnection);  // ✅ No queue delay
```

### 3.5 Thread Shutdown Sequence

```cpp
// HardwareManager::~HardwareManager()
if (m_dayVideoProcessor && m_dayVideoProcessor->isRunning()) {
    m_dayVideoProcessor->stop();
    if (!m_dayVideoProcessor->wait(2000)) {
        qWarning() << "Day video processor did not stop gracefully";
        m_dayVideoProcessor->terminate();
        if (!m_dayVideoProcessor->wait(1000)) {
            qCritical() << "RESOURCE LEAK!";
        }
    }
}
```

**Timeout Strategy:**
1. Request graceful stop
2. Wait 2000 ms for clean exit
3. Force terminate if timeout
4. Wait 1000 ms for termination
5. Log critical error if still running

---

## 4. Safety Enforcement Flow

### 4.1 SafetyInterlock Architecture

SafetyInterlock is the **central safety authority** created first by ControllerRegistry:

```cpp
// ControllerRegistry::createHardwareControllers()
m_safetyInterlock = new SafetyInterlock(m_systemStateModel, this);
// Created BEFORE any other controller
```

### 4.2 Authorization Priority Order

Safety checks execute in strict priority order. Higher priority failures short-circuit lower checks.

| Priority | Check | Denial Reason |
|----------|-------|---------------|
| 1 | Emergency Stop | `EmergencyStopActive` |
| 2 | Station Enabled | `StationNotEnabled` |
| 3 | Dead Man Switch | `DeadManSwitchNotHeld` |
| 4 | Gun Armed | `GunNotArmed` |
| 5 | Operational Mode | `NotInEngagementMode` |
| 6 | System Authorization | `SystemNotAuthorized` |
| 7 | No-Fire Zone | `InNoFireZone` |
| 8 | Charging State | `ChargingInProgress` / `ChargingFault` |

### 4.3 Template Method Pattern (Motion Modes)

```cpp
// GimbalMotionModeBase (base class)
void GimbalMotionModeBase::updateWithSafety(GimbalController* controller, double dt)
{
    // Step 1: Check safety via SafetyInterlock
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);  // IMMEDIATE HALT
        return;
    }

    // Step 2: Safety passed - delegate to derived class
    updateImpl(controller, dt);
}

// Derived classes ONLY override updateImpl()
void ManualMotionMode::updateImpl(GimbalController* controller, double dt)
{
    // Pure motion logic - safety already guaranteed
}
```

**Guarantee:** All motion modes pass through `updateWithSafety()` before any motion commands execute.

### 4.4 Fail-Safe Defaults

```cpp
bool SafetyInterlock::canFire(SafetyDenialReason* reason) const
{
    if (!m_systemState) {
        // STATE UNAVAILABLE → DENY
        if (reason) *reason = SafetyDenialReason::SystemNotAuthorized;
        return false;
    }
    // ... continue checks
}
```

**Philosophy:** When state is unknown, default to safest condition (DENY).

### 4.5 Audit Trail

All safety decisions are logged with timestamps:

```cpp
qInfo() << "[SafetyInterlock] FIRE DENIED:"
        << denialReasonToString(reason)
        << "at" << QDateTime::currentDateTime().toString(Qt::ISODate);
```

### 4.6 No-Fire Zone Enforcement

```cpp
// SystemStateModel
bool isPointInNoFireZone(float az, float el) const;

// Called by:
// 1. SafetyInterlock::canFire()
// 2. GimbalController::onSystemStateChanged()
// 3. ZoneEnforcementService (traversal prevention)
```

### 4.7 No-Traverse Zone Enforcement

```cpp
// GimbalMotionModeBase::sendStabilizedServoCommands()
ssm->computeAllowedDeltas(
    currentAz, currentEl,
    intendedAzDelta, intendedElDelta,
    allowedAzDelta, allowedElDelta,
    dt
);
// Deltas are clamped to zone boundaries
```

---

## 5. Error Handling and Recovery

### 5.1 Device Disconnection Handling

Each device tracks connection status:

```cpp
struct Plc42Data {
    bool isConnected;
    QDateTime lastCommunicationTime;
    // ...
};
```

Controllers check connection before commands:

```cpp
if (m_plc42 && m_plc42->data()->isConnected) {
    m_plc42->setHomePosition();
} else {
    qCritical() << "Cannot start homing - PLC42 not connected";
    failHoming("PLC42 not connected");
}
```

### 5.2 Timeout Watchdogs

#### Homing Timeout

```cpp
m_timeoutTimer->start(m_homingTimeoutMs);  // Default: 30000 ms

void HomingController::onHomingTimeout()
{
    qCritical() << "HOME sequence TIMEOUT";
    qCritical() << "Possible causes:";
    qCritical() << "  - Wiring issue (I0_7 not connected)";
    qCritical() << "  - Oriental Motor fault";
    qCritical() << "  - Mechanical obstruction";

    failHoming("Timeout - HOME-END not received");
}
```

#### Charging Timeout

```cpp
m_timeoutTimer->start(COCKING_TIMEOUT_MS);  // 5000 ms

void ChargingStateMachine::onChargingTimeout()
{
    qWarning() << "CHARGING TIMEOUT - actuator did not reach position";
    transitionTo(ChargingState::Fault);
    emit cycleFaulted();
    // NOTE: Does NOT auto-retract - operator must clear jam first
}
```

### 5.3 Jam Detection and Recovery

```cpp
void ChargingStateMachine::checkForJam(const ServoActuatorData& data)
{
    double positionDelta = std::abs(data.position_mm - m_previousFeedbackPosition);

    bool highTorque = (data.torque_percent > JAM_TORQUE_THRESHOLD_PERCENT);
    bool stalled = (positionDelta < POSITION_STALL_TOLERANCE_MM);

    if (highTorque && stalled) {
        m_jamDetectionCounter++;
        if (m_jamDetectionCounter >= JAM_CONFIRM_SAMPLES) {
            executeJamRecovery();
        }
    } else {
        m_jamDetectionCounter = 0;  // Reset on normal operation
    }
}

void ChargingStateMachine::executeJamRecovery()
{
    // 1. IMMEDIATE STOP
    emit requestActuatorStop();

    // 2. TRANSITION TO JAM STATE
    transitionTo(ChargingState::JamDetected);

    // 3. DELAYED BACKOFF (allow motor to stabilize)
    QTimer::singleShot(BACKOFF_STABILIZE_MS, this, [this]() {
        emit requestActuatorMove(COCKING_RETRACT_POS);
    });
}
```

### 5.4 Emergency Stop Recovery

```cpp
// WeaponController::onSystemStateChanged()
if (!newData.emergencyStopActive && m_wasInEmergencyStop) {
    // E-STOP CLEARED - SAFE RECOVERY

    if (m_chargingStateMachine->currentState() == ChargingState::Fault) {
        // Auto-initiate safe retraction
        m_chargingStateMachine->resetFault();
    }

    qInfo() << "Operator must re-arm and re-charge weapon";
}
```

### 5.5 Alarm Management (Servo Drivers)

```cpp
// Two-step reset sequence
void GimbalController::clearAlarms()
{
    m_plc42->setResetAlarm(0);

    QTimer::singleShot(1000, this, [this]() {
        m_plc42->setResetAlarm(1);
    });
}
```

**Rationale:** Oriental Motor drivers require specific reset timing.

### 5.6 Startup Auto-Recovery

```cpp
// WeaponController constructor
QTimer::singleShot(500, this, [this]() {
    if (m_stateModel) {
        m_chargingStateMachine->performStartupRetraction(
            m_stateModel->data().actuatorPosition
        );
    }
});
```

Per CROWS M153 specification: Actuator must be retracted on startup to prevent interference with firing.

---

## 6. Data Flow

### 6.1 Hardware to State Model

```
┌─────────────┐    ┌───────────────┐    ┌────────────┐    ┌─────────────┐
│  Transport  │───>│ Protocol      │───>│   Device   │───>│ Data Model  │
│ (Serial/    │    │ Parser        │    │            │    │             │
│  Modbus)    │    │               │    │            │    │             │
└─────────────┘    └───────────────┘    └────────────┘    └─────────────┘
      │                                                          │
      │                                                          │
      │                                                          v
      │                                                   ┌─────────────────┐
      │                                                   │ SystemStateModel│
      └─────────────────────────────────────────────────->│                 │
                    Transport provides                    │ Single Source   │
                    isConnected status                    │ of Truth        │
                                                          └─────────────────┘
```

#### Connection Chain Example (IMU)

```cpp
// HardwareManager::connectDevicesToModels()
connect(m_gyroDevice, &ImuDevice::imuDataChanged,
        m_gyroModel, &GyroDataModel::updateData);

// HardwareManager::connectModelsToSystemState()
connect(m_gyroModel, &GyroDataModel::dataChanged,
        m_systemStateModel, &SystemStateModel::onGyroDataChanged);
```

### 6.2 State Model to Controllers (Observer Pattern)

```cpp
// GimbalController constructor
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &GimbalController::onSystemStateChanged);

// WeaponController constructor
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &WeaponController::onSystemStateChanged,
        Qt::DirectConnection);  // Latency-sensitive
```

### 6.3 State Model to ViewModels

```cpp
// OsdController::initialize()
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &OsdController::onStateChanged);

// OsdController updates OsdViewModel
m_viewModel->setAzimuth(data.gimbalAz);
m_viewModel->setElevation(data.gimbalEl);
```

### 6.4 Controllers to Actuators

```cpp
// Motion Mode → Servo Driver
void GimbalMotionModeBase::writeVelocityCommandOptimized(
    ServoDriverDevice* driverInterface,
    GimbalAxis axis,
    double finalVelocity,
    double scalingFactor,
    qint32& lastSpeedHz)
{
    // Build Modbus packet
    QVector<quint16> packet = (axis == GimbalAxis::Azimuth)
        ? s_azVelocityPacketTemplate
        : s_elVelocityPacketTemplate;

    // Single Modbus write (optimized)
    driverInterface->writeData(AzdReg::OpSpeed, packet);
}
```

### 6.5 Memory Optimization (Video Frames)

**Problem:** FrameData struct contains 3.1 MB QImage × 60fps × 2 cameras = 372 MB/sec potential copies.

**Solution:** Strip unused data at connection boundaries:

```cpp
// ControllerRegistry::connectVideoToOsd()
connect(m_hardwareManager->dayVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
        m_osdController, [this](const FrameData& data) {
            FrameData osdData = data;
            osdData.baseImage = QImage();  // Clear 3.1 MB image
            m_osdController->onFrameDataReady(osdData);
        });
```

### 6.6 State Change Detection

```cpp
void SystemStateModel::updateData(const SystemStateData& newState)
{
    if (m_currentStateData != newState) {
        bool gimbalChanged =
            !qFuzzyCompare(m_currentStateData.gimbalAz, newState.gimbalAz) ||
            !qFuzzyCompare(m_currentStateData.gimbalEl, newState.gimbalEl);

        bool ballisticOffsetsChanged = ...;

        m_currentStateData = newState;
        processStateTransitions(oldData, m_currentStateData);

        if (ballisticOffsetsChanged) {
            recalculateDerivedAimpointData();
        }

        emit dataChanged(m_currentStateData);

        if (gimbalChanged) {
            emit gimbalPositionChanged(m_currentStateData.gimbalAz,
                                       m_currentStateData.gimbalEl);
        }
    }
}
```

### 6.7 Data Flow Summary Table

| Source | Destination | Connection | Rate |
|--------|-------------|------------|------|
| Servo Driver | ServoDriverDataModel | Direct | 110 Hz |
| DataModel | SystemStateModel | Signal | On change |
| SystemStateModel | Controllers | DirectConnection | On change |
| SystemStateModel | ViewModels | QueuedConnection | On change |
| Video Thread | VideoImageProvider | QueuedConnection | 60 fps |
| Controller | Hardware Device | Direct call | 20 Hz |

---

## Appendix A: Timer Summary

| Component | Timer | Interval | Type | Purpose |
|-----------|-------|----------|------|---------|
| GimbalController | m_updateTimer | 50 ms | Periodic | Motion control loop |
| HomingController | m_timeoutTimer | 30000 ms | SingleShot | Homing watchdog |
| ChargingStateMachine | m_timeoutTimer | 5000 ms | SingleShot | Actuator watchdog |
| ChargingStateMachine | m_lockoutTimer | 4000 ms | SingleShot | CROWS M153 lockout |
| EmergencyStopMonitor | Recovery | 1000 ms | SingleShot | Recovery delay |
| GimbalController | Alarm reset | 1000 ms | SingleShot | Two-step reset |
| WeaponController | Startup retract | 500 ms | SingleShot | Initial retraction |
| ChargingStateMachine | Backoff delay | 200 ms | SingleShot | Motor stabilization |

---

## Appendix B: Configuration Files

| File | Purpose | Location |
|------|---------|----------|
| `devices.json` | Hardware port/baudrate configuration | `config/` or `:/config/` |
| `motion_tuning.json` | PID gains, filter constants, limits | `config/` or `:/config/` |
| `zones.json` | Area zones, scan zones, TRPs | `config/` (writable) |
| `m2_ball.json` | M2HB ballistic lookup table | `:/ballistic/tables/` |

---

## Appendix C: Error Codes and Recovery

| Error | Detection | Recovery Action |
|-------|-----------|-----------------|
| Homing timeout | 30s timer | Transition to Failed; operator retry |
| Charging timeout | 5s timer | Transition to Fault; operator reset |
| Jam detected | Torque + stall | Auto-backoff; operator check |
| Device disconnected | isConnected flag | Command rejection; reconnect |
| E-Stop active | Physical button | Immediate halt; recovery delay |
| Thread hang | wait() timeout | Force terminate; log critical |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | January 2026 | AI Assistant | Initial TDS creation |

---

*End of Technical Design Specification*

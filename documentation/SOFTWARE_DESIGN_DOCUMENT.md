# Software Design Document (SDD)
## El-7aress Remote Controlled Weapon Station (RCWS)

**Document Version:** 2.0
**Date:** December 2024
**Classification:** CONFIDENTIAL - RESTRICTED DISTRIBUTION
**Compliance:** MIL-STD-498, IEEE 1016-2009

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [System Overview](#2-system-overview)
3. [Architectural Design](#3-architectural-design)
4. [Component Design](#4-component-design)
5. [Data Flow Design](#5-data-flow-design)
6. [Interface Design](#6-interface-design)
7. [Design Patterns](#7-design-patterns)
8. [Threading Model](#8-threading-model)
9. [External Dependencies](#9-external-dependencies)
10. [Timing Budget & Performance Requirements](#10-timing-budget--performance-requirements)
11. [State Transition Tables](#11-state-transition-tables)
12. [Safety Analysis (FMEA)](#12-safety-analysis-fmea)
13. [Error Recovery Procedures](#13-error-recovery-procedures)
14. [Sequence Diagrams](#14-sequence-diagrams)
15. [Protocol Specifications](#15-protocol-specifications)

---

## 1. Introduction

### 1.1 Purpose

This Software Design Document describes the software architecture and detailed design of the El-7aress Remote Controlled Weapon Station (RCWS) Fire Control System. The system is built on Qt 6 framework using QML for the user interface, running on NVIDIA Jetson embedded platform with EGLFS for direct rendering.

### 1.2 Scope

The RCWS software provides:
- Real-time video processing with GPU-accelerated target tracking (VPI DCF Tracker)
- 2-axis gimbal control with stabilization
- Fire control solution with ballistic calculations
- Multiple operational modes (Manual, Tracking, Sector Scan, TRP Scan)
- Day/Night camera switching with LRF integration
- Zone management (No-Fire, No-Traverse, Sector Scan, TRP)
- Professional On-Screen Display (OSD) overlay

### 1.3 Design Goals

| Priority | Goal | Implementation |
|----------|------|----------------|
| 1 | Real-time Performance | Direct connections, frame-synchronized updates, 20Hz control loop |
| 2 | Safety | Multi-layer interlocks, zone enforcement, dead-man switch |
| 3 | Reliability | Watchdog timers, fault recovery FSMs, graceful degradation |
| 4 | Maintainability | MVVM pattern, dependency injection, strategy pattern |

---

## 2. System Overview

### 2.1 High-Level Architecture

```
+------------------------------------------------------------------+
|                      QML UI LAYER (Views)                         |
|  main.qml | OsdOverlay.qml | MainMenu.qml | ZoneDefinitionOverlay |
+------------------------------------------------------------------+
                              |
                    Q_PROPERTY bindings
                              v
+------------------------------------------------------------------+
|                   VIEWMODEL LAYER (MVVM)                          |
|  OsdViewModel | MenuViewModel(x3) | ZoneDefinitionViewModel       |
|                     ViewModelRegistry                             |
+------------------------------------------------------------------+
                              |
                       Updates via slots
                              v
+------------------------------------------------------------------+
|                 CONTROLLER LAYER (Business Logic)                 |
| +---------------------------+  +-------------------------------+  |
| | Hardware Controllers      |  | QML Controllers               |  |
| | - GimbalController        |  | - ApplicationController       |  |
| | - WeaponController        |  | - OsdController               |  |
| | - CameraController        |  | - MainMenuController          |  |
| | - JoystickController      |  | - ZeroingController           |  |
| +---------------------------+  +-------------------------------+  |
|                     ControllerRegistry                            |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                  CENTRAL DATA HUB                                 |
|                    SystemStateModel                               |
|              (Single Source of Truth - ~180 variables)            |
|                    Contains: SystemStateData                      |
+------------------------------------------------------------------+
                              ^
                              |
                     Aggregates data from
                              |
+------------------------------------------------------------------+
|                     DATA MODELS                                   |
| GyroDataModel | LrfDataModel | Plc21DataModel | Plc42DataModel   |
| ServoDriverDataModel | JoystickDataModel | CameraDataModel       |
+------------------------------------------------------------------+
                              ^
                              |
+------------------------------------------------------------------+
|                   HARDWARE MANAGER                                |
|              (Device Creation & Lifecycle)                        |
+------------------------------------------------------------------+
                              |
        +---------------------+---------------------+
        v                     v                     v
+----------------+   +------------------+   +------------------+
| DEVICE LAYER   |   | PROTOCOL LAYER   |   | TRANSPORT LAYER  |
| ServoDriver    |-->| ServoDriverProto |-->| ModbusTransport  |
| ImuDevice      |-->| Imu3DMGX3Proto   |-->| SerialTransport  |
| Plc21/42Device |-->| PlcProtocolParser|-->| ModbusTransport  |
| CameraVideo    |-->| GStreamer+VPI    |-->| V4L2/USB         |
| LRFDevice      |-->| LrfProtocolParser|-->| SerialTransport  |
| JoystickDevice |-->| SDL2             |-->| HID              |
+----------------+   +------------------+   +------------------+
```

### 2.2 Boot Sequence (3-Phase Initialization)

```
main.cpp
    |
    v
+-- PHASE 1: initializeHardware() ----------------------------------+
| 1. Create SystemStateModel (central data hub)                     |
| 2. Create HardwareManager, ViewModelRegistry, ControllerRegistry  |
| 3. HardwareManager.createHardware() -> Transports, Parsers, Devs  |
| 4. HardwareManager.connectDevicesToModels()                       |
| 5. HardwareManager.connectModelsToSystemState()                   |
| 6. ControllerRegistry.createHardwareControllers()                 |
+-------------------------------------------------------------------+
    |
    v
+-- PHASE 2: initializeQmlSystem(engine) ---------------------------+
| 1. Create VideoImageProvider, register with QML engine            |
| 2. Connect video streams to provider (Day/Night cameras)          |
| 3. ViewModelRegistry.createViewModels()                           |
| 4. ControllerRegistry.createQmlControllers()                      |
| 5. ControllerRegistry.initializeControllers()                     |
| 6. Register ViewModels + Controllers with QML context             |
| 7. Load main.qml                                                  |
+-------------------------------------------------------------------+
    |
    v
+-- PHASE 3: startSystem() -----------------------------------------+
| 1. OsdController.startStartupSequence() -> Boot messages          |
| 2. HardwareManager.startHardware() -> Open transports             |
| 3. GimbalController.clearAlarms()                                 |
| 4. System enters operational mode                                 |
+-------------------------------------------------------------------+
```

---

## 3. Architectural Design

### 3.1 Layer Responsibilities

| Layer | Responsibility | Key Classes |
|-------|----------------|-------------|
| **Entry Point** | App init, GStreamer init, 3-phase boot | `main.cpp` |
| **System Orchestrator** | Coordinates all phases, owns managers | `SystemController` |
| **Hardware Manager** | Creates/owns all devices, transports, parsers | `HardwareManager` |
| **ViewModel Registry** | Factory for all ViewModels, QML registration | `ViewModelRegistry` |
| **Controller Registry** | Factory for Hardware + QML controllers | `ControllerRegistry` |
| **Central State** | Single source of truth (~180+ variables) | `SystemStateModel`, `SystemStateData` |

### 3.2 Class Hierarchy

```
SystemController (root)
|
+-- HardwareManager
|   +-- Transport Layer (ModbusTransport, SerialPortTransport)
|   +-- Protocol Layer (11 protocol parsers)
|   +-- Device Layer (14 hardware devices)
|   +-- Data Models (12 domain data models)
|
+-- ViewModelRegistry
|   +-- OsdViewModel
|   +-- MenuViewModel (x3: Main, Reticle, Color)
|   +-- ZeroingViewModel, WindageViewModel, EnvironmentalViewModel
|   +-- ZoneDefinitionViewModel, ZoneMapViewModel
|   +-- AboutViewModel
|
+-- ControllerRegistry
|   +-- Hardware Controllers
|   |   +-- GimbalController (owns MotionMode strategy objects)
|   |   +-- WeaponController (owns BallisticsProcessorLUT)
|   |   +-- CameraController
|   |   +-- JoystickController
|   |
|   +-- QML Controllers
|       +-- OsdController
|       +-- ApplicationController (menu orchestrator)
|       +-- MainMenuController, ReticleMenuController, ColorMenuController
|       +-- ZeroingController, WindageController, EnvironmentalController
|       +-- ZoneDefinitionController
|       +-- AboutController
|
+-- SystemStateModel (shared across all controllers)
```

### 3.3 QML Component Hierarchy

```
main.qml (Window)
|
+-- Image (videoDisplay) -------------- VideoImageProvider
|
+-- OsdOverlay.qml
|   +-- AzimuthIndicator.qml
|   +-- ElevationScale.qml
|   +-- ReticleRenderer.qml
|   +-- TrackingBox.qml
|   +-- CcipPipper.qml
|   +-- StatusPanel.qml
|
+-- MainMenu.qml (x3 instances)
+-- ZeroingOverlay.qml
+-- WindageOverlay.qml
+-- EnvironmentalOverlay.qml
+-- ZoneDefinitionOverlay.qml
|   +-- ZoneMapCanvas.qml
|   +-- AreaZoneParameterPanel.qml
|   +-- SectorScanParameterPanel.qml
|   +-- TRPParameterPanel.qml
|
+-- AboutDialog.qml
```

---

## 4. Component Design

### 4.1 GimbalController

**Location:** `src/controllers/gimbalcontroller.cpp/h`

**Purpose:** Manages gimbal motion through pluggable motion modes using the Strategy pattern.

**Key Features:**
- 20Hz update loop (50ms timer)
- Strategy pattern for motion modes (Manual, Tracking, SectorScan, TRPScan, RadarSlew)
- Homing state machine with 30-second timeout
- Emergency stop handling
- Free mode monitoring (physical toggle switch)
- Lead angle compensation support in manual mode (CROWS-style)

**Motion Modes:**

| Mode | Class | Description |
|------|-------|-------------|
| Manual | `ManualMotionMode` | Joystick-controlled motion with speed scaling |
| AutoTrack | `TrackingMotionMode` | VPI tracker-driven closed-loop control |
| AutoSectorScan | `AutoSectorScanMotionMode` | Automatic sector sweep between waypoints |
| TRPScan | `TRPScanMotionMode` | Target Reference Point patrol |
| RadarSlew | `RadarSlewMotionMode` | Slew to radar-designated target |

**State Machines:**

```
Homing State Machine:
  Idle --> Requested --> InProgress --> Completed
                    |         |
                    v         v
                 Aborted    Failed (timeout)

Emergency Stop Flow:
  Active --> STOP command to PLC42 --> Abort homing --> Halt servos
  Released --> Manual mode --> Resume operations
```

### 4.2 WeaponController

**Location:** `src/controllers/weaponcontroller.cpp/h`

**Purpose:** Fire control solution and ammunition feed management.

**Key Features:**
- Ballistic LUT system (Kongsberg/Rafael approach)
- Automatic drop compensation when LRF range valid
- Motion lead compensation when LAC toggle active
- Crosswind calculation from windage + gimbal azimuth
- FSM-controlled ammunition feed cycle (MIL-STD compliant)
- No-Fire zone enforcement

**Ballistics Processing:**

```
Fire Control Solution = Zeroing Offset + Ballistic Drop + Motion Lead + Wind Deflection

Where:
- Zeroing Offset: Operator boresight correction (manual)
- Ballistic Drop: Auto-applied when LRF range valid (gravity compensation)
- Motion Lead: Applied when LAC active (target velocity x TOF)
- Wind Deflection: Crosswind component based on wind direction vs gimbal azimuth
```

**Ammo Feed FSM:**

```
    +-------+
    | Idle  |<---------------------------+
    +---+---+                            |
        | (button press)                 |
        v                                |
  +-----------+                    +-----------+
  | Extending |---(timeout)------->|   Fault   |
  +-----+-----+                    +-----+-----+
        | (position reached)             |
        v                          (operator reset)
  +-----------+                          |
  | Extended  | (hold while button held) |
  +-----+-----+                          |
        | (button released)              |
        v                                |
  +------------+                         |
  | Retracting |---(timeout)-------------+
  +-----+------+
        | (home position reached)
        v
    +-------+
    | Idle  | (cycle complete, ammo loaded)
    +-------+
```

### 4.3 CameraController

**Location:** `src/controllers/cameracontroller.cpp/h`

**Purpose:** Day/Night camera switching, zoom control, tracking management, LRF control.

**Key Features:**
- Day camera: VISCA protocol zoom/focus
- Night camera: Thermal LUT selection, digital zoom, FFC control
- Camera-aware tracking (stops tracker on inactive camera)
- LRF trigger (single shot and continuous modes)
- Thread-safe with QMutex

### 4.4 JoystickController

**Location:** `src/controllers/joystickcontroller.cpp/h`

**Purpose:** Routes joystick inputs to appropriate system functions.

**Button Mapping:**

| Button | Function | Notes |
|--------|----------|-------|
| 0 | Engagement Mode | Momentary (press=engage, release=return) |
| 1 | LRF Trigger | Single press=single shot, Double press=toggle continuous |
| 2 | LAC Toggle | Requires dead-man switch active |
| 3 | Dead Man Switch | Safety interlock |
| 4 | Track | Single=cycle phase, Double=abort tracking |
| 5 | Fire Weapon | Requires system armed |
| 6/8 | Zoom In/Out | Day camera optical, Night camera digital |
| 7/9 | Video LUT +/- | Thermal camera only |
| 10 | LRF Clear | Clears current range |
| 11/13 | Mode Cycle | Manual->SectorScan->TRP->Radar->Manual |
| 14/16 | Zone Up/Down | Navigate scan zones |
| Hat | Gate Size | Resize tracking acquisition box |

### 4.5 OsdController

**Location:** `src/controllers/osdcontroller.cpp/h`

**Purpose:** Manages On-Screen Display updates and startup sequence.

**Key Features:**
- Frame-synchronized OSD updates (camera thread → main thread)
- Active camera awareness (ignores frames from inactive camera)
- Startup sequence with IMU gyro bias capture detection
- Device health monitoring
- Zone warning display

### 4.6 ApplicationController

**Location:** `src/controllers/applicationcontroller.cpp/h`

**Purpose:** Central orchestrator for menu navigation and UI state.

**Menu State Machine:**

```
MenuState::None
    | (MENU/VAL press)
    v
MenuState::MainMenu
    | (select option)
    +---> MenuState::ReticleMenu ---> (return) --+
    +---> MenuState::ColorMenu -----> (return) --+
    +---> MenuState::ZeroingProcedure -> (finish) --+
    +---> MenuState::WindageProcedure -> (finish) --+
    +---> MenuState::EnvironmentalProcedure -> (finish) --+
    +---> MenuState::ZoneDefinition ---> (close) --+
    +---> MenuState::HelpAbout --------> (close) --+
    |                                              |
    +<---------------------------------------------+
```

---

## 5. Data Flow Design

### 5.1 Real-Time Data Flow

```
+---------------+        +----------------+        +------------------+
| HARDWARE      |        | DATA MODELS    |        | SYSTEMSTATE      |
| (Serial/USB)  |        | (Per-device)   |        |    MODEL         |
+---------------+        +----------------+        +------------------+
| IMU (100Hz)   |--data->| GyroDataModel  |--slot->|                  |
| ServoDrv(50Hz)|--data->| ServoDriverDM  |--slot->| SystemStateData  |
| PLC21 (20Hz)  |--data->| Plc21DataModel |--slot->|   (~180 vars)    |
| PLC42 (20Hz)  |--data->| Plc42DataModel |--slot->|                  |
| Joystick(60Hz)|--data->| JoystickDataMdl|--slot->|                  |
| Camera (30Hz) |--frame->| FrameData     |--slot->|                  |
+---------------+        +----------------+        +--------+---------+
                                                            |
          +------------------+---------------------+--------+
          |                  |                     |
          v                  v                     v
  +----------------+  +----------------+  +----------------+
  |GimbalController|  |WeaponController|  | OsdController  |
  | - Motion logic |  | - Fire control |  | - Update OSD   |
  | - Servo cmds   |  | - Ballistics   |  | - Tracking box |
  +-------+--------+  +-------+--------+  +-------+--------+
          |                   |                   |
          v                   v                   v
  ServoDriverDevice    Plc42Device         OsdViewModel
  (Speed commands)     (Solenoid ctrl)     (Q_PROPERTY)
```

### 5.2 QML-C++ Interaction Pattern

```
+------------------------------------------------------------------+
|                         QML LAYER                                 |
| +--------------------------------------------------------------+ |
| | OsdOverlay.qml                                                | |
| |   Text { text: osdViewModelInstance.azimuthText }  <---+      | |
| |   Text { text: osdViewModelInstance.elevationText }<---+      | |
| |   visible: osdViewModelInstance.trackingBoxVisible <---+      | |
| +--------------------------------------------------------------+ |
+------------------------------------------------------------------+
                                                   |
                          Q_PROPERTY notify signals|
                                                   |
+------------------------------------------------------------------+
|                       C++ BACKEND                                 |
| +--------------------------------------------------------------+ |
| | OsdViewModel (Q_OBJECT)                                       | |
| |   Q_PROPERTY(QString azimuthText READ azimuthText NOTIFY...)  | |
| |   Q_PROPERTY(QString elevationText READ elevationText NOTIFY) | |
| |   Q_PROPERTY(bool trackingBoxVisible READ... NOTIFY...)       | |
| +--------------------------------------------------------------+ |
|                               ^                                   |
|                               | set via OsdController             |
| +--------------------------------------------------------------+ |
| | OsdController::onFrameDataReady(FrameData& data)              | |
| |   m_viewModel->updateAzimuth(data.azimuth);                   | |
| |   m_viewModel->updateElevation(data.elevation);               | |
| +--------------------------------------------------------------+ |
+------------------------------------------------------------------+
```

### 5.3 Tracking Data Flow

```
Camera Frame (30Hz)
       |
       v
CameraVideoStreamDevice (GStreamer thread)
       |
       +-- VPI DCF Tracker (CUDA)
       |       |
       |       v
       |   Track result: bbox, confidence, state
       |
       v
FrameData struct (copied to main thread via Qt::QueuedConnection)
       |
       +---> OsdController (OSD update)
       |         |
       |         v
       |     OsdViewModel --> QML TrackingBox
       |
       +---> GimbalController (servo commands)
                 |
                 v
             TrackingMotionMode
                 |
                 +-- Calculate angular offset from pixel error
                 +-- PID control for servo velocity
                 |
                 v
             ServoDriverDevice (Modbus commands)
```

---

## 6. Interface Design

### 6.1 Hardware Interfaces

| Device | Protocol | Transport | Rate | Notes |
|--------|----------|-----------|------|-------|
| Azimuth Servo | Oriental AZ-Series | Modbus RTU | 50Hz | Separate thread |
| Elevation Servo | Oriental AZ-Series | Modbus RTU | 50Hz | Separate thread |
| PLC21 (Weapon Control) | Custom Modbus | Modbus RTU | 20Hz | Fire control, switches |
| PLC42 (Gimbal Control) | Custom Modbus | Modbus RTU | 20Hz | Solenoids, sensors |
| IMU | LORD MicroStrain 3DM-GX3 | RS-232 | 100Hz | AHRS data |
| LRF | Eye-safe laser | RS-232 | On-demand | Single/continuous |
| Day Camera (Control) | VISCA | RS-232 | On-demand | Zoom/focus |
| Night Camera (Control) | FLIR TAU2 | RS-232 | On-demand | LUT/FFC |
| Day Camera (Video) | USB Video | V4L2 | 30Hz | 1024x768 |
| Night Camera (Video) | USB Video | V4L2 | 30Hz | 640x512 |
| Joystick | SDL2 HID | USB | 60Hz | Axes, buttons, hat |
| Radar | Custom | RS-232 | Variable | Plot data |

### 6.2 Key Data Structures

**SystemStateData** (Central state ~180 variables):

```cpp
struct SystemStateData {
    // Operational State & Modes
    OperationalMode opMode;           // Idle, Surveillance, Tracking, Engagement, EmergencyStop
    MotionMode motionMode;            // Manual, AutoTrack, AutoSectorScan, TRPScan, RadarSlew

    // Gimbal Position
    double gimbalAz, gimbalEl;        // Current position (degrees)
    bool azServoConnected, elServoConnected;
    bool azFault, elFault;

    // Camera Systems
    bool activeCameraIsDay;
    double dayCurrentHFOV, nightCurrentHFOV;

    // Tracking System
    TrackingPhase currentTrackingPhase;  // Off, Acquisition, LockPending, ActiveLock, Coast
    float trackedTargetCenterX_px, trackedTargetCenterY_px;
    float trackingConfidence;

    // Weapon System
    bool gunArmed, ammoLoaded;
    bool deadManSwitchActive;
    AmmoFeedState ammoFeedState;

    // Ballistics
    float currentTargetRange;
    float zeroingAzimuthOffset, zeroingElevationOffset;
    float ballisticDropOffsetAz, ballisticDropOffsetEl;
    float motionLeadOffsetAz, motionLeadOffsetEl;
    LeadAngleStatus currentLeadAngleStatus;

    // Zones
    std::vector<AreaZone> areaZones;
    std::vector<AutoSectorScanZone> sectorScanZones;
    std::vector<TargetReferencePoint> targetReferencePoints;
    bool isReticleInNoFireZone, isReticleInNoTraverseZone;

    // ... ~150 more variables ...
};
```

**FrameData** (Camera frame with tracking results):

```cpp
struct FrameData {
    QImage baseImage;                 // Raw camera frame
    int cameraIndex;                  // 0=Day, 1=Night
    QRectF trackingBbox;              // Tracker bounding box
    VPITrackingState trackingState;   // VPI tracker state
    float trackingConfidence;         // 0.0-1.0
    TrackingPhase currentTrackingPhase;
    // + OSD data snapshot for frame-synchronized display
};
```

---

## 7. Design Patterns

| Pattern | Implementation | Location | Purpose |
|---------|----------------|----------|---------|
| **MVVM** | ViewModels expose state via Q_PROPERTY | `src/models/*viewmodel.*` | Decouple QML from business logic |
| **Strategy** | Motion modes are pluggable | `src/controllers/motion_modes/*` | Flexible gimbal control |
| **Observer** | Qt signals/slots throughout | All controllers | Event-driven updates |
| **Registry** | Centralized factory/lookup | `ViewModelRegistry`, `ControllerRegistry` | Dependency management |
| **Dependency Injection** | Controllers receive deps via ctor | All controllers | Testability, loose coupling |
| **Template Method** | `TemplatedDevice<TData>` base class | `src/hardware/devices/TemplatedDevice.h` | Common device behavior |
| **State Machine** | TrackingPhase, HomingState, AmmoFeedState | `systemstatedata.h` | Complex state management |
| **Facade** | SystemController hides complexity | `systemcontroller.cpp` | Simplified initialization |

---

## 8. Threading Model

```
+------------------------------------------------------------------+
|                    THREAD ARCHITECTURE                            |
+------------------------------------------------------------------+

+-----------------------------+    +-----------------------------+
|       MAIN THREAD           |    |    SERVO AZ THREAD          |
|    (Qt Event Loop)          |    |  (m_servoAzThread)          |
+-----------------------------+    +-----------------------------+
| - QML Rendering             |<-->| - ServoDriverDevice (Az)    |
| - UI Controllers            |    | - Modbus polling (50Hz)     |
| - GimbalController          |    | - Position/velocity updates |
| - SystemStateModel          |    +-----------------------------+
| - VideoImageProvider        |
| - Most I/O devices          |    +-----------------------------+
+-----------------------------+    |    SERVO EL THREAD          |
                                   |  (m_servoElThread)          |
+-----------------------------+    +-----------------------------+
|  VIDEO PROCESSOR THREADS    |    | - ServoDriverDevice (El)    |
| (CameraVideoStreamDevice)   |    | - Modbus polling (50Hz)     |
+-----------------------------+    | - Position/velocity updates |
| - GStreamer pipeline thread |    +-----------------------------+
| - VPI DCF Tracker (CUDA)    |
| - Frame processing (30Hz)   |    +-----------------------------+
| - Emits: frameDataReady()   |    |    JOYSTICK THREAD          |
+-----------------------------+    |  (SDL2 event loop)          |
                                   +-----------------------------+
Thread Safety:                     | - JoystickDevice            |
- Qt::QueuedConnection             | - Axis/button polling       |
- TemplatedDevice<T> pattern       | - Dead-man switch monitor   |
- QMutex for shared resources      +-----------------------------+
```

---

## 9. External Dependencies

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| Qt | 6.5+ | Framework (Quick, SerialBus, SerialPort, D-Bus) | LGPL/Commercial |
| GStreamer | 1.0 | Video capture and pipeline | LGPL |
| NVIDIA VPI | 3.x | GPU-accelerated DCF tracking | NVIDIA |
| OpenCV | 4.x | Image format conversion | Apache 2.0 |
| CUDA | 11+ | GPU compute for VPI/inference | NVIDIA |
| SDL2 | 2.x | Joystick HID input | zlib |
| Eigen3 | 3.x | Matrix operations (ballistics) | MPL2 |

---

## Appendix A: Enumeration Reference

### OperationalMode
```cpp
enum class OperationalMode {
    Idle,           // System idle
    Surveillance,   // Area surveillance
    Tracking,       // Target tracking
    Engagement,     // Active engagement
    EmergencyStop   // Emergency stop active
};
```

### MotionMode
```cpp
enum class MotionMode {
    Manual,         // Joystick control
    AutoTrack,      // VPI tracker control
    ManualTrack,    // Manual target tracking
    AutoSectorScan, // Automatic sector sweep
    TRPScan,        // Target Reference Point patrol
    RadarSlew,      // Radar-designated slew
    MotionFree,     // Brakes released (physical toggle)
    Idle            // No motion
};
```

### TrackingPhase
```cpp
enum class TrackingPhase {
    Off,                  // Tracking inactive
    Acquisition,          // User positioning gate
    Tracking_LockPending, // Attempting lock
    Tracking_ActiveLock,  // Solid lock, following target
    Tracking_Coast,       // Target temporarily lost
    Tracking_Firing       // Weapon fired, holding position
};
```

### ZoneType
```cpp
enum class ZoneType {
    None,                  // No zone
    Safety,                // General restriction
    NoTraverse,            // Movement blocked
    NoFire,                // Firing prohibited
    AutoSectorScan,        // Scan area
    TargetReferencePoint   // TRP zone
};
```

---

## Appendix B: Configuration Files

| File | Location | Purpose |
|------|----------|---------|
| `devices.json` | `config/` | Hardware device configuration (ports, addresses) |
| `motion_tuning.json` | `config/` | PID gains, speed limits, stabilizer parameters |
| `m2_ball.json` | `ballistic/tables/` | Ballistic lookup table for .50 cal |
| `zones.json` | `config/` | Saved zone definitions |

---

## 10. Timing Budget & Performance Requirements

### 10.1 Control Loop Timing

| Parameter | Requirement | Actual | Margin | Source File |
|-----------|-------------|--------|--------|-------------|
| **Gimbal Control Loop** | ≤50ms | 50ms | 0% | `motion_tuning.json: updateIntervalS` |
| **Servo Position Update** | ≤20ms | 20ms (50Hz) | 0% | `devices.json: pollIntervalMs` |
| **IMU Data Rate** | ≤10ms | 10ms (100Hz) | - | `devices.json: samplingRateHz=100` |
| **Video Frame Rate** | ≤33ms | 33ms (30Hz) | 0% | `devices.json: framerate=30` |
| **Joystick Input** | ≤17ms | 16.7ms (60Hz) | 2% | SDL2 polling rate |
| **OSD Refresh** | ≤33ms | 33ms (30Hz) | 0% | Frame-synchronized |

### 10.2 End-to-End Latency Budget

```
JOYSTICK TO SERVO RESPONSE (Manual Mode):
├── Joystick polling:           ~8ms (60Hz average)
├── JoystickController process: ~1ms
├── GimbalController update:    ~2ms (filtering + rate limit)
├── Modbus write + response:    ~5ms (230400 baud)
├── Servo motor response:       ~10ms (acceleration ramp)
└── TOTAL:                      ~26ms (< 50ms requirement) ✓

TRACKER TO SERVO RESPONSE (Tracking Mode):
├── Frame capture (30Hz):       ~16ms (average wait)
├── VPI DCF Tracker (CUDA):     ~5ms
├── Qt::QueuedConnection:       ~2ms
├── GimbalController PID:       ~2ms
├── Modbus write + response:    ~5ms
├── Servo motor response:       ~10ms
└── TOTAL:                      ~40ms (< 50ms requirement) ✓

FIRE COMMAND TO SOLENOID ACTIVATION:
├── Button debounce:            ~10ms
├── JoystickController:         ~1ms
├── WeaponController safety:    ~1ms
├── PLC42 Modbus write:         ~5ms (115200 baud)
├── PLC relay activation:       ~5ms
└── TOTAL:                      ~22ms (< 30ms requirement) ✓
```

### 10.3 Timing Constants (from `motion_tuning.json`)

| Constant | Value | Unit | Purpose |
|----------|-------|------|---------|
| `updateIntervalS` | 0.05 | seconds | Main control loop period |
| `trackingPositionTau` | 0.12 | seconds | Target position filter τ |
| `trackingVelocityTau` | 0.08 | seconds | Target velocity filter τ |
| `manualJoystickTau` | 0.08 | seconds | Joystick input filter τ |
| `maxAccelerationDegS2` | 50.0 | °/s² | Tracking mode acceleration |
| `scanMaxAccelDegS2` | 20.0 | °/s² | Sector scan acceleration |
| `maxVelocityDegS` | 60.0 | °/s | Maximum gimbal velocity |
| `arrivalThresholdDeg` | 0.5 | ° | Position arrival deadband |

### 10.4 PID Tuning Parameters

| Axis | Mode | Kp | Ki | Kd | Max Integral |
|------|------|-----|-----|-----|--------------|
| Azimuth | Tracking | 1.2 | 0.01 | 0.05 | 5.0 |
| Elevation | Tracking | 1.0 | 0.01 | 0.08 | 5.0 |
| Azimuth | SectorScan | 0.7 | 0.01 | 0.05 | 20.0 |
| Elevation | SectorScan | 1.0 | 0.01 | 0.05 | 20.0 |
| Azimuth | TRPScan | 0.7 | 0.01 | 0.05 | 20.0 |
| Elevation | TRPScan | 1.2 | 0.015 | 0.08 | 25.0 |
| Azimuth | RadarSlew | 1.5 | 0.08 | 0.15 | 30.0 |
| Elevation | RadarSlew | 1.5 | 0.08 | 0.15 | 30.0 |

---

## 11. State Transition Tables

### 11.1 Tracking Phase State Machine

| Current State | Event | Next State | Actions | Guards |
|---------------|-------|------------|---------|--------|
| `Off` | Track button (single) | `Acquisition` | Show acquisition box, center on screen | Dead-man switch active |
| `Acquisition` | Track button (single) | `Tracking_LockPending` | Initialize VPI tracker with gate | Gate size > 0 |
| `Acquisition` | Track button (double) | `Off` | Hide acquisition box | - |
| `Tracking_LockPending` | Tracker confidence > 0.25 | `Tracking_ActiveLock` | Enable motion mode, show green box | - |
| `Tracking_LockPending` | Timeout (2s) | `Off` | Show "LOCK FAILED", stop tracker | - |
| `Tracking_ActiveLock` | Confidence < 0.15 for 0.5s | `Tracking_Coast` | Show yellow box, continue prediction | - |
| `Tracking_ActiveLock` | Track button (double) | `Off` | Stop tracker, return to Manual | - |
| `Tracking_ActiveLock` | Fire button pressed | `Tracking_Firing` | Hold position, inhibit motion | Weapon armed |
| `Tracking_Coast` | Confidence > 0.25 | `Tracking_ActiveLock` | Resume tracking, show green box | - |
| `Tracking_Coast` | Coast timeout (3s) | `Off` | Stop tracker, show "TRACK LOST" | - |
| `Tracking_Firing` | Fire button released | `Tracking_ActiveLock` | Resume tracking | - |

### 11.2 Homing State Machine

| Current State | Event | Next State | Actions | Guards |
|---------------|-------|------------|---------|--------|
| `Idle` | Home command | `Requested` | Set motion mode to Manual, prepare | Station enabled |
| `Requested` | PLC42 acknowledges | `InProgress` | Send home position command | Emergency stop not active |
| `InProgress` | Position reached | `Completed` | Update OSD "HOMING COMPLETE" | |Az| < 0.5° AND |El| < 0.5° |
| `InProgress` | Timeout (30s) | `Failed` | Show "HOMING TIMEOUT", stop servos | - |
| `InProgress` | Emergency stop | `Aborted` | Immediate halt, show "HOMING ABORTED" | - |
| `Completed` | Any motion command | `Idle` | Ready for normal operation | - |
| `Failed` | Operator reset | `Idle` | Clear fault, allow retry | - |
| `Aborted` | Emergency stop released | `Idle` | Resume normal operation | - |

### 11.3 Ammunition Feed FSM

| Current State | Event | Next State | Actions | Guards |
|---------------|-------|------------|---------|--------|
| `Idle` | Feed button pressed | `Extending` | Command actuator extend, start timer | Actuator connected |
| `Extending` | Position ≥ FEED_EXTEND_MM (45mm) | `Extended` | Stop actuator, hold position | - |
| `Extending` | Timeout (5s) | `Fault` | Show "FEED FAULT", stop actuator | - |
| `Extended` | Feed button released | `Retracting` | Command actuator retract | - |
| `Extended` | Feed button held | `Extended` | Maintain extended position | - |
| `Retracting` | Position ≤ FEED_HOME_MM (5mm) | `Idle` | Set ammoLoaded=true, cycle complete | - |
| `Retracting` | Timeout (5s) | `Fault` | Show "RETRACT FAULT" | - |
| `Fault` | Operator reset | `Idle` | Clear fault, ready for retry | Manual intervention |

### 11.4 Operational Mode Transitions

| Current Mode | Event | Next Mode | Actions | Guards |
|--------------|-------|-----------|---------|--------|
| `Idle` | Station enabled | `Surveillance` | Enable servos, start video | Hatch closed |
| `Surveillance` | Track lock acquired | `Tracking` | Enable TrackingMotionMode | Dead-man switch |
| `Surveillance` | Engagement button | `Engagement` | Arm weapon systems | System ready |
| `Tracking` | Track lost | `Surveillance` | Return to Manual mode | - |
| `Tracking` | Engagement button | `Engagement` | Keep tracking, arm weapon | System ready |
| `Engagement` | Engagement released | `Surveillance`/`Tracking` | Disarm weapon | - |
| `Engagement` | Fire completed | `Engagement` | Maintain armed state | - |
| `*` (any) | Emergency stop | `EmergencyStop` | Immediate halt all motion | - |
| `EmergencyStop` | E-stop released | `Surveillance` | Resume monitoring | Station enabled |

---

## 12. Safety Analysis (FMEA)

### 12.1 Failure Mode Effects Analysis

| ID | Component | Failure Mode | Effect | Severity | Detection | Mitigation | RPN |
|----|-----------|--------------|--------|----------|-----------|------------|-----|
| **F-001** | IMU | Complete loss | No stabilization, drift | HIGH | IMU connection monitor | Graceful degradation to encoder-only | 48 |
| **F-002** | Az Servo | Communication loss | Cannot slew horizontally | CRITICAL | Modbus timeout (500ms) | Halt motion, alert operator | 72 |
| **F-003** | El Servo | Communication loss | Cannot slew vertically | CRITICAL | Modbus timeout (500ms) | Halt motion, alert operator | 72 |
| **F-004** | Az Servo | Motor fault | Sudden stop/runaway | CRITICAL | Alarm register polling | Emergency stop, brake engage | 64 |
| **F-005** | El Servo | Motor fault | Elevation runaway | CRITICAL | Alarm register + limits | Hardware elevation limits | 56 |
| **F-006** | Joystick | Dead-man switch stuck | False "safe" indication | HIGH | Periodic reset requirement | Timeout release detection | 36 |
| **F-007** | VPI Tracker | Track loss during engage | Missed target | MEDIUM | Confidence monitoring | Coast mode, manual takeover | 24 |
| **F-008** | LRF | No return/fault | Incorrect range | MEDIUM | Error code parsing | Manual range entry option | 20 |
| **F-009** | PLC21 | Fire button stuck | Continuous fire | CRITICAL | Edge detection, timing | Hardware interlock (E-stop) | 80 |
| **F-010** | PLC42 | Solenoid driver fail | No fire capability | MEDIUM | Feedback sensing | Redundant fire path | 30 |
| **F-011** | Day Camera | Video loss | No visual | HIGH | Frame timeout (1s) | Auto-switch to Night | 32 |
| **F-012** | Night Camera | Video loss | No thermal | HIGH | Frame timeout (1s) | Auto-switch to Day | 32 |
| **F-013** | Video Pipeline | GStreamer crash | Black screen | HIGH | Process monitoring | Auto-restart pipeline | 40 |
| **F-014** | Zone Database | Corruption | Zone violation | HIGH | CRC validation | Factory reset capability | 28 |

**RPN Calculation:** Severity (1-10) × Occurrence (1-10) × Detection (1-10)

### 12.2 Safety-Critical Paths

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    FIRE COMMAND SAFETY CHAIN                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  [Fire Button]                                                           │
│       │                                                                  │
│       ├── CHECK: Dead-Man Switch Active? ──NO──► BLOCKED                │
│       │                                                                  │
│       ├── CHECK: Station Enabled? ──────────NO──► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: Weapon Armed? ─────────────NO──► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: Emergency Stop Inactive? ──NO──► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: In No-Fire Zone? ──────────YES─► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: Ammo Loaded? ──────────────NO──► WARNING               │
│       │                                                                  │
│       └── ALL CHECKS PASS ──► FIRE COMMAND TO PLC42                     │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                    MOTION COMMAND SAFETY CHAIN                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  [Motion Command from any source]                                        │
│       │                                                                  │
│       ├── CHECK: Emergency Stop? ───────────YES─► HALT                  │
│       │                                                                  │
│       ├── CHECK: Free Mode Toggle? ─────────YES─► BRAKE RELEASE         │
│       │                                                                  │
│       ├── CHECK: Station Enabled? ──────────NO──► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: Dead-Man (Manual/Track)? ──NO──► BLOCKED               │
│       │                                                                  │
│       ├── CHECK: In No-Traverse Zone? ──────YES─► CLAMP VELOCITY        │
│       │                                                                  │
│       ├── CHECK: Elevation Limits? ─────────YES─► CLAMP ELEVATION       │
│       │                                                                  │
│       └── ALL CHECKS PASS ──► SERVO COMMAND                             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 12.3 Safety Interlock Matrix

| Condition | Fire | Motion | Track | Scan | Home |
|-----------|------|--------|-------|------|------|
| Emergency Stop Active | ✗ | ✗ | ✗ | ✗ | ✗ |
| Station Disabled | ✗ | ✗ | ✗ | ✗ | ✗ |
| Dead-Man Switch Released | ✗ | ✗ (Manual) | ✗ | ✓ | ✓ |
| Weapon Not Armed | ✗ | ✓ | ✓ | ✓ | ✓ |
| In No-Fire Zone | ✗ | ✓ | ✓ | ✓ | ✓ |
| In No-Traverse Zone | ✓ | CLAMP | CLAMP | SKIP | CLAMP |
| Servo Fault Active | ✗ | ✗ | ✗ | ✗ | ✗ |
| IMU Disconnected | ✓ | ✓ (reduced) | ✓ (reduced) | ✓ | ✓ |

---

## 13. Error Recovery Procedures

### 13.1 Servo Communication Loss

```
┌─────────────────────────────────────────────────────────────────┐
│  SERVO COMMUNICATION LOSS RECOVERY                               │
├─────────────────────────────────────────────────────────────────┤
│  1. Detection: Modbus timeout > 500ms (3 consecutive failures)   │
│                                                                  │
│  2. Immediate Actions:                                           │
│     ├── Set axis fault flag (azFault/elFault = true)            │
│     ├── Stop sending commands to affected axis                   │
│     ├── Display "AZ/EL SERVO DISCONNECTED" on OSD               │
│     └── Log error with timestamp                                 │
│                                                                  │
│  3. Recovery Attempt (automatic):                                │
│     ├── Wait 2 seconds                                          │
│     ├── Attempt reconnection (up to 3 retries)                  │
│     ├── If successful: Clear fault, resume operation            │
│     └── If failed: Require manual intervention                   │
│                                                                  │
│  4. Manual Recovery:                                             │
│     ├── Operator selects "Reset Servo Alarm" from menu          │
│     ├── System sends alarm reset command (register 388)          │
│     └── Verify fault cleared, resume operation                   │
└─────────────────────────────────────────────────────────────────┘
```

### 13.2 Track Loss During Engagement

```
┌─────────────────────────────────────────────────────────────────┐
│  TRACK LOSS DURING ENGAGEMENT                                    │
├─────────────────────────────────────────────────────────────────┤
│  1. Detection: Tracking confidence < 0.15 for > 500ms           │
│                                                                  │
│  2. Phase 1 - Coast Mode (0-3 seconds):                         │
│     ├── Continue predicting target position                     │
│     ├── Display yellow tracking box + "COAST"                   │
│     ├── Maintain weapon armed state                             │
│     └── If confidence recovers > 0.25: Resume ActiveLock        │
│                                                                  │
│  3. Phase 2 - Track Lost (after 3 seconds):                     │
│     ├── Transition to TrackingPhase::Off                        │
│     ├── Display "TRACK LOST" on OSD                             │
│     ├── Return gimbal to Manual mode                            │
│     ├── Maintain weapon armed (operator decision)               │
│     └── Operator can re-initiate tracking                       │
│                                                                  │
│  4. Critical: Fire Inhibit NOT automatic                         │
│     └── Operator maintains fire authority throughout             │
└─────────────────────────────────────────────────────────────────┘
```

### 13.3 IMU Failure Recovery

```
┌─────────────────────────────────────────────────────────────────┐
│  IMU FAILURE RECOVERY                                            │
├─────────────────────────────────────────────────────────────────┤
│  1. Detection: No valid IMU data for > 100ms                    │
│                                                                  │
│  2. Degraded Mode Activation:                                    │
│     ├── Set imuConnected = false                                │
│     ├── Disable hybrid stabilization                            │
│     ├── Fall back to encoder-only position feedback             │
│     ├── Display "STAB OFF" on OSD                               │
│     └── Continue operation with reduced accuracy                 │
│                                                                  │
│  3. Impact by Mode:                                              │
│     ├── Manual: Full functionality (no stabilization)           │
│     ├── Tracking: Reduced accuracy (no platform compensation)   │
│     ├── SectorScan: Normal operation (encoder-based)            │
│     └── TRP: Normal operation (encoder-based)                   │
│                                                                  │
│  4. Recovery:                                                    │
│     ├── IMU reconnection detected automatically                 │
│     ├── 10-second gyro bias capture period                      │
│     ├── Stabilization re-enabled                                │
│     └── Display "STAB ON" on OSD                                │
└─────────────────────────────────────────────────────────────────┘
```

### 13.4 Video Pipeline Failure

```
┌─────────────────────────────────────────────────────────────────┐
│  VIDEO PIPELINE FAILURE RECOVERY                                 │
├─────────────────────────────────────────────────────────────────┤
│  1. Detection: No frames for > 1000ms                           │
│                                                                  │
│  2. Immediate Actions:                                           │
│     ├── Set camera error flag                                   │
│     ├── Display "NO VIDEO SIGNAL" on last frame                 │
│     ├── Stop VPI tracker (no valid input)                       │
│     └── If tracking was active: transition to TrackingPhase::Off│
│                                                                  │
│  3. Auto-Recovery Attempt:                                       │
│     ├── Pipeline restart after 2 seconds                        │
│     ├── GStreamer element recreation                            │
│     └── VPI tracker reinitialization                            │
│                                                                  │
│  4. Fallback:                                                    │
│     ├── If Day camera fails: Attempt Night camera switch        │
│     ├── If Night camera fails: Attempt Day camera switch        │
│     └── If both fail: Display static "VIDEO FAULT" screen       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 14. Sequence Diagrams

### 14.1 Target Acquisition to Fire Sequence

```
 Operator     Joystick     CameraCtrl    Tracker(VPI)    GimbalCtrl    WeaponCtrl    PLC42
    │            │             │              │              │              │           │
    │─TRACK_BTN─►│             │              │              │              │           │
    │            │─startAcq───►│              │              │              │           │
    │            │             │─initTracker─►│              │              │           │
    │            │             │              │              │              │           │
    │◄───────────────────────────────"ACQUISITION" OSD──────────────────────│           │
    │                                                                       │           │
    │──(position gate with joystick)──►                                     │           │
    │                                                                       │           │
    │─TRACK_BTN─►│             │              │              │              │           │
    │            │─confirmGate►│              │              │              │           │
    │            │             │─startTrack──►│              │              │           │
    │            │             │              │─trackResult─►│              │           │
    │            │             │              │              │─velocityCmd─►│           │
    │            │             │              │              │              │           │
    │◄─────────────────────────────"TRACKING" OSD (green box)───────────────│           │
    │                                                                       │           │
    │─LRF_BTN───►│             │              │              │              │           │
    │            │─triggerLrf─►│              │              │              │           │
    │            │             │◄──range=850m─│              │              │           │
    │            │             │              │              │              │           │
    │            │             │─────────────────────────────►│             │           │
    │            │             │              │              ─ballisticCalc─►│           │
    │◄────────────────────────────"RNG: 850m  DROP: +2.1°"──────────────────│           │
    │                                                                       │           │
    │─ENGAGE_BTN►│             │              │              │              │           │
    │            │─armWeapon──►│              │              │              │           │
    │            │             │──────────────────────────────────────arm───►│          │
    │◄────────────────────────────"ARMED" OSD───────────────────────────────│           │
    │                                                                       │           │
    │─FIRE_BTN──►│             │              │              │              │           │
    │            │─fire───────────────────────────────────────────────────fire────►    │
    │            │             │              │              │              │─solenoid─►│
    │            │             │              │              │              │◄──ack─────│
    │◄──────────────────────────"FIRING"────────────────────────────────────│           │
    │                                                                       │           │
```

### 14.2 Emergency Stop Sequence

```
 E-STOP_HW    PLC42      GimbalCtrl    WeaponCtrl    CameraCtrl    OsdCtrl
     │          │             │             │             │            │
     │─ACTIVE──►│             │             │             │            │
     │          │─eStopSig───►│             │             │            │
     │          │             │─HALT_AZ────►│             │            │
     │          │             │─HALT_EL────►│             │            │
     │          │             │             │             │            │
     │          │─eStopSig───────────────►│             │            │
     │          │             │             │─safeWeapon─►│            │
     │          │             │             │             │            │
     │          │─eStopSig──────────────────────────────────►│         │
     │          │             │             │             │─stopTrack─►│
     │          │             │             │             │            │
     │          │─eStopSig─────────────────────────────────────────────►│
     │          │             │             │             │            │─showESTOP─►
     │          │             │             │             │            │
     ╔══════════╧═════════════╧═════════════╧═════════════╧════════════╧══════════╗
     ║  SYSTEM IN EMERGENCY STOP STATE - ALL MOTION HALTED - WEAPON SAFE         ║
     ╚════════════════════════════════════════════════════════════════════════════╝
     │          │             │             │             │            │
     │─RELEASE─►│             │             │             │            │
     │          │─clearSig───►│             │             │            │
     │          │             │─resumeOp───►│             │            │
     │          │             │             │             │            │─clearOSD──►
     │          │             │             │             │            │
```

### 14.3 Ammo Feed Cycle Sequence

```
 Operator    Joystick    WeaponCtrl    Actuator    OsdCtrl    SystemState
    │           │            │            │           │            │
    │─FEED_BTN─►│            │            │           │            │
    │           │─feedCmd───►│            │           │            │
    │           │            │─extend────►│           │            │
    │           │            │            │           │            │
    │           │            │─setState(Extending)────────────────►│
    │           │            │            │           │◄─updateOSD─│
    │◄──────────────────────────"FEED..."─────────────│            │
    │           │            │            │           │            │
    │           │            │◄─pos=45mm──│           │            │
    │           │            │─setState(Extended)─────────────────►│
    │           │            │            │           │◄─updateOSD─│
    │◄──────────────────────────"HOLD"────────────────│            │
    │           │            │            │           │            │
    │─(release)►│            │            │           │            │
    │           │─releaseCmd►│            │           │            │
    │           │            │─retract───►│           │            │
    │           │            │─setState(Retracting)───────────────►│
    │           │            │            │           │◄─updateOSD─│
    │◄──────────────────────────"RETRACTING..."───────│            │
    │           │            │            │           │            │
    │           │            │◄─pos=5mm───│           │            │
    │           │            │─setState(Idle)─────────────────────►│
    │           │            │─setAmmoLoaded(true)────────────────►│
    │           │            │            │           │◄─updateOSD─│
    │◄──────────────────────────"LOADED"──────────────│            │
    │           │            │            │           │            │
```

---

## 15. Protocol Specifications

### 15.1 Servo Driver Modbus RTU

**Transport:** RS-485, 230400 baud, 8N1
**Slave ID:** Az=2, El=1 (configurable)
**Poll Rate:** 50Hz (20ms)

| Register | Address | Type | Size | Description |
|----------|---------|------|------|-------------|
| Position | 0x00CC (204) | Input | 2 regs (32-bit) | Current position in steps |
| Motor Temp | 0x00F8 (248) | Input | 2 regs | Motor temperature °C×10 |
| Driver Temp | 0x00FA (250) | Input | 2 regs | Driver temperature °C×10 |
| Alarm Status | 0x00AC (172) | Input | 20 regs | Active alarm codes |
| Alarm History | 0x0082 (130) | Input | 20 regs | Stored alarm history |
| Op Type | 0x005A (90) | Holding | 2 regs | Operation type (16=velocity) |
| Op Speed | 0x005E (94) | Holding | 2 regs (signed) | Speed ±4,000,000 Hz |
| Op Accel | 0x0060 (96) | Holding | 2 regs | Acceleration rate Hz/s |
| Op Trigger | 0x0066 (102) | Holding | 2 regs | Command trigger (-4=update) |
| Alarm Reset | 0x0184 (388) | Holding | 1 reg | Write 1 to reset alarms |

**Velocity Command Sequence:**
1. Write Op Speed (0x005E) = target velocity in Hz
2. Write Op Trigger (0x0066) = 0xFFFFFFC (-4 = update speed)

### 15.2 PLC21 Modbus RTU (Weapon Control Panel)

**Transport:** RS-485, 115200 baud, 8E1
**Slave ID:** 31
**Poll Rate:** 20Hz (50ms)

| Register | Address | Type | Bit | Description |
|----------|---------|------|-----|-------------|
| DI0 | 0x0000 | Discrete | 0 | Station Enabled |
| DI1 | 0x0000 | Discrete | 1 | System Charged |
| DI2 | 0x0000 | Discrete | 2 | Weapon Armed |
| DI3 | 0x0000 | Discrete | 3 | Fire Mode Single |
| DI4 | 0x0000 | Discrete | 4 | Fire Mode Burst |
| DI5 | 0x0000 | Discrete | 5 | Emergency Stop |
| DI6 | 0x0000 | Discrete | 6 | Menu Up Button |
| DI7 | 0x0000 | Discrete | 7 | Menu Down Button |
| DI8 | 0x0000 | Discrete | 8 | Menu Val Button |
| DI9 | 0x0000 | Discrete | 9 | Fire Rate Full |
| DI10 | 0x0000 | Discrete | 10 | Ammunition Low |
| DI11 | 0x0000 | Discrete | 11 | Cocklock Depressed |
| DI12 | 0x0000 | Discrete | 12 | Cocklock Released |

### 15.3 PLC42 Modbus RTU (Gimbal Control)

**Transport:** RS-485, 115200 baud, 8E1
**Slave ID:** 31
**Poll Rate:** 20Hz (50ms)

| Register | Address | Type | Description |
|----------|---------|------|-------------|
| DI0-6 | 0x0000 | Discrete | Various limit switches |
| HR0 | 0x0000 | Holding | Solenoid mode (1=Single,2=Burst,3=Cont) |
| HR1 | 0x0001 | Holding | Fire rate (0=Reduced,1=Full) |
| HR2 | 0x0002 | Holding | Gimbal mode (0=Manual,1=Stop,3=Home,4=Free) |
| HR8 | 0x0008 | Holding | Solenoid state (0=OFF,1=ON) |
| HR9 | 0x0009 | Holding | Alarm reset (0=Normal,1=Reset) |

### 15.4 IMU 3DM-GX3 Protocol

**Transport:** RS-232, 115200 baud, 8N1
**Rate:** 100Hz (internal decimation from higher rate)

**Initialization Sequence:**
1. Send 0xCD (stop streaming)
2. Wait 10s (gyro bias capture - system stationary)
3. Send 0xCE (set data format: Euler + Gyro)
4. Send 0xCB (continuous mode at configured rate)

**Data Packet (26 bytes):**
```
[0xCB] [Roll_H] [Roll_L] [Pitch_H] [Pitch_L] [Yaw_H] [Yaw_L]
       [GyroX_H] [GyroX_L] [GyroY_H] [GyroY_L] [GyroZ_H] [GyroZ_L]
       [Checksum_H] [Checksum_L]

Scaling: Angles = raw × (360.0 / 65536.0) degrees
         Gyro = raw × (360.0 / 65536.0) degrees/second
```

### 15.5 LRF Protocol

**Transport:** RS-232, 115200 baud, 8N1
**Mode:** Command/Response

**Fire Command:** `0x5A 0x04 0x01 0x5F` (single shot)
**Continuous:** `0x5A 0x04 0x02 0x60` (start) / `0x5A 0x04 0x03 0x61` (stop)

**Response Format:**
```
[0x59] [0x59] [Dist_L] [Dist_H] [Strength_L] [Strength_H] [Temp] [Checksum]

Range = (Dist_H << 8 | Dist_L) centimeters
Valid if Strength > 100
```

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Dec 2024 | Engineering Team | Initial release |
| 2.0 | Dec 2024 | Engineering Team | Added timing budgets, state transition tables, FMEA safety analysis, error recovery procedures, sequence diagrams, protocol specifications |

---

## Appendix C: Requirements Traceability Matrix (Partial)

| Req ID | Requirement | Implementation | Verification |
|--------|-------------|----------------|--------------|
| REQ-SAF-001 | Fire shall require dead-man switch | `WeaponController::canFire()` checks `deadManSwitchActive` | Test TC-SAF-001 |
| REQ-SAF-002 | E-stop shall halt all motion <100ms | `GimbalController::emergencyStop()` via Qt::DirectConnection | Test TC-SAF-002 |
| REQ-SAF-003 | No-Fire zones shall inhibit fire | `SystemStateModel::isReticleInNoFireZone` check | Test TC-SAF-003 |
| REQ-PER-001 | Joystick to servo latency <50ms | Measured 26ms typical (Section 10.2) | Test TC-PER-001 |
| REQ-PER-002 | Tracking loop latency <50ms | Measured 40ms typical (Section 10.2) | Test TC-PER-002 |
| REQ-TRK-001 | Track lock confidence >0.25 | `TrackingMotionMode::m_targetValid` threshold | Test TC-TRK-001 |
| REQ-TRK-002 | Coast timeout 3 seconds | `CameraVideoStreamDevice::COAST_TIMEOUT_MS` | Test TC-TRK-002 |
| REQ-BAL-001 | Ballistic drop auto-apply on range | `WeaponController::updateBallisticDrop()` | Test TC-BAL-001 |
| REQ-BAL-002 | Motion lead requires LAC toggle | `leadAngleCompensationActive` guard | Test TC-BAL-002 |

---

*This document is classified CONFIDENTIAL and is intended for authorized personnel only.*

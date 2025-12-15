# Software Design Document (SDD)
## El-7aress Remote Controlled Weapon Station (RCWS)

**Document Version:** 1.0
**Date:** December 2024
**Classification:** CONFIDENTIAL - RESTRICTED DISTRIBUTION

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

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Dec 2024 | Engineering Team | Initial release |

---

*This document is classified CONFIDENTIAL and is intended for authorized personnel only.*

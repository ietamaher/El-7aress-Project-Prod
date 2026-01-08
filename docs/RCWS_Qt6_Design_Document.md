# Remote Controlled Weapon System (RCWS) Design Document

**Document Version:** 1.0
**Classification:** Design Description
**Application:** El 7arress RCWS Qt6/QML Application
**Framework:** Qt6 with QML User Interface

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [High-Level Architecture](#2-high-level-architecture)
3. [Subsystem Descriptions](#3-subsystem-descriptions)
4. [Operational Sequences](#4-operational-sequences)
5. [State Management Model](#5-state-management-model)
6. [Explicit Non-Responsibilities](#6-explicit-non-responsibilities)

---

## 1. System Overview

### 1.1 Purpose

This application provides the command and control software for a Remote Controlled Weapon System (RCWS). It enables an operator to remotely observe, aim, and engage targets while remaining protected within an armored vehicle. The system integrates gimbal motion control, video surveillance, target tracking, fire control computation, and comprehensive safety interlocking into a unified operator interface.

### 1.2 Major Subsystems

The application is organized into the following major functional domains:

| Subsystem | Responsibility |
|-----------|----------------|
| **Safety Subsystem** | Central safety authority for all lethal and motion operations |
| **Hardware Abstraction Layer** | Device communication, protocol parsing, and data transformation |
| **Motion Control Subsystem** | Gimbal positioning, stabilization, and motion mode coordination |
| **Weapon Control Subsystem** | Fire control, charging sequences, and ballistic computation |
| **Vision Subsystem** | Video acquisition, tracking, and object detection |
| **Operator Interface** | Video display, on-screen display overlay, and menu navigation |
| **Zone Management** | Spatial constraints including no-fire and no-traverse zones |

### 1.3 Safety Philosophy

The system implements a defense-in-depth safety architecture based on established military weapon system standards:

**Fail-Safe Default Behavior**
- All safety queries default to **DENY** when system state is unavailable
- Communication loss results in immediate motion halt and fire inhibit
- Unknown or corrupted state data is treated as unsafe

**Hierarchical Safety Authority**
- A single, centralized safety authority governs all lethal and motion decisions
- Safety checks cannot be bypassed by any controller or motion mode
- All motion commands pass through a template method pattern that enforces safety queries

**Interlocking Requirements**
- Emergency stop takes absolute precedence over all operations
- Station enable acts as a master inhibit for all powered functions
- Dead man switch is required for manual motion and firing operations
- Weapon arm state separates preparation activities from engagement readiness
- Zone enforcement provides spatial constraints independent of operator commands

**Audit Trail**
- All safety decisions are logged with timestamps for certification traceability
- Denial reasons are captured for post-incident analysis
- Rate-limited logging prevents log flooding while ensuring state changes are recorded

---

## 2. High-Level Architecture

### 2.1 Layered Architecture Model

The system follows a strict three-layer architecture pattern, separating concerns between communication, data management, and control logic:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PRESENTATION LAYER                           │
│    QML Views ─── ViewModels ─── QML Controllers                     │
├─────────────────────────────────────────────────────────────────────┤
│                        APPLICATION LAYER                            │
│    Hardware Controllers ─── Motion Modes ─── Safety Interlock       │
├─────────────────────────────────────────────────────────────────────┤
│                     DOMAIN STATE LAYER                              │
│              System State Model (Single Source of Truth)            │
├─────────────────────────────────────────────────────────────────────┤
│                   HARDWARE ABSTRACTION LAYER                        │
│    Devices ─── Protocol Parsers ─── Transports                      │
├─────────────────────────────────────────────────────────────────────┤
│                        PHYSICAL LAYER                               │
│    Serial Ports ─── Modbus RTU ─── Video Capture                    │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Control Flow Between Subsystems

**Upward Data Flow (Sensors to State)**
1. Physical devices transmit data over serial or Modbus interfaces
2. Transport layer handles I/O operations and byte buffering
3. Protocol parsers extract structured messages from byte streams
4. Device layer validates data and emits typed signals
5. Data models aggregate device outputs into domain structures
6. System state model receives updates and notifies observers

**Downward Command Flow (Control to Actuators)**
1. Operator input or automatic control generates command intent
2. Controllers validate commands against safety interlock
3. Approved commands are transformed to device-specific formats
4. Protocol encoders build properly framed messages
5. Transport layer transmits bytes to physical devices

### 2.3 State Ownership Model

**Single Source of Truth**

All mutable system state resides in a centralized domain model. This model maintains:

- Gimbal position and velocity
- Operator panel inputs (buttons, switches, joystick)
- Sensor readings (IMU, LRF, environmental)
- Camera state and tracking status
- Safety status (emergency stop, interlocks, zones)
- Operational mode and arming state
- Ballistic correction parameters

**Observer Pattern for State Distribution**

Components requiring state access connect to change signals emitted by the domain model. This ensures:

- No component holds stale copies of shared state
- State changes propagate deterministically
- Write conflicts are impossible (single writer per partition)

### 2.4 Safety Authority Hierarchy

```
                    ┌──────────────────────┐
                    │   EMERGENCY STOP     │ ◄── Highest Priority
                    │   (Hardware Input)   │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │   STATION ENABLE     │
                    │   (Master Inhibit)   │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │  DEAD MAN SWITCH     │
                    │  (Operator Presence) │
                    └──────────┬───────────┘
                               │
          ┌────────────────────┼────────────────────┐
          │                    │                    │
┌─────────▼─────────┐ ┌────────▼────────┐ ┌────────▼────────┐
│   ZONE CHECKS     │ │   ARM STATE     │ │  MODE CHECKS    │
│ (Spatial Limits)  │ │ (Weapon Ready)  │ │ (Engagement)    │
└───────────────────┘ └─────────────────┘ └─────────────────┘
```

The safety interlock evaluates conditions in priority order. Any denial at a higher level prevents evaluation of lower-level conditions.

---

## 3. Subsystem Descriptions

### 3.1 Safety Subsystem

**Role and Responsibility**

The safety subsystem provides centralized authorization for all potentially hazardous operations. It is the single point of authority for determining whether firing, charging, or motion commands may proceed.

**Inputs**
- System state model (emergency stop, station enable, dead man switch, arm state)
- Zone enforcement results (no-fire zone, no-traverse zone status)
- Operational mode
- Charging state machine status

**Outputs**
- Boolean permission decisions with denial reasons
- Audit log entries for certification compliance

**Owned State**
- Last denial reason per operation type (for change detection)
- Audit log timestamps (for rate limiting)

**Interaction with Other Subsystems**
- Motion modes query safety before every update cycle
- Weapon controller queries safety before fire and charge operations
- Zone enforcement service updates zone status that safety reads

**Safety Considerations**
- Mutex-protected for thread safety
- Fail-safe behavior: returns DENY when state model unavailable
- Priority ordering ensures emergency stop is always evaluated first

---

### 3.2 Hardware Abstraction Layer

**Role and Responsibility**

This subsystem abstracts physical device communication behind consistent interfaces, enabling device-agnostic control logic and supporting device replacement without application changes.

**Three-Layer Internal Structure**

| Layer | Responsibility |
|-------|----------------|
| **Transport** | Physical I/O (serial ports, Modbus masters) |
| **Protocol Parser** | Message framing, checksum validation, field extraction |
| **Device** | Business logic, command generation, status interpretation |

**Device Categories**

*Imaging Devices*
- Day camera control (Pelco-D protocol)
- Thermal camera control (vendor-specific protocol)
- Video stream processors (GStreamer pipelines)

*Sensor Devices*
- Inertial measurement unit (3DM-GX3 binary protocol)
- Laser range finder (serial protocol)
- Radar (serial protocol)

*Operator Interface Devices*
- Joystick (USB HID via SDL)
- Operator panel (Modbus RTU)

*Actuation Devices*
- Gimbal servo drivers (Modbus RTU to Oriental Motor AZD-KX)
- Cocking actuator (serial protocol)
- System PLC (Modbus RTU)

**Inputs**
- Physical bytes from serial ports and Modbus responses
- Configuration from JSON files

**Outputs**
- Typed data structures representing device state
- Command bytes transmitted to physical devices

**Owned State**
- Connection status per device
- Device-specific cached state (position feedback, sensor readings)

**Interaction with Other Subsystems**
- Controllers receive device references during initialization
- Data models subscribe to device signals for state aggregation
- Hardware manager orchestrates creation and lifecycle

**Safety Considerations**
- Connection loss detection with configurable timeouts
- Graceful degradation when non-critical devices fail
- Thread isolation for high-frequency servo communication

---

### 3.3 Motion Control Subsystem

**Role and Responsibility**

This subsystem coordinates gimbal motion across multiple operating modes, translating high-level intent (manual joystick, automatic tracking, pattern scanning) into servo velocity commands while maintaining platform stabilization.

**Motion Modes**

| Mode | Behavior |
|------|----------|
| **Manual** | Direct joystick velocity control with rate limiting |
| **Auto Track** | PID-based tracking with lead angle compensation |
| **Auto Sector Scan** | Autonomous azimuth sweeping between waypoints |
| **TRP Scan** | Target Reference Point patrol sequence |
| **Radar Slew** | Automatic slewing to radar-designated bearings |

**Strategy Pattern Architecture**

Motion modes are implemented as interchangeable strategy objects. The gimbal controller delegates motion computation to the currently active mode while retaining responsibility for:

- Mode switching
- Safety enforcement (via template method)
- Servo command transmission
- Alarm monitoring

**Homing Sequence**

A dedicated state machine manages the home position acquisition sequence:

1. Operator or system requests homing
2. Home command transmitted to motor controllers
3. Both axes complete return-to-origin
4. Home-end signals received from motor controllers
5. Sequence completes and previous mode restores

Timeout supervision ensures the system does not hang waiting for unresponsive hardware.

**Inputs**
- System state (joystick axes, tracking target, waypoints)
- IMU data (platform attitude for stabilization)
- Servo feedback (actual position and velocity)

**Outputs**
- Velocity commands to azimuth and elevation servos
- Stabilization corrections compensating for platform motion

**Owned State**
- Current motion mode type
- Mode-specific internal state (PID integrators, scan phase)
- Homing sequence state

**Interaction with Other Subsystems**
- Safety interlock queried before every motion update
- Camera controller provides tracking error for auto-track mode
- Zone enforcement may limit motion in restricted areas

**Safety Considerations**
- Template method pattern guarantees safety checks cannot be skipped
- Elevation hardware limits monitored continuously
- Emergency stop immediately halts all motion

---

### 3.4 Weapon Control Subsystem

**Role and Responsibility**

This subsystem manages weapon operations including arming, firing authorization, charging sequences, and fire control computation. It ensures that firing only occurs when all safety conditions are satisfied.

**Charging Cycle State Machine**

The weapon charging sequence (cocking actuator cycle) follows a deterministic state machine:

```
     Idle ──► Extending ──► Extended ──► Retracting ──► Lockout ──► Idle
               │                           │
               ▼                           ▼
           [Timeout]                   [Timeout]
               │                           │
               └────────► Fault ◄──────────┘
                            │
                            ▼
                     [Operator Reset]
                            │
                            ▼
                       SafeRetract ──► Idle
```

Key characteristics:
- Cycle is non-interruptible once started (button release ignored mid-cycle)
- Post-charge lockout period prevents immediate re-charging
- Jam detection triggers automatic backoff recovery
- Timeout supervision prevents indefinite stall

**Fire Control Solution**

The fire control computer calculates aiming corrections that account for:

| Factor | Source | Application |
|--------|--------|-------------|
| Ballistic drop | Range from LRF | Auto-applied when range valid |
| Crosswind deflection | Windage parameters | Applied via ballistics tables |
| Motion lead | Target angular rate | Applied only when LAC active |
| Environmental corrections | Temperature, altitude | Applied via density altitude factor |

Fire control uses lookup table interpolation rather than real-time physics simulation, providing deterministic computation times suitable for hard real-time operation.

**Inputs**
- Safety interlock authorization
- Trigger inputs from operator panel
- LRF range and target tracking data
- Environmental sensor readings

**Outputs**
- Fire solenoid commands
- Cocking actuator position commands
- Computed reticle offsets for display

**Owned State**
- Charging state machine state
- Fire control computation results
- Weapon arm status

**Interaction with Other Subsystems**
- Safety interlock queried before all operations
- Motion control provides gimbal rates for lead computation
- Camera controller provides tracking confidence

**Safety Considerations**
- Firing requires multiple simultaneous conditions (station enable, armed, authorized, engagement mode, not in no-fire zone, dead man held)
- Charging cycle cannot be interrupted to prevent mechanical damage
- Fault states require explicit operator acknowledgment

---

### 3.5 Vision Subsystem

**Role and Responsibility**

This subsystem acquires video from day and thermal cameras, processes frames for target tracking and object detection, and delivers imagery to the operator display.

**Video Pipeline**

```
Camera ──► GStreamer ──► Frame Buffer ──► [Tracking] ──► Image Provider ──► QML Display
              │                              │
              │                              ▼
              │                        [Detection]
              │                              │
              ▼                              ▼
         Video Output                  Target Data
```

**Camera Control**

The camera controller manages:
- Day/thermal camera switching
- Optical zoom control
- Digital zoom and LUT selection (thermal)
- Focus control (auto and manual)
- Flat field correction (thermal)

**Target Tracking**

Video processors implement correlation-based tracking:
- Operator designates target via reticle position
- Tracker maintains lock across frames
- Tracking error published as angular offset from reticle center
- Lost track detection after configurable timeout

**Object Detection**

Neural network inference runs in parallel with tracking:
- YOLO-based detection identifies objects of interest
- Detections displayed as bounding boxes on OSD
- Detection confidence thresholds configurable

**Inputs**
- Raw video frames from camera hardware
- Tracking enable/disable commands
- Target designation coordinates

**Outputs**
- Processed video frames for display
- Tracking error (angular offset)
- Detection bounding boxes

**Owned State**
- Active camera selection
- Tracker state (template, search region)
- Detection model weights

**Interaction with Other Subsystems**
- Motion control uses tracking error for auto-track mode
- Fire control uses tracking confidence for engagement authorization
- OSD overlay renders tracking box and detection results

**Safety Considerations**
- Tracking loss clearly indicated to operator
- Detection is advisory only—does not authorize engagement

---

### 3.6 Operator Interface Subsystem

**Role and Responsibility**

This subsystem presents situational awareness information and system status to the operator while accepting commands through physical controls and on-screen menus.

**Display Architecture**

The display follows a layered composition model:

| Layer | Content |
|-------|---------|
| Video Background | Live camera feed |
| OSD Overlay | Reticle, azimuth indicator, elevation scale, status readouts |
| Menu Overlay | Configuration menus when active |
| Dialog Overlay | Critical confirmations (shutdown, emergency) |

**On-Screen Display Elements**

- Crosshair reticle with configurable style and color
- Computed Continuously Computed Impact Point (CCIP) pipper
- Azimuth compass rose with heading indication
- Elevation scale with current angle
- Range readout from LRF
- System status indicators (armed, tracking, zone status)
- Motion mode indicator

**Menu System**

A three-button navigation model (Up, Down, Menu/Validate) provides access to:
- Reticle customization
- Color theme selection
- Zeroing procedure
- Windage parameters
- Environmental parameters
- Brightness adjustment
- Home position presets
- Zone definition
- System shutdown

**Inputs**
- Physical panel buttons (via PLC)
- System state model (for status display)

**Outputs**
- Menu navigation commands to controllers
- OSD element visibility and content

**Owned State**
- Current menu hierarchy position
- Parameter editing state

**Interaction with Other Subsystems**
- Subscribes to system state for display updates
- Drives menu controllers for configuration changes
- Receives video frames from vision subsystem

**Safety Considerations**
- Shutdown requires explicit confirmation
- Armed state prominently displayed
- Zone violations highlighted

---

### 3.7 Zone Management Subsystem

**Role and Responsibility**

This subsystem defines and enforces spatial constraints that restrict weapon operation or gimbal motion within specified regions.

**Zone Types**

| Type | Effect |
|------|--------|
| **No-Fire Zone** | Prohibits weapon discharge |
| **No-Traverse Zone** | Prohibits gimbal motion into area |
| **Area Zone** | Defines bounded region of interest |
| **Auto Sector Scan Zone** | Defines automatic scan boundaries |
| **Target Reference Point (TRP)** | Predefined engagement waypoint |

**Zone Enforcement**

The zone enforcement service continuously evaluates gimbal position against defined zones:

1. Current gimbal angles received from state model
2. Position tested against all active zone boundaries
3. Zone violation status published to state model
4. Safety interlock reads zone status during authorization queries

**Inputs**
- Zone definitions (from configuration files)
- Current gimbal position
- Zone enable/disable commands

**Outputs**
- Zone violation flags (no-fire, no-traverse)
- Zone approach warnings

**Owned State**
- Zone definition database
- Evaluation cache

**Interaction with Other Subsystems**
- Safety interlock uses zone status for firing authorization
- Motion modes may limit travel near zone boundaries
- OSD displays zone boundaries on azimuth indicator

**Safety Considerations**
- Zone checks are mandatory—cannot be disabled
- Zones persist across power cycles
- Zone editing requires explicit operator action

---

## 4. Operational Sequences

### 4.1 System Startup

**Phase 1: Hardware Initialization**
1. Application main creates system controller
2. Hardware manager instantiates transport layer components
3. Protocol parsers created and attached to transports
4. Device objects created with parser references
5. Data models created for each device category
6. System state model created as central repository

**Phase 2: Hardware Startup**
1. Transport connections opened (serial ports, Modbus masters)
2. Initial device queries establish baseline state
3. Data models connected to device signals
4. Device data begins flowing to system state model

**Phase 3: User Interface Initialization**
1. QML engine created
2. View models created and registered with QML context
3. QML controllers created with view model references
4. Main QML view loaded
5. OSD overlay begins rendering

**Phase 4: Control System Activation**
1. Controller registry creates safety interlock
2. Hardware controllers created with device and safety references
3. Motion mode library initialized
4. Controllers connected to system state signals
5. Periodic update timers started

**Phase 5: Ready for Operation**
1. System performs self-test verification
2. Operator station enable allows powered operations
3. Gimbal homing sequence available
4. System enters Surveillance operational mode

### 4.2 Normal Operation

**Surveillance Mode**
- Video displayed continuously
- Gimbal responds to joystick (when dead man held)
- Tracking and detection available
- Weapon remains safe (not armed)

**Engagement Mode**
- Operator arms weapon via panel switch
- Fire authorization requires all interlocks satisfied
- Trigger pull initiates fire if authorized
- Charging available between engagements

**Automatic Modes**
- Operator selects auto sector scan or TRP patrol
- System autonomously positions gimbal per defined patterns
- Operator may interrupt via joystick override
- Dead man requirement depends on mode

### 4.3 Mode Transitions

**Manual to Auto-Track**
1. Operator designates target with track button
2. Vision subsystem initiates tracking
3. Motion mode transitions to tracking mode
4. PID loop assumes gimbal control
5. Joystick provides nudge offset capability

**Auto-Track to Manual**
1. Operator releases track or presses again
2. Tracking disengaged
3. Motion mode returns to manual
4. Joystick resumes direct control

**Any Mode to Homing**
1. Operator presses home button
2. Current mode stored for restoration
3. Homing sequence state machine activates
4. Home commands sent to motor controllers
5. Both axes reach home position
6. Previous mode restored automatically

### 4.4 Emergency Stop Handling

**Activation**
1. Emergency stop input detected (hardware signal)
2. System state model updates emergency stop flag
3. All controllers receive state change notification
4. Motion immediately halted (zero velocity command)
5. Fire authorization revoked
6. Charging aborted if in progress
7. OSD displays emergency stop indication

**Recovery**
1. Emergency stop physically cleared
2. Hardware debounce delay expires
3. Emergency stop flag clears in state model
4. Controllers resume normal state monitoring
5. Manual operation available
6. Weapon requires re-arming

### 4.5 Recovery and Return to Service

**After Emergency Stop**
- Verify physical emergency stop released
- Confirm station enable active
- Re-establish dead man contact
- Verify gimbal position via homing if uncertain
- Re-arm weapon if engagement required

**After Communication Loss**
- Automatic reconnection attempted for affected devices
- Degraded operation may continue with available devices
- Critical device loss (servo, PLC) requires operator acknowledgment
- Full capability restored when communication recovers

**After Fault Conditions**
- Charging faults require explicit operator reset
- Servo alarms require alarm clear command
- Actuator jams require backoff and reset sequence
- System maintains safe state until faults cleared

---

## 5. State Management Model

### 5.1 Domain State Partitioning

System state is partitioned into logical domains with clear ownership:

| Partition | Owner | Contents |
|-----------|-------|----------|
| Gimbal State | Motion Controller | Position, velocity, motion mode, homing state |
| Weapon State | Weapon Controller | Arm status, charging state, fire control results |
| Operator Input | Hardware Manager | Panel buttons, joystick axes, dead man switch |
| Sensor Data | Hardware Manager | IMU readings, LRF range, environmental |
| Camera State | Camera Controller | Active camera, zoom level, tracking status |
| Safety State | System (Read-Only) | Emergency stop, zone violations |
| Configuration | Persistence Layer | Zeroing, windage, zone definitions |

### 5.2 Write Ownership Rules

**Exclusive Write Access**
- Each state partition has exactly one writer
- Writers update state through the system state model interface
- Model validates updates and emits change signals

**Read Access**
- Any component may read state through model accessors
- State snapshots provide consistent reads across multiple fields
- Observers receive change notifications for reactive updates

**Cross-Partition Coordination**
- Controllers read from partitions they don't own
- Safety interlock has read access to all safety-relevant state
- State model ensures atomic updates within partitions

### 5.3 Change Propagation Strategy

**Signal-Based Notification**

The system state model emits a unified change signal whenever state updates. Observers connect to this signal and:

1. Receive complete state snapshot
2. Compare with cached previous state
3. React only to relevant changes
4. Update internal state as needed

**Periodic Update Cycle**

High-frequency subsystems operate on timer-driven update cycles:

- Motion control: 20 Hz (50 ms interval)
- Video display: 30 Hz (33 ms interval)
- Sensor polling: 100 Hz (10 ms interval)
- OSD update: 30 Hz (33 ms interval)

**Event-Driven Updates**

Discrete events trigger immediate processing:

- Button press/release edges
- Tracking target acquisition/loss
- Emergency stop activation
- Zone boundary crossing

---

## 6. Explicit Non-Responsibilities

### 6.1 What This Application Does NOT Handle

**Network Communication**
- No remote telemetry transmission
- No external command reception
- No video streaming to remote displays
- All operation is local to the operator station

**Ammunition Management**
- Does not track round count
- Does not select ammunition type
- Does not manage ammunition feed
- Physical ammunition is operator responsibility

**Maintenance Functions**
- No built-in diagnostics beyond fault indication
- No automatic calibration routines
- No firmware update capability
- Maintenance requires specialized tools

**External System Integration**
- No battlefield management system interface
- No GPS integration for position reporting
- No vehicle CAN bus integration
- No networked fire coordination

**Recording and Playback**
- No video recording to storage
- No event logging to removable media
- No mission replay capability
- Operational data is not persisted

**User Authentication**
- No login or operator identification
- No role-based access control
- No audit logging of operator identity
- Physical access control is external

### 6.2 Boundary Conditions

**Physical Safety**
- Application relies on hardware interlocks for ultimate safety
- Software safety is defense-in-depth, not sole protection
- Physical emergency stop must function independent of software
- Mechanical limits enforce ultimate travel boundaries

**Environmental Limits**
- Operating temperature determined by hardware specifications
- Vibration tolerance depends on mounting and hardware
- EMI susceptibility determined by enclosure shielding
- Application assumes hardware operates within specifications

**Operator Training**
- Application provides tool, not training
- Safe operation requires qualified operator
- Judgment calls remain with human operator
- Application enforces rules, not doctrine

---

## Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-08 | System Architecture | Initial release |

---

*This document describes the design of the RCWS application software. It is intended for design reviews, certification discussions, and new engineer onboarding. Implementation details are intentionally omitted to ensure the document remains valid as internal implementations evolve.*

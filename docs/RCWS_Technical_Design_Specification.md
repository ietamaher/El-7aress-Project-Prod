# RCWS Qt6/QML Application - Technical Design Specification (TDS)

**Document Version:** 1.1
**Date:** January 2026
**Classification:** UNCLASSIFIED // FOR OFFICIAL USE ONLY
**Prepared for:** Engineering Team - El Harress RCWS Project

---

## 1. Purpose and Scope

### 1.1 Document Purpose

This Technical Design Specification (TDS) provides the technical realization of the architecture described in the Descriptive Design Document (RCWS_Qt6_Design_Document.md). It focuses on state machines, timing, execution model, and safety enforcement. It does not replace the descriptive architecture and does not document source code.

**Intended Audience:** Software engineers, system integrators, and verification/validation teams requiring detailed understanding of behavioral mechanisms for implementation, debugging, and certification activities.

### 1.2 Relationship to Descriptive Design Document

| Document | Focus | Level of Detail |
|----------|-------|-----------------|
| Descriptive Design (SDD) | System structure, component responsibilities, interaction patterns | Architectural |
| Technical Design (TDS) | State behavior, timing constraints, execution mechanics | Behavioral |

The SDD answers "What components exist and how do they interact?"
The TDS answers "How do those components behave at runtime?"

### 1.3 Scope Statement

This document covers:
- State machine behavior for critical subsystems
- Timing constraints and rate requirements
- Execution model and threading architecture
- Safety enforcement flows and decision logic
- Fault classification and recovery mechanisms
- Data integrity rules and consistency guarantees

---

## 2. Explicit Out-of-Scope Items

The following topics are intentionally excluded from this specification:

| Topic | Rationale |
|-------|-----------|
| UI layout and visual design | Covered in QML component specifications |
| Operator procedures | Covered in Operator Manual |
| Hardware electrical interfaces | Covered in Hardware ICD |
| Detailed algorithms (PID, stabilization) | Implementation detail, not behavioral |
| Performance optimization techniques | Implementation detail |
| Source code structure | Covered in code-level documentation |
| Configuration file formats | Covered in Configuration Guide |
| Test procedures | Covered in Verification Plan |

---

## 3. State Machines

State machines are described at the behavioral level. Internal implementation details are intentionally omitted. Each state machine is specified using state tables, transition diagrams, and trigger → action → next state notation.

### 3.1 Homing State Machine

The homing state machine manages the gimbal zeroing sequence, establishing a known reference position for accurate aiming.

#### 3.1.1 States

| State | Entry Condition | Behavior | Exit Condition |
|-------|-----------------|----------|----------------|
| Idle | System startup or homing complete | No homing activity; monitor for operator request | Operator presses home button |
| Requested | Home button rising edge detected | Store current motion mode; prepare HOME command | HOME command transmitted |
| InProgress | HOME command sent to drive controller | Monitor for HOME-END signals from both axes | Both HOME-END received OR timeout OR abort |
| Completed | Both axes report HOME-END | Clear homing flags; restore previous motion mode | Auto-transition to Idle (next cycle) |
| Failed | Timeout elapsed without HOME-END | Log failure cause; retain fault indication | Operator acknowledges or system reset |
| Aborted | Emergency stop or operator cancel | Halt homing sequence; log abort reason | Immediate transition to Idle |

#### 3.1.2 State Transition Table

| Current State | Trigger | Guard Condition | Action | Next State |
|---------------|---------|-----------------|--------|------------|
| Idle | Home button pressed | Station enabled | Store mode; send HOME | Requested |
| Requested | Command sent | — | Start timeout timer | InProgress |
| InProgress | HOME-END (Az) | — | Mark Az complete | InProgress |
| InProgress | HOME-END (El) | — | Mark El complete | InProgress |
| InProgress | Both axes complete | — | Stop timer; signal success | Completed |
| InProgress | Timeout | — | Log failure | Failed |
| InProgress | E-Stop activated | — | Abort sequence | Aborted |
| Completed | Next cycle | — | Restore motion mode | Idle |
| Failed | Reset | — | Clear fault | Idle |
| Aborted | — | — | Clear flags | Idle |

#### 3.1.3 State Diagram

```
                     ┌─────────────────────────────────────┐
                     │                                     │
                     v                                     │
    ┌──────┐   button   ┌───────────┐   command    ┌────────────┐
    │ Idle │ ─────────> │ Requested │ ──────────> │ InProgress │
    └──────┘   pressed  └───────────┘    sent     └────────────┘
       ^                                               │
       │                                    ┌──────────┼──────────┐
       │                                    │          │          │
       │                              HOME-END   Timeout     E-Stop
       │                             (both axes)    │          │
       │                                    v          v          v
       │                              ┌───────────┐ ┌────────┐ ┌─────────┐
       └──────────────────────────────│ Completed │ │ Failed │ │ Aborted │
             (auto-transition)        └───────────┘ └────────┘ └─────────┘
```

#### 3.1.4 Timing Constraints

| Parameter | Value | Description |
|-----------|-------|-------------|
| Homing timeout | 30 seconds | Maximum duration for complete homing sequence |
| State clear delay | 1 control cycle | Completed state auto-clears to Idle |

---

### 3.2 Charging State Machine

The charging (cocking) state machine manages the weapon charging cycle, including multi-cycle support for closed-bolt weapons and jam detection with recovery.

#### 3.2.1 States

| State | Entry Condition | Behavior | Exit Condition |
|-------|-----------------|----------|----------------|
| Idle | Actuator retracted; no active cycle | Monitor for charge request | Charge button pressed |
| Extending | Charge initiated | Command actuator extension; monitor position | Position reached OR jam OR timeout |
| Extended | Full extension reached | Hold position (continuous hold mode) | Button released |
| Retracting | Extension complete or button released | Command actuator retraction | Position reached OR jam OR timeout |
| Lockout | All cycles completed | Enforce post-charge delay | Lockout timer expires |
| Fault | Timeout or error | Await operator acknowledgment | Operator reset |
| JamDetected | High torque + stall detected | Execute backoff sequence | Backoff complete |
| SafeRetract | Fault recovery initiated | Retract to home position | Position reached |

#### 3.2.2 State Transition Table

| Current State | Trigger | Guard Condition | Action | Next State |
|---------------|---------|-----------------|--------|------------|
| Idle | Charge button | Safety permits | Start extension; start timer | Extending |
| Extending | Position reached | Button held | Hold position | Extended |
| Extending | Position reached | Button released | Increment cycle; retract | Retracting |
| Extending | Jam detected | — | Stop actuator; backoff | JamDetected |
| Extending | Timeout | — | Signal fault | Fault |
| Extended | Button released | — | Increment cycle; retract | Retracting |
| Retracting | Position reached | More cycles needed | Start extension | Extending |
| Retracting | Position reached | All cycles done | Start lockout | Lockout |
| Retracting | Jam detected | — | Stop actuator; backoff | JamDetected |
| Retracting | Timeout | — | Signal fault | Fault |
| Lockout | Timer expires | — | Clear lockout flag | Idle |
| JamDetected | Backoff complete | — | Await operator | Fault |
| Fault | Operator reset | — | Initiate safe retract | SafeRetract |
| SafeRetract | Position reached | — | Clear fault | Idle |

#### 3.2.3 State Diagram

```
                                    Short Press Path
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
          reached)                                                      │
                                      Continuous Hold Path ─────────────┘
```

#### 3.2.4 Timing Constraints

| Parameter | Value | Description |
|-----------|-------|-------------|
| Extend/retract timeout | 5 seconds | Watchdog for actuator movement |
| Post-charge lockout | 4 seconds | CROWS M153 mandatory delay |
| Backoff stabilization delay | 200 ms | Motor stabilization before reverse |
| Startup retraction timeout | 2.5 seconds | Faster timeout for startup |

#### 3.2.5 Jam Detection Criteria

| Criterion | Threshold | Description |
|-----------|-----------|-------------|
| Torque threshold | 80% | High torque indication |
| Position stall | < 0.5 mm movement | No progress detected |
| Confirmation samples | 3 consecutive | Sustained condition required |

#### 3.2.6 Multi-Cycle Requirements

| Weapon Type | Required Cycles | Rationale |
|-------------|-----------------|-----------|
| M2HB (.50 cal) | 2 | Closed bolt: Cycle 1 picks round, Cycle 2 chambers |
| M240B, M249, MK19 | 1 | Open bolt weapons |

---

### 3.3 Motion Mode Transition State Machine

The motion mode transition state machine manages smooth transitions between gimbal control modes with lead angle compensation blending.

#### 3.3.1 States (Tracking with LAC)

| State | Entry Condition | Behavior | Exit Condition |
|-------|-----------------|----------|----------------|
| TRACK | Normal tracking active | PID control; target centered | LAC armed AND fire trigger |
| FIRE_LEAD | LAC engaged during firing | Open-loop lead injection; target drifts | Trigger released |
| RECENTER | Transitioning from lead | Blend lead out; target re-centering | Blend factor reaches zero |

#### 3.3.2 State Transition Table

| Current State | Trigger | Guard Condition | Action | Next State |
|---------------|---------|-----------------|--------|------------|
| TRACK | Fire trigger | LAC armed | Begin blend ramp-in | FIRE_LEAD |
| FIRE_LEAD | Trigger released | — | Begin blend ramp-out | RECENTER |
| RECENTER | Blend = 0 | — | Resume tracking | TRACK |

#### 3.3.3 Blend Timing

| Parameter | Value | Resulting Duration |
|-----------|-------|-------------------|
| Ramp-in rate | 5.0 per second | ~200 ms to full lead |
| Ramp-out rate | 3.0 per second | ~333 ms to centered |

#### 3.3.4 Rigid Cradle Constraint

**Critical Design Constraint:** Camera and gun are mechanically locked (rigid cradle). During lead injection (FIRE_LEAD state), the target intentionally drifts off-center because the gun must aim ahead of the target's current position. This apparent "tracking error" is correct ballistic behavior.

---

### 3.4 Emergency Stop Lifecycle

The emergency stop lifecycle manages debounced state transitions and recovery timing for the physical E-Stop mechanism.

#### 3.4.1 States

| State | Entry Condition | Behavior | Exit Condition |
|-------|-----------------|----------|----------------|
| Inactive | E-Stop not pressed | Normal operation permitted | E-Stop button pressed |
| Debouncing | Button state change detected | Filter contact bounce | Debounce period elapsed OR state reverts |
| Active | Debounced activation confirmed | All motion/firing inhibited | E-Stop button released |
| Recovery | Debounced deactivation confirmed | Enforcing recovery delay | Recovery period elapsed |

#### 3.4.2 State Transition Table

| Current State | Trigger | Guard Condition | Action | Next State |
|---------------|---------|-----------------|--------|------------|
| Inactive | Button pressed | — | Start debounce timer | Debouncing |
| Debouncing | Debounce elapsed | State still changed | Log activation; inhibit all | Active |
| Debouncing | State reverted | — | Cancel debounce | Inactive |
| Active | Button released | — | Start debounce timer | Debouncing |
| Debouncing | Debounce elapsed | State changed to inactive | Start recovery timer | Recovery |
| Recovery | Timer elapsed | — | Log recovery complete | Inactive |

#### 3.4.3 Timing Constraints

| Parameter | Value | Description |
|-----------|-------|-------------|
| Debounce period | 50 ms | Contact bounce filtering |
| Recovery delay | 1000 ms | Safe period before normal operation |
| Event history size | 100 events | Audit trail buffer |

---

## 4. Timing and Rates

### 4.1 Control Loop Frequencies

| Control Loop | Frequency | Period | Purpose |
|--------------|-----------|--------|---------|
| Motion control (gimbal) | 20 Hz | 50 ms | Servo velocity commands |
| Safety monitoring | 20 Hz | 50 ms | Interlock evaluation |
| State change processing | Event-driven | — | Reactive to hardware |

### 4.2 Hardware Polling and Update Rates

| Device/Interface | Update Rate | Direction | Notes |
|------------------|-------------|-----------|-------|
| Servo drive feedback | 110 Hz | Read | Position, velocity, status |
| Servo velocity commands | 20 Hz | Write | Velocity mode commands |
| PLC I/O (panel) | 50 Hz | Read/Write | Switches, indicators |
| PLC I/O (gimbal) | 50 Hz | Read/Write | Limit switches, E-stop |
| IMU data | 100 Hz | Read | Attitude, angular rates |
| Joystick axes | Event-driven | Read | Position change events |
| LRF | On-demand | Read | Range measurement trigger |
| Video frames | 60 fps | Read | Camera image stream |

### 4.3 Safety Response Bounds

| Safety Event | Maximum Response Time | Enforcement |
|--------------|----------------------|-------------|
| E-Stop activation | < 50 ms | Debounce period |
| Fire inhibit (zone entry) | < 50 ms | Next control cycle |
| Motion halt (E-Stop) | < 100 ms | Control cycle + servo response |
| Charging abort | < 50 ms | Next control cycle |

### 4.4 Timeout Values

| Operation | Timeout | Action on Expiry |
|-----------|---------|------------------|
| Homing sequence | 30 seconds | Transition to Failed state |
| Actuator extend/retract | 5 seconds | Transition to Fault state |
| Post-charge lockout | 4 seconds | Return to Idle |
| E-Stop recovery | 1 second | Enable normal operation |
| Thread shutdown | 2 seconds | Force terminate |

### 4.5 Timing Assumptions and Limitations

**Assumptions:**

1. **Soft Real-Time:** The system operates as a soft real-time system within a Qt event loop. Timing guarantees are probabilistic, not deterministic.

2. **Event Loop Latency:** Qt's event loop introduces non-deterministic latency. Design assumes worst-case event processing delay of 50 ms under normal load.

3. **Modbus Latency:** Serial Modbus RTU communications have inherent latency (1-5 ms per transaction). Control algorithms account for this delay.

4. **Single-Core Assumption:** The main control logic assumes single-threaded execution. Thread safety is achieved through connection types, not locks.

**Limitations:**

1. **No Hard Real-Time Guarantees:** This system cannot guarantee microsecond-level timing precision. Safety-critical timing constraints are designed with margins.

2. **Jitter Tolerance:** Control loops tolerate ±20% timing jitter without stability impact. Jitter beyond this threshold triggers diagnostic logging.

3. **Video Latency:** End-to-end video latency (capture to display) is approximately 30-60 ms. Fire control calculations do not depend on video timing.

---

## 5. Execution Model

### 5.1 Threading Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        Main Thread (Qt Event Loop)                        │
│                                                                          │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  Event-Driven Components                                         │   │
│   │  • State Model (central data hub)                               │   │
│   │  • All Controllers (gimbal, weapon, camera, joystick)           │   │
│   │  • Safety Interlock (authorization authority)                   │   │
│   │  • Hardware Devices (Modbus-based)                              │   │
│   │  • QML Engine and UI                                            │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  Timer-Driven Components                                         │   │
│   │  • Motion Control Loop (20 Hz periodic timer)                   │   │
│   │  • Watchdog Timers (single-shot)                                │   │
│   └─────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          v                v                v
   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
   │ Day Video   │  │ Night Video │  │ Servo I/O   │
   │ Thread      │  │ Thread      │  │ Thread      │
   │             │  │             │  │             │
   │ • Capture   │  │ • Capture   │  │ • Async     │
   │ • Decode    │  │ • Decode    │  │   Modbus    │
   │ • Process   │  │ • Process   │  │             │
   └─────────────┘  └─────────────┘  └─────────────┘
```

### 5.2 Main Thread Responsibilities

The main thread executes all control logic for the following reasons:

1. **Modbus Constraint:** Qt's Modbus implementation requires all operations on a client to occur in the creating thread.

2. **State Consistency:** Single-threaded execution eliminates race conditions in state model updates.

3. **Safety Authority:** The safety interlock executes atomically with control decisions.

### 5.3 Timer-Driven vs Event-Driven Logic

| Component Type | Execution Model | Trigger |
|----------------|-----------------|---------|
| Motion control | Timer-driven | 50 ms periodic timer |
| State change handlers | Event-driven | Signal emission |
| Watchdogs | Timer-driven | Single-shot timers |
| Safety checks | Event-driven | State change + periodic |
| Video processing | Thread + signal | Frame ready signal |

### 5.4 Signal Connection Semantics

| Connection Scenario | Semantics | Rationale |
|--------------------|-----------|-----------|
| Same thread, timing-critical | Direct (synchronous) | No queue delay |
| Cross-thread | Queued (asynchronous) | Thread safety |
| State model to controllers | Direct | Latency-sensitive |
| Video to state model | Queued | Cross-thread |

### 5.5 Intentional Single-Threading Decisions

The following components are intentionally single-threaded:

1. **Safety Interlock:** All safety decisions execute atomically in the main thread to prevent race conditions between check and action.

2. **State Model Updates:** All state mutations occur in the main thread to ensure consistency between readers.

3. **Modbus Devices:** Qt Modbus requires thread affinity; all device I/O occurs in the main thread.

4. **Control Loops:** Control algorithms execute in the main thread to ensure deterministic ordering of read-compute-write cycles.

---

## 6. Safety Enforcement Traceability

### 6.1 Fire Authorization Flow

```
┌──────────────────────────────────────────────────────────────────────────┐
│                     FIRE AUTHORIZATION SEQUENCE                           │
└──────────────────────────────────────────────────────────────────────────┘

  Operator Fire Request
         │
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ E-Stop Inactive? │ ────────> │ DENY: Emergency stop active             │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Station Enabled? │ ────────> │ DENY: Station not enabled               │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Dead Man Held?   │ ────────> │ DENY: Dead man switch not held          │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Gun Armed?       │ ────────> │ DENY: Gun not armed                     │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Engagement Mode? │ ────────> │ DENY: Not in engagement mode            │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ System Authzd?   │ ────────> │ DENY: System not authorized             │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    YES    ┌─────────────────────────────────────────┐
  │ In No-Fire Zone? │ ────────> │ DENY: Position in no-fire zone          │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ NO
         v
  ┌──────────────────┐    YES    ┌─────────────────────────────────────────┐
  │ Charging Active? │ ────────> │ DENY: Charging cycle in progress        │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ NO
         v
  ┌──────────────────────────────────────────────────────────────────────┐
  │                    FIRE AUTHORIZED - Execute command                  │
  └──────────────────────────────────────────────────────────────────────┘
```

### 6.2 Motion Authorization Flow

```
  Motion Command Request
         │
         v
  ┌──────────────────┐    YES    ┌─────────────────────────────────────────┐
  │ E-Stop Active?   │ ────────> │ HALT: Stop all motion immediately       │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ NO
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Station Enabled? │ ────────> │ HALT: Motion not permitted              │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO     ┌─────────────────────────────────────────┐
  │ Dead Man Held?   │ ────────> │ HALT: Dead man required for mode        │
  │ (if required)    │           └─────────────────────────────────────────┘
  └──────────────────┘
         │ YES/N/A
         v
  ┌──────────────────┐    YES    ┌─────────────────────────────────────────┐
  │ In No-Traverse?  │ ────────> │ CLAMP: Limit velocity at boundary       │
  └──────────────────┘           └─────────────────────────────────────────┘
         │ NO
         v
  ┌──────────────────────────────────────────────────────────────────────┐
  │                 MOTION AUTHORIZED - Execute velocity command          │
  └──────────────────────────────────────────────────────────────────────┘
```

### 6.3 Emergency Stop Propagation

```
  E-Stop Button Pressed
         │
         v
  ┌───────────────────────────────────────────────────────────────────┐
  │ PHASE 1: IMMEDIATE INHIBIT (< 50 ms)                              │
  │                                                                   │
  │   • Set E-Stop flag in state model                               │
  │   • Signal emitted to all observers                              │
  └───────────────────────────────────────────────────────────────────┘
         │
         v
  ┌───────────────────────────────────────────────────────────────────┐
  │ PHASE 2: HALT COMMANDS (< 100 ms)                                 │
  │                                                                   │
  │   • Gimbal Controller: Send STOP to servos                       │
  │   • Weapon Controller: Disable fire solenoid                     │
  │   • Charging FSM: Abort any in-progress cycle                    │
  │   • Homing FSM: Abort if in progress                             │
  └───────────────────────────────────────────────────────────────────┘
         │
         v
  ┌───────────────────────────────────────────────────────────────────┐
  │ PHASE 3: SUSTAINED INHIBIT                                        │
  │                                                                   │
  │   • All authorization requests return DENY                       │
  │   • All motion commands blocked                                  │
  │   • Fire commands blocked                                        │
  │   • Charging requests blocked                                    │
  └───────────────────────────────────────────────────────────────────┘
         │
         v
  E-Stop Released → Recovery sequence (1 second delay)
```

### 6.4 Authorization Priority Order

Checks execute in strict priority order. Higher-priority failures short-circuit lower checks.

| Priority | Check | Denial Enumeration |
|----------|-------|-------------------|
| 1 | Emergency Stop | EmergencyStopActive |
| 2 | Station Enabled | StationNotEnabled |
| 3 | Dead Man Switch | DeadManSwitchNotHeld |
| 4 | Gun Armed | GunNotArmed |
| 5 | Operational Mode | NotInEngagementMode |
| 6 | System Authorization | SystemNotAuthorized |
| 7 | No-Fire Zone | InNoFireZone |
| 8 | Charging State | ChargingInProgress / ChargingFault |

### 6.5 Fail-Safe Default Behavior

When system state is unavailable or indeterminate:

| Query | Default Response | Rationale |
|-------|------------------|-----------|
| Can fire? | DENY | Safest assumption |
| Can move? | DENY | Safest assumption |
| Can charge? | DENY | Safest assumption |
| Can home? | DENY | Safest assumption |

### 6.6 Audit Trail Requirements

All safety decisions are logged with:
- Timestamp (ISO 8601 format)
- Decision (PERMIT / DENY)
- Denial reason (if denied)
- Requesting operation

---

## 7. Fault Handling and Recovery

### 7.1 Fault Classification

#### 7.1.1 Recoverable Faults

Faults that can be cleared through automatic retry or simple operator action:

| Fault | Detection | Automatic Recovery | Operator Action |
|-------|-----------|-------------------|-----------------|
| Communication timeout | No response within period | Retry (3 attempts) | Reconnect if retries fail |
| Position overshoot | Position exceeds tolerance | Re-command position | None required |
| Watchdog timeout | Timer expiry | Retry operation | Acknowledge if retry fails |

#### 7.1.2 Non-Recoverable Faults

Faults requiring system intervention or maintenance:

| Fault | Detection | System Response | Required Action |
|-------|-----------|-----------------|-----------------|
| Device disconnected | Transport failure | Inhibit device operations | Repair connection |
| Servo alarm | Alarm code from drive | Halt axis motion | Clear alarm; diagnose |
| Jam detected | Torque + stall | Auto-backoff; enter Fault | Physical inspection |

#### 7.1.3 Operator-Assisted Recovery

Faults requiring explicit operator acknowledgment or intervention:

| Fault | State Indication | Recovery Sequence |
|-------|------------------|-------------------|
| Charging fault | Fault state; OSD indicator | Press charge button to initiate safe retract |
| Homing failure | Failed state; OSD indicator | Press home button to retry |
| E-Stop recovery | Recovery indicator | Wait for recovery period; resume operations |
| Servo alarm | Alarm indicator | Clear alarms via menu; diagnose cause |

### 7.2 Timeout Watchdog Behavior

| Watchdog | Timeout | Expiry Action | Recovery |
|----------|---------|---------------|----------|
| Homing | 30 s | Transition to Failed | Operator retry |
| Charging | 5 s | Transition to Fault | Operator reset |
| E-Stop recovery | 1 s | Enable operation | Automatic |
| Thread shutdown | 2 s | Force terminate | System restart |

### 7.3 Jam Detection and Recovery Sequence

```
  Normal Operation
         │
         v
  ┌──────────────────┐    NO
  │ High Torque?     │ ────────> Continue normal operation
  └──────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO
  │ Stalled?         │ ────────> Continue (high load but moving)
  └──────────────────┘
         │ YES
         v
  ┌──────────────────┐    NO
  │ Confirmed?       │ ────────> Increment counter; continue monitoring
  │ (3 samples)      │
  └──────────────────┘
         │ YES
         v
  ┌───────────────────────────────────────────────────────────────────┐
  │ JAM CONFIRMED                                                      │
  │                                                                   │
  │   1. Immediate stop command                                       │
  │   2. Transition to JamDetected state                             │
  │   3. Wait 200 ms (motor stabilization)                           │
  │   4. Command backoff to home position                            │
  │   5. Wait for position reached                                   │
  │   6. Transition to Fault state                                   │
  │   7. Await operator acknowledgment                               │
  └───────────────────────────────────────────────────────────────────┘
```

### 7.4 Startup Recovery

On system startup:
1. Check actuator position
2. If extended beyond threshold: Initiate automatic retraction
3. Wait for retraction complete (with reduced timeout)
4. Transition to Idle state

---

## 8. Data Integrity and State Consistency

### 8.1 Single-Writer Principle

Each state partition has exactly one designated writer. Multiple readers are permitted.

| State Partition | Designated Writer | Readers |
|-----------------|-------------------|---------|
| Gimbal position | Servo driver (via hardware) | All controllers, OSD |
| IMU attitude | IMU device (via hardware) | Motion modes, OSD |
| Panel switches | PLC21 device (via hardware) | All controllers, safety |
| Weapon state | Weapon controller | Safety interlock, OSD |
| Homing state | Homing controller | Gimbal controller, OSD |
| Charging state | Charging FSM | Weapon controller, OSD |
| Tracking state | Camera controller | Motion modes, OSD |
| Safety decisions | Safety interlock | All controllers |

### 8.2 State Update Propagation Guarantees

1. **Atomicity:** State updates are atomic; observers never see partial updates.

2. **Ordering:** Observers receive state changes in the order they occurred.

3. **Completeness:** All registered observers receive all state change notifications.

4. **Timeliness:** State changes propagate within one control cycle (50 ms) under normal conditions.

### 8.3 Invalidation Rules (Stale Data Handling)

| Data Type | Staleness Threshold | Invalid Action |
|-----------|---------------------|----------------|
| IMU attitude | 500 ms | Disable stabilization |
| Gimbal position | 500 ms | Halt motion |
| Range measurement | Manual invalidation | Use default range |
| Tracking target | 200 ms | Coast or stop tracking |

### 8.4 Safety vs Non-Safety State Separation

| Category | Examples | Update Authority | Access Control |
|----------|----------|------------------|----------------|
| Safety-Critical | E-stop, arming, zones | Hardware/Safety interlock | Read-only for other components |
| Operational | Motion mode, tracking | Controllers | Validated updates only |
| Display-Only | OSD overlays, indicators | View models | No impact on control |

### 8.5 State Consistency Invariants

The following invariants are maintained at all times:

1. **E-Stop Dominance:** If E-Stop is active, no motion or fire commands execute.

2. **Authorization Consistency:** A PERMIT decision remains valid only if all conditions remain true.

3. **Zone Consistency:** Zone boundaries are immutable during motion execution.

4. **Mode Exclusivity:** Only one motion mode is active at any time.

---

## Appendix A: Timer Inventory

| Timer | Interval | Type | Owner | Purpose |
|-------|----------|------|-------|---------|
| Motion control | 50 ms | Periodic | Gimbal Controller | Servo command generation |
| Homing timeout | 30 s | Single-shot | Homing Controller | Sequence watchdog |
| Charging timeout | 5 s | Single-shot | Charging FSM | Movement watchdog |
| Lockout | 4 s | Single-shot | Charging FSM | Post-charge delay |
| Recovery delay | 1 s | Single-shot | E-Stop Monitor | Safe recovery period |
| Alarm reset | 1 s | Single-shot | Gimbal Controller | Two-step reset sequence |
| Startup retract | 500 ms | Single-shot | Weapon Controller | Delayed initialization |
| Backoff delay | 200 ms | Single-shot | Charging FSM | Motor stabilization |

---

## Appendix B: State Machine Summary

| State Machine | States | Primary Purpose |
|---------------|--------|-----------------|
| Homing | 6 | Gimbal position reference |
| Charging | 8 | Weapon cocking cycle |
| LAC Tracking | 3 | Lead angle transitions |
| E-Stop | 4 | Emergency stop lifecycle |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | January 2026 | Engineering Team | Initial TDS creation |
| 1.1 | January 2026 | Engineering Team | Architectural refinement per review feedback |

---

*End of Technical Design Specification*

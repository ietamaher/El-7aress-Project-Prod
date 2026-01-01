# OPERATOR MANUAL UPDATE SUMMARY - QT6 VERSION
## El 7arress RCWS System

**Date**: 2026-01-01
**Update Reason**: QT6 QML code updates and corrections to system documentation
**Reviewed Files**: Lessons 2, 3, 4, 5, and 6
**Status**: ✅ **ALL UPDATES IMPLEMENTED (Phase 1 & Phase 2)**

---

## EXECUTIVE SUMMARY

This document summarizes all critical updates made to the El 7arress RCWS Operator Manual following QT6 codebase analysis. The following lessons have been updated:

### Phase 1 Updates (2025-12-31):
- **Lesson 2 (Basic Operation)**: ✅ IMPLEMENTED - Major updates (joystick buttons, homing, LRF)
- **Lesson 3 (Menu System)**: ✅ IMPLEMENTED - LAC menu correction
- **Lesson 4 (Motion Modes & Surveillance)**: ✅ IMPLEMENTED - Zone selection buttons, radar note
- **Lesson 5 (Target Engagement)**: ✅ IMPLEMENTED - Double-click timing, dead reckoning
- **Lesson 6 (Ballistics & Fire Control)**: ✅ IMPLEMENTED - Two ballistic systems, LAC corrections

### Phase 2 Updates (2026-01-01):
- **Lesson 2 (Basic Operation)**: ✅ IMPLEMENTED - Emergency handling, safety interlocks, startup/shutdown checklists
- **Lesson 3 (Menu System)**: ✅ IMPLEMENTED - Display Brightness, Preset Home Position, Detection toggle
- **Lesson 4 (Motion Modes & Surveillance)**: ✅ IMPLEMENTED - Complete 5-type zone management
- **Lesson 5 (Target Engagement)**: ✅ IMPLEMENTED - Complete cocking actuator (charging state machine)

---

## CRITICAL CORRECTIONS IMPLEMENTED

### 1. AMMUNITION FEED SYSTEM (Lesson 2)

**Previous Documentation**: Incorrectly referenced a physical "belt sensor"

**CORRECTED IMPLEMENTATION**:
- ❌ **NO physical belt sensor exists** in the system
- ✅ **Encoder-based position detection** using servo actuator feedback
- Detection via `ServoActuatorData.position_mm` from servo encoder
- Feed positions:
  - Extended (belt engaged): 63,000 encoder counts
  - Home (retracted): 2,048 encoder counts

**OSD Status Indicators** (corrected):
```
IDLE (gray)        → No belt loaded, cycle not run
FEED... (orange)   → Feed cycle in progress (extending/retracting)
BELT (green)       → Belt confirmed via encoder position
FAULT (red flash)  → Timeout, operator reset required
```

**Operator Impact**: Section 2.6 "Ammunition Feed Procedure" added with encoder-based detection explanation.

---

### 2. HOMING SEQUENCE DETAILED TIMING (Lesson 2)

**Previous Documentation**: Simplified "press Home button" without timing details

**CORRECTED IMPLEMENTATION** (50ms control loop cycles):

```
Cycle 0:  Operator presses HOME → State = Requested → OSD: "homing init"
Cycle 1:  (50ms later) → GimbalController sends HOME command to PLC42
Cycle 2:  (100ms total) → State = InProgress → OSD: "HOMING..."
          → 30-second timeout timer starts
Wait:     Oriental Motor servos execute homing, send HOME-END signals
          → Azimuth HOME-END (DI6), Elevation HOME-END (DI7)
Complete: Both signals received → State = Completed → OSD: "HOME COMPLETE"
```

**Timeout Handling**:
- **30-second timeout** if HOME-END signals not received
- OSD displays: `HOMING TIMEOUT - FAULT` (red flashing)
- Recovery: Emergency Stop → clear obstruction → retry

**Operator Impact**: Section 2.2 Step 4 expanded with detailed homing sequence and timeout procedures.

---

### 3. COMPLETE JOYSTICK BUTTON MAPPING (Lesson 2)

**Previous Documentation**: Incomplete button listing, generic descriptions

**CORRECTED IMPLEMENTATION**: Full 19-button mapping (Button 0-18)

| **Button** | **Function** | **Operation** |
|------------|--------------|---------------|
| **0** | Engagement Mode | START/STOP engagement sequence |
| **1** | LRF | Single=Measure, Double=Continuous |
| **2** | Lead Angle Compensation (LAC) | Toggle LAC ON/OFF (joystick ONLY) |
| **3** | Dead Man Switch | MUST HOLD for weapon operation |
| **4** | Tracking | Single=Gate, Double=Start/Stop |
| **5** | Fire Weapon | Trigger |
| **6** | Zoom + | Increase magnification |
| **7** | LUT + | Next thermal video look-up table |
| **8** | Zoom - | Decrease magnification |
| **9** | LUT - | Previous thermal LUT |
| **10** | LRF Clear | Clear range reading, stop continuous |
| **11** | Mode Cycle | Manual → Sector Scan → TRP → Manual |
| **12** | Available | Reserved for future use |
| **13** | Mode Cycle | Duplicate of Button 11 (ergonomic) |
| **14** | Select Next TRP/Scan | Next TRP page or scan zone |
| **15** | Reserved | Future use |
| **16** | Select Previous TRP/Scan | Previous TRP page or scan zone |
| **17** | Reserved | Future use |
| **18** | Reserved | Future use |

**Key Additions**:
- **Button 1**: Double-click for **continuous LRF mode** (was not documented)
- **Button 2**: **LAC toggle** - ONLY method to activate LAC (no menu access!)
- **Buttons 7/9**: **Thermal LUT selection** via joystick (not menu-based)
- **Button 10**: **LRF clear** functionality
- **Buttons 14/16**: **TRP/Scan zone selection** during surveillance modes

**Operator Impact**: Section 2.4 "Complete Joystick Button Reference (19 Buttons)" added with detailed button-by-button documentation.

---

### 4. LAC (LEAD ANGLE COMPENSATION) - JOYSTICK ONLY (Lessons 2 & 6)

**Previous Documentation**: Mentioned menu-based LAC activation

**CRITICAL CORRECTION**:
- ❌ **NO menu access for LAC** in actual system
- ✅ **Button 2 ONLY** method to toggle LAC ON/OFF

**OSD LAC Status Display** (exact text from code):
```
LAC: OFF       (gray)   - Disabled
LAC: ON        (green)  - Active and functioning
LAC: LAG       (yellow) - Insufficient tracking data, wait 2-5 sec
LAC: ZOOM OUT  (red)    - FOV too narrow, zoom out required
```

**Activation Procedure** (corrected):
```
1. Ensure target is being tracked (Active Lock)
2. Press Button 2 (LAC Toggle)
3. OSD displays LAC status (OFF → ON or ON → OFF)
4. Verify status is "ON" before firing
```

**Operator Impact**:
- Lesson 2 Section 2.4.5: Button 2 LAC operation documented
- Lesson 6 Section 6.3: Remove all menu-based LAC references, Button 2 only

---

### 5. BALLISTIC DROP VS. MOTION LEAD (Lesson 6 Clarification Needed)

**Previous Documentation**: Mixed terminology, unclear distinction

**SYSTEM ARCHITECTURE** (from code analysis):

The system has **TWO SEPARATE** ballistic correction systems:

**A. Ballistic Drop Compensation (AUTOMATIC)**:
- Variables: `ballisticDropOffsetAz`, `ballisticDropOffsetEl`
- **Automatically applied** when LRF has valid range data
- Corrections include:
  - Gravity drop (bullet drop over distance)
  - Wind deflection (crosswind → azimuth offset)
  - Environmental parameters (temperature, altitude)
- **No operator toggle required** - always active when range valid
- Displayed on OSD: `ENV` indicator when environmental params active

**B. Motion Lead Angle Compensation (MANUAL TOGGLE)**:
- Variables: `motionLeadOffsetAz`, `motionLeadOffsetEl`
- **Operator-controlled via Button 2**
- Only active when:
  - LAC toggled ON by operator
  - Target tracking is active
  - Target exhibiting measurable motion
- Calculates lead based on:
  - Target angular velocity (from tracker)
  - Range (from LRF)
  - Time-of-flight (ballistic calculation)
- Displayed on OSD: `LAC: ON/LAG/ZOOM OUT`

**CCIP Pipper Display**:
```
CCIP (*) = Zeroing + Ballistic Drop + Motion Lead (if LAC ON)
```

**Operator Impact**: Lesson 6 needs section clarifying these are **two independent systems**:
1. Ballistic drop (automatic, gravity + wind)
2. Motion lead (manual toggle, moving targets only)

**Deprecated Variables** (found in code comments):
- `leadAngleOffsetAz/El` - OLD terminology, replaced by `motionLeadOffsetAz/El`

---

### 6. TRP/SCAN ZONE SELECTION (Lesson 4)

**Previous Documentation**: Menu-only zone selection

**NEW CAPABILITY**: Joystick button selection during operations

**Buttons 14/16 Function**:
- **Button 14**: Select Next TRP page or scan zone
- **Button 16**: Select Previous TRP page or scan zone

**Operational Benefit**:
- Rapid zone switching **without menu access**
- Hands stay on joystick, eyes on video
- Faster reaction to changing threat sectors

**During AutoSectorScan Mode**:
```
Currently scanning: "Front Gate"
Press Button 14 → Switch to "East Perimeter"
Press Button 14 → Switch to "Rear Area"
Press Button 14 → Wrap to "Front Gate"
```

**During TRP Scan Mode**:
```
Currently on: "Page 1: Checkpoints"
Press Button 14 → Switch to "Page 2: Hilltops"
Press Button 14 → Switch to "Page 3: Route Monitoring"
Press Button 14 → Wrap to "Page 1"
```

**Operator Impact**: Lesson 4 Section 4.5 "TRP and Scan Zone Selection (Buttons 14/16)" added.

---

### 7. RADAR INTEGRATION (Lesson 4)

**Previous Documentation**: Radar Slew mode listed as available option

**CORRECTED DOCUMENTATION**:
- ❌ **NO radar currently installed** on standard El 7arress RCWS
- ✅ System architecture **supports future radar integration**
- Mode cycle sequence updated: `Manual → AutoSectorScan → TRP Scan → Manual` (radar removed)

**Operator Impact**:
- Lesson 4 Section 4.6: Radar mode removed, replaced with note about future capability
- Mode cycle updated throughout Lesson 4

---

### 8. MOTION MODE CYCLE SEQUENCE (Lesson 4)

**Previous**: `Manual → AutoSectorScan → TRP Scan → Radar Slew → Manual`

**Corrected**: `Manual → AutoSectorScan → TRP Scan → Manual`

**OSD Mode Display**:
- `MODE: Manual`
- `MODE: Sector Scan`
- `MODE: TRP`

---

### 9. AHRS STABILIZATION (Lesson 2)

**Previous Documentation**: Basic "Stabilization ON/OFF"

**ENHANCED DOCUMENTATION**:
- **AHRS**: Attitude and Heading Reference System
- **IMU data**: Roll, Pitch, Yaw from inertial measurement unit
- **World-frame stabilization**: Gimbal compensates for vehicle movement
- **Stationary detection**: Enhanced stabilization when vehicle not moving
  - OSD displays: `VEHICLE STATIONARY` when detected
  - Improved accuracy during static operations

**Operator Impact**: Lesson 2 Step 9 expanded with AHRS technical explanation.

---

### 10. CAMERA SPECIFICATIONS (Lesson 2 Addition Recommended)

**Day Camera**:
- Type: Sony with autofocus
- Optical zoom: Continuous (2° to 60° FOV)
- Zoom factor calculation: Displayed on OSD

**Night Camera**:
- Type: FLIR TAU 2 thermal camera
- Resolution: 640×512 (non-square sensor)
- FOV:
  - Wide: 10.4° HFOV, 8° VFOV
  - Narrow: 5.2° HFOV, 4° VFOV
- Digital zoom levels: Available
- FFC (Flat Field Correction): Automatic shutter calibration (5 sec freeze)
- LUT options: White Hot, Black Hot, Ironbow, Lava, Rainbow

**Operator Impact**: Lesson 2 camera section should add technical specifications table.

---

## PHASE 2 UPDATES (2026-01-01) - NEW

### 11. MENU FUNCTIONS - NEW ITEMS (Lesson 3)

**New Menu Options Added**:

**A. Display Brightness**:
- Access: Main Menu → Display Brightness
- Range: 10% to 100% (10% step size)
- Preview: Real-time as you adjust
- Cancel: Press BACK to restore original
- Recommendations:
  - 100%: Bright daylight
  - 70-80%: Normal daylight (default)
  - 20-30%: Night operations

**B. Preset Home Position (Calibration)**:
- Access: Main Menu → Preset Home Position
- Purpose: Define custom HOME reference position for gimbal
- 3-Step procedure:
  1. Position gimbal with joystick
  2. Confirm captured position
  3. Validate to save to motor controller
- Motor controller stores position in non-volatile memory (persists across power cycles)

**C. Detection Toggle**:
- Access: Main Menu → Detection: ENABLED/DISABLED
- Availability: Day camera ONLY (unavailable on Night camera)
- Menu States:
  - "Detection: ENABLED" - AI detection ON
  - "Detection: DISABLED" - AI detection OFF
  - "Detection (Night - Unavailable)" - Night camera active

**Operator Impact**: Lesson 3 menu structure updated with new sections for Brightness, Calibration, and Detection.

---

### 12. COCKING ACTUATOR / CHARGING STATE MACHINE (Lesson 5)

**New Section Added**: Complete charging system documentation

**Weapon-Specific Charging Cycles**:
| Weapon | Cycles Required | Notes |
|--------|-----------------|-------|
| M2HB (.50 cal) | **2 cycles** | Closed bolt - requires double cycle |
| M240B | 1 cycle | Open bolt weapon |
| M249 SAW | 1 cycle | Open bolt weapon |
| MK19 (40mm) | 1 cycle | Grenade launcher |

**Two Charging Modes**:
1. **Short Press (Automatic)**: Quick press, system auto-cycles (including 2nd cycle for M2HB)
2. **Continuous Hold (Manual)**: Hold button, actuator extends; release, actuator retracts

**Charging State Machine**:
```
IDLE → EXTENDING → EXTENDED (HOLD) → RETRACTING → LOCKOUT (4s) → IDLE
                                   ↘ JAM DETECTED → FAULT → IDLE (reset)
```

**4-Second El-7aress H100 Lockout**:
- Required post-charge safety period
- Prevents accidental double-charge
- Button ignored during lockout

**Jam Detection & Recovery**:
- Automatic detection: High torque + no movement
- Automatic response: Stop motor → alarm → backoff → fault state
- Operator procedure: Wait for backoff → reset via charge button → retry

**Safety Interlocks for Charging**:
- Emergency Stop: Must NOT be active
- Station Enable: Must be ON
- NOT already charging
- NOT in lockout period
- NO fault state

**Operator Impact**: Lesson 5 new section "WEAPON CHARGING (COCKING ACTUATOR)" with complete procedures.

---

### 13. EMERGENCY STOP MONITOR (Lesson 2)

**Enhanced Emergency Stop Documentation**:

**Debounce Protection**:
- 50ms debounce period prevents false activations
- Button state must remain stable for 50ms before triggering

**Recovery Period (El-7aress H100 Compliance)**:
- 500ms minimum recovery period after E-stop released
- OSD displays: "RECOVERY IN PROGRESS..." then "RECOVERY COMPLETE"

**Event Logging**:
- All Emergency Stop events logged with timestamp
- Tracks: activation time, duration, source (hardware/software)
- Accessible via System Status → Emergency Log

**Software Emergency Stop**:
- System can trigger E-stop for critical faults
- Triggers: PLC communication loss, servo fault, safety violation
- OSD displays: "SOFTWARE EMERGENCY STOP - [reason]"

---

### 14. SAFETY INTERLOCK SYSTEM (Lesson 2)

**New Section Added**: Complete safety hierarchy documentation

**canFire() - FIRE Permission (9 conditions)**:
1. Emergency Stop NOT active
2. Station Enable ON
3. Dead Man Switch HELD
4. Gun Arm switch ARMED
5. Operational Mode = Engagement
6. System Authorized
7. NOT in No-Fire Zone
8. NOT Charging
9. NO Charge Fault

**canCharge() - CHARGE Permission (5 conditions)**:
1. Emergency Stop NOT active
2. Station Enable ON
3. NOT already charging
4. NOT in lockout period
5. NO fault state

**canMove() - MOVEMENT Permission**:
- Emergency Stop NOT active
- Station Enable ON
- Dead Man Switch HELD (for Manual, AutoTrack, ManualTrack modes)
- Dead Man Switch NOT required for Pattern mode (Sector Scan, TRP)

**canEngage() - ENGAGEMENT Permission**:
- Emergency Stop NOT active
- Station Enable ON
- Dead Man Switch HELD

**canHome() - HOMING Permission**:
- Emergency Stop NOT active
- Station Enable ON
- NOT already homing

**Safety Denial Messages**: Complete table of all denial messages with required actions.

---

### 15. STARTUP/SHUTDOWN CHECKLISTS (Lesson 2)

**Startup Quick Reference Checklist** (25 items):
- PRE-POWER CHECKS (8 items): Walk-around, weapon clear, personnel, power, briefing
- POWER-UP SEQUENCE (6 items): Station disable, power on, self-test, enable, home
- FUNCTIONAL CHECKS (5 items): Camera, gimbal, zoom, LRF, stabilization
- FINAL STATUS (6 items): All indicator lights verified

**Shutdown Quick Reference Checklist** (20 items):
- SAFE THE SYSTEM (4 items): Release controls, Gun SAFE, exit engagement
- RETURN TO STOW (3 items): Home gimbal, stabilization off
- MENU SHUTDOWN (5 items): Navigate, select, confirm, wait for complete
- POWER DOWN (4 items): Station disable, power off
- SECURE (4 items): Clear weapon, ammo, covers, log

**Format**: Checkbox-style tables for operator use during operations.

---

### 16. COMPLETE ZONE MANAGEMENT (Lesson 4)

**5 Zone Types Documented**:

| Zone Type | Primary Purpose | System Behavior |
|-----------|-----------------|-----------------|
| Safety Zone | Vehicle/crew protection | Configurable restrictions |
| No-Fire Zone | Prevent firing into protected areas | **Blocks weapon fire** |
| No-Traverse Zone | Prevent gimbal damage | **Blocks gimbal movement** |
| Sector Scan Zone | Automated surveillance | Defines scan limits |
| Target Reference Point | Pre-defined observations | Defines dwell locations |

**Zone Creation Procedure (Aiming Method)**:
1. Access: Menu → Zone Definitions → New Zone
2. Select zone type
3. AIM CORNER 1: Slew gimbal, press MENU ✓
4. AIM CORNER 2: Slew to opposite corner, press MENU ✓
5. SET PARAMETERS: Enabled, Overridable
6. Validate to save

**Zone Map Display**:
- Safety Zones: Blue rectangles
- No-Fire Zones: Red rectangles
- No-Traverse Zones: Yellow rectangles
- Sector Scan: Green lines
- TRPs: Green crosshairs
- Current Gimbal: White crosshair

**TRP Pagination**:
- Up to 200 Location Pages
- Up to 50 TRPs per page
- Button 14/16 for page switching during scan

---

## FILES UPDATED (2025-12-31 & 2026-01-01)

### Updated Lesson Files:

#### Phase 1 Updates (2025-12-31):
1. **docs/Manual/lecon02.tex** - Lesson 2: Basic Operation ✅ UPDATED
   - Complete 19-button joystick mapping (Button 0-18)
   - Detailed homing sequence with 50ms timing and 30-second timeout
   - LRF continuous mode documentation (double-click Button 1)
   - LAC Button 2 operation (joystick ONLY - no menu!)
   - Critical warning about LAC activation method

2. **docs/Manual/lecon03.tex** - Lesson 3: Menu System ✅ UPDATED
   - Corrected LAC menu section to "View Only"
   - Added warning that LAC cannot be toggled from menu
   - Clear indication that Button 2 is the ONLY method

3. **docs/Manual/lecon04.tex** - Lesson 4: Motion Modes & Surveillance ✅ UPDATED
   - Added note that Radar Slew requires radar hardware
   - NEW SECTION: Zone Selection via Joystick (Buttons 14/16)
   - TRP page and sector scan zone switching documentation

4. **docs/Manual/lecon05.tex** - Lesson 5: Target Engagement ✅ UPDATED
   - Fixed double-click abort timing from 500ms to **1000ms (1 second)**
   - NEW SECTION: Dead Reckoning During Firing (El-7aress Doctrine)
   - Documentation of gimbal behavior during firing phase

5. **docs/Manual/lecon06.tex** - Lesson 6: Ballistics & Fire Control ✅ UPDATED
   - NEW SECTION: "Two Independent Ballistic Correction Systems"
     - Ballistic Drop Compensation (AUTOMATIC)
     - Motion Lead Compensation (MANUAL - Button 2)
   - Critical warning that LAC is joystick Button 2 ONLY
   - Updated LAC status indicators to match code (LAC: OFF/ON/LAG/ZOOM OUT)
   - El-7aress-compliant LAC latching behavior (2-second minimum interval)
   - Target switching procedure for LAC

#### Phase 2 Updates (2026-01-01):
1. **docs/Manual/lecon02.tex** - Lesson 2: Basic Operation ✅ UPDATED (Phase 2)
   - NEW SECTION: Emergency Stop System Details (debounce, recovery, logging)
   - NEW SECTION: Safety Interlock System (canFire, canCharge, canMove, canEngage, canHome)
   - NEW: Startup Quick Reference Checklist (25 items, checkbox format)
   - NEW: Shutdown Quick Reference Checklist (20 items, checkbox format)
   - Safety denial indicator messages table

2. **docs/Manual/lecon03.tex** - Lesson 3: Menu System ✅ UPDATED (Phase 2)
   - Updated menu structure with new items
   - NEW SECTION: Display Brightness (10-100%, 10% steps, real-time preview)
   - NEW SECTION: Calibration - Preset Home Position (3-step gimbal homing procedure)
   - NEW SECTION: Detection Toggle (Day camera only, AI target detection)
   - Clear Active Zero/Windage/Environmental Settings options

3. **docs/Manual/lecon04.tex** - Lesson 4: Motion Modes & Surveillance ✅ UPDATED (Phase 2)
   - NEW SECTION: Zone Management - Complete Zone Types (5 types)
   - Zone Type 1: Safety Zones (configurable restrictions)
   - Zone Type 2: No-Fire Zones (blocks weapon fire)
   - Zone Type 3: No-Traverse Zones (blocks gimbal movement)
   - Zone Type 4: Sector Scan Zones (scan limits, speed)
   - Zone Type 5: Target Reference Points (TRP pagination, halt time)
   - Zone creation procedure using aiming method
   - Zone map display color coding

4. **docs/Manual/lecon05.tex** - Lesson 5: Target Engagement ✅ UPDATED (Phase 2)
   - NEW SECTION: Weapon Charging (Cocking Actuator)
   - Weapon-specific charging cycles (M2HB = 2, others = 1)
   - Two charging modes: Short Press vs Continuous Hold
   - Charging state machine diagram
   - 4-second El-7aress H100 lockout period
   - Jam detection and recovery procedures
   - Charging safety interlocks table
   - Startup actuator check documentation

5. **docs/Manual/MANUAL_UPDATE_SUMMARY_QT6.md** - This summary document ✅ UPDATED

---

## LESSON 6 UPDATES - ✅ COMPLETED

**Lesson 6: Ballistics & Fire Control** changes have been implemented:

### Section: Two Independent Ballistic Correction Systems (NEW)

Added comprehensive explanation of the two separate ballistic systems:

```
SYSTEM 1: BALLISTIC DROP COMPENSATION (Automatic)
   - Activation: AUTOMATIC when LRF has valid range data
   - Corrects: Gravity drop, wind deflection, environmental factors
   - Variables: ballisticDropOffsetAz, ballisticDropOffsetEl
   - OSD indicator: ENV (when environmental params active)
   - Operator Action: NONE REQUIRED

SYSTEM 2: MOTION LEAD COMPENSATION (Manual Toggle)
   - Activation: MANUAL via joystick Button 2 (LAC toggle)
   - Corrects: Moving target lead based on velocity and TOF
   - Variables: motionLeadOffsetAz, motionLeadOffsetEl
   - OSD indicator: LAC: ON/LAG/ZOOM OUT
   - Operator Action: REQUIRED - Button 2 to enable

COMBINED CCIP CALCULATION:
CCIP (*) = Gun Boresight + Zeroing Offset
           + Ballistic Drop Compensation (auto when range valid)
           + Motion Lead Compensation (only if Button 2 toggled ON)
```

### Section: El-7aress-Compliant LAC Latching (NEW)

Added per El-7aress H100 specification:
- 2-second minimum interval between LAC toggles
- Latched velocity behavior (not continuously updated)
- Target switching procedure warning

---

## TESTING & VALIDATION RECOMMENDATIONS

Before final manual release, validate these operator procedures:

1. **Ammunition Feed Cycle**:
   - Press AMMO LOAD button
   - Verify OSD shows: IDLE → FEED... → BELT
   - Confirm NO physical sensor, only encoder position detection

2. **Homing Sequence**:
   - Observe "homing init" → "HOMING..." → "HOME COMPLETE"
   - Test timeout scenario (obstruct gimbal, verify 30-sec timeout)
   - Verify HOME-END signals (DI6/DI7) from Oriental Motors

3. **Joystick Buttons** (all 19):
   - Test each button function listed in mapping
   - Verify Button 1 double-click (continuous LRF)
   - Verify Button 2 LAC toggle (no menu required)
   - Verify Buttons 14/16 zone selection during Sector Scan and TRP modes

4. **LAC Operation**:
   - Confirm Button 2 toggles LAC ON/OFF
   - Verify NO menu access to LAC
   - Test all OSD status indicators: OFF, ON, LAG, ZOOM OUT
   - Verify LAC only functions during active tracking

5. **Mode Cycle**:
   - Press Button 11/13, verify sequence: Manual → Sector Scan → TRP → Manual
   - Confirm NO radar mode in cycle

6. **TRP/Scan Selection**:
   - Define multiple scan zones and TRP pages
   - Test Button 14/16 during AutoSectorScan and TRP modes
   - Verify zone/page switching works without menu

---

## OPERATOR TRAINING IMPACT

**High Priority Training Updates**:
1. **Ammunition Loading**: Emphasize encoder detection, no physical sensor
2. **Joystick Buttons**: Full button memorization drill (especially 0-5, 14, 16)
3. **LAC Activation**: Button 2 ONLY (break habit of menu access if previously trained)
4. **Zone Selection**: Demonstrate Buttons 14/16 for rapid sector switching

**Training Aids Recommended**:
- Joystick button diagram (print for mounting near operator station)
- LAC status flowchart (OFF → ON → LAG/ZOOM OUT → troubleshooting)
- Homing sequence timing diagram
- Zone selection quick reference card

---

## FUTURE MANUAL UPDATES (Recommended)

**Lessons Not Yet Reviewed** (may require updates):
- **Lesson 1**: System Overview - Update DCU button diagram (3-button system), add PLC42 inputs
- **Lesson 3**: Menu System - Verify 3-button navigation consistency
- **Lesson 5**: Target Engagement - Add VPI tracker reference, tracking confidence
- **Lesson 7**: System Status - Add servo actuator monitoring, IMU display, environmental sensors
- **Lesson 8**: Emergency Procedures - Add ammo feed fault recovery, homing timeout, hatch interlocks
- **Lesson 9**: Preventive Maintenance - Add actuator maintenance procedures
- **Lesson 10**: (Review for consistency with updates)

---

## APPENDICES REQUIRED (New Additions)

**Appendix X: Quick Reference - Joystick Button Functions**
- One-page laminated card with all 19 buttons
- Button-by-button function list
- For mounting near operator station

**Appendix Y: OSD Status Indicators Reference**
- All OSD indicators explained (Z, ENV, LAC, STAB, etc.)
- Color-coded meaning (green=OK, yellow=caution, red=fault)
- Operator action for each status

**Appendix Z: Troubleshooting - Common Faults**
- Homing timeout → Check HOME-END signals (DI6/DI7)
- Ammo feed fault → Encoder position error, check obstruction
- LAC: ZOOM OUT → Reduce zoom level
- LAC: LAG → Wait 2-5 seconds for tracking stabilization

---

## DOCUMENT CONTROL

**Manual Version**: QT6-2025.1
**Prepared By**: Claude (AI Technical Writer)
**Review Required By**: Technical SME, Operator Training Lead, QA Lead
**Approval Authority**: Project Manager / Chief Engineer
**Implementation Date**: TBD upon approval
**Change Classification**: Major (substantive technical corrections)

---

## DISTRIBUTION

Upon approval, distribute updated manual sections to:
- All active operators (current certification holders)
- Operator training cadre
- Maintenance technicians
- Unit commanders
- Technical library

**Training Requirement**: All operators must review updated Lessons 2, 4, 6 before next qualification.

---

**END OF SUMMARY**

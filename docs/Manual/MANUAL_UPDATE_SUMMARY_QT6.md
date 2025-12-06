# OPERATOR MANUAL UPDATE SUMMARY - QT6 VERSION
## El 7arress RCWS System

**Date**: 2025-12-06
**Update Reason**: QT6 QML code updates and corrections to system documentation
**Reviewed Files**: Lessons 2, 4, and 6 (most significant updates)

---

## EXECUTIVE SUMMARY

This document summarizes all critical updates made to the El 7arress RCWS Operator Manual following QT6 codebase analysis. Three primary lessons have been substantially revised:

- **Lesson 2 (Basic Operation)**: Major updates
- **Lesson 4 (Motion Modes & Surveillance)**: Moderate updates
- **Lesson 6 (Ballistics & Fire Control)**: Clarifications needed

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

## FILES UPDATED

### Generated Updated Files:
1. **docs/Manual/lecon02_UPDATED.tex** - Lesson 2: Basic Operation
   - Complete ammunition feed procedure (encoder-based)
   - Detailed homing sequence with 50ms timing
   - Full 19-button joystick mapping
   - LRF continuous mode documentation
   - LAC Button 2 operation
   - AHRS stabilization explanation
   - Updated OSD section with LAC status

2. **docs/Manual/lecon04_UPDATED.tex** - Lesson 4: Motion Modes & Surveillance
   - Radar mode removed (future capability note added)
   - Updated mode cycle sequence
   - TRP/Scan zone selection with Buttons 14/16
   - Enhanced zone management procedures
   - TRP page organization explained

3. **docs/Manual/MANUAL_UPDATE_SUMMARY_QT6.md** - This summary document

---

## LESSON 6 UPDATES REQUIRED (Not Yet Generated)

**Lesson 6: Ballistics & Fire Control** requires these specific changes:

### Section 6.3 - Lead Angle Compensation

**REMOVE**:
- All menu-based LAC activation procedures
- Any reference to "Environmental Parameters → LAC" menu path
- Menu toggle activation steps

**ADD**:
- **Joystick Button 2 ONLY** activation procedure
- Clarification: "LAC can ONLY be toggled via Button 2 on the joystick"
- Updated OSD status indicators:
  - `LAC: OFF` (gray)
  - `LAC: ON` (green)
  - `LAC: LAG` (yellow)
  - `LAC: ZOOM OUT` (red)

**CLARIFY** (new section recommended):

**6.X - Two Ballistic Correction Systems**

```
The El 7arress RCWS employs two independent ballistic correction systems:

1. BALLISTIC DROP COMPENSATION (Automatic)
   - Always active when LRF range data valid
   - Corrects for gravity drop and wind deflection
   - Uses environmental parameters (temp, altitude, crosswind)
   - No operator intervention required
   - Variables: ballisticDropOffsetAz, ballisticDropOffsetEl
   - OSD indicator: ENV (when environmental params active)

2. MOTION LEAD ANGLE COMPENSATION (Manual Toggle)
   - Operator activates via Button 2 (joystick ONLY)
   - Only functions during active target tracking
   - Calculates lead for moving targets
   - Based on target velocity, range, time-of-flight
   - Variables: motionLeadOffsetAz, motionLeadOffsetEl
   - OSD indicator: LAC: ON/LAG/ZOOM OUT

FINAL CCIP AIM POINT:
CCIP (*) = Zeroing Offset
           + Ballistic Drop Compensation (auto if range valid)
           + Motion Lead Compensation (if Button 2 toggled ON)
```

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

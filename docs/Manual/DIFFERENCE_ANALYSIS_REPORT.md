# RCWS Operator Manual - Difference Analysis Report

**Document Version:** 1.0
**Analysis Date:** 2026-01-07
**Scope:** Comparison of existing operator manual content against Qt6/QML software implementation

---

## Executive Summary

This report identifies discrepancies between the current RCWS operator manual (Lessons 2-10) and the actual system behavior as implemented in the Qt6/QML software. Each difference is documented from an operator's perspective with recommended manual updates.

---

## Difference Categories

| Category | Description |
|----------|-------------|
| **UI Change** | Display layout, indicator position, or visual presentation differs |
| **Workflow Change** | Operator sequence or procedure steps differ |
| **Mode Behavior Change** | How a mode operates differs from description |
| **New Restriction/Inhibition** | System blocks actions not mentioned in manual |
| **Obsolete/Missing Description** | Feature removed, renamed, or undocumented |

---

## LESSON 6: Ballistics & Fire Control

### Difference 6.1: Lead Angle Compensation (LAC) Activation Method

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, LAC Activation |
| **Described Behavior** | Operator presses Button 2 while holding Button 3 (Palm Switch) to activate LAC. Single combined action. |
| **Actual Behavior** | LAC operates in a two-stage CROWS-compliant workflow: (1) Press Button 2 while holding Button 3 to **ARM** LAC, which latches the current tracking rate. (2) LAC only becomes **ENGAGED** (lead applied) when the fire trigger (Button 0) is pressed. Releasing the trigger disengages LAC but keeps it armed. |
| **Change Type** | Workflow Change |
| **Recommendation** | Update manual to describe the ARM → ENGAGE → DISENGAGE cycle. Add explanation: "Arming LAC captures the target's motion. Lead is only applied during firing. Release trigger to remove lead while keeping LAC armed for subsequent bursts." |

### Difference 6.2: LAC Status Indicators

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, LAC Status Display |
| **Described Behavior** | OSD shows LAC status as: ON, LAG, or ZOOM OUT. |
| **Actual Behavior** | OSD displays: **LAC ARMED** (armed but not firing), **LEAD ANGLE ON** (engaged with valid solution), **LAG** (at maximum lead limit), **ZOOM OUT** (predicted impact point outside field of view). |
| **Change Type** | UI Change |
| **Recommendation** | Add "LAC ARMED" status to the status indicator table. Update descriptions: "LAC ARMED appears when lead angle is armed but the fire trigger is not pressed. Lead is not being applied until you fire." |

### Difference 6.3: LAC 2-Second Rearm Delay

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, LAC Activation |
| **Described Behavior** | Manual mentions a 2-second minimum wait before reuse, but does not explain what happens if operator tries to rearm too quickly. |
| **Actual Behavior** | If operator attempts to arm LAC within 2 seconds of last disarm, the system blocks the action and logs a warning. The button press is ignored entirely. |
| **Change Type** | New Restriction/Inhibition |
| **Recommendation** | Add explicit statement: "If you attempt to re-arm LAC within 2 seconds of the last toggle, the system will ignore the command. Wait for the 2-second interval to pass before re-arming." |

### Difference 6.4: Dead Reckoning During Firing

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, Tracking and Firing |
| **Described Behavior** | Not mentioned in current manual. |
| **Actual Behavior** | When the fire trigger is pressed during active tracking, the system enters Dead Reckoning mode. The gimbal continues moving at the last known target velocity but stops receiving tracker updates. This prevents tracker interference from muzzle flash or recoil. |
| **Change Type** | Obsolete/Missing Description |
| **Recommendation** | Add new section: "During firing, the system automatically enters Dead Reckoning mode. The gimbal maintains the target's last known speed and direction. The tracker is temporarily suspended. After releasing the trigger, you must re-acquire the target to resume active tracking." |

### Difference 6.5: CCIP (Continuously Computed Impact Point) Display

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, Ballistic Indicators |
| **Described Behavior** | Manual describes reticle offset for ballistic drop and lead. |
| **Actual Behavior** | System displays two separate elements: (1) **Reticle** shows gun boresight with zeroing only (where the gun is actually pointed). (2) **CCIP Pipper** shows predicted impact point including ballistic drop, wind deflection, and motion lead when LAC is engaged. The CCIP is visible whenever a valid LRF range exists. |
| **Change Type** | UI Change |
| **Recommendation** | Add CCIP pipper description: "The CCIP pipper (predicted impact marker) appears when LRF provides a valid range. It shows where your rounds will actually impact, accounting for gravity drop and wind. When LAC is engaged, the CCIP also includes lead for moving targets." |

### Difference 6.6: Ballistic Drop Automatic Application

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 6, Ballistic Correction |
| **Described Behavior** | Manual implies operator must apply ballistic correction. |
| **Actual Behavior** | Ballistic drop compensation is **automatically applied** whenever the LRF has a valid range. This is independent of LAC. The CCIP pipper always shows gravity-compensated impact point when range is known. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Clarify: "Ballistic drop is calculated and applied automatically when a valid LRF measurement exists. You do not need to activate any switch. The CCIP pipper always shows the corrected impact point. LAC only adds motion lead on top of this automatic drop compensation." |

---

## LESSON 7: System Status & Monitoring

### Difference 7.1: Homing Sequence States

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 7, System Status |
| **Described Behavior** | Manual does not detail homing sequence states. |
| **Actual Behavior** | Homing display shows: **Idle** (no operation), **Requested** (command sent), **In Progress** (motors seeking home), **Completed** (home found), **Failed** (timeout occurred), **Aborted** (cancelled by E-Stop or other condition). |
| **Change Type** | UI Change |
| **Recommendation** | Add homing status indicators table. Include: "During homing, the OSD displays the current homing phase. If homing fails or times out, operator must acknowledge and may need to retry. Motion modes are blocked during homing." |

### Difference 7.2: Charging State Display

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 7, Weapon Status Indicators |
| **Described Behavior** | Manual mentions charged/not charged status. |
| **Actual Behavior** | OSD shows detailed charging FSM states: **Idle** (ready), **Extending** (actuator moving), **Extended** (holding), **Retracting** (returning), **Lockout** (4-second cooldown), **Fault** (error, requires reset). Progress shows cycles completed vs required (e.g., "1/2" for M2HB). |
| **Change Type** | UI Change |
| **Recommendation** | Update charging status section to show all states. Add: "After a successful charge cycle, a 4-second lockout prevents additional charging. The display shows lockout countdown. For the M2HB, two charge cycles are required." |

### Difference 7.3: FREE Mode Indication

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 7, System Status |
| **Described Behavior** | Manual mentions FREE toggle but not OSD indication. |
| **Actual Behavior** | When the FREE toggle switch is activated, the motion mode changes to **MotionFree**. The gimbal brakes are released and no servo commands are sent. The OSD displays the motion mode as "FREE" and logs indicate manual positioning is enabled. |
| **Change Type** | UI Change |
| **Recommendation** | Add FREE mode to motion mode display table: "When the local FREE toggle switch is ON, motion mode displays as FREE. The gimbal can be manually positioned. No powered control is active. Ensure area is clear before physically moving the gimbal." |

---

## LESSON 8: Emergency Procedures

### Difference 8.1: Tracking Abort Method

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 8, Aborting Tracking |
| **Described Behavior** | Manual states double-click Button 4 to abort tracking. |
| **Actual Behavior** | Correct. Double-press of Button 4 within 1 second aborts tracking and returns to Off phase. Single press advances tracking phases (Off → Acquisition → Lock Pending → Active Lock). |
| **Change Type** | (Accurate, no change needed) |
| **Recommendation** | Current description is accurate. Consider adding: "The double-click window is 1 second. If clicks are more than 1 second apart, they are treated as separate single presses." |

### Difference 8.2: Emergency Stop Recovery Sequence

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 8, E-Stop Recovery |
| **Described Behavior** | Manual describes basic E-Stop and recovery. |
| **Actual Behavior** | Upon E-Stop release, the system automatically: (1) Returns servos to MANUAL mode. (2) If charging was interrupted, the cocking actuator automatically retracts to safe home position. (3) Any active tracking is stopped. (4) Operator must re-arm and re-charge weapon manually. Motion mode is restored to previous state after servos are ready. |
| **Change Type** | Workflow Change |
| **Recommendation** | Expand recovery procedure: "After releasing E-Stop, the system automatically retracts any extended charging actuator to home position. All weapon operations are reset. You must: (1) Wait for servos to initialize, (2) Re-arm the weapon, (3) Re-charge if needed, (4) Resume normal operations. The previous motion mode is restored automatically." |

### Difference 8.3: Motion Mode Blocked During Tracking

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 8, Emergency Mode Changes |
| **Described Behavior** | Manual does not mention this restriction. |
| **Actual Behavior** | Operator cannot change motion modes (using Button 11/13 or Button 12) while in any tracking phase. The system blocks the command and logs a warning. Operator must first abort tracking (double-press Button 4), then change modes. |
| **Change Type** | New Restriction/Inhibition |
| **Recommendation** | Add to safety procedures: "Motion mode changes are blocked during any tracking phase (Acquisition, Lock Pending, Active Lock, Coast, or Firing). To change modes, first abort tracking by double-pressing the TRACK button, then select the new mode." |

---

## LESSON 9: Operator Maintenance & Troubleshooting

### Difference 9.1: Continuous LRF Mode

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 9, LRF Operation |
| **Described Behavior** | Manual describes single-shot LRF operation via Button 1. |
| **Actual Behavior** | Single press triggers single measurement. **Double-press within 1 second toggles continuous LRF mode** (5Hz automatic ranging). When continuous mode is active, single presses are ignored. Double-press again to disable continuous mode and return to single-shot. |
| **Change Type** | Obsolete/Missing Description |
| **Recommendation** | Add continuous LRF section: "Double-press the LRF button (Button 1) within 1 second to activate continuous ranging at 5Hz. The system will automatically measure distance approximately 5 times per second. Double-press again to return to single-shot mode. Use continuous mode for tracking moving targets where range changes rapidly." |

### Difference 9.2: LRF Clear vs Zeroing Clear

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Lesson 9, LRF Procedures |
| **Described Behavior** | Manual mentions LRF clear but conflates with zeroing. |
| **Actual Behavior** | Button 10 clears only the LRF measurement (sets stored range to 0). This is separate from the menu option "Clear Active Zero" which clears weapon zeroing offsets. These are independent functions. |
| **Change Type** | Workflow Change |
| **Recommendation** | Clarify: "LRF Clear (Button 10) removes the current range measurement only. It does not affect weapon zeroing or any other ballistic settings. Use the menu option 'Clear Active Zero' to remove zeroing offsets." |

---

## CONTROL MAPPINGS: Joystick Button Reference

### Difference C.1: Complete Button Mapping Update

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Control Reference, Joystick Buttons |
| **Described Behavior** | Manual provides button mapping but may be outdated. |
| **Actual Behavior** | Current implemented button assignments: |
| **Change Type** | Workflow Change |

**Actual Button Mapping Table:**

| Button | Function | Notes |
|--------|----------|-------|
| 0 | Fire Intent (Half-press) | Engages LAC if armed, enters dead reckoning during tracking. Release disengages LAC (stays armed). Also triggers engagement mode. |
| 1 | LRF Trigger | Single = measure. Double-click = toggle continuous 5Hz mode. |
| 2 | LAC Toggle | Press with Palm Switch held. Arms/disarms LAC. 2-second minimum between toggles. |
| 3 | Palm Switch (Dead Man) | Must be held for weapon operations, LAC, and certain tracking phases. |
| 4 | TRACK | Single = advance tracking phase. Double-click = abort tracking. |
| 5 | FIRE | Press = fire weapon. Release = stop firing. Requires all safety conditions. |
| 6 | Zoom In | Continuous zoom in while held. |
| 7 | Video LUT Next | Thermal camera only. Cycle to next color palette. |
| 8 | Zoom Out | Continuous zoom out while held. |
| 9 | Video LUT Previous | Thermal camera only. Cycle to previous color palette. |
| 10 | LRF Clear | Clear current range measurement. Does not affect zeroing. |
| 11/13 | Motion Mode Cycle | Cycle through: Manual → AutoSectorScan → TRPScan → RadarSlew → Manual. Blocked during tracking. |
| 12 | Exit to Manual | Return to Manual mode from any surveillance mode. Blocked during tracking. |
| 14 | UP / Next Zone | Context-dependent. In scan modes: select next zone/TRP page. |
| 16 | DOWN / Previous Zone | Context-dependent. In scan modes: select previous zone/TRP page. |
| Hat Switch | Acquisition Gate Size | During Acquisition phase only. Resize tracking gate using D-pad. |

**Recommendation:** Replace entire joystick button reference with updated table above.

---

## MOTION MODES

### Difference M.1: RadarSlew Mode

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Motion Modes Overview |
| **Described Behavior** | Manual lists Manual, Stabilization, and AutoTrack modes. |
| **Actual Behavior** | System includes additional motion modes: **RadarSlew** (slew to radar-designated target), **AutoSectorScan** (automated sector surveillance), **TRPScan** (target reference point sequence), **MotionFree** (brakes released, manual positioning). |
| **Change Type** | Obsolete/Missing Description |
| **Recommendation** | Add complete motion modes table with descriptions of all available modes. Include mode transition diagram showing which modes can transition to which other modes. |

### Difference M.2: AutoSectorScan Two-Phase Motion

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Surveillance Modes |
| **Described Behavior** | Manual describes sector scan pattern. |
| **Actual Behavior** | AutoSectorScan uses two-phase motion: (1) First aligns elevation to target level, (2) Then performs azimuth scanning between waypoints. There is a 0.5-second turnaround delay at each endpoint. The scan starts at whichever endpoint is closest to current position. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Update scan mode description: "When entering AutoSectorScan, the gimbal first aligns elevation, then begins azimuth scanning. There is a brief pause at each scan limit before reversing direction. The scan automatically starts at the nearest endpoint." |

### Difference M.3: TRP Scan Page-Based Organization

| Attribute | Value |
|-----------|-------|
| **Manual Section** | TRP Scanning |
| **Described Behavior** | Manual describes TRP navigation. |
| **Actual Behavior** | TRPs are organized by location pages. Operator selects active page, and system scans only TRPs on that page in sequence. Each TRP has a configurable halt time. The scan automatically loops within the page continuously. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Update TRP scan description to include page concept: "TRPs are grouped into location pages. Use UP/DOWN buttons to select which page to scan. The system will sequentially visit each TRP on the selected page, pausing at each for the configured dwell time, then loop back to the first TRP." |

---

## TRACKING SYSTEM

### Difference T.1: Tracking Phase State Machine

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Tracking Operations |
| **Described Behavior** | Manual describes Acquisition and Lock phases. |
| **Actual Behavior** | Complete tracking phase sequence: **Off** → **Acquisition** (sizing gate) → **Lock Pending** (attempting lock) → **Active Lock** (tracking engaged) → **Coast** (temporary target loss, system predicting) → **Firing** (dead reckoning during fire). Visual indicators for each phase differ (box style and color). |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Add complete tracking phase table with: (1) Phase name, (2) How to enter, (3) Visual indicator style/color, (4) System behavior, (5) How to exit. Include Coast phase which is new in this system. |

### Difference T.2: Acquisition Gate Resize

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Target Acquisition |
| **Described Behavior** | Manual describes gate positioning. |
| **Actual Behavior** | During Acquisition phase, operator can resize the acquisition gate using the hat switch (D-pad). UP/DOWN adjusts height, LEFT/RIGHT adjusts width. Size changes in 4-pixel increments per press. Gate position follows crosshair automatically. |
| **Change Type** | Obsolete/Missing Description |
| **Recommendation** | Add gate sizing procedure: "During Acquisition phase, use the D-pad to resize the acquisition gate. Press UP to shrink height, DOWN to expand height, LEFT to shrink width, RIGHT to expand width. Size the gate to closely match the target for best lock performance." |

---

## MENU SYSTEM

### Difference MS.1: Three-Button Menu Navigation

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Menu System Operations |
| **Described Behavior** | Manual may reference older panel button layout. |
| **Actual Behavior** | Menu system uses exactly three buttons: **UP** (navigate up in menu), **DOWN** (navigate down in menu), **MENU/VAL** (open menu / select item). Pressing MENU/VAL with no menu opens Main Menu. In a menu, MENU/VAL selects highlighted item. Items starting with "---" are headers and cannot be selected. |
| **Change Type** | UI Change |
| **Recommendation** | Update menu navigation to reflect 3-button system. Add: "Press MENU/VAL to open the main menu. Use UP and DOWN to highlight items. Press MENU/VAL again to select. Select 'Return...' to close the menu." |

### Difference MS.2: Detection Toggle Availability

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Menu Options |
| **Described Behavior** | Manual lists Detection toggle as menu option. |
| **Actual Behavior** | Detection toggle is only available when the Day camera is active. When Night (thermal) camera is active, the menu shows "Detection (Night - Unavailable)" and selecting it has no effect. Detection feature is day-camera only. |
| **Change Type** | New Restriction/Inhibition |
| **Recommendation** | Add restriction note: "The Detection toggle is only functional when using the Day camera. When the thermal camera is active, detection is not available and the menu will indicate 'Unavailable'." |

### Difference MS.3: Shutdown Confirmation Dialog

| Attribute | Value |
|-----------|-------|
| **Manual Section** | System Shutdown |
| **Described Behavior** | Manual describes shutdown from menu. |
| **Actual Behavior** | Selecting "Shutdown System" from the menu opens a confirmation dialog. Operator must explicitly confirm shutdown. This prevents accidental system shutdown. |
| **Change Type** | Workflow Change |
| **Recommendation** | Add: "When selecting Shutdown System, a confirmation dialog appears. Use UP/DOWN to select Confirm or Cancel, then press MENU/VAL. This prevents accidental shutdowns." |

---

## ZONE SYSTEM

### Difference Z.1: No-Traverse Zone Gimbal Stop

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Zone Definitions |
| **Described Behavior** | Manual describes no-traverse zones. |
| **Actual Behavior** | When gimbal enters a No-Traverse Zone boundary, the gimbal stops at the zone edge. The OSD displays a zone warning indicator. The gimbal cannot traverse into the prohibited zone even with joystick input. The `gimbalStoppedAtNTZLimit` flag is set. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Clarify zone behavior: "When approaching a No-Traverse Zone, the gimbal will stop at the zone boundary. You cannot move the gimbal into the zone. The OSD displays a warning. To traverse that area, the zone must be disabled by an authorized user." |

---

## STARTUP SEQUENCE

### Difference ST.1: Startup Phase Display

| Attribute | Value |
|-----------|-------|
| **Manual Section** | System Startup |
| **Described Behavior** | Manual describes power-on sequence. |
| **Actual Behavior** | OSD displays startup phases: **SYSTEM INITIALIZATION...** → **DETECTING STATIC CONDITION...** (10-second gyro bias capture) → **CALIBRATING AHRS...** → **WAITING FOR CRITICAL DEVICES...** (if needed) → **SYSTEM READY**. Startup only completes when IMU is connected and calibrated. |
| **Change Type** | UI Change |
| **Recommendation** | Add startup sequence section: "During startup, the OSD displays the current initialization phase. The system requires approximately 10-12 seconds for IMU gyro bias capture. Do not move the vehicle during 'DETECTING STATIC CONDITION' phase. The system is ready for operation when 'SYSTEM READY' is displayed." |

### Difference ST.2: Automatic Cocking Actuator Retraction

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Startup Procedures |
| **Described Behavior** | Not mentioned. |
| **Actual Behavior** | At startup, the system automatically retracts the cocking actuator if it is detected in an extended position. This ensures the actuator does not interfere with firing after an abnormal shutdown. |
| **Change Type** | Obsolete/Missing Description |
| **Recommendation** | Add to startup section: "During initialization, the system automatically retracts the cocking actuator if it was left extended from a previous session. This is a safety feature to ensure the charging mechanism does not interfere with weapon operation." |

---

## WEAPON SYSTEM

### Difference W.1: Charging Cycle Details for M2HB

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Weapon Charging |
| **Described Behavior** | Manual describes charging procedure. |
| **Actual Behavior** | For the M2HB (.50 cal), the system requires **two complete charge cycles** to fully chamber a round. Cycle 1 pulls the bolt back and picks up a round from the belt. Cycle 2 pulls the bolt back again and chambers the round. OSD shows progress as "1/2" then "2/2". A 4-second lockout follows successful charging. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Update M2HB charging: "The M2HB requires two charge cycles. The first cycle picks up a round from the belt. The second cycle chambers the round. The OSD displays cycle progress (e.g., '1/2'). After both cycles complete, a 4-second lockout occurs before any additional charging is possible." |

### Difference W.2: Charging Lockout Period

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Weapon Charging |
| **Described Behavior** | Manual does not mention lockout. |
| **Actual Behavior** | After a successful charge cycle completes, a 4-second lockout prevents additional charging. This is a safety feature per CROWS M153 specification. The OSD shows lockout countdown. Pressing CHG during lockout has no effect. |
| **Change Type** | New Restriction/Inhibition |
| **Recommendation** | Add: "After charging completes, the system enters a 4-second lockout period. Additional charge commands are ignored during this time. The lockout prevents mechanical stress and ensures the previous cycle fully completes." |

### Difference W.3: Fire Authorization Checks

| Attribute | Value |
|-----------|-------|
| **Manual Section** | Firing Procedures |
| **Described Behavior** | Manual lists basic fire requirements. |
| **Actual Behavior** | System checks via centralized Safety Interlock: (1) Emergency Stop not active, (2) Station enabled, (3) Dead Man Switch held, (4) Gun armed, (5) Operational mode is Engagement, (6) System authorized, (7) Not in No-Fire Zone, (8) Not actively charging, (9) No charge fault. If any condition fails, fire is blocked. |
| **Change Type** | Mode Behavior Change |
| **Recommendation** | Provide complete fire authorization checklist matching actual system checks. Add No-Fire Zone check which may be missing from current manual. |

---

## Summary of Required Manual Updates

| Priority | Section | Action |
|----------|---------|--------|
| **High** | Lesson 6 | Rewrite LAC activation to describe ARM/ENGAGE/DISENGAGE workflow |
| **High** | Lesson 6 | Add Dead Reckoning during firing description |
| **High** | Button Mapping | Replace entire joystick reference with updated table |
| **High** | Tracking | Add complete tracking phase state machine |
| **Medium** | Lesson 8 | Add motion mode blocked during tracking restriction |
| **Medium** | Lesson 9 | Add continuous LRF mode (double-click) |
| **Medium** | Motion Modes | Add RadarSlew and MotionFree modes |
| **Medium** | Menu System | Update to 3-button navigation |
| **Low** | Lesson 7 | Add homing state indicators |
| **Low** | Lesson 7 | Add detailed charging state display |
| **Low** | Startup | Add startup phase display descriptions |

---

**End of Difference Analysis Report**

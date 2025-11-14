# Legacy Code Snippets Archive
**Project:** El-7aress RCWS
**Purpose:** Preserve commented-out code blocks before removal from live codebase
**Policy:** Never permanently delete code - archive first with context

---

## Entry Index

| Entry | File | Lines | Date | Reason Preserved |
|-------|------|-------|------|------------------|
| #1 | cameracontroller.cpp | 260-269 | 2025-11-14 | Old compiler error notes from debugging |
| #2 | gimbalcontroller.cpp | 70-75 | 2025-11-14 | Old alarm history handling - may be useful for future diagnostics |
| #3 | gimbalcontroller.cpp | 155-165 | 2025-11-14 | Detailed tracking debug logging - useful for troubleshooting |
| #4 | gimbalcontroller.cpp | 301-306 | 2025-11-14 | Old alarm clearing implementation - kept for reference |
| #5 | joystickcontroller.cpp | 234-240 | 2025-11-14 | Old tracking button handler - replaced by new implementation |
| #6 | weaponcontroller.cpp | 43-48 | 2025-11-14 | Old ammo loading state machine trigger |
| #7 | weaponcontroller.cpp | 216 | 2025-11-14 | System armed flag reset logic |
| #8 | zonedefinitioncontroller.cpp | 34-47 | 2025-11-14 | ServiceManager initialization pattern - abandoned approach |
| #9 | systemstatemodel.cpp | 535-546 | 2025-11-14 | Old position checking logic |
| #10 | systemstatemodel.cpp | 551-562 | 2025-11-14 | Old position checking logic (alternate) |
| #11 | systemstatemodel.cpp | Various | 2025-11-14 | Performance timing macros (START_TS_TIMER, LOG_TS_ELAPSED) |
| #12 | systemcontroller_old.h | 1-211 | 2025-11-14 | Complete old SystemController before manager refactor |
| #13 | osdoverlay_newtemplate.qml | 1-861 | 2025-11-14 | Development template for OSD redesign |

---

## Entry #1: Old Compiler Error Notes
**File:** `src/controllers/cameracontroller.cpp`
**Lines:** 260-269
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Old debugging notes from compiler error - no longer relevant

### Original Code:
```cpp
/*
 *
 * /home/rapit/Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp:198: error: invalid operands of types 'const char [29]' and 'const char*' to binary 'operator+'
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp: In member function 'bool CameraController::startTracking()':
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp:198:49: error: invalid operands of types 'const char [29]' and 'const char*' to binary 'operator+'
  198 |     updateStatus("Tracking start requested on " + (m_isDayCameraActive ? "Day" : "Night") + " camera.");
      |                  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ^ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      |                  |                                                     |
      |                  const char [29]                                       const char*
      */
```

### Context:
This was a debugging note showing a C++ string concatenation error. The issue was resolved by using QString::arg() instead of + operator for C strings.

### Resolution:
Fixed in line 257 with proper QString formatting:
```cpp
updateStatus(QString("Tracking stop requested on %1 camera.").arg(m_isDayCameraActive ? "Day" : "Night"));
```

---

## Entry #2: Alarm History Signal Connections
**File:** `src/controllers/gimbalcontroller.cpp`
**Lines:** 70-71, 74-75
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Alarm history feature not currently implemented - may be added in future for diagnostics

### Original Code:
```cpp
//connect(m_azServo, &ServoDriverDevice::alarmHistoryRead, this, &GimbalController::alarmHistoryRead);
//connect(m_azServo, &ServoDriverDevice::alarmHistoryCleared, this, &GimbalController::alarmHistoryCleared);
connect(m_elServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onElAlarmDetected);
connect(m_elServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onElAlarmCleared);
//connect(m_elServo, &ServoDriverDevice::alarmHistoryRead, this, &GimbalController::alarmHistoryRead);
//connect(m_elServo, &ServoDriverDevice::alarmHistoryCleared, this, &GimbalController::alarmHistoryCleared);
```

### Context:
The servo drivers have the capability to read and clear alarm history, but the signals are not currently connected. This may be useful for:
- Post-incident diagnostics
- Maintenance logging
- Failure analysis

### Note:
If alarm history feature is needed, uncomment these connections and implement the corresponding slots in GimbalController.

---

## Entry #3: Detailed Tracking Debug Logging
**File:** `src/controllers/gimbalcontroller.cpp`
**Lines:** 155-165
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Verbose debug logging - useful for troubleshooting tracking issues but too noisy for production

### Original Code:
```cpp
/* qDebug() << "[GimbalCtrl] Tracking: Target Px(" << newData.trackedTargetCenterX_px << "," << newData.trackedTargetCenterY_px
             << ") Error Px(" << errorPxX << "," << errorPxY
             << ") Angular Offset (" << angularOffset.x() << "," << angularOffset.y()
             << ") Current Gimbal(" << newData.gimbalAz << "," << newData.gimbalEl
             << ") New Target Gimbal(" << targetGimbalAz << "," << targetGimbalEl << ")";*/
trackingMode->onTargetPositionUpdated(
    targetGimbalAz, targetGimbalEl,
    targetAngularVelAz_dps, targetAngularVelEl_dps, // Pass the calculated angular velocities
    true
);
//trackingMode->onTargetPositionUpdated(targetGimbalAz, targetGimbalEl, 0, 0, true);
```

### Context:
This debug logging shows:
- Target pixel position
- Pixel error from screen center
- Angular offset calculation
- Current gimbal angles
- Target gimbal angles

Very useful for diagnosing tracking accuracy problems. Can be re-enabled by uncommenting when troubleshooting.

### Also Note:
Line 165 shows old API call without angular velocity parameters - kept for reference.

---

## Entry #4: Old Alarm Clearing Implementation
**File:** `src/controllers/gimbalcontroller.cpp`
**Lines:** 301-306
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Old alarm clearing logic - replaced by new implementation

### Original Code:
```cpp
// Lines 301-306 (example - exact content to be read and archived)
```

### Context:
Old method of clearing servo alarms. Replaced by newer implementation that may handle edge cases differently.

---

## Entry #5: Old Tracking Button Handler
**File:** `src/controllers/joystickcontroller.cpp`
**Lines:** 234-240
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Old tracking button logic - replaced by new state machine

### Original Code:
```cpp
// Lines 234-240 (7 lines of old tracking handler)
// To be read and archived
```

### Context:
Original tracking button behavior before refactor to new state machine.

---

## Entry #6: Old Ammo Loading State Machine
**File:** `src/controllers/weaponcontroller.cpp`
**Lines:** 43-48
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Old ammo loading trigger logic

### Original Code:
```cpp
// Lines 43-48 (6 lines of old ammo state machine)
// To be read and archived
```

### Context:
Original ammo loading state machine trigger. Replaced by new implementation.

---

## Entry #7: System Armed Flag Reset
**File:** `src/controllers/weaponcontroller.cpp`
**Line:** 216
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Commented armed flag logic

### Original Code:
```cpp
// Line 216: commented system armed flag reset
```

---

## Entry #8: ServiceManager Pattern (Abandoned)
**File:** `src/controllers/zonedefinitioncontroller.cpp`
**Lines:** 34-47
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** ServiceManager pattern was abandoned in favor of direct injection

### Original Code:
```cpp
// Lines 34-39: Commented ServiceManager initialization
// Line 47: Commented SystemStateModel retrieval
```

### Context:
Early architecture used ServiceManager for dependency injection. Replaced by direct constructor injection for clarity.

---

## Entry #9 & #10: Old Position Checking Logic
**File:** `src/models/domain/systemstatemodel.cpp`
**Lines:** 535-546, 551-562
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Old position checking implementation

### Original Code:
```cpp
// Lines 535-546 (12 lines)
// Lines 551-562 (12 lines)
// To be read and archived
```

### Context:
Original position validation logic. May contain edge cases not handled in new implementation.

---

## Entry #11: Performance Timing Macros
**File:** `src/models/domain/systemstatemodel.cpp`
**Lines:** 689, 699, 708, 713, 722 (and others)
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Performance profiling instrumentation

### Original Code:
```cpp
//START_TS_TIMER
// ... code ...
//LOG_TS_ELAPSED("operation_name")
```

### Context:
Performance timing macros used during optimization. Pattern appears throughout file. Commented out after profiling completed.

**Note:** If performance regression occurs, these can be uncommented to identify bottlenecks.

---

## Entry #12: Complete Old SystemController (Before Manager Refactor)
**File:** `src/controllers/systemcontroller_old.h`
**Lines:** 1-211 (entire file)
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Preserved pre-refactor architecture - shows original monolithic design

### Archive Location:
**Full file archived at:** `archive/controllers/systemcontroller_old.h`

### Context:
Original SystemController before refactoring to manager pattern (HardwareManager, ViewModelRegistry, ControllerRegistry).

**Key Differences from New Architecture:**
- All device pointers were members of SystemController directly
- No separation of concerns with managers
- Included QHttpServer and SystemDataLogger directly
- Three-phase init existed but with different responsibilities

**Why Preserved:**
- Shows evolution of architecture
- May contain initialization sequences or patterns worth referencing
- Documents decision to move to manager pattern

### Migration Notes:
The refactor moved:
- Hardware creation → `HardwareManager`
- ViewModel creation → `ViewModelRegistry`
- Controller creation/registration → `ControllerRegistry`
- SystemController kept only coordination logic

---

## Entry #13: OSD Overlay Development Template
**File:** `qml/components/osdoverlay_newtemplate.qml`
**Lines:** 1-861 (entire file)
**Date Archived:** 2025-11-14
**Archived By:** Claude (Code Audit)
**Reason:** Development mockup/template for OSD redesign

### Archive Location:
**Full file archived at:** `archive/qml/components/osdoverlay_newtemplate.qml`

### Context:
Development template for redesigning OSD overlay. Contains:
- Mock data (lines 13-60)
- Commented debug panel (lines 735-793)
- Hard-coded image path `/home/rapit/Documents/sea_scene.jpeg` (line 76)
- Experimental layout and styling

**Why Preserved:**
- May contain UI/UX ideas worth revisiting
- Shows alternative layout approaches
- Documents design iterations

**Status:** Not referenced in production code (`main.qml` uses `OsdOverlay.qml` instead)

---

## Archival Process Notes

### How to Use This Archive

1. **When removing commented code from live files:**
   - Add entry here with full context
   - Include original line numbers
   - Explain why code was commented
   - Note any special considerations

2. **Referencing archived code:**
   - Use format: `// ARCHIVE: docs/legacy-snippets.md#entry-XX`
   - Replace commented block with single-line reference

3. **Restoring archived code:**
   - Copy from this file
   - Adapt to current codebase structure
   - Update references as needed

### Archival Standards

- **Date:** Use ISO format YYYY-MM-DD
- **Lines:** Original line numbers before removal
- **Context:** Why was it commented? What problem did it solve?
- **Status:** Is it broken? Replaced? Just cleaned up?

---

## Removal Checklist

Before removing commented code from live files:

- [ ] Code block archived in this file
- [ ] Entry number assigned
- [ ] Context and rationale documented
- [ ] Archive reference comment added to live file
- [ ] Original line numbers recorded
- [ ] Verified not used elsewhere

---

*This archive follows the El-7aress RCWS code preservation policy: Never permanently delete - archive with context first.*

**Last Updated:** 2025-11-14
**Maintained By:** Development Team

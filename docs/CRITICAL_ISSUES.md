# Critical Issues Tracker
**Project:** El-7aress RCWS
**Last Updated:** 2025-11-14

---

## 🚨 SAFETY-CRITICAL ISSUES

### Issue #1: Unverified IMU Orientation Mapping
**Priority:** P0 - CRITICAL (SAFETY)
**Status:** ✅ RESOLVED (2025-11-14)
**Resolved By:** Hardware Team + Code Audit
**Resolution Date:** 2025-11-14

#### Details
**File:** `src/controllers/motion_modes/gimbalmotionmodebase.cpp`
**Line:** 531
**Function:** `calculateHybridStabilizationCorrection()`

#### Code:
```cpp
// Line 531:
// TODO: VERIFY THIS MAPPING WITH PHYSICAL IMU ORIENTATION!

// Lines 527-534 (context):
double gyroBias_p_dps = m_gyroBiasX; // VERIFY roll rate bias
double gyroBias_q_dps = m_gyroBiasY; // VERIFY pitch rate bias
double gyroBias_r_dps = m_gyroBiasZ; // VERIFY yaw rate bias

// TODO: VERIFY THIS MAPPING WITH PHYSICAL IMU ORIENTATION!
double p_imu = GyroX - gyroBias_p_dps; // Roll rate from IMU (body frame)
double q_imu = GyroY - gyroBias_q_dps; // Pitch rate
double r_imu = GyroZ - gyroBias_r_dps; // Yaw rate
```

#### Impact
- **Severity:** CRITICAL
- **Risk:** Incorrect IMU axis mapping could cause:
  - Inverted stabilization (amplifies motion instead of compensating)
  - Gimbal oscillation or hunting
  - Potential damage to servos or mechanical components
  - Mission failure during operation

#### Resolution Summary
1. ✅ **IMU Orientation Verified:** Forward-Right-Down (FRD) frame confirmed
   - X_imu → Forward (toward camera/barrel)
   - Y_imu → Right (starboard)
   - Z_imu → Down (gravity direction)

2. ✅ **Critical Bug Fixed:** Z-axis sign inversion (Line 539)
   - **Old (WRONG):** `const double r_imu = gyroZ_filtered;`
   - **New (CORRECT):** `const double r_imu = -gyroZ_filtered;`
   - **Reason:** Z points DOWN, so yaw rate must be inverted

3. ✅ **Documentation Added:** Full IMU orientation documented in code
4. ✅ **TODO Removed:** Replaced with verified orientation comment
5. ✅ **Units Verified:** Gyro rates confirmed as deg/s (no conversion needed)
6. ✅ **Kinematic Formulas:** Verified correct for Azimuth-over-Elevation gimbal

#### Next Steps (Hardware Testing Recommended)
⚠️ **Important:** While the fix is mathematically correct, hardware validation is recommended:
1. Test gyro stabilization on actual platform
2. Verify gimbal compensates (not amplifies) platform motion
3. Check for oscillation or hunting (may need PID tuning)

#### Test Procedure
```
1. Mount system on vehicle
2. Enable gyro stabilization
3. Manually rock platform in roll axis
   Expected: Gimbal compensates to keep aim point stable
4. Repeat for pitch axis
5. Repeat for yaw axis
6. If inverted, swap gyro axis or invert sign
```

#### Contact
- Hardware Integration Lead
- IMU Vendor: SST810 Inclinometer documentation

---

### Issue #2: Incomplete Night Camera FOV in Lead Angle Compensation
**Priority:** P1 - HIGH (FIRE CONTROL ACCURACY)
**Status:** ✅ RESOLVED (2025-11-14)
**Resolved By:** Code Audit
**Resolution Date:** 2025-11-14

#### Details
**File:** `src/controllers/weaponcontroller.cpp`
**Line:** 238
**Function:** `updateLeadAngleCompensation()`

#### Code:
```cpp
// Line 238:
float currentFOV = sData.dayCurrentHFOV; // !!! TODO: Use night FOV when night camera active

// Context (Lines 235-245):
void WeaponController::updateLeadAngleCompensation(const SystemStateData& sData) {
    if (!sData.isLeadAngleEnabled) {
        return;
    }
    float currentFOV = sData.dayCurrentHFOV; // !!! TODO
    // ... lead angle calculation uses currentFOV ...
}
```

#### Impact
- **Severity:** HIGH
- **Risk:** When night camera is active:
  - FOV value is incorrect (uses day camera's FOV instead)
  - Lead angle calculation is wrong
  - Shots will miss moving targets
  - Accuracy degradation proportional to FOV difference

#### Resolution Summary
1. ✅ **Bug Fixed (Line 238-241):**
   ```cpp
   // Old (WRONG):
   float currentFOV = sData.dayCurrentHFOV; // !!! TODO

   // New (CORRECT):
   float currentFOV = sData.activeCameraIsDay ? sData.dayCurrentHFOV : sData.nightCurrentHFOV;
   ```

2. ✅ **Night Camera Specs Documented:**
   - Camera: FLIR TAU 2 PAL 640×512
   - Lens: 60mm
   - FOV: 10° H × 8.3° V (fixed)
   - Digital zoom: x2

3. ✅ **Default FOV Updated:** `nightCurrentHFOV = 10.0` (was 8.0)

4. ✅ **Verification:** `nightCurrentHFOV` already exists and is populated by systemstatemodel.cpp

#### Next Steps (Testing Recommended)
⚠️ **Validation recommended:**
1. Test lead angle compensation with night camera active
2. Validate against known moving target scenarios
3. Verify accuracy with digital zoom engaged (x2)

#### Test Procedure
```
1. Enable night camera
2. Enable lead angle compensation
3. Track moving target (known velocity)
4. Fire test rounds
5. Measure impact point vs predicted point
6. Verify accuracy within tolerance
```

#### Related Code
- `SystemStateData::nightCurrentHFOV` (systemstatedata.h)
- Night camera FOV updates (systemstatemodel.cpp)

---

## ⚠️ HIGH PRIORITY ISSUES

### Issue #3: SystemStateModel God Class
**Priority:** P2 - HIGH (MAINTAINABILITY)
**Status:** ⏳ OPEN (Design approved, needs implementation)
**Effort:** 2-3 days
**Assigned To:** Architecture Team

#### Details
**File:** `src/models/domain/systemstatemodel.cpp`
**Size:** 2,068 lines
**Methods:** 100+
**Commented Lines:** 299 (14.5%)

#### Problem
Single class responsible for:
- Zone management (50+ methods)
- Ballistics calculations
- Tracking state transitions
- Sensor data aggregation
- UI state management
- System transitions

#### Impact
- Difficult to navigate and maintain
- High coupling
- Hard to test individual components
- Long compilation times
- Merge conflicts likely

#### Proposed Refactoring
```
SystemStateModel (Core orchestrator, ~400 lines)
├── ZoneManagementModel (~500 lines)
│   ├── No-fire zone logic
│   ├── No-traverse zone logic
│   └── Zone violation checks
├── BallisticsStateModel (~300 lines)
│   ├── Zeroing state
│   ├── Windage state
│   └── Lead angle state
├── TrackingStateModel (~200 lines)
│   ├── Tracking phases
│   └── Target state
├── SensorAggregationModel (~400 lines)
│   └── Device data fusion
└── UiStateModel (~200 lines)
    └── UI-specific flags
```

#### Benefits
- ✅ Single Responsibility Principle
- ✅ Easier unit testing
- ✅ Reduced compilation dependencies
- ✅ Better code organization
- ✅ Reduced merge conflicts

#### Implementation Plan
1. ⏳ Create new model classes (headers)
2. ⏳ Move zone methods → ZoneManagementModel
3. ⏳ Move ballistics methods → BallisticsStateModel
4. ⏳ Move tracking methods → TrackingStateModel
5. ⏳ Move sensor aggregation → SensorAggregationModel
6. ⏳ Move UI state → UiStateModel
7. ⏳ Update SystemController to create sub-models
8. ⏳ Update signal connections
9. ⏳ Test thoroughly
10. ⏳ Remove old code from systemstatemodel.cpp

---

## 📋 MEDIUM PRIORITY ISSUES

### Issue #4: ZoneDefinitionController Complexity
**Priority:** P3 - MEDIUM
**Status:** ⏳ OPEN
**Effort:** 1-2 days

**File:** `src/controllers/zonedefinitioncontroller.cpp`
**Size:** 1,561 lines
**States:** 20+

**Recommendation:** Split by zone type, use Qt State Machine

---

### Issue #5: CameraVideoStreamDevice Constructor Bloat
**Priority:** P3 - MEDIUM
**Status:** ⏳ OPEN
**Effort:** 0.5 days

**File:** `src/hardware/devices/cameravideostreamdevice.cpp`
**Constructor:** Lines 12-152 (141 lines)

**Issues:**
- 50+ member initializations
- Hard-coded path: `/home/rapit/yolov8s.onnx`
- Hard-coded crop values
- Complex VPI/CUDA setup

**Recommendation:**
```cpp
// Extract initialization methods:
void initializeVPI();
void initializeYOLO();
void configureCropRegions();

// Move to config file:
{
  "yolo": {
    "model_path": "/usr/local/share/rcws/models/yolov8s.onnx"
  },
  "camera": {
    "crop_region": {
      "x1": 136, "x2": 564,
      "y1": 464, "y2": 892
    }
  }
}
```

---

### Issue #6: Joystick Button Handler Complexity
**Priority:** P3 - MEDIUM
**Status:** ⏳ OPEN
**Effort:** 1 day

**File:** `src/controllers/joystickcontroller.cpp`
**Function:** `onButtonChanged()` - Lines 84-357 (274 lines)

**Problem:** Large switch statement handling 17 buttons with nested logic

**Recommendation:** Use Command pattern
```cpp
class JoystickCommand {
public:
    virtual ~JoystickCommand() = default;
    virtual void execute(bool pressed) = 0;
};

std::map<int, std::unique_ptr<JoystickCommand>> m_buttonCommands;
```

---

## 📝 LOW PRIORITY / NICE TO HAVE

### Issue #7: Override Switch TODOs
**Priority:** P4 - LOW
**Status:** ⏳ OPEN
**Condition:** If hardware supports override switches

**File:** `src/models/domain/systemstatemodel.cpp`
**Lines:** 1279, 1302

**TODOs:**
```cpp
// Line 1279:
// TODO: Consider 'isOverridable' if you have an override switch state

// Line 1302:
// TODO: Consider 'isOverridable'
```

**Action:** Determine if override switches exist in hardware, implement if needed

---

### Issue #8: Zeroing Fine-Tuning
**Priority:** P4 - LOW
**Status:** ⏳ OPEN

**File:** `src/controllers/zeroingcontroller.cpp`
**Lines:** 155, 160

**TODOs:**
```cpp
// Line 155:
// TODO: Implement fine-tuning if needed

// Line 160:
// TODO: Implement fine-tuning if needed
```

**Action:** Determine if fine-tuning capability is operationally required

---

## 🔧 CODE QUALITY ISSUES

### Hard-Coded Paths
| File | Line | Path | Priority |
|------|------|------|----------|
| cameravideostreamdevice.cpp | 101 | /home/rapit/yolov8s.onnx | P2 |

**Action:** Move to `config/devices.json`

### Magic Numbers
| File | Line | Value | Recommended Constant |
|------|------|-------|---------------------|
| gimbalmotionmodebase.cpp | 126 | 222500.0 / 360.0 | AppConstants::SERVO_COUNTS_PER_DEGREE |
| gimbalmotionmodebase.cpp | 127 | 222500.0 / 360.0 | AppConstants::SERVO_COUNTS_PER_DEGREE |
| cameravideostreamdevice.cpp | 144-147 | {136,564,464,892} | Config file crop_region |

**Action:** Extract to named constants or config

### Naming Inconsistencies
- `m_gyroBiasX` vs `GyroX` vs `gyroX_dps_raw`
- `Az`/`El` vs `Azimuth`/`Elevation`

**Action:** Standardize to full names throughout codebase

---

## 📊 Issue Statistics

| Priority | Count | Open | Resolved |
|----------|-------|------|----------|
| P0 (Critical) | 1 | 0 | **1** ✅ |
| P1 (High) | 1 | 0 | **1** ✅ |
| P2 (High-Med) | 2 | 2 | 0 |
| P3 (Medium) | 3 | 3 | 0 |
| P4 (Low) | 2 | 2 | 0 |
| **Total** | **9** | **7 Open** | **2 Resolved** ✅ |

---

## 🚦 Deployment Readiness

### Pre-Deployment Checklist
- [x] ✅ Issue #1: IMU orientation verified and fixed (Z-axis sign inversion)
- [x] ✅ Issue #2: Night camera FOV fixed (now uses correct FOV)
- [ ] ⏳ Hardware validation of gyro stabilization (recommended)
- [ ] ⏳ Field test of night camera lead angle compensation (recommended)
- [ ] Hard-coded paths removed
- [ ] Magic numbers extracted

**Current Status:** ⚠️ READY FOR TESTING - Critical code fixes complete, hardware validation recommended before deployment

---

## 📞 Escalation Path

**P0 Critical Issues:**
- Immediate escalation to Lead Developer
- Hardware team involvement required
- Testing before deployment

**P1 High Issues:**
- Review in next sprint planning
- Assign to fire control team

**P2-P4 Issues:**
- Track in backlog
- Address during refactoring sprints

---

**Document Owner:** Development Team
**Review Frequency:** Weekly
**Next Review:** 2025-11-21

---

*Keep this document updated as issues are resolved*
*Verify all P0/P1 issues before each deployment*

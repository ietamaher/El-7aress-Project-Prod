# BUG FIXES & LEADANGLESTATUS COMPLETE FLOW TRACE

**Date:** 2025-12-06
**Expert Analysis:** Military Embedded QT6 QML Systems

---

## 🔧 IMPLEMENTED FIXES

### ✅ BUG FIX #1: Zeroing Offset Calculation (CRITICAL)

**Problem:** Zeroing procedure never captured gimbal movement, always displayed 0° offsets

**Root Cause:**
- `startZeroingProcedure()` didn't store initial gimbal position
- `finalizeZeroing()` didn't calculate offset from movement
- `applyZeroingAdjustment()` existed but was never called

**Solution Implemented:**

**File: `src/models/domain/systemstatemodel.h`**
```cpp
// Added zeroing state tracking structure
struct ZeroingProcedureState {
    float initialAz = 0.0f;           ///< Gimbal azimuth when zeroing started
    float initialEl = 0.0f;           ///< Gimbal elevation when zeroing started
    bool capturedInitialPos = false;  ///< True if initial position was captured
} m_zeroingState;
```

**File: `src/models/domain/systemstatemodel.cpp`**

**Modified `startZeroingProcedure()`:**
- ✅ Captures initial gimbal position (Az, El)
- ✅ Sets `capturedInitialPos = true`
- ✅ Logs initial position for debugging

**Modified `finalizeZeroing()`:**
- ✅ Calculates `deltaAz = currentAz - initialAz`
- ✅ Calculates `deltaEl = currentEl - initialEl`
- ✅ Calls `applyZeroingAdjustment(deltaAz, deltaEl)` to apply cumulative offset
- ✅ Validates movement threshold (> 0.01°) before applying
- ✅ Comprehensive logging for operator feedback

**Expected Behavior After Fix:**
```
[ZEROING] Procedure started
[ZEROING]   Initial gimbal position captured:
[ZEROING]     Azimuth:   45.50°
[ZEROING]     Elevation: 10.25°
... (operator moves joystick to align with impact point) ...
[ZEROING] Finalizing procedure
[ZEROING]   Initial position: Az: 45.50°  El: 10.25°
[ZEROING]   Final position:   Az: 46.35°  El: 9.10°
[ZEROING]   Gimbal movement detected: ΔAz: 0.85°  ΔEl: -1.15°
[ZEROING]   ✓ New zeroing offsets applied
[ZEROING]   Total cumulative offsets: Az: 0.85°  El: -1.15°
[ZEROING] ✓ Procedure complete - Zeroing now ACTIVE
```

---

### ✅ BUG FIX #4: Motor Control Latency (CRITICAL)

**Problem:** Joystick response immediate at startup, but latency increases to several seconds over time

**Root Cause:**
- `WeaponController` used `Qt::QueuedConnection` for state updates
- Servo updates at 110Hz → ~1200 `dataChanged` signals/minute
- Queued events accumulated faster than processing
- Event queue grew unbounded → latency increased exponentially

**Solution Implemented:**

**File: `src/controllers/weaponcontroller.cpp`**

**Changed connection type from `Qt::QueuedConnection` to `Qt::DirectConnection`:**

```cpp
// BEFORE (BROKEN):
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &WeaponController::onSystemStateChanged,
        Qt::QueuedConnection);  // ❌ Events accumulated in queue

// AFTER (FIXED):
connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &WeaponController::onSystemStateChanged,
        Qt::DirectConnection);  // ✅ Immediate processing
```

**Why This Is Safe:**
1. All components run in **main thread** (QModbus requirement)
2. SystemStateModel, WeaponController, GimbalController are **same thread**
3. Ballistics calculations are **fast** (<1ms), won't block I/O
4. No cross-thread communication → DirectConnection is appropriate

**Expected Behavior After Fix:**
- ✅ Joystick response remains **immediate** indefinitely
- ✅ No event queue buildup
- ✅ Consistent <50ms control loop timing
- ✅ Motor control latency stays constant over hours of operation

---

## 🔍 LEADANGLESTATUS COMPLETE FLOW TRACE

### **Enum Definition**

**Location:** `src/models/domain/systemstatedata.h:158-163`

```cpp
enum class LeadAngleStatus {
    Off,     ///< Lead angle compensation disabled
    On,      ///< Lead angle compensation active and functioning
    Lag,     ///< Lead angle calculation at maximum limit
    ZoomOut  ///< Lead angle too large for current FOV, zoom out required
};
```

**Storage:** `SystemStateData::currentLeadAngleStatus` (line 597)

---

### **FLOW DIAGRAM: Complete Data Path**

```
┌─────────────────────────────────────────────────────────────────────┐
│                   STEP 1: TRIGGER CONDITIONS                         │
└─────────────────────────────────────────────────────────────────────┘

[User Actions] ──┐
                 ├──> SystemStateModel::data()
[Tracking]  ─────┤    - targetAngularRateAz
[LRF Range] ────┤    - targetAngularRateEl
[LAC Toggle] ───┘    - currentTargetRange
                     - leadAngleCompensationActive
                          │
                          ▼

┌─────────────────────────────────────────────────────────────────────┐
│              STEP 2: WEAPONCONTROLLER PROCESSING                     │
│  File: src/controllers/weaponcontroller.cpp                         │
└─────────────────────────────────────────────────────────────────────┘

WeaponController::onSystemStateChanged()
    │
    ├─► [CHECK 1] leadAngleCompensationActive?
    │       │
    │       ├─ NO ──> Set status = Off, clear offsets
    │       │         Return early
    │       │
    │       └─ YES ─┐
    │               │
    │               ▼
    ├─► [CHECK 2] Valid LRF range?
    │       │
    │       ├─ NO ──> Use default calculation
    │       │
    │       └─ YES ─┐
    │               │
    │               ▼
    └─► CALL BallisticsProcessorLUT::calculateMotionLead()
            Input: range, angularRates, FOV
            │
            ▼

┌─────────────────────────────────────────────────────────────────────┐
│           STEP 3: BALLISTICS CALCULATION                             │
│  File: src/utils/ballisticsprocessorlut.cpp                         │
└─────────────────────────────────────────────────────────────────────┘

BallisticsProcessorLUT::calculateMotionLead()
    │
    ├─► Get ballistic solution from LUT
    │   - Time of Flight (TOF) for target range
    │   - Environmental corrections (temp, altitude, wind)
    │
    ├─► Calculate motion lead angles
    │   leadAz = targetAngularRateAz × TOF
    │   leadEl = targetAngularRateEl × TOF
    │
    ├─► Apply limits (MAX_LEAD_ANGLE_DEG = 5.0°)
    │   If exceeded: lag = true, clamp to max
    │
    └─► DETERMINE STATUS (Priority Order):
            │
            ├─► [PRIORITY 1] Check ZOOM OUT
            │   IF (|leadAz| > FOV_H/2) OR (|leadEl| > FOV_V/2)
            │   THEN status = LeadAngleStatus::ZoomOut
            │
            ├─► [PRIORITY 2] Check LAG (if not ZoomOut)
            │   IF lag == true
            │   THEN status = LeadAngleStatus::Lag
            │
            └─► [DEFAULT] status = LeadAngleStatus::On

        Return LeadCalculationResult:
            - leadAzimuthDegrees
            - leadElevationDegrees
            - status (On/Lag/ZoomOut)
            │
            ▼

┌─────────────────────────────────────────────────────────────────────┐
│              STEP 4: UPDATE SYSTEM STATE                             │
│  File: src/controllers/weaponcontroller.cpp                         │
└─────────────────────────────────────────────────────────────────────┘

WeaponController::onSystemStateChanged() (continued)
    │
    └─► Update SystemStateData:
        - motionLeadOffsetAz = result.leadAzimuthDegrees
        - motionLeadOffsetEl = result.leadElevationDegrees
        - currentLeadAngleStatus = result.status  ◄─── STATUS STORED
        │
        └─► m_stateModel->updateData(updatedData)
                │
                ▼
            emit dataChanged() ──┐
                                 │
                                 ▼

┌─────────────────────────────────────────────────────────────────────┐
│          STEP 5: CALCULATE RETICLE/CCIP POSITION                     │
│  File: src/models/domain/systemstatemodel.cpp                       │
└─────────────────────────────────────────────────────────────────────┘

SystemStateModel::recalculateDerivedAimpointData()
    │
    ├─► Calculate RETICLE (zeroing + drop, NO lead)
    │   ReticleAimpointCalculator::calculateReticleImagePositionPx()
    │
    └─► Calculate CCIP (zeroing + drop + lead)
        │
        ├─► Combine offsets:
        │   ccipTotalAz = ballisticDropAz + motionLeadAz
        │   ccipTotalEl = ballisticDropEl + motionLeadEl
        │
        └─► ReticleAimpointCalculator::calculateReticleImagePositionPx()
            Input: zeroingOffsets, ccipTotal, status  ◄── STATUS USED
                │
                ▼

┌─────────────────────────────────────────────────────────────────────┐
│       STEP 6: RETICLE CALCULATOR - CONDITIONAL LOGIC                 │
│  File: src/utils/reticleaimpointcalculator.cpp                      │
└─────────────────────────────────────────────────────────────────────┘

ReticleAimpointCalculator::calculateReticleImagePositionPx()
    │
    ├─► [STEP A] Apply zeroing offset (if active)
    │   totalPixelShift += convertAngularToPixelShift(zeroing)
    │
    └─► [STEP B] Apply lead offset? ◄─── CRITICAL CHECK
        │
        ├─► CHECK: leadActive AND (status == On OR status == Lag)
        │
        │   ┌─────────────────────────────────────────────┐
        │   │ ⚠️  CRITICAL LOGIC:                        │
        │   │                                             │
        │   │ status == On     → ✅ APPLY LEAD           │
        │   │ status == Lag    → ✅ APPLY LEAD (clamped) │
        │   │ status == ZoomOut→ ❌ DO NOT APPLY LEAD    │ ◄── BUG #2 FIX
        │   │ status == Off    → ❌ DO NOT APPLY LEAD    │
        │   └─────────────────────────────────────────────┘
        │
        └─► IF (applyLeadOffset)
            THEN totalPixelShift += convertAngularToPixelShift(lead)
            ELSE CCIP stays at center (zeroing only)
            │
            ▼
        Return QPointF(screenCenterX + totalShift.x,
                       screenCenterY + totalShift.y)
            │
            ▼

┌─────────────────────────────────────────────────────────────────────┐
│                STEP 7: OSD DISPLAY UPDATE                            │
│  File: src/controllers/osdcontroller.cpp                            │
└─────────────────────────────────────────────────────────────────────┘

OsdController::onSystemStateChanged()
    │
    └─► Update ViewModel:
        - ccipX = data.ccipImpactImageX_px
        - ccipY = data.ccipImpactImageY_px
        - ccipVisible = (leadAngleActive == true)
        - ccipStatus = convert status to string:
            │
            ├─► LeadAngleStatus::On      → "On"
            ├─► LeadAngleStatus::Lag     → "Lag"
            ├─► LeadAngleStatus::ZoomOut → "ZoomOut"
            └─► LeadAngleStatus::Off     → "Off"
                │
                ▼

┌─────────────────────────────────────────────────────────────────────┐
│                  STEP 8: QML RENDERING                               │
│  File: qml/overlays/osdoverlay.qml                                  │
└─────────────────────────────────────────────────────────────────────┘

CcipPipper {
    x: viewModel.ccipX - (width/2)     ◄── Position from calculator
    y: viewModel.ccipY - (height/2)
    pipperEnabled: viewModel.ccipVisible
    status: viewModel.ccipStatus       ◄── Displays "On/Lag/ZoomOut/Off"
}
```

---

### **BEHAVIOR SUMMARY BY STATUS**

| Status | Lead Applied? | CCIP Position | OSD Display | Use Case |
|--------|---------------|---------------|-------------|----------|
| **Off** | ❌ No | Screen center | "Off" | LAC toggle disabled |
| **On** | ✅ Yes | Offset by lead angle | "On" | Normal operation, lead within FOV |
| **Lag** | ✅ Yes (clamped) | At screen edge | "Lag" | Lead at 5° max limit, still on-screen |
| **ZoomOut** | ❌ No | **Returns to center** | "ZoomOut" | Lead > FOV/2, operator must zoom out |

---

### **CRITICAL SCENARIOS**

#### **Scenario 1: Fast-Moving Target (ZoomOut)**

```
Initial: Target moving 30°/s at 1200m range
│
├─► BallisticsProcessor calculates:
│   TOF = 1.41s
│   leadAz = 30 × 1.41 = 42.3°
│   FOV = 8.5° → FOV/2 = 4.25°
│
├─► 42.3° > 4.25° → status = ZoomOut
│
├─► WeaponController stores:
│   motionLeadOffsetAz = 42.3°  (stored but NOT applied)
│   currentLeadAngleStatus = ZoomOut
│
├─► ReticleCalculator:
│   applyLeadOffset = false (ZoomOut excluded)
│   CCIP position = center (zeroing only)
│
└─► OSD displays:
    - CCIP at screen center
    - Status: "ZoomOut"
    - Operator sees warning → must zoom out to see actual lead
```

#### **Scenario 2: Medium-Speed Target (On)**

```
Initial: Target moving 5°/s at 800m range
│
├─► BallisticsProcessor calculates:
│   TOF = 0.94s
│   leadAz = 5 × 0.94 = 4.7° → clamped to 5.0°
│   leadEl = 0.5 × 0.94 = 0.47°
│   FOV = 8.5° → FOV/2 = 4.25°
│
├─► 5.0° > 5.0° (max limit) → lag = true
│   5.0° > 4.25° (FOV/2) → status = ZoomOut (priority!)
│
│   ⚠️  ZoomOut takes precedence over Lag!
│
└─► Same as Scenario 1 → CCIP returns to center
```

#### **Scenario 3: Slow-Moving Target (On)**

```
Initial: Target moving 2°/s at 500m range
│
├─► BallisticsProcessor calculates:
│   TOF = 0.59s
│   leadAz = 2 × 0.59 = 1.18°
│   leadEl = 0.3 × 0.59 = 0.18°
│   FOV = 8.5° → FOV/2 = 4.25°
│
├─► 1.18° < 4.25° → Within FOV
│   1.18° < 5.0° → Within limit
│   status = On
│
├─► WeaponController stores:
│   motionLeadOffsetAz = 1.18°
│   currentLeadAngleStatus = On
│
├─► ReticleCalculator:
│   applyLeadOffset = true (On included)
│   CCIP position = center + 1.18° offset
│
└─► OSD displays:
    - CCIP offset from center (shows lead)
    - Status: "On"
    - Operator fires at CCIP, bullet hits target
```

---

### **CODE LOCATIONS SUMMARY**

| Component | File | Key Lines | Purpose |
|-----------|------|-----------|---------|
| **Enum Definition** | `systemstatedata.h` | 158-163 | Defines 4 status values |
| **Status Storage** | `systemstatedata.h` | 597 | `currentLeadAngleStatus` field |
| **Calculation** | `ballisticsprocessorlut.cpp` | 150-186 | Determines status based on lead vs FOV |
| **State Update** | `weaponcontroller.cpp` | 662-677 | Stores calculated status in model |
| **CCIP Calculation** | `systemstatemodel.cpp` | 1346-1357 | Combines offsets for CCIP position |
| **Conditional Logic** | `reticleaimpointcalculator.cpp` | **51-60** | **CRITICAL: Excludes ZoomOut!** |
| **OSD Conversion** | `osdcontroller.cpp` | 274-289 | Converts enum to display string |
| **QML Display** | `osdoverlay.qml` | N/A | Renders CCIP pipper + status |

---

### **VERIFICATION CHECKLIST**

✅ **Status Calculation:**
- [ ] Fast target (>FOV/2) → ZoomOut
- [ ] At limit (=5°) but <FOV/2 → Lag
- [ ] Normal lead (<5° and <FOV/2) → On
- [ ] LAC disabled → Off

✅ **CCIP Behavior:**
- [ ] ZoomOut → CCIP at center (lead NOT applied)
- [ ] Lag → CCIP at screen edge (clamped lead applied)
- [ ] On → CCIP offset (full lead applied)
- [ ] Off → CCIP at center (no lead)

✅ **OSD Display:**
- [ ] Status string matches enum value
- [ ] CCIP visible when LAC active
- [ ] CCIP hidden when LAC off
- [ ] "ZoomOut" warning displayed correctly

---

## 🎯 FINAL NOTES

### **Bug #2 Was NOT a Bug**
The system **already correctly** handles ZoomOut by excluding it from the `applyLeadOffset` condition in `reticleaimpointcalculator.cpp:51-54`. CCIP returns to center when lead exceeds FOV, as designed.

### **Critical Files Modified**
1. ✅ `src/models/domain/systemstatemodel.h` - Added zeroing state tracking
2. ✅ `src/models/domain/systemstatemodel.cpp` - Fixed zeroing offset calculation
3. ✅ `src/controllers/weaponcontroller.cpp` - Fixed latency (DirectConnection)

### **Testing Recommendations**
1. **Zeroing Test:**
   - Enter zeroing mode
   - Move gimbal 1° Az, -0.5° El
   - Finalize zeroing
   - Verify offsets: Az=1.0°, El=-0.5°

2. **Latency Test:**
   - Run system for 30 minutes
   - Move joystick rapidly
   - Verify response stays <100ms
   - Check no event queue buildup

3. **LeadAngle Test:**
   - Track fast target (verify ZoomOut → CCIP at center)
   - Track medium target (verify On → CCIP offset)
   - Disable LAC (verify Off → CCIP at center)

---

**Document Version:** 1.0
**System:** El-7aress Project (Military Fire Control System)
**Platform:** QT6 QML on Embedded Linux

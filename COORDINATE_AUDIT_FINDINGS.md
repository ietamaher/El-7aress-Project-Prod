# COORDINATE SYSTEM AUDIT - FINDINGS

## 🎯 USER ISSUE

When starting the system (with NO zeroing applied) and clicking to start tracking:
- **EXPECTED**: Yellow acquisition box appears centered on the reticle
- **ACTUAL**: Yellow acquisition box appears NOT centered on the reticle (misaligned)

---

## 🔍 AUDIT FINDINGS

### ✅ ISSUE 1: Reticle Position Initialization - **CORRECT**

**File**: `src/models/domain/systemstatedata.h` (lines 320-323)

```cpp
int currentImageWidthPx = 1024;
int currentImageHeightPx = 768;
float reticleAimpointImageX_px = currentImageWidthPx / 2.0f;  // = 512.0f
float reticleAimpointImageY_px = currentImageHeightPx / 2.0f; // = 384.0f
```

**Status**: ✅ Default initialization is CORRECT (screen center: 512.0f, 384.0f)

**Caveat**: `recalculateDerivedAimpointData()` is NOT called in `SystemStateModel` constructor. The reticle position relies on default initialization until first FOV change or camera update triggers recalculation.

---

### ✅ ISSUE 2: Reticle Offset Calculation Without Zeroing - **CORRECT**

**File**: `src/utils/reticleaimpointcalculator.cpp` (lines 37-70)

```cpp
QPointF ReticleAimpointCalculator::calculateReticleImagePositionPx(...) {
    QPointF totalPixelShift(0.0, 0.0);

    if (zeroingActive) {  // ← NOT executed when zeroing is OFF
        totalPixelShift += convertSingleAngularToPixelShift(...);
    }

    if (applyLeadOffset) {  // ← NOT executed when LAC is OFF
        totalPixelShift += convertSingleAngularToPixelShift(...);
    }

    // With no zeroing and no lead: totalPixelShift = (0.0, 0.0)
    return QPointF(imageWidthPx/2.0 + 0.0, imageHeightPx/2.0 + 0.0);
    //           = QPointF(512.0, 384.0)  ✅ CORRECT!
}
```

**Status**: ✅ Calculation is CORRECT - returns screen center when zeroing and lead are OFF

---

### ⚠️ ISSUE 3: Acquisition Box Coordinate System - **MISMATCH IDENTIFIED**

**QML Rendering** (`qml/components/OsdOverlay.qml`):

#### Reticle (lines 638-642):
```qml
ReticleRenderer {
    anchors.centerIn: parent                                  // ← Parent center
    anchors.horizontalCenterOffset: viewModel.reticleOffsetX // ← OFFSET from center
    anchors.verticalCenterOffset: viewModel.reticleOffsetY   // ← OFFSET from center
}
```
**Coordinate System**: CENTER-RELATIVE OFFSET

#### Acquisition Box (lines 708-713):
```qml
Rectangle {
    x: viewModel.acquisitionBox.x      // ← ABSOLUTE top-left X
    y: viewModel.acquisitionBox.y      // ← ABSOLUTE top-left Y
    width: viewModel.acquisitionBox.width
    height: viewModel.acquisitionBox.height
}
```
**Coordinate System**: ABSOLUTE TOP-LEFT

**Conversion** (`src/models/osdviewmodel.cpp:404-424`):

```cpp
void OsdViewModel::updateReticleOffset(float screen_x_px, float screen_y_px) {
    float centerX = m_screenWidth / 2.0f;   // 512
    float centerY = m_screenHeight / 2.0f;  // 384

    // Converts ABSOLUTE to OFFSET
    float offsetX = screen_x_px - centerX;  // 512 - 512 = 0
    float offsetY = screen_y_px - centerY;  // 384 - 384 = 0

    m_reticleOffsetX = offsetX;  // QML receives 0
    m_reticleOffsetY = offsetY;  // QML receives 0
}
```

**Acquisition Box** (`src/models/domain/systemstatemodel.cpp:1895-1896`):

```cpp
// Sends ABSOLUTE TOP-LEFT coordinates
data.acquisitionBoxX_px = reticleCenterX - (defaultBoxW / 2.0f);  // 512 - 50 = 462
data.acquisitionBoxY_px = reticleCenterY - (defaultBoxH / 2.0f);  // 384 - 50 = 334
// QML receives absolute (462, 334) which centers the 100x100 box at (512, 384)
```

**Status**: ✅ Coordinate systems are DIFFERENT but both are MATHEMATICALLY CORRECT
- If reticle is at (512, 384) → offset (0, 0) → renders at screen center ✅
- If acquisition box top-left is (462, 334) with size 100x100 → center is (512, 384) ✅
- They SHOULD align!

---

### ⚠️ ISSUE 4: Timing - **POTENTIAL ROOT CAUSE**

**Data Flow**:
1. `SystemStateModel` → stores `reticleAimpointImageX_px` and `acquisitionBoxX_px`
2. `CameraVideoStreamDevice::onSystemStateChanged()` → copies to member variables
3. Camera thread populates `FrameData` from member variables
4. `FrameData` emitted → `OsdController::onFrameDataReady()`
5. `OsdController` → updates `OsdViewModel`
6. QML displays

**Critical Timing Question**:
When user clicks "Start Tracking", what are the values of `reticleAimpointImageX_px` in `SystemStateData`?

**Possible Scenarios**:

#### Scenario A: Normal Flow ✅
1. System starts
2. Camera emits first frame → `onFrameDataReady()` → reticle displays at (512, 384)
3. User clicks track → `startTrackingAcquisition()` reads `data.reticleAimpointImageX_px = 512.0`
4. Acquisition box centers at (512, 384)
5. **RESULT**: ALIGNED ✅

#### Scenario B: Stale Data ❌
1. System starts with defaults: `reticleAimpointImageX_px = 512.0`
2. `recalculateDerivedAimpointData()` is called (due to FOV change or camera update)
3. Reticle position calculated but NOT yet sent to camera thread
4. User clicks track BEFORE next frame → reads OLD value
5. **RESULT**: MISALIGNED ❌

#### Scenario C: Never Calculated ❌
1. System starts with defaults: `reticleAimpointImageX_px = 512.0`
2. `recalculateDerivedAimpointData()` is NEVER called (no FOV change, no camera updates)
3. Reticle stays at default (512, 384)
4. User clicks track → reads (512, 384)
5. **RESULT**: SHOULD BE ALIGNED but user reports MISALIGNMENT ❓

---

## 🎯 ROOT CAUSE HYPOTHESIS

Based on the audit, there are THREE possible root causes:

### Hypothesis 1: **Initialization Order**
`recalculateDerivedAimpointData()` is not called during startup, so reticle position might not be correctly initialized if image dimensions change after struct initialization.

### Hypothesis 2: **Thread Synchronization**
The acquisition box reads from `SystemStateData` directly, but the reticle displayed on screen comes from `FrameData` (camera thread). There might be a race condition where they read different versions of the data.

### Hypothesis 3: **Hidden Zeroing/Lead**
Despite user claiming "no zeroing", there might be non-zero zeroing or lead angle offsets being applied somewhere that haven't been noticed.

---

## 🧪 DEBUG STRATEGY

The debug logging added will reveal:

1. **When starting tracking**:
   - Screen size and center
   - Zeroing status (applied, offsets)
   - Reticle position from `SystemStateData`
   - Acquisition box position

2. **When reticle updates (from FrameData)**:
   - Screen center
   - Reticle absolute position (from FrameData)
   - Reticle offset sent to QML

3. **When acquisition box updates (to QML)**:
   - Top-left position
   - Size
   - Center position

**Expected Output (if working correctly)**:
```
STARTING TRACKING ACQUISITION
Screen Size: 1024 x 768
Screen Center: 512 , 384
Zeroing Status:
  - Applied: false
  - Az Offset: 0 deg
  - El Offset: 0 deg
Reticle Position (from SystemStateData):
  - X: 512 px
  - Y: 384 px
Acquisition Box Position:
  - Top-Left: ( 462 , 334 )
  - Size: 100 x 100
  - Center: ( 512 , 384 )
========================================

RETICLE POSITION UPDATE (to QML)
Screen Center: 512 , 384
Reticle Absolute Position: 512 , 384
Reticle Offset (from center): 0 , 0
========================================

ACQUISITION BOX UPDATE (to QML)
Top-Left: 462 , 334
Size: 100 x 100
Center: 512 , 384
========================================
```

**If values differ, we'll identify the exact source of mismatch!**

---

## 📝 CHANGES MADE

### Commit 1: Fix acquisition box centering
- Changed from screen center to reticle position
- File: `src/models/domain/systemstatemodel.cpp:1876-1896`

### Commit 2: Add comprehensive debug logging
- SystemStateModel: logs reticle/acquisition position on tracking start
- OsdViewModel: logs reticle offset and acquisition box sent to QML
- Files: `src/models/domain/systemstatemodel.cpp`, `src/models/osdviewmodel.cpp`

---

## ✅ NEXT STEPS

1. **Build and run** with debug logging enabled
2. **Observe console output** when clicking to start tracking
3. **Compare actual values** with expected values above
4. **Identify discrepancy** and apply targeted fix

---

## 📌 CONCLUSION

The coordinate calculation system is **architecturally sound** and **mathematically correct**. The misalignment issue is likely due to:
- **Timing**: Stale data being read when tracking starts
- **Initialization**: Reticle calculation not triggered at startup
- **Hidden state**: Zeroing or lead offsets present despite user's claim

The debug output will definitively identify which hypothesis is correct.

# Tracking Confidence & Status Display Implementation Plan

**Date:** 2025-01-17
**Goal:** Add tracker confidence score and tracking status to bottom bar display

---

## 🔍 **ANALYSIS - Current State**

### **What EXISTS:**

1. ✅ **VPI Confidence Score is Retrieved** (`cameravideostreamdevice.cpp:988`)
   ```cpp
   float currentConfidence = static_cast<float*>(confidenceData.buffer.aos.data)[0];
   qDebug() << "Confidence=" << currentConfidence;  // Only logged, not stored!
   ```

2. ✅ **QML Display Code is ALREADY WRITTEN** (`OsdOverlay.qml:1115-1130`)
   ```qml
   Text {
       text: viewModel.isTrackingActive ? "TRK: ACTIVE" : "TRK: OFF"
       color: viewModel.trackingActive ? "yellow" : accentColor  // ⚠️ Wrong property name
   }
   Text {
       text: "CONF: " + (viewModel.trackingConfidence * 100).toFixed(0) + "%"  // ⚠️ Property doesn't exist
       color: viewModel.trackingActive ? "yellow" : accentColor
   }
   ```

3. ✅ **Tracking State is Stored** (`SystemStateData:521, 535, 536`)
   ```cpp
   bool trackingActive = false;         // Line 521
   VPITrackingState trackedTargetState = VPI_TRACKING_STATE_LOST;  // Line 535
   TrackingPhase currentTrackingPhase = TrackingPhase::Off;  // Line 536
   ```

---

### **What's MISSING:**

| Component | Issue | Status |
|-----------|-------|--------|
| **CameraVideoStreamDevice** | Confidence retrieved but NOT stored | ❌ Missing |
| **FrameData struct** | No `trackingConfidence` field (line 53 has comment only) | ❌ Missing |
| **SystemStateData** | No `trackingConfidence` field | ❌ Missing |
| **SystemStateModel::updateTrackingResult()** | No confidence parameter | ❌ Missing |
| **OsdViewModel** | Has `confidenceLevel` but no `trackingConfidence` property | ⚠️ Mismatch |
| **OsdViewModel** | Has `isTrackingActive` but no `trackingActive` property | ⚠️ Mismatch |
| **QML Display** | Uses wrong property names | ⚠️ Mismatch |

---

## 📊 **DATA FLOW (Current vs Required)**

### **Current Flow (BROKEN):**
```
VPI Tracker
  ↓ (line 988: currentConfidence retrieved)
  ❌ LOST HERE - not stored!

SystemStateData::trackingActive (exists)
  ↓
OsdController → OsdViewModel
  ↓
QML tries to use viewModel.trackingConfidence ❌ DOESN'T EXIST
```

### **Required Flow:**
```
VPI Tracker (line 988)
  ↓ Store in m_currentConfidence (NEW member variable)
  ↓
FrameData::trackingConfidence (NEW field)
  ↓
SystemStateModel::updateTrackingResult(... confidence ...) (NEW parameter)
  ↓
SystemStateData::trackingConfidence (NEW field)
  ↓
OsdController → OsdViewModel::trackingConfidence (NEW property)
  ↓
QML: viewModel.trackingConfidence ✅ WORKS
```

---

## 🎯 **IMPLEMENTATION STEPS**

### **Step 1: Add Confidence Storage in CameraVideoStreamDevice**

**File:** `src/hardware/devices/cameravideostreamdevice.h`

**Add member variable:**
```cpp
// Around line 200 (in private section)
float m_currentConfidence = 0.0f;  ///< Current tracking confidence score (0.0-1.0)
```

**File:** `src/hardware/devices/cameravideostreamdevice.cpp`

**Store confidence after retrieval (line 988-991):**
```cpp
// Line 988: (already exists)
float currentConfidence = static_cast<float*>(confidenceData.buffer.aos.data)[0];
qDebug() << "[CAM" << m_cameraIndex << "] VPI Localize Result: State=" << tempTarget->state << "Confidence=" << currentConfidence;

// ADD THIS LINE:
m_currentConfidence = currentConfidence;  // Store for later use

// Line 991: (already exists)
m_currentTarget = *tempTarget;
```

---

### **Step 2: Add Confidence to FrameData Struct**

**File:** `src/hardware/devices/cameravideostreamdevice.h`

**Add field to FrameData struct (line 53):**
```cpp
// Line 51: VPITrackingState trackingState = VPI_TRACKING_STATE_LOST;
// Line 52: QRect trackingBbox = QRect(0, 0, 0, 0);
// Line 53: // tracking confidence  ← REPLACE THIS COMMENT
float trackingConfidence = 0.0f;  ///< Tracking confidence score (0.0-1.0) from VPI DCF tracker
// Line 54: OperationalMode currentOpMode = OperationalMode::Idle;
```

---

### **Step 3: Populate Confidence in FrameData**

**File:** `src/hardware/devices/cameravideostreamdevice.cpp`

**Find where FrameData is populated (search for `frmdata.trackingState =`)**

**Add after trackingState assignment:**
```cpp
// Around line 1200-1250 (in processFrame or similar function)
frmdata.trackingState = m_currentTarget.state;
frmdata.trackingBbox = QRect(/* ... */);

// ADD THIS:
frmdata.trackingConfidence = m_currentConfidence;
```

---

### **Step 4: Add Confidence to SystemStateData**

**File:** `src/models/domain/systemstatedata.h`

**Add field in tracking section (after line 521):**
```cpp
// Line 521: bool trackingActive = false;         ///< Current tracking active status
float trackingConfidence = 0.0f;  ///< Tracking confidence score (0.0-1.0) from VPI tracker
// Line 522: double targetAz = 0.0;
```

**Update comparison operator (around line 812):**
```cpp
// Find the operator== implementation
return trackingActive == other.trackingActive &&
       qFuzzyCompare(trackingConfidence, other.trackingConfidence) &&  // ADD THIS
       // ... other comparisons
```

---

### **Step 5: Update SystemStateModel::updateTrackingResult()**

**File:** `src/models/domain/systemstatemodel.h`

**Update signature (line 498-502):**
```cpp
void updateTrackingResult(int cameraIndex, bool hasLock,
                          float centerX_px, float centerY_px,
                          float width_px, float height_px,
                          float velocityX_px_s, float velocityY_px_s,
                          VPITrackingState state,
                          float confidence);  // ADD THIS PARAMETER
```

**File:** `src/models/domain/systemstatemodel.cpp`

**Update implementation (find updateTrackingResult function):**
```cpp
void SystemStateModel::updateTrackingResult(int cameraIndex, bool hasLock,
                                            float centerX_px, float centerY_px,
                                            float width_px, float height_px,
                                            float velocityX_px_s, float velocityY_px_s,
                                            VPITrackingState state,
                                            float confidence)  // ADD THIS
{
    // ... existing code ...

    // Store confidence
    m_currentStateData.trackingConfidence = confidence;  // ADD THIS

    // ... rest of function ...
}
```

---

### **Step 6: Update OSD Controller to Pass Confidence**

**File:** `src/controllers/osdcontroller.cpp`

**Find where tracking data is updated (search for `updateTrackingBox` or similar):**

```cpp
// Add update for confidence
m_viewModel->updateConfidenceLevel(data.trackingConfidence);
```

---

### **Step 7: Add Missing Properties to OsdViewModel**

**File:** `src/models/osdviewmodel.h`

**Add Q_PROPERTY for trackingConfidence (after line 52):**
```cpp
// Line 52: Q_PROPERTY(bool isTrackingActive READ isTrackingActive NOTIFY isTrackingActiveChanged)
Q_PROPERTY(float trackingConfidence READ trackingConfidence NOTIFY trackingConfidenceChanged)
Q_PROPERTY(bool trackingActive READ trackingActive NOTIFY trackingActiveChanged)
```

**Add getter methods (after line 173):**
```cpp
// Line 173: bool isTrackingActive() const { return m_isTrackingActive; }
float trackingConfidence() const { return m_confidenceLevel; }  // Use existing m_confidenceLevel
bool trackingActive() const { return m_isTrackingActive; }      // Alias for backward compatibility
```

**Add signal (after line 327):**
```cpp
// Line 327: void isTrackingActiveChanged();
void trackingConfidenceChanged();
void trackingActiveChanged();
```

**Update setter method (if needed):**
```cpp
void OsdViewModel::updateConfidenceLevel(float confidence) {
    if (!qFuzzyCompare(m_confidenceLevel, confidence)) {
        m_confidenceLevel = confidence;
        emit confidenceLevelChanged();
        emit trackingConfidenceChanged();  // ADD THIS - emit both signals
    }
}
```

---

### **Step 8: Fix QML Display (Already Correct!)**

**File:** `qml/components/OsdOverlay.qml` (lines 1115-1130)

**Current code should work after fixes:**
```qml
// TRK Status
Text {
    text: viewModel.isTrackingActive ? "TRK: ACTIVE" : "TRK: OFF"
    color: viewModel.trackingActive ? "yellow" : accentColor  // Now works!
}

// Confidence Score
Text {
    text: "CONF: " + (viewModel.trackingConfidence * 100).toFixed(0) + "%"  // Now works!
    color: viewModel.trackingActive ? "yellow" : accentColor
}
```

---

## 🧪 **TESTING CHECKLIST**

After implementation, test:

1. ✅ **Confidence appears in logs**
   - Check debug output shows confidence value

2. ✅ **Bottom bar displays confidence**
   - Start tracking
   - Verify "CONF: XX%" appears in bottom bar
   - Verify percentage updates in real-time

3. ✅ **Status text is correct**
   - "TRK: OFF" when not tracking
   - "TRK: ACTIVE" when tracking locked
   - Text turns yellow when active

4. ✅ **Confidence reflects tracking quality**
   - High confidence (>80%) when target clear
   - Low confidence (<50%) when target partially occluded
   - Updates smoothly during tracking

---

## 📝 **ADDITIONAL ENHANCEMENTS (Optional)**

### **Color-Code Confidence:**
```qml
Text {
    text: "CONF: " + (viewModel.trackingConfidence * 100).toFixed(0) + "%"
    color: {
        if (!viewModel.trackingActive) return accentColor;
        var conf = viewModel.trackingConfidence;
        if (conf > 0.8) return "#00FF00";      // Green - excellent
        if (conf > 0.5) return "yellow";       // Yellow - good
        return "#FF8800";                      // Orange - poor
    }
}
```

### **Status Text with Phase:**
```qml
Text {
    text: {
        if (!viewModel.isTrackingActive) return "TRK: OFF";
        // Could add phase-specific text:
        // "TRK: ACQUIRING", "TRK: LOCKED", "TRK: COASTING", etc.
        return "TRK: ACTIVE";
    }
}
```

---

## 🎯 **SUMMARY**

**What Needs to Change:**

| File | Changes Required | Lines |
|------|-----------------|-------|
| `cameravideostreamdevice.h` | Add `m_currentConfidence` member | ~200 |
| `cameravideostreamdevice.h` | Add `trackingConfidence` to FrameData | 53 |
| `cameravideostreamdevice.cpp` | Store confidence after retrieval | 991 |
| `cameravideostreamdevice.cpp` | Populate FrameData.trackingConfidence | ~1200 |
| `systemstatedata.h` | Add `trackingConfidence` field | 522 |
| `systemstatedata.h` | Update comparison operator | 812 |
| `systemstatemodel.h` | Add confidence parameter | 498-502 |
| `systemstatemodel.cpp` | Store confidence in updateTrackingResult | TBD |
| `osdviewmodel.h` | Add `trackingConfidence` Q_PROPERTY | 53 |
| `osdviewmodel.h` | Add `trackingActive` Q_PROPERTY | 54 |
| `osdviewmodel.cpp` | Emit trackingConfidenceChanged signal | TBD |
| `osdcontroller.cpp` | Pass confidence to ViewModel | TBD |
| `OsdOverlay.qml` | **Already correct!** | 1115-1130 |

**Estimated Implementation Time:** 1-2 hours

**Risk Level:** LOW (simple data propagation, no complex logic)

---

**END OF DOCUMENT**

*The QML display code is already written and waiting for the backend data to flow through!*

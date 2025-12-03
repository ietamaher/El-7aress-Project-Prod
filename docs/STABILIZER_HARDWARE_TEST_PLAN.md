# GimbalStabilizer Hardware Integration Test Plan

## Purpose
Validate production-grade stabilization system on embedded military platform hardware.

## Test Environment
- **Platform**: QT6 QML on Jetson Orin AGX / Ubuntu 22.04 x86_64
- **Hardware**: AZD-KD servo drivers (Operation Type 16, velocity control)
- **IMU**: 3DM-GX3-25 AHRS (gyro rates in deg/s)
- **Update rate**: 50ms / 20Hz control loop

## Pre-Test Verification
- [ ] Compile test passes (Docker build)
- [ ] Startup sanity checks log clean (no warnings)
- [ ] IMU units verified as deg/s (check log output)
- [ ] Stabilizer limits validated (maxTotalVel ≥ sum of components)

## Test Cases

### TC-1: Stationary Platform (Bias Estimation)
**Objective**: Verify gyro bias estimation and zero-motion stability

**Procedure**:
1. Platform stationary, gimbal at (0°, 0°)
2. Enable stabilization
3. Observe for 60 seconds

**Pass Criteria**:
- Gyro bias converges within 50 samples (2.5s)
- Gimbal drift < 0.5° over 60s
- No servo oscillations (check Modbus write rate)

---

### TC-2: Roll-Only Motion (Single-Axis Validation)
**Objective**: Verify roll-to-azimuth coupling and sign convention

**Procedure**:
1. Target set to world (0°, 0°) - inertial hold
2. Manually roll platform +10° (slowly, 2s ramp)
3. Return to 0° roll
4. Repeat with -10° roll

**Expected Behavior**:
- Gimbal azimuth compensates roll (opposite direction)
- Boresight remains locked to world target
- No elevation drift

**Pass Criteria**:
- World-frame azimuth error < 1° during motion
- Elevation error < 0.5° throughout test
- Servo velocity commands smooth (no spikes)

---

### TC-3: Pitch-Only Motion (Elevation Coupling)
**Objective**: Verify pitch-to-elevation coupling and sign convention

**Procedure**:
1. Target at world (0°, 0°)
2. Manually pitch platform +10° (slowly, 2s ramp)
3. Return to 0° pitch
4. Repeat with -10° pitch

**Expected Behavior**:
- Gimbal elevation compensates pitch (opposite direction)
- Boresight remains locked to world target
- No azimuth drift

**Pass Criteria**:
- World-frame elevation error < 1° during motion
- Azimuth error < 0.5° throughout test
- No oscillations near ±90° gimbal elevation

---

### TC-4: Combined Motion (3D Validation)
**Objective**: Validate matrix-based transformation with complex attitudes

**Procedure**:
1. Target at world (0°, 0°)
2. Manually move platform through:
   - Roll: +3°, Pitch: +2°, Yaw: +5° (expert review test case)
   - Return to level
3. Repeat with different attitude combinations

**Expected Behavior**:
- Required gimbal angles match unit test expectations:
  - For (roll=3°, pitch=2°, yaw=5°): az ≈ -4.89°, el ≈ -2.25°
- Boresight holds world target throughout motion
- No gimbal lock symptoms

**Pass Criteria**:
- World-frame pointing error < 1.5° during motion
- Servo commands remain within configured limits
- No numerical instabilities (NaN, Inf in logs)

---

### TC-5: High-Elevation Singularity Protection
**Objective**: Verify tan(el) clamping prevents explosion near ±90°

**Procedure**:
1. Command gimbal to +80° elevation (platform level)
2. Enable stabilization
3. Slowly pitch platform ±5°
4. Repeat at -80° elevation

**Expected Behavior**:
- Rate feed-forward remains bounded (< maxVelocityCorr)
- No servo rate spikes or saturation
- Controlled behavior even at extreme angles

**Pass Criteria**:
- Azimuth rate feed-forward < maxVelocityCorr (default: 5 deg/s)
- No qWarning messages about singularities
- Smooth servo motion (no jerking)

---

### TC-6: Aggressive Maneuver (Stress Test)
**Objective**: Validate stabilization under dynamic platform motion

**Procedure**:
1. Target at world (45°, 10°)
2. Execute aggressive platform maneuver:
   - Roll: ±15° at 10 deg/s
   - Pitch: ±10° at 8 deg/s
   - Yaw: ±20° at 15 deg/s
3. Monitor for 30 seconds

**Expected Behavior**:
- Stabilization maintains target lock
- Velocity composition doesn't exceed maxTotalVel
- No Modbus write saturation (check change detection working)

**Pass Criteria**:
- World-frame pointing error < 3° during maneuver
- Stabilized velocity < maxTotalVel (default: 18 deg/s)
- Modbus write rate < 10 ops/sec (change detection working)

---

## Pass/Fail Criteria Summary

**PASS**: All test cases meet criteria + no critical warnings in logs
**FAIL**: Any test case fails OR startup sanity checks fail OR servo oscillations observed

## Data Collection

For each test, log:
- IMU rates (GyroX/Y/Z) - verify deg/s units
- Gimbal angles (az, el) - actual vs required
- Servo velocity commands - check saturation
- Stabilizer component contributions (position corr, rate ff, total)
- Modbus write count - verify change detection

## Expected Timeline

- Pre-test setup: 15 min
- Test execution: 45 min (6 tests × 7-8 min each)
- Data analysis: 30 min
- **Total**: ~90 minutes

## Hardware-Specific Notes

1. **Modbus timing**: AZD-KD response time ~2ms, budget 5ms per write
2. **IMU latency**: 3DM-GX3-25 update rate 100Hz, expect <10ms lag
3. **Servo resolution**: Az=222500 steps/360°, El=200000 steps/360°
4. **Platform motion limits**: Typical vehicle: roll ±30°, pitch ±20°, yaw continuous

## Next Steps After Tests

1. If TC-1 fails → Check IMU connection and bias estimation logic
2. If TC-2/TC-3 fails → Verify sign conventions and IMU→body-rate mapping
3. If TC-4 fails → Review matrix math (should not fail - unit test passed)
4. If TC-5 fails → Adjust maxTanEl limit (try 8.0 or 12.0)
5. If TC-6 fails → Tune limits (increase maxTotalVel or reduce component limits)

## Success Criteria for Production Release

✅ All 6 test cases PASS
✅ No startup sanity check warnings
✅ Modbus write rate < 10 ops/sec (latency fix working)
✅ Stabilization error < 1.5° RMS across all conditions
✅ No oscillations or instabilities observed

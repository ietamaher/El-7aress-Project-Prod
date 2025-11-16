# Motion Control Tuning Guide - El 7arress RCWS

**Version:** 1.0
**Author:** Motion Control Team
**Last Updated:** 2025-01-16
**Target Audience:** Field Engineers, System Integrators, Test Engineers

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Configuration File Structure](#configuration-file-structure)
4. [Filter Parameters](#filter-parameters)
5. [Motion Limits](#motion-limits)
6. [PID Controller Tuning](#pid-controller-tuning)
7. [Mode-Specific Parameters](#mode-specific-parameters)
8. [Field Tuning Workflow](#field-tuning-workflow)
9. [Common Tuning Scenarios](#common-tuning-scenarios)
10. [Safety Considerations](#safety-considerations)
11. [Troubleshooting](#troubleshooting)

---

## Overview

### What is Runtime Motion Tuning?

The El 7arress RCWS implements a **runtime-configurable motion control system** that allows you to adjust critical motion parameters **without recompiling the application**. This enables rapid tuning during field trials, live testing, and deployment optimization.

### Key Benefits

✅ **No Rebuild Required** - Edit JSON file, restart application
✅ **Field-Tunable** - Adjust parameters on-site during trials
✅ **Version Controlled** - Track tuning changes in git
✅ **Validated** - Parameters are range-checked on startup
✅ **Safe Defaults** - Fallback to known-good values if config is missing

### Architecture

```
┌─────────────────────────────────────┐
│   config/motion_tuning.json         │  ← Edit this file
│   (Human-readable parameters)       │
└──────────────┬──────────────────────┘
               │
               │ Loaded on startup
               ▼
┌─────────────────────────────────────┐
│   MotionTuningConfig::instance()    │  ← Singleton config object
│   (In-memory parameter storage)     │
└──────────────┬──────────────────────┘
               │
               │ Accessed via inline functions
               ▼
┌─────────────────────────────────────┐
│   Motion Mode Classes               │
│   - ManualMotionMode                │
│   - TrackingMotionMode              │
│   - AutoSectorScanMotionMode        │
│   - TRPScanMotionMode               │
│   - RadarSlewMotionMode             │
└─────────────────────────────────────┘
```

---

## Quick Start

### Step 1: Locate Configuration File

```bash
cd /path/to/El-7aress-Project-Prod
nano config/motion_tuning.json
```

### Step 2: Make Your Changes

Example - Increase tracking responsiveness:

```json
"pid": {
  "tracking": {
    "azimuth": {
      "kp": 1.5,    // ← Increase from 1.0 to 1.5
      "ki": 0.005,
      "kd": 0.01,
      "maxIntegral": 10.0
    }
  }
}
```

### Step 3: Restart Application

```bash
# Stop the application
pkill -f El-7aress

# Restart with validation
./El-7aress-Project-Prod
```

### Step 4: Verify Changes

Watch the startup log for:

```
[MotionTuningConfig] ✓ Loaded successfully
[MotionTuningConfig] Configuration summary:
  Gyro filter cutoff: 5 Hz
  Max acceleration: 50 deg/s²
  Tracking Az PID: Kp=1.5 Ki=0.005 Kd=0.01
```

---

## Configuration File Structure

### Top-Level Sections

```json
{
  "version": "1.0",
  "filters": { ... },           // Low-pass filters & smoothing
  "motion": { ... },            // Acceleration/velocity limits
  "servo": { ... },             // Hardware conversion constants
  "pid": { ... },               // PID controller gains per mode
  "accelLimits": { ... },       // Manual mode acceleration
  "tuningNotes": { ... }        // Helpful guidance
}
```

---

## Filter Parameters

Filters smooth noisy sensor data and user inputs to prevent jitter and oscillation.

### Gyro Low-Pass Filter

```json
"filters": {
  "gyro": {
    "cutoffFreqHz": 5.0,
    "comment": "Gyro low-pass filter cutoff frequency (1-20 Hz recommended)"
  }
}
```

**What it does:**
Filters gyroscope measurements used for platform stabilization.

**Effects:**
- **Lower values (1-3 Hz):** Smoother, but slower response to platform motion
- **Higher values (10-20 Hz):** Faster response, but more noise in stabilization
- **Recommended:** 5 Hz for most platforms

**When to adjust:**
- Platform vibrates excessively → **Lower** to 3 Hz
- Stabilization feels sluggish → **Raise** to 7-10 Hz

---

### Tracking Filter Time Constants

```json
"tracking": {
  "positionTau": 0.12,    // Target position smoothing (seconds)
  "velocityTau": 0.08,    // Target velocity smoothing (seconds)
  "comment": "Time constants for target position/velocity smoothing (seconds)"
}
```

**What it does:**
Smooths vision-based target position and velocity estimates.

**Effects:**
- **Lower tau (0.05-0.08s):** Fast response, may oscillate on noisy detections
- **Higher tau (0.15-0.20s):** Smooth tracking, may lag behind fast targets

**Physical meaning:**
Tau = time constant = time to reach 63% of step change

**When to adjust:**
- Tracking jitters on stationary targets → **Increase** positionTau to 0.15s
- Tracking lags behind moving targets → **Decrease** velocityTau to 0.05s

---

### Manual Joystick Filter

```json
"manual": {
  "joystickTau": 0.08,
  "comment": "Joystick input filter time constant (seconds)"
}
```

**What it does:**
Smooths raw joystick readings to prevent jerky motion.

**Effects:**
- **Lower tau (0.05s):** Very responsive, feels "twitchy"
- **Higher tau (0.12s):** Smooth, feels "damped"

**When to adjust:**
- Operator complains of "jumpy" control → **Increase** to 0.10-0.12s
- Operator wants more responsive control → **Decrease** to 0.06s

---

## Motion Limits

### Acceleration Limits (deg/s²)

```json
"motion": {
  "maxAccelerationDegS2": 50.0,       // General max (tracking, radar slew)
  "scanMaxAccelDegS2": 20.0,          // AutoSectorScan (smooth surveillance)
  "trpMaxAccelDegS2": 50.0            // TRP scan (point-to-point)
}
```

**What it does:**
Limits how quickly the gimbal can change velocity (prevents motor overload).

**Effects:**
- **Lower acceleration:** Smoother motion, slower to reach target
- **Higher acceleration:** Faster response, may stress servos

**Recommended values:**
- **Surveillance scanning:** 20 deg/s² (smooth, continuous)
- **Target tracking:** 50 deg/s² (responsive to moving targets)
- **Emergency slew:** 100 deg/s² (fast reaction, use sparingly)

**When to adjust:**
- Servos overheat or fault → **Decrease** acceleration
- Motion feels sluggish → **Increase** acceleration (monitor servo temps!)

**⚠️ SAFETY WARNING:**
Do NOT exceed 200 deg/s² - risk of servo damage and mechanical wear!

---

### Velocity Limits (deg/s)

```json
"motion": {
  "maxVelocityDegS": 30.0,            // General speed limit
  "trpDefaultTravelSpeed": 15.0       // TRP point-to-point speed
}
```

**What it does:**
Caps maximum gimbal rotation speed.

**Effects:**
- **Lower velocity:** Slower motion, less power consumption
- **Higher velocity:** Faster slewing, higher power draw

**When to adjust:**
- Need faster target acquisition → **Increase** to 40-50 deg/s
- Power budget limited → **Decrease** to 20 deg/s

---

### Arrival Thresholds (deg)

```json
"motion": {
  "arrivalThresholdDeg": 0.5
}
```

**What it does:**
Defines "close enough" to consider a target reached.

**Effects:**
- **Smaller threshold (0.1 deg):** Precise positioning, may never settle
- **Larger threshold (1.0 deg):** Faster waypoint transitions, less precise

**When to adjust:**
- TRP scan never transitions to next point → **Increase** to 0.7-1.0 deg
- Need precise positioning → **Decrease** to 0.2-0.3 deg

---

## PID Controller Tuning

### What is PID Control?

A PID controller calculates the control output based on three terms:

```
Output = Kp × error + Ki × ∫error·dt + Kd × d(error)/dt
         └─────┘       └──────────┘     └────────────┘
       Proportional     Integral         Derivative
```

**Proportional (Kp):** Reacts to current error
**Integral (Ki):** Eliminates steady-state error
**Derivative (Kd):** Dampens oscillation

---

### PID Tuning by Mode

#### Tracking Mode (Vision-Based Target Following)

```json
"pid": {
  "tracking": {
    "azimuth": {
      "kp": 1.0,
      "ki": 0.005,
      "kd": 0.01,
      "maxIntegral": 10.0,
      "comment": "Tracking mode azimuth PID gains (reduced for stability)"
    }
  }
}
```

**Characteristics:**
- **Moderate Kp:** Balance between responsiveness and stability
- **Low Ki:** Prevent integral windup from vision noise
- **Low Kd:** Vision data is noisy, derivative amplifies noise

**Tuning procedure:**
1. Start with Kp = 1.0, Ki = 0, Kd = 0
2. Increase Kp until gimbal follows target with minimal lag
3. If oscillates, reduce Kp by 20%
4. Add small Ki (0.005) to eliminate steady-state offset
5. Add small Kd (0.01) to dampen overshoot

**Common issues:**
- **Oscillation:** Reduce Kp or increase Kd
- **Steady-state offset:** Increase Ki slightly
- **Jumpy motion:** Reduce Kd, increase filter tau

---

#### AutoSectorScan Mode (Surveillance Scanning)

```json
"autoSectorScan": {
  "azimuth": {
    "kp": 1.0,
    "ki": 0.01,
    "kd": 0.05,
    "maxIntegral": 20.0
  },
  "decelerationDistanceDeg": 5.0,
  "arrivalThresholdDeg": 0.2
}
```

**Characteristics:**
- **Smooth deceleration:** PID only active near waypoints
- **Cruise control:** Constant velocity between waypoints
- **Precise stopping:** Small arrival threshold

**Tuning procedure:**
1. Set `decelerationDistanceDeg` based on scan speed:
   - Formula: `decelDist = speed² / (2 × acceleration)`
   - Example: 20 deg/s @ 20 deg/s² → 10 deg deceleration distance
2. Tune PID for smooth stops (only affects deceleration zone)
3. Adjust `arrivalThresholdDeg` for waypoint precision

**Common issues:**
- **Overshoots waypoints:** Increase deceleration distance or Kd
- **Never reaches waypoints:** Increase arrival threshold
- **Jerky turnarounds:** Decrease Kp

---

#### TRP Scan Mode (Point-to-Point Navigation)

```json
"trpScan": {
  "azimuth": {
    "kp": 1.2,
    "ki": 0.015,
    "kd": 0.08,
    "maxIntegral": 25.0
  },
  "decelerationDistanceDeg": 3.0,
  "arrivalThresholdDeg": 0.1
}
```

**Characteristics:**
- **Higher gains:** Faster response for point-to-point moves
- **Tighter tolerances:** Precise arrival at TRP coordinates

**When to adjust:**
- **TRP arrival takes too long:** Increase Kp to 1.5
- **Overshoots TRPs:** Increase Kd to 0.10
- **Never "arrives":** Increase arrivalThresholdDeg to 0.15

---

#### Radar Slew Mode (Fast Target Acquisition)

```json
"radarSlew": {
  "azimuth": {
    "kp": 1.5,
    "ki": 0.08,
    "kd": 0.15,
    "maxIntegral": 30.0
  }
}
```

**Characteristics:**
- **Aggressive gains:** Fast slew for target acquisition
- **High Kd:** Dampen overshoot from fast motion

**When to adjust:**
- **Slew too slow:** Increase Kp to 2.0 (monitor servo stress!)
- **Overshoots target:** Increase Kd to 0.20

---

### PID Integral Windup Protection

```json
"maxIntegral": 10.0    // Clamps integral term to prevent windup
```

**What it does:**
Prevents integral term from accumulating excessively during large errors.

**Effects:**
- **Too low (5.0):** Integral can't overcome friction/deadzone
- **Too high (50.0):** Integral windup causes overshoot

**When to adjust:**
- Gimbal overshoots after long moves → **Decrease** maxIntegral
- Can't reach final position (friction) → **Increase** maxIntegral

---

## Mode-Specific Parameters

### Manual Mode (Joystick Control)

```json
"accelLimits": {
  "manualMaxAccelHzPerSec": 500000.0,
  "comment": "Maximum acceleration for manual joystick mode (Hz/s)"
}
```

**What it does:**
Limits rate of change of servo command (in Hz/s, not deg/s²).

**Conversion:**
`Hz/s = deg/s² × steps_per_degree`

**When to adjust:**
- Joystick feels jerky → **Decrease** to 300000 Hz/s
- Joystick feels sluggish → **Increase** to 700000 Hz/s

---

### Servo Conversion Constants

```json
"servo": {
  "azStepsPerDegree": 618.0556,      // 222500 steps / 360 deg
  "elStepsPerDegree": 555.5556       // 200000 steps / 360 deg
}
```

**⚠️ CRITICAL:**
**DO NOT CHANGE** unless you replace servo motors with different gear ratios!

These values are hardware-specific and calculated from:
- Encoder resolution (steps per revolution)
- Gear reduction ratio

**Incorrect values will cause:**
- Wrong positioning
- Speed miscalculation
- Potential mechanical damage

---

## Field Tuning Workflow

### Pre-Flight Checklist

1. ✅ Backup current `motion_tuning.json`
2. ✅ Document baseline performance (video if possible)
3. ✅ Ensure servo temperatures are normal
4. ✅ Check mechanical clearances
5. ✅ Have emergency stop procedure ready

---

### Step-by-Step Tuning Process

#### Phase 1: Baseline Test

```bash
# 1. Run with default config
./El-7aress-Project-Prod

# 2. Test each mode:
# - Manual joystick control
# - AutoSectorScan
# - TRP scan
# - Target tracking

# 3. Note problems:
# - Oscillation?
# - Slow response?
# - Overshoot?
# - Never reaches target?
```

#### Phase 2: Identify Problem Mode

Focus on ONE mode at a time:

```json
// Example: Tracking mode oscillates
"tracking": {
  "azimuth": {
    "kp": 1.0,  // ← Start here
    "ki": 0.005,
    "kd": 0.01,
    "maxIntegral": 10.0
  }
}
```

#### Phase 3: Systematic Tuning

**For oscillation:**
1. Reduce Kp by 20% → Test
2. If still oscillates, reduce another 20%
3. Add/increase Kd for damping

**For slow response:**
1. Increase Kp by 20% → Test
2. Check servo temperature!
3. If overheats, reduce acceleration instead

**For steady-state error:**
1. Increase Ki by 50% → Test
2. Monitor for integral windup
3. Adjust maxIntegral if overshoots

#### Phase 4: Document Changes

```bash
# Commit your tuning changes
git add config/motion_tuning.json
git commit -m "Tuning: Tracking mode - reduced Kp to 0.8 for stability"
git tag tuning-2025-01-16-field-test-1
```

---

## Common Tuning Scenarios

### Scenario 1: Tracking Jitters on Stationary Target

**Symptoms:**
- Gimbal oscillates ±1-2 degrees
- Target is stationary
- Vision system reports stable position

**Root cause:**
Vision noise amplified by high Kp or low filter tau

**Solution:**
```json
// Increase position filter tau
"filters": {
  "tracking": {
    "positionTau": 0.15,  // ← Increase from 0.12
    "velocityTau": 0.08
  }
}

// OR reduce PID Kp
"tracking": {
  "azimuth": {
    "kp": 0.8,  // ← Decrease from 1.0
```

---

### Scenario 2: Scan Never Reaches Waypoints

**Symptoms:**
- AutoSectorScan moves toward waypoint
- Slows down near waypoint
- Never triggers "arrived" condition
- Oscillates near waypoint

**Root cause:**
Arrival threshold too tight

**Solution:**
```json
"autoSectorScan": {
  "arrivalThresholdDeg": 0.5  // ← Increase from 0.2
}
```

---

### Scenario 3: Manual Mode Feels "Twitchy"

**Symptoms:**
- Small joystick movements cause jerky motion
- Difficult to make smooth adjustments

**Root cause:**
Joystick filter tau too low or acceleration too high

**Solution:**
```json
// Increase joystick smoothing
"filters": {
  "manual": {
    "joystickTau": 0.12  // ← Increase from 0.08
  }
}

// OR reduce acceleration
"accelLimits": {
  "manualMaxAccelHzPerSec": 300000.0  // ← Decrease from 500000
}
```

---

### Scenario 4: Tracking Lags Behind Fast Targets

**Symptoms:**
- Target moves quickly
- Gimbal lags 5-10 degrees behind
- Never catches up

**Root cause:**
Filter tau too high or Kp too low

**Solution:**
```json
// Decrease velocity filter tau
"filters": {
  "tracking": {
    "positionTau": 0.12,
    "velocityTau": 0.05  // ← Decrease from 0.08
  }
}

// AND increase PID Kp
"tracking": {
  "azimuth": {
    "kp": 1.5,  // ← Increase from 1.0
```

---

### Scenario 5: Servo Overheats During Tracking

**Symptoms:**
- Servo temperature >75°C
- Servo faults or shuts down
- Excessive current draw

**Root cause:**
Acceleration too high or oscillation causing constant corrections

**Solution:**
```json
// Reduce acceleration
"motion": {
  "maxAccelerationDegS2": 30.0  // ← Decrease from 50.0
}

// AND increase damping
"tracking": {
  "azimuth": {
    "kd": 0.015  // ← Increase from 0.01
  }
}
```

---

## Safety Considerations

### Parameter Limits

**NEVER exceed these values:**

| Parameter | Maximum Safe Value | Consequence if Exceeded |
|-----------|-------------------|------------------------|
| maxAccelerationDegS2 | 200.0 | Servo damage, mechanical wear |
| maxVelocityDegS | 120.0 | Loss of control, collision risk |
| Kp (any mode) | 10.0 | Violent oscillation |
| Ki (any mode) | 1.0 | Integral windup, overshoot |
| gyroCutoffFreqHz | 20.0 | Noise amplification |

### Testing Procedure

**Always follow this sequence:**

1. **Bench test** with no payload
2. **Low-speed test** (50% of max velocity)
3. **Monitor servo temperatures** continuously
4. **Gradual increase** to full speed
5. **Emergency stop** procedure tested

### Temperature Monitoring

```bash
# Monitor servo temps during tuning
tail -f /var/log/rcws.log | grep -i "temp\|fault"
```

**Safe operating temperatures:**
- Motor: <75°C (warning at 70°C)
- Driver: <80°C (warning at 75°C)

**If temperature exceeds limits:**
1. Immediately stop motion
2. Reduce acceleration by 30%
3. Reduce velocity by 20%
4. Re-test incrementally

---

## Troubleshooting

### Configuration Not Loading

**Problem:** Changes to `motion_tuning.json` have no effect

**Diagnosis:**
```bash
# Check startup log
./El-7aress-Project-Prod 2>&1 | grep MotionTuning

# Expected output:
# [MotionTuningConfig] Loading from: ./config/motion_tuning.json
# [MotionTuningConfig] ✓ Loaded successfully
```

**Solutions:**
- Verify JSON syntax (use `jsonlint config/motion_tuning.json`)
- Check file permissions (`chmod 644 config/motion_tuning.json`)
- Ensure file is in correct location

---

### Validation Errors

**Problem:** Application exits with "Configuration validation FAILED"

**Diagnosis:**
```bash
# Check validation output
./El-7aress-Project-Prod 2>&1 | grep "✗"

# Example error:
# ✗ Tracking Az Kp must be in range (0, 10]
```

**Solutions:**
- Check parameter is within valid range
- Fix typos in JSON (e.g., "Kp" not "kp")
- Restore from backup if corrupted

---

### Unexpected Behavior After Tuning

**Problem:** Motion worse after parameter changes

**Solution:**
```bash
# Restore default config
cp config/motion_tuning.json.backup config/motion_tuning.json

# Restart
./El-7aress-Project-Prod
```

**Prevention:**
- Always backup before changes
- Change one parameter at a time
- Document changes in git

---

## Appendix A: Parameter Quick Reference

### Filters

| Parameter | Default | Range | Units | Purpose |
|-----------|---------|-------|-------|---------|
| gyroCutoffFreqHz | 5.0 | 1-20 | Hz | Gyro noise filtering |
| trackingPositionTau | 0.12 | 0.01-1.0 | seconds | Target smoothing |
| trackingVelocityTau | 0.08 | 0.01-1.0 | seconds | Velocity smoothing |
| manualJoystickTau | 0.08 | 0.01-0.5 | seconds | Joystick filtering |

### Motion Limits

| Parameter | Default | Range | Units | Purpose |
|-----------|---------|-------|-------|---------|
| maxAccelerationDegS2 | 50.0 | 1-200 | deg/s² | General accel limit |
| scanMaxAccelDegS2 | 20.0 | 1-100 | deg/s² | Scan mode accel |
| trpMaxAccelDegS2 | 50.0 | 1-200 | deg/s² | TRP scan accel |
| trpDefaultTravelSpeed | 15.0 | 1-120 | deg/s | TRP travel speed |
| maxVelocityDegS | 30.0 | 1-120 | deg/s | Speed limit |
| arrivalThresholdDeg | 0.5 | 0.01-5.0 | degrees | Waypoint tolerance |

### PID Gains (Tracking Mode)

| Parameter | Default | Range | Units | Purpose |
|-----------|---------|-------|-------|---------|
| trackingAz.kp | 1.0 | 0-10 | - | Proportional gain |
| trackingAz.ki | 0.005 | 0-1.0 | - | Integral gain |
| trackingAz.kd | 0.01 | 0-2.0 | - | Derivative gain |
| trackingAz.maxIntegral | 10.0 | 1-50 | - | Windup limit |

---

## Appendix B: Mathematical Background

### Time Constant (Tau)

Exponential filter equation:
```
y[n] = α × x[n] + (1 - α) × y[n-1]
```

Where alpha is computed from tau and dt:
```
α = 1 - exp(-dt / tau)
```

**Physical meaning:**
- tau = 0.1s → reaches 63% of step input in 100ms
- tau = 0.05s → faster (63% in 50ms)
- tau = 0.2s → slower (63% in 200ms)

### PID Output Calculation

```cpp
double p_term = Kp × error;
double i_term = Ki × integral;  // integral += error × dt
double d_term = Kd × derivative;  // derivative = (error - prev_error) / dt

double output = p_term + i_term + d_term;
```

### Deceleration Distance

Kinematic formula for constant deceleration:
```
d = v² / (2a)
```

Where:
- d = stopping distance (degrees)
- v = current velocity (deg/s)
- a = deceleration (deg/s²)

Example:
- v = 20 deg/s
- a = 20 deg/s²
- d = 20² / (2×20) = 10 degrees

---

## Appendix C: Version History

### Version 1.0 (2025-01-16)
- Initial release
- All motion modes configurable
- Comprehensive validation
- Field-tested defaults

---

## Support & Feedback

For questions, issues, or improvement suggestions:

**Email:** support@el7aress.mil
**Issue Tracker:** https://github.com/your-org/El-7aress-Project-Prod/issues
**Documentation:** ./docs/

---

**END OF DOCUMENT**

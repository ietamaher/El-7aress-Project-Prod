# Eigen3 Installation Guide
**Date:** 2025-11-14
**Required for:** 3D rotation matrix calculations in gimbal stabilization

---

## Why Eigen is Needed

The codebase now uses **Eigen3** for professional-grade 3D transformations:

- **File:** `src/controllers/motion_modes/gimbalmotionmodebase.cpp`
- **Method:** `convertGimbalToWorldFrame()` - Gimbal-to-world frame velocity transformations
- **Purpose:** Rotation matrices for azimuth-over-elevation gimbal kinematics

**Benefits:**
- ✅ Industry-standard library (used in ROS, aerospace, robotics)
- ✅ Optimized matrix operations
- ✅ Clear, maintainable code
- ✅ Header-only (no linking required)

---

## Installation

### Ubuntu/Debian Systems

```bash
sudo apt-get update
sudo apt-get install -y libeigen3-dev
```

### Verify Installation

```bash
dpkg -l | grep eigen3
# Should show: libeigen3-dev

ls /usr/include/eigen3/
# Should show: Eigen/ directory
```

---

## Build Configuration

The project `.pro` file **already includes** Eigen:

```qmake
# In El-7aress-Project-Prod.pro (line 23):
INCLUDEPATH += "/usr/include/eigen3"
```

**No additional build configuration needed!**

---

## What Uses Eigen

### 1. Rotation Matrix Transformations (`gimbalmotionmodebase.cpp`)

```cpp
// Convert gimbal velocities to world frame using rotation matrices
void convertGimbalToWorldFrame(
    const Eigen::Vector3d& linVel_gimbal_mps,
    const Eigen::Vector3d& angVel_gimbal_dps,
    double azDeg,
    double elDeg,
    Eigen::Vector3d& linVel_world_mps,
    Eigen::Vector3d& angVel_world_dps);
```

**Rotation Order:** `R_world = R_az(Z) × R_el(Y) × v_gimbal`

- Azimuth rotation (outer gimbal): Around Z-axis
- Elevation rotation (inner gimbal): Around Y-axis

### 2. Vector Helpers (`gimbalmotionmodebase.h`)

```cpp
// Element-wise deg/rad conversion for Eigen vectors
static inline Eigen::Vector3d degToRad(const Eigen::Vector3d& deg);
static inline Eigen::Vector3d radToDeg(const Eigen::Vector3d& rad);
```

---

## Version Requirements

- **Minimum:** Eigen 3.3.0
- **Recommended:** Eigen 3.4.0 or later
- **Tested:** Eigen 3.4.0 (Ubuntu 22.04 default)

---

## Troubleshooting

### "Eigen/Dense: No such file or directory"

**Cause:** Eigen not installed or include path incorrect

**Fix:**
```bash
# Install Eigen
sudo apt-get install libeigen3-dev

# Verify include path
ls /usr/include/eigen3/Eigen/Dense
# Should exist
```

### "Undefined reference to Eigen symbols"

**Cause:** Eigen is **header-only**, should not happen

**Fix:** Clean and rebuild
```bash
make clean
qmake El-7aress-Project-Prod.pro
make -j$(nproc)
```

### Wrong Eigen version

**Check version:**
```bash
dpkg -s libeigen3-dev | grep Version
```

**Upgrade if needed:**
```bash
sudo apt-get update
sudo apt-get upgrade libeigen3-dev
```

---

## Alternative: Manual Installation

If apt-get doesn't work:

```bash
# Download Eigen (header-only)
cd /tmp
wget https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
tar -xzf eigen-3.4.0.tar.gz

# Copy headers
sudo mkdir -p /usr/include/eigen3
sudo cp -r eigen-3.4.0/Eigen /usr/include/eigen3/

# Verify
ls /usr/include/eigen3/Eigen/Dense
```

---

## Build Verification

After installing Eigen, verify the build:

```bash
cd /path/to/El-7aress-Project-Prod
qmake El-7aress-Project-Prod.pro
make -j$(nproc)
```

**Success:** No Eigen-related errors

**If errors occur:** Check include path in .pro file matches your Eigen installation location

---

## Docker/Container Deployment

If deploying in Docker, add to Dockerfile:

```dockerfile
RUN apt-get update && \
    apt-get install -y libeigen3-dev && \
    rm -rf /var/lib/apt/lists/*
```

---

## Mathematical Correctness

The Eigen implementation has been verified:

✅ **Rotation order:** Correct for azimuth-over-elevation gimbal
✅ **Unit handling:** deg/s ↔ rad/s conversions correct
✅ **Matrix multiplication:** `R_az × R_el` (outer × inner) ✅
✅ **Vector transformations:** Linear and angular velocities handled correctly

See `docs/CODEBASE_AUDIT_REPORT.md` for full verification details.

---

**Installation complete!** The gimbal kinematics now use professional-grade rotation matrices. 🎯

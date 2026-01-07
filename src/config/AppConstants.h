#ifndef APPCONSTANTS_H
#define APPCONSTANTS_H

/**
 * @file AppConstants.h
 * @brief Centralized application constants for El 7arress RCWS
 *
 * This file contains compile-time constants organized by functional area.
 * Use these constants instead of magic numbers throughout the codebase.
 *
 * For runtime-configurable values, use DeviceConfiguration (loaded from config.json).
 * For user preferences, use QSettings.
 */

#include <cmath>

namespace RcwsConstants {

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================
namespace Math {
constexpr double PI = M_PI;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;
constexpr double MIL_TO_DEG = 0.05625;  // 1 mil = 1/6400 of circle
}  // namespace Math

// ============================================================================
// VIDEO PROCESSING
// ============================================================================
namespace Video {
// Default video dimensions
constexpr int DEFAULT_SOURCE_WIDTH = 1280;
constexpr int DEFAULT_SOURCE_HEIGHT = 720;
constexpr int DEFAULT_FRAMERATE = 30;

// Video processing limits
constexpr int MIN_VIDEO_WIDTH = 640;
constexpr int MAX_VIDEO_WIDTH = 4096;
constexpr int MIN_VIDEO_HEIGHT = 480;
constexpr int MAX_VIDEO_HEIGHT = 2160;

// Zoom levels
constexpr int MIN_ZOOM_LEVEL = 1;
constexpr int MAX_ZOOM_LEVEL = 32;
constexpr float DEFAULT_FOV_WIDE = 60.0f;  // degrees
constexpr float DEFAULT_FOV_TELE = 2.0f;   // degrees

// Tracking
constexpr float MIN_TRACKING_CONFIDENCE = 0.25f;
constexpr int TRACKING_LOST_FRAMES_THRESHOLD = 30;  // frames before declaring lost
}  // namespace Video

// ============================================================================
// OSD (ON-SCREEN DISPLAY)
// ============================================================================
namespace Osd {
// Display limits
constexpr int MAX_ELEMENTS = 512;
constexpr int MAX_TEXT_LENGTH = 256;

// Font settings
constexpr int DEFAULT_FONT_SIZE = 14;
constexpr int MIN_FONT_SIZE = 8;
constexpr int MAX_FONT_SIZE = 24;

// Reticle rendering
constexpr float RETICLE_LINE_WIDTH = 2.0f;
constexpr float RETICLE_CROSS_SIZE = 20.0f;     // pixels
constexpr float RETICLE_CIRCLE_RADIUS = 50.0f;  // pixels
constexpr int RETICLE_NUM_CIRCLE_POINTS = 100;

// Tracking box
constexpr float TRACKING_BOX_LINE_WIDTH = 2.0f;
constexpr float TRACKING_BOX_CORNER_SIZE = 10.0f;  // pixels

// Update rates
constexpr int OSD_UPDATE_RATE_HZ = 30;
constexpr int OSD_UPDATE_INTERVAL_MS = 1000 / OSD_UPDATE_RATE_HZ;
}  // namespace Osd

// ============================================================================
// GIMBAL CONTROL
// ============================================================================
namespace Gimbal {
// Physical limits (degrees) - defaults, overridden by config
constexpr float DEFAULT_MIN_AZIMUTH = -180.0f;
constexpr float DEFAULT_MAX_AZIMUTH = 180.0f;
constexpr float DEFAULT_MIN_ELEVATION = -20.0f;
constexpr float DEFAULT_MAX_ELEVATION = 60.0f;

// Speed limits (deg/s)
constexpr float MAX_SLEW_SPEED = 120.0f;
constexpr float MIN_SLEW_SPEED = 0.1f;
constexpr float DEFAULT_SLEW_SPEED = 30.0f;

// Acceleration (deg/s²)
constexpr float MAX_ACCELERATION = 100.0f;
constexpr float DEFAULT_ACCELERATION = 50.0f;

// Dead zones
constexpr float JOYSTICK_DEAD_ZONE = 0.05f;  // 5% dead zone
constexpr float POSITION_TOLERANCE = 0.01f;  // degrees

// Control loop timing
constexpr int CONTROL_LOOP_HZ = 100;
constexpr int CONTROL_LOOP_INTERVAL_MS = 1000 / CONTROL_LOOP_HZ;
}  // namespace Gimbal

// ============================================================================
// BALLISTICS
// ============================================================================
namespace Ballistics {
// Zeroing limits
constexpr float MAX_ZEROING_AZIMUTH_OFFSET = 10.0f;    // degrees
constexpr float MAX_ZEROING_ELEVATION_OFFSET = 10.0f;  // degrees
constexpr float ZEROING_STEP_SIZE = 0.1f;              // degrees

// Windage
constexpr float MAX_WIND_SPEED = 50.0f;  // knots
constexpr float MIN_WIND_SPEED = 0.0f;   // knots
constexpr float WIND_STEP_SIZE = 1.0f;   // knots

// Lead angle
constexpr float MAX_TARGET_SPEED = 100.0f;      // m/s
constexpr float DEFAULT_BULLET_SPEED = 850.0f;  // m/s
constexpr float MIN_BULLET_SPEED = 300.0f;      // m/s
constexpr float MAX_BULLET_SPEED = 1500.0f;     // m/s
}  // namespace Ballistics

// ============================================================================
// COMMUNICATION PROTOCOLS
// ============================================================================
namespace Protocol {
// Serial port settings
constexpr int PELCO_D_BAUDRATE = 9600;
constexpr int TAU2_BAUDRATE = 921600;  // FLIR Boson 640 uses 921600, not TAU2's 57600
constexpr int DEFAULT_SERIAL_BAUDRATE = 115200;
constexpr int MODBUS_RTU_BAUDRATE = 115200;
constexpr int SERVO_HIGH_SPEED_BAUDRATE = 230400;

// Timeouts (milliseconds)
constexpr int SERIAL_RESPONSE_TIMEOUT_MS = 1000;
constexpr int MODBUS_RESPONSE_TIMEOUT_MS = 500;
constexpr int DEVICE_RECONNECT_INTERVAL_MS = 5000;
constexpr int HEARTBEAT_INTERVAL_MS = 1000;

// Buffer sizes
constexpr int SERIAL_BUFFER_SIZE = 4096;
constexpr int MODBUS_MAX_FRAME_SIZE = 256;

// Retry parameters
constexpr int MAX_RETRY_ATTEMPTS = 3;
constexpr int RETRY_DELAY_MS = 100;
}  // namespace Protocol

// ============================================================================
// SENSORS
// ============================================================================
namespace Sensors {
// IMU
constexpr double IMU_SAMPLE_RATE_HZ = 100.0;
constexpr double IMU_MIN_TEMP_C = -40.0;
constexpr double IMU_MAX_TEMP_C = 85.0;
constexpr double IMU_TILT_WARNING_DEG = 30.0;  // Vehicle tilt warning threshold

// LRF (Laser Range Finder)
constexpr float LRF_MIN_RANGE_M = 50.0f;
constexpr float LRF_MAX_RANGE_M = 4000.0f;
constexpr float LRF_ACCURACY_M = 1.0f;

// Joystick
constexpr int JOYSTICK_AXIS_MIN = -32768;
constexpr int JOYSTICK_AXIS_MAX = 32767;
constexpr int JOYSTICK_AXIS_CENTER = 0;
}  // namespace Sensors

// ============================================================================
// ZONE MANAGEMENT
// ============================================================================
namespace Zones {
// Zone limits
constexpr int MAX_NO_FIRE_ZONES = 32;
constexpr int MAX_NO_TRAVERSE_ZONES = 16;
constexpr int MAX_TRP_POINTS = 64;  // Target Reference Points
constexpr int MAX_SECTOR_SCANS = 16;

// Zone definition
constexpr float MIN_ZONE_SIZE_DEG = 1.0f;
constexpr float MAX_ZONE_SIZE_DEG = 360.0f;

// Zone file format
constexpr int ZONE_FILE_VERSION = 1;
constexpr int MAX_ZONE_NAME_LENGTH = 64;
}  // namespace Zones

// ============================================================================
// WEAPON CONTROL
// ============================================================================
namespace Weapon {
// Fire control
constexpr int TRIGGER_DEBOUNCE_MS = 50;
constexpr int SAFETY_INTERLOCK_CHECK_MS = 100;

// Burst fire
constexpr int MIN_BURST_ROUNDS = 1;
constexpr int MAX_BURST_ROUNDS = 10;
constexpr int DEFAULT_BURST_ROUNDS = 3;
constexpr int ROUNDS_PER_MINUTE = 600;  // ROF
constexpr int MS_BETWEEN_ROUNDS = 60000 / ROUNDS_PER_MINUTE;

// Ammo tracking
constexpr int MAX_AMMO_COUNT = 9999;
}  // namespace Weapon

// ============================================================================
// DETECTION (YOLO)
// ============================================================================
namespace Detection {
// YOLO detection
constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.5f;
constexpr float MIN_CONFIDENCE_THRESHOLD = 0.1f;
constexpr float MAX_CONFIDENCE_THRESHOLD = 1.0f;

constexpr float DEFAULT_NMS_THRESHOLD = 0.4f;  // Non-Maximum Suppression

constexpr int MAX_DETECTIONS_PER_FRAME = 100;

// Object classes (COCO dataset)
constexpr int CLASS_PERSON = 0;
constexpr int CLASS_VEHICLE = 2;
constexpr int CLASS_TRUCK = 7;
}  // namespace Detection

// ============================================================================
// SYSTEM PERFORMANCE
// ============================================================================
namespace Performance {
// Thread priorities
constexpr int VIDEO_THREAD_PRIORITY = 5;    // High priority
constexpr int SERVO_THREAD_PRIORITY = 4;    // High priority
constexpr int CONTROL_THREAD_PRIORITY = 3;  // Medium priority
constexpr int UI_THREAD_PRIORITY = 2;       // Normal priority

// Memory limits
constexpr int VIDEO_FRAME_BUFFER_SIZE = 10;       // frames
constexpr int TRACKING_HISTORY_SIZE = 1000;       // positions
constexpr int GIMBAL_MOTION_BUFFER_SIZE = 60000;  // 1 min @ 100Hz
constexpr int IMU_DATA_BUFFER_SIZE = 120000;      // 20 min @ 100Hz
constexpr int TRACKING_DATA_BUFFER_SIZE = 36000;  // 20 min @ 30Hz

// Watchdog
constexpr int WATCHDOG_TIMEOUT_MS = 5000;
constexpr int WATCHDOG_CHECK_INTERVAL_MS = 1000;
}  // namespace Performance

// ============================================================================
// SAFETY
// ============================================================================
namespace Safety {
// Emergency stop
constexpr int EMERGENCY_STOP_DEBOUNCE_MS = 20;
constexpr int EMERGENCY_STOP_CLEAR_DELAY_MS = 2000;

// Interlocks
constexpr bool REQUIRE_STATION_ENABLED = true;
constexpr bool REQUIRE_GUN_ARMED = true;
constexpr bool REQUIRE_VALID_TARGET = false;  // For training mode

// Temperature limits (Celsius)
constexpr double MOTOR_MAX_TEMP_C = 80.0;
constexpr double MOTOR_WARNING_TEMP_C = 70.0;
constexpr double DRIVER_MAX_TEMP_C = 85.0;
constexpr double DRIVER_WARNING_TEMP_C = 75.0;
}  // namespace Safety

// ============================================================================
// UI COLORS (Default theme)
// ============================================================================
namespace Colors {
// Primary accent color
constexpr const char* DEFAULT_ACCENT = "#46E2A5";

// Status colors
constexpr const char* COLOR_NORMAL = "#46E2A5";   // Green
constexpr const char* COLOR_WARNING = "#FFD700";  // Gold
constexpr const char* COLOR_ERROR = "#FF4444";    // Red
constexpr const char* COLOR_INFO = "#4A90E2";     // Blue

// Tracking states
constexpr const char* COLOR_TRACKING_LOST = "#FF4444";
constexpr const char* COLOR_TRACKING_PENDING = "#FFD700";
constexpr const char* COLOR_TRACKING_ACTIVE = "#46E2A5";

// Zone colors
constexpr const char* COLOR_NO_FIRE_ZONE = "#FF4444";
constexpr const char* COLOR_NO_TRAVERSE_ZONE = "#FFD700";
constexpr const char* COLOR_TRP = "#4A90E2";
}  // namespace Colors

// ============================================================================
// FILE PATHS
// ============================================================================
namespace Paths {
constexpr const char* CONFIG_FILE = "./config/devices.json";
constexpr const char* ZONE_DATA_DIR = "./data/zones/";
constexpr const char* LOG_DIR = "./logs/";
constexpr const char* DATABASE_FILE = "./data/rcws_history.db";
constexpr const char* YOLO_MODEL_PATH = "./models/yolov8s.onnx";
}  // namespace Paths

// ============================================================================
// VERSION INFO
// ============================================================================
namespace Version {
constexpr const char* APP_NAME = "El 7arress RCWS";
constexpr const char* sAPP_VERSION = "4.5";
constexpr const char* BUILD_DATE = __DATE__;
constexpr const char* BUILD_TIME = __TIME__;
}  // namespace Version

}  // namespace RcwsConstants

#endif  // APPCONSTANTS_H

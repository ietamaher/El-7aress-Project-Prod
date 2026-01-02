#include "MotionTuningConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Initialize static members
MotionTuningConfig MotionTuningConfig::m_instance;
bool MotionTuningConfig::m_loaded = false;

bool MotionTuningConfig::load(const QString& path)
{
    qInfo() << "[MotionTuningConfig] Loading from:" << path;

    if (loadFromFile(path)) {
        m_loaded = true;
        qInfo() << "[MotionTuningConfig] ✓ Loaded successfully";
        return true;
    }

    qWarning() << "[MotionTuningConfig] ⚠ Failed to load, using default values";
    m_loaded = false;
    return false;
}

const MotionTuningConfig& MotionTuningConfig::instance()
{
    if (!m_loaded) {
        qWarning() << "[MotionTuningConfig] Configuration not loaded! Using defaults.";
        qWarning() << "[MotionTuningConfig] Call load() before instance()";
    }
    return m_instance;
}

bool MotionTuningConfig::isLoaded()
{
    return m_loaded;
}

bool MotionTuningConfig::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "[MotionTuningConfig] Cannot open file:" << filePath;
        qCritical() << "[MotionTuningConfig] Error:" << file.errorString();
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << "[MotionTuningConfig] JSON parse error:" << parseError.errorString();
        qCritical() << "[MotionTuningConfig] at offset:" << parseError.offset;
        return false;
    }

    if (!doc.isObject()) {
        qCritical() << "[MotionTuningConfig] Root element is not a JSON object";
        return false;
    }

    QJsonObject root = doc.object();

    // ========================================================================
    // FILTERS
    // ========================================================================
    if (root.contains("filters") && root["filters"].isObject()) {
        QJsonObject filters = root["filters"].toObject();

        if (filters.contains("gyro") && filters["gyro"].isObject()) {
            QJsonObject gyro = filters["gyro"].toObject();
            m_instance.filters.gyroCutoffFreqHz = gyro.value("cutoffFreqHz").toDouble(5.0);
        }

        if (filters.contains("tracking") && filters["tracking"].isObject()) {
            QJsonObject tracking = filters["tracking"].toObject();
            m_instance.filters.trackingPositionTau = tracking.value("positionTau").toDouble(0.12);
            m_instance.filters.trackingVelocityTau = tracking.value("velocityTau").toDouble(0.08);
        }

        if (filters.contains("manual") && filters["manual"].isObject()) {
            QJsonObject manual = filters["manual"].toObject();
            m_instance.filters.manualJoystickTau = manual.value("joystickTau").toDouble(0.08);
        }
    }

    // ========================================================================
    // MOTION LIMITS
    // ========================================================================
    if (root.contains("motion") && root["motion"].isObject()) {
        QJsonObject motion = root["motion"].toObject();
        m_instance.motion.maxAccelerationDegS2 = motion.value("maxAccelerationDegS2").toDouble(50.0);
        m_instance.motion.scanMaxAccelDegS2 = motion.value("scanMaxAccelDegS2").toDouble(20.0);
        m_instance.motion.trpMaxAccelDegS2 = motion.value("trpMaxAccelDegS2").toDouble(50.0);
        m_instance.motion.trpDefaultTravelSpeed = motion.value("trpDefaultTravelSpeed").toDouble(15.0);
        m_instance.motion.maxVelocityDegS = motion.value("maxVelocityDegS").toDouble(30.0);
        m_instance.motion.arrivalThresholdDeg = motion.value("arrivalThresholdDeg").toDouble(0.5);
        m_instance.motion.updateIntervalS = motion.value("updateIntervalS").toDouble(0.05);
    }

    // ========================================================================
    // SERVO CONSTANTS
    // ========================================================================
    if (root.contains("servo") && root["servo"].isObject()) {
        QJsonObject servo = root["servo"].toObject();
        m_instance.servo.azStepsPerDegree = servo.value("azStepsPerDegree").toDouble(618.0556);
        m_instance.servo.elStepsPerDegree = servo.value("elStepsPerDegree").toDouble(555.5556);
    }

    // ========================================================================
    // PID GAINS
    // ========================================================================
    if (root.contains("pid") && root["pid"].isObject()) {
        QJsonObject pid = root["pid"].toObject();

        // Tracking mode
        if (pid.contains("tracking") && pid["tracking"].isObject()) {
            QJsonObject tracking = pid["tracking"].toObject();
            if (tracking.contains("azimuth")) {
                m_instance.trackingAz = parsePIDGains(tracking["azimuth"].toObject());
            }
            if (tracking.contains("elevation")) {
                m_instance.trackingEl = parsePIDGains(tracking["elevation"].toObject());
            }
        }

        // AutoSectorScan mode
        if (pid.contains("autoSectorScan") && pid["autoSectorScan"].isObject()) {
            QJsonObject autoScan = pid["autoSectorScan"].toObject();
            if (autoScan.contains("azimuth")) {
                m_instance.autoScanAz = parsePIDGains(autoScan["azimuth"].toObject());
            }
            if (autoScan.contains("elevation")) {
                m_instance.autoScanEl = parsePIDGains(autoScan["elevation"].toObject());
            }
            m_instance.autoScanParams.decelerationDistanceDeg =
                autoScan.value("decelerationDistanceDeg").toDouble(5.0);
            m_instance.autoScanParams.arrivalThresholdDeg =
                autoScan.value("arrivalThresholdDeg").toDouble(0.2);
        }

        // TRP scan mode
        if (pid.contains("trpScan") && pid["trpScan"].isObject()) {
            QJsonObject trpScan = pid["trpScan"].toObject();
            if (trpScan.contains("azimuth")) {
                m_instance.trpScanAz = parsePIDGains(trpScan["azimuth"].toObject());
            }
            if (trpScan.contains("elevation")) {
                m_instance.trpScanEl = parsePIDGains(trpScan["elevation"].toObject());
            }
            m_instance.trpScanParams.decelerationDistanceDeg =
                trpScan.value("decelerationDistanceDeg").toDouble(3.0);
            m_instance.trpScanParams.arrivalThresholdDeg =
                trpScan.value("arrivalThresholdDeg").toDouble(0.1);
        }

        // Radar slew mode
        if (pid.contains("radarSlew") && pid["radarSlew"].isObject()) {
            QJsonObject radarSlew = pid["radarSlew"].toObject();
            if (radarSlew.contains("azimuth")) {
                m_instance.radarSlewAz = parsePIDGains(radarSlew["azimuth"].toObject());
            }
            if (radarSlew.contains("elevation")) {
                m_instance.radarSlewEl = parsePIDGains(radarSlew["elevation"].toObject());
            }
        }
    }

    // ========================================================================
    // ACCELERATION LIMITS
    // ========================================================================
    if (root.contains("accelLimits") && root["accelLimits"].isObject()) {
        QJsonObject accelLimits = root["accelLimits"].toObject();
        m_instance.manualLimits.maxAccelHzPerSec =
            accelLimits.value("manualMaxAccelHzPerSec").toDouble(500000.0);
    }

    // ========================================================================
    // AXIS-SPECIFIC SERVO PARAMETERS (AZD-KX Optimization)
    // ========================================================================
    // Defaults optimized for RCWS:
    // - Azimuth: Heavy turret, smooth decel (100000 Hz/s) to avoid overvoltage
    // - Elevation: Light gun, crisp decel (300000 Hz/s) to avoid overshoot
    // - Elevation current: 70% to prevent overheating/stall on lighter axis
    if (root.contains("axisServo") && root["axisServo"].isObject()) {
        QJsonObject axisServo = root["axisServo"].toObject();

        if (axisServo.contains("azimuth") && axisServo["azimuth"].isObject()) {
            QJsonObject az = axisServo["azimuth"].toObject();
            m_instance.axisServo.azimuth.accelHz =
                static_cast<quint32>(az.value("accelHz").toDouble(150000));
            m_instance.axisServo.azimuth.decelHz =
                static_cast<quint32>(az.value("decelHz").toDouble(100000));
            m_instance.axisServo.azimuth.currentPercent =
                static_cast<quint32>(az.value("currentPercent").toDouble(1000));
        }

        if (axisServo.contains("elevation") && axisServo["elevation"].isObject()) {
            QJsonObject el = axisServo["elevation"].toObject();
            m_instance.axisServo.elevation.accelHz =
                static_cast<quint32>(el.value("accelHz").toDouble(150000));
            m_instance.axisServo.elevation.decelHz =
                static_cast<quint32>(el.value("decelHz").toDouble(300000));
            m_instance.axisServo.elevation.currentPercent =
                static_cast<quint32>(el.value("currentPercent").toDouble(700));
        }
    }

    qInfo() << "[MotionTuningConfig] Configuration summary:";
    qInfo() << "  Gyro filter cutoff:" << m_instance.filters.gyroCutoffFreqHz << "Hz";
    qInfo() << "  Max acceleration:" << m_instance.motion.maxAccelerationDegS2 << "deg/s²";
    qInfo() << "  Tracking Az PID: Kp=" << m_instance.trackingAz.kp
            << "Ki=" << m_instance.trackingAz.ki
            << "Kd=" << m_instance.trackingAz.kd;
    qInfo() << "  TRP travel speed:" << m_instance.motion.trpDefaultTravelSpeed << "deg/s";
    qInfo() << "  Axis Servo - Azimuth: accel=" << m_instance.axisServo.azimuth.accelHz
            << "Hz/s, decel=" << m_instance.axisServo.azimuth.decelHz
            << "Hz/s, current=" << m_instance.axisServo.azimuth.currentPercent / 10.0 << "%";
    qInfo() << "  Axis Servo - Elevation: accel=" << m_instance.axisServo.elevation.accelHz
            << "Hz/s, decel=" << m_instance.axisServo.elevation.decelHz
            << "Hz/s, current=" << m_instance.axisServo.elevation.currentPercent / 10.0 << "%";

    return true;
}

MotionTuningConfig::PIDGains MotionTuningConfig::parsePIDGains(const QJsonObject& obj)
{
    PIDGains gains;
    gains.kp = obj.value("kp").toDouble(1.0);
    gains.ki = obj.value("ki").toDouble(0.01);
    gains.kd = obj.value("kd").toDouble(0.05);
    gains.maxIntegral = obj.value("maxIntegral").toDouble(20.0);
    return gains;
}

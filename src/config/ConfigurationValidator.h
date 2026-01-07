#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QString>
#include <QStringList>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
class DeviceConfiguration;

/**
 * @class ConfigurationValidator
 * @brief Validates configuration settings against defined constraints.
 *
 * This class ensures that configuration values loaded from config.json
 * are within acceptable ranges and meet system requirements before
 * the system starts.
 */
class ConfigurationValidator {
public:
    /**
     * @brief Validates all configuration sections
     * @return true if all validations pass
     */
    static bool validateAll();

    /**
     * @brief Gets list of validation errors
     * @return List of error messages from last validation
     */
    static const QStringList& errors() {
        return m_errors;
    }

    /**
     * @brief Gets list of validation warnings
     * @return List of warning messages from last validation
     */
    static const QStringList& warnings() {
        return m_warnings;
    }

private:
    // Individual validation methods
    static bool validateSystem();
    static bool validateVideo();
    static bool validateGimbal();
    static bool validateBallistics();
    static bool validateUI();
    static bool validateSafety();
    static bool validatePerformance();
    static bool validateHardware();
    static bool validateMotionTuning();

    // Helper methods
    static void addError(const QString& message);
    static void addWarning(const QString& message);
    static void clearMessages();

    static bool validateRange(float value, float min, float max, const QString& fieldName);
    static bool validateRange(int value, int min, int max, const QString& fieldName);
    static bool validateFileExists(const QString& path, const QString& fieldName,
                                   bool required = true);
    static bool validatePortPath(const QString& port, const QString& fieldName);

    static QStringList m_errors;
    static QStringList m_warnings;
};


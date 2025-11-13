#include "ballisticslut.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cmath>

BallisticsLUT::BallisticsLUT()
{
}

BallisticsLUT::~BallisticsLUT()
{
}

bool BallisticsLUT::loadTable(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[BallisticsLUT] Failed to open table file:" << filepath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[BallisticsLUT] Invalid JSON in table file:" << filepath;
        return false;
    }

    QJsonObject root = doc.object();

    // Parse ammunition metadata
    QJsonObject ammoObj = root["ammunition"].toObject();
    m_metadata.name = ammoObj["name"].toString();
    m_metadata.diameter_mm = static_cast<float>(ammoObj["diameter_mm"].toDouble());
    m_metadata.mass_grams = static_cast<float>(ammoObj["mass_grams"].toDouble());
    m_metadata.bc_g1 = static_cast<float>(ammoObj["bc_g1"].toDouble());
    m_metadata.muzzle_velocity_ms = static_cast<float>(ammoObj["muzzle_velocity_ms"].toDouble());

    // Parse lookup table
    QJsonArray tableArray = root["lookup_table"].toArray();
    m_table.clear();
    m_table.reserve(tableArray.size());

    for (const QJsonValue& val : tableArray) {
        QJsonObject entry = val.toObject();

        BallisticEntry be;
        be.range_m = static_cast<uint16_t>(entry["range_m"].toInt());
        be.elevation_mils = static_cast<float>(entry["elevation_mils"].toDouble());
        be.tof_s = static_cast<float>(entry["tof_s"].toDouble());
        be.impact_velocity_ms = static_cast<float>(entry["impact_velocity_ms"].toDouble());

        m_table.append(be);
    }

    m_tablePath = filepath;

    qInfo() << "[BallisticsLUT] Loaded table:" << m_metadata.name
            << "| Entries:" << m_table.size()
            << "| Range:" << getMinRange() << "-" << getMaxRange() << "m"
            << "| MV:" << m_metadata.muzzle_velocity_ms << "m/s"
            << "| BC:" << m_metadata.bc_g1;

    return !m_table.isEmpty();
}

int BallisticsLUT::findBracket(float target_range) const
{
    const int size = m_table.size();

    // Edge cases
    if (target_range <= m_table.first().range_m) {
        return 0;
    }
    if (target_range >= m_table.last().range_m) {
        return size - 2;  // Last valid bracket
    }

    // Binary search - O(log n) performance
    int left = 0;
    int right = size - 1;

    while (right - left > 1) {
        int mid = (left + right) / 2;
        if (m_table[mid].range_m < target_range) {
            left = mid;
        } else {
            right = mid;
        }
    }

    return left;  // Returns index where table[left].range <= target < table[left+1].range
}

BallisticSolution BallisticsLUT::interpolate(int idx, float target_range) const
{
    BallisticSolution sol;

    // Validate index
    if (idx < 0 || idx >= m_table.size() - 1) {
        sol.valid = false;
        return sol;
    }

    // Get bracket entries
    const BallisticEntry& e1 = m_table[idx];
    const BallisticEntry& e2 = m_table[idx + 1];

    // Calculate interpolation factor
    float r1 = static_cast<float>(e1.range_m);
    float r2 = static_cast<float>(e2.range_m);
    float t = (target_range - r1) / (r2 - r1);

    // Clamp t to [0, 1] for safety
    t = qBound(0.0f, t, 1.0f);

    // Linear interpolation for all values
    sol.elevation_mils = e1.elevation_mils + t * (e2.elevation_mils - e1.elevation_mils);
    sol.tof_s = e1.tof_s + t * (e2.tof_s - e1.tof_s);
    sol.impact_velocity_ms = e1.impact_velocity_ms + t * (e2.impact_velocity_ms - e1.impact_velocity_ms);

    // Convert mils to degrees for compatibility
    sol.elevation_deg = sol.elevation_mils * 0.05625f;  // 1 mil = 0.05625 deg

    sol.azimuth_correction_mils = 0.0f;  // Will be set by wind correction
    sol.valid = true;

    return sol;
}

float BallisticsLUT::applyTemperatureCorrection(float elevation_mils, float temp_celsius) const
{
    // Air density changes with temperature
    // Standard conditions: 15°C (288.15 K)
    // Formula: ρ_correction = sqrt(T_standard / T_actual)

    float temp_kelvin = temp_celsius + 273.15f;
    float temp_correction = std::sqrt(288.15f / temp_kelvin);

    return elevation_mils * temp_correction;
}

float BallisticsLUT::applyAltitudeCorrection(float elevation_mils, float altitude_m) const
{
    // Air density decreases exponentially with altitude
    // Scale height: ~8500m
    // Formula: ρ(h) = ρ₀ × exp(-h / 8500)

    float rho_ratio = std::exp(-altitude_m / 8500.0f);
    float altitude_correction = 1.0f / rho_ratio;

    return elevation_mils * altitude_correction;
}

float BallisticsLUT::calculateWindCorrection(float range_m, float tof_s, float crosswind_ms) const
{
    // Wind deflection during bullet flight
    // deflection = crosswind × TOF
    // lead_angle (mils) = (deflection / range) × 1000

    if (range_m < 1.0f) {
        return 0.0f;  // Avoid division by zero
    }

    float deflection_m = crosswind_ms * tof_s;
    float lead_mils = (deflection_m / range_m) * 1000.0f;

    return lead_mils;
}

BallisticSolution BallisticsLUT::getSolution(float target_range_m,
                                               float temp_celsius,
                                               float altitude_m,
                                               float crosswind_ms) const
{
    BallisticSolution sol;

    // Validate table is loaded
    if (m_table.isEmpty()) {
        qWarning() << "[BallisticsLUT] getSolution() called with empty table!";
        sol.valid = false;
        return sol;
    }

    // Validate range is within table bounds
    if (target_range_m < getMinRange() || target_range_m > getMaxRange()) {
        qDebug() << "[BallisticsLUT] Range out of bounds:"
                 << target_range_m << "m (valid range:"
                 << getMinRange() << "-" << getMaxRange() << "m)";
        sol.valid = false;
        return sol;
    }

    // STEP 1: Find bracket and interpolate base solution
    int idx = findBracket(target_range_m);
    sol = interpolate(idx, target_range_m);

    if (!sol.valid) {
        return sol;
    }

    // STEP 2: Apply environmental corrections

    // Temperature correction
    sol.elevation_mils = applyTemperatureCorrection(sol.elevation_mils, temp_celsius);

    // Altitude correction
    sol.elevation_mils = applyAltitudeCorrection(sol.elevation_mils, altitude_m);

    // Update degrees after corrections
    sol.elevation_deg = sol.elevation_mils * 0.05625f;

    // Wind correction (azimuth only)
    sol.azimuth_correction_mils = calculateWindCorrection(target_range_m, sol.tof_s, crosswind_ms);

    /*qDebug() << "[BallisticsLUT] Solution @ " << target_range_m << "m:"
             << "Elev" << sol.elevation_mils << "mils"
             << "TOF" << sol.tof_s << "s"
             << "Vel" << sol.impact_velocity_ms << "m/s"
             << "Wind" << sol.azimuth_correction_mils << "mils";*/

    return sol;
}

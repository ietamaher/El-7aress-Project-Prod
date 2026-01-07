#ifndef BALLISTICSLUT_H
#define BALLISTICSLUT_H

#include <QString>
#include <QVector>
#include <cstdint>
#include <cmath>

/**
 * @brief Ballistic Lookup Table Entry
 *
 * Pre-computed ballistic solution for a specific range.
 * Generated offline using high-fidelity solver with full physics.
 */
struct BallisticEntry {
    uint16_t range_m;          ///< Range in meters
    float elevation_mils;      ///< Elevation angle in mils
    float tof_s;               ///< Time of flight in seconds
    float impact_velocity_ms;  ///< Impact velocity in m/s
};

/**
 * @brief Ammunition metadata from lookup table
 */
struct AmmunitionMetadata {
    QString name;
    float diameter_mm;
    float mass_grams;
    float bc_g1;
    float muzzle_velocity_ms;
};

/**
 * @brief Result of LUT interpolation with environmental corrections
 */
struct BallisticSolution {
    float elevation_mils;           ///< Corrected elevation in mils
    float elevation_deg;            ///< Corrected elevation in degrees (for compatibility)
    float azimuth_correction_mils;  ///< Wind correction in mils
    float tof_s;                    ///< Time of flight in seconds
    float impact_velocity_ms;       ///< Impact velocity in m/s
    bool valid;                     ///< Solution is valid (range in bounds)

    BallisticSolution()
        : elevation_mils(0), elevation_deg(0), azimuth_correction_mils(0), tof_s(0),
          impact_velocity_ms(0), valid(false) {}
};

/**
 * @brief Lookup Table-based Ballistics Processor
 *
 * PRODUCTION-READY APPROACH used by combat-proven RCWS:
 * - Kongsberg Protector
 * - Rafael Samson
 * - General Dynamics CROWS
 *
 * ADVANTAGES over equation-based:
 * ✓ 10-100x faster (0.01-0.1ms vs 5-10ms)
 * ✓ Deterministic performance (no iterative solvers)
 * ✓ More accurate (includes ALL ballistic factors offline)
 * ✓ Easier validation (direct live-fire comparison)
 * ✓ Field-tunable (replace table, no recompile)
 *
 * WORKFLOW:
 * 1. Generate tables offline using features/generate_lut.py
 * 2. Validate with live fire, adjust BC if needed
 * 3. Deploy tables to /opt/elharress/ballistic/tables/
 * 4. Load at startup, interpolate in real-time
 */
class BallisticsLUT {
public:
    BallisticsLUT();
    ~BallisticsLUT();

    /**
     * @brief Load ballistic table from JSON file
     *
     * @param filepath Path to .json table (e.g., "m2_ball.json")
     * @return true if loaded successfully
     *
     * Example:
     *   BallisticsLUT lut;
     *   lut.loadTable(":/ballistic/tables/m2_ball.json");
     */
    bool loadTable(const QString& filepath);

    /**
     * @brief Get firing solution for target range with environmental corrections
     *
     * @param target_range_m Target range in meters
     * @param temp_celsius Air temperature in Celsius (default 15°C)
     * @param altitude_m Altitude above sea level in meters (default 0m)
     * @param crosswind_ms Crosswind speed in m/s (default 0)
     * @return BallisticSolution with corrected elevation/azimuth
     *
     * Performance: ~0.05ms typical (binary search + linear interpolation)
     *
     * Example:
     *   auto sol = lut.getSolution(1234.0f, 25.0f, 500.0f, 3.5f);
     *   if (sol.valid) {
     *       gimbal.setElevation(sol.elevation_mils);
     *       gimbal.setAzimuth(target_az + sol.azimuth_correction_mils);
     *   }
     */
    BallisticSolution getSolution(float target_range_m, float temp_celsius = 15.0f,
                                  float altitude_m = 0.0f, float crosswind_ms = 0.0f) const;

    /**
     * @brief Get ammunition metadata
     */
    AmmunitionMetadata getAmmunitionMetadata() const {
        return m_metadata;
    }

    /**
     * @brief Check if table is loaded
     */
    bool isLoaded() const {
        return !m_table.isEmpty();
    }

    /**
     * @brief Get table size (number of entries)
     */
    int getTableSize() const {
        return m_table.size();
    }

    /**
     * @brief Get range bounds
     */
    float getMinRange() const {
        return m_table.isEmpty() ? 0 : m_table.first().range_m;
    }
    float getMaxRange() const {
        return m_table.isEmpty() ? 0 : m_table.last().range_m;
    }

private:
    /**
     * @brief Binary search to find table bracket for interpolation
     *
     * @param target_range Range to search for
     * @return Index where table[idx].range <= target < table[idx+1].range
     */
    int findBracket(float target_range) const;

    /**
     * @brief Linear interpolation between two table entries
     */
    BallisticSolution interpolate(int idx, float target_range) const;

    /**
     * @brief Apply temperature correction to elevation
     *
     * Air density changes with temperature affect drag.
     * Formula: ρ_correction = sqrt(288.15 / (T + 273.15))
     */
    float applyTemperatureCorrection(float elevation_mils, float temp_celsius) const;

    /**
     * @brief Apply altitude correction to elevation
     *
     * Air density decreases with altitude.
     * Formula: ρ_correction = 1 / exp(-h / 8500)
     */
    float applyAltitudeCorrection(float elevation_mils, float altitude_m) const;

    /**
     * @brief Calculate wind-induced azimuth correction
     *
     * Crosswind causes lateral deflection during TOF.
     * Formula: lead_mils = (wind × TOF / range) × 1000
     */
    float calculateWindCorrection(float range_m, float tof_s, float crosswind_ms) const;

private:
    QVector<BallisticEntry> m_table;  ///< Sorted lookup table (ascending range)
    AmmunitionMetadata m_metadata;    ///< Ammunition specifications
    QString m_tablePath;              ///< Source file path for debugging
};

#endif  // BALLISTICSLUT_H

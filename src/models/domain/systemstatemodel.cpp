/**
 * @file SystemStateModel.cpp
 * @brief Implementation of SystemStateModel class
 * 
 * This file contains ALL method implementations for the SystemStateModel class,
 * organized to match the EXACT order in SystemStateModel.h for easy navigation.
 * 
 * ORGANIZATION (matches header file 1:1):
 * 1. Constructor
 * 2. Core System Data Management
 * 3. User Interface Controls
 * 4. Weapon Control and Tracking
 * 5. Fire Control and Safety Zones
 * 6. Lead Angle Compensation
 * 7. Area Zone Management
 * 8. Auto Sector Scan Management
 * 9. Target Reference Point (TRP) Management
 * 10. Configuration File Management
 * 11. Weapon Zeroing Procedures
 * 12. Windage Compensation
 * 13. Tracking System Control
 * 14. State Transition Methods
 * 15. Radar Interface
 * 16. Hardware Interface Slots
 * 17. Sensor Data Slots
 * 18. Joystick Control Slots
 * 19. System Mode Control Slots
 * 20. Utility Methods
 * 21. Private Helper Methods
 * 
 * @author MB
 * @date 19 Juin 2025
 * @version 1.1 - Fully reorganized to match header structure
 */

#include "systemstatemodel.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>  // For applicationDirPath()
#include <QDateTime>         // For home calibration timestamp
#include <algorithm> // For std::find_if, std::sort (if needed)
#include <set>       // For getting unique page numbers

SystemStateModel::SystemStateModel(QObject *parent)
    : QObject(parent),
      m_nextAreaZoneId(1), // Start IDs from 1
      m_nextSectorScanId(1),
      m_nextTRPId(1)
{
    // Initialize m_currentStateData with defaults if needed
    clearZeroing(); // Zero is lost on power down
    clearWindage(); // Windage is zero on startup

    // ✅ CRITICAL FIX: Calculate initial reticle and CCIP positions
    // Without this, SystemStateData has default initialization values which
    // may be incorrect if image dimensions or FOV change after initialization.
    // This ensures CCIP and acquisition box have correct positions from startup.
    recalculateDerivedAimpointData();
    qDebug() << "✓ SystemStateModel initialized - reticle and CCIP positions calculated"
             << "at (" << m_currentStateData.reticleAimpointImageX_px << ","
             << m_currentStateData.reticleAimpointImageY_px << ")";

    // ========================================================================
    // ZONES.JSON LOADING - FIRST-RUN TEMPLATE COPY
    // If zones.json doesn't exist in filesystem, copy from embedded resource
    // ========================================================================
    QString zonesPath = QCoreApplication::applicationDirPath() + "/config/zones.json";

    // First-run: copy template from embedded resource if file doesn't exist
    if (!QFile::exists(zonesPath)) {
        qInfo() << "zones.json not found, creating from embedded template";

        // Copy from resource to filesystem
        if (QFile::copy(":/config/zones.json", zonesPath)) {
            // Make the file writable (resource files are read-only by default)
            QFile::setPermissions(zonesPath, QFile::WriteOwner | QFile::ReadOwner | QFile::ReadGroup);
            qInfo() << "Created default zones.json at:" << zonesPath;
        } else {
            qWarning() << "Failed to copy zones.json template from resources";
            qWarning() << "Starting with empty zones - operator can define zones manually";
            // Don't fail - operator can create zones during operation
        }
    }

    // Load zones from filesystem (or start with empty zones if copy failed)
    if (QFile::exists(zonesPath)) {
        if (loadZonesFromFile(zonesPath)) {
            qInfo() << "Loaded zones.json from:" << zonesPath;
        } else {
            qWarning() << "Failed to load zones.json - starting with empty zones";
        }
    } else {
        qInfo() << "No zones file available - starting with empty zones";
    }

    // ========================================================================
    // AZIMUTH HOME CALIBRATION LOADING
    // Load any saved home offset calibration for ABZO encoder drift compensation
    // ========================================================================
    loadHomeCalibration();

    // --- POPULATE DUMMY RADAR DATA FOR TESTING ---
    QVector<SimpleRadarPlot> dummyPlots;
    dummyPlots.append({101, 45.0f, 1500.0f, 180.0f, 0.0f});   // ID 101, NE quadrant, 1.5km, stationary (course away)
    dummyPlots.append({102, 110.0f, 850.0f, 290.0f, 5.0f});   // ID 102, SE quadrant, 850m, moving slowly
    dummyPlots.append({103, 315.0f, 2200.0f, 120.0f, 15.0f}); // ID 103, NW quadrant, 2.2km, moving moderately
    dummyPlots.append({104, 260.0f, 500.0f, 80.0f, 25.0f});   // ID 104, SW quadrant, 500m, moving quickly
    dummyPlots.append({105, 5.0f, 3100.0f, 175.0f, -2.0f});  // ID 105, Directly ahead, 3.1km, moving away slowly
    dummyPlots.append({106, 178.0f, 4500.0f, 0.0f, 2.0f});   // ID 106, Directly behind, 4.5km, moving towards

    // Create a new SystemStateData object, populate it, and set it in the model
     SystemStateData initialData = m_currentStateData;
    initialData.radarPlots = dummyPlots;
    // Set the initially selected target to be the first one in the list, or 0 for none
    /*if (!dummyPlots.isEmpty()) {
        initialData.selectedRadarTrackId = dummyPlots.first().id;
    }*/
    //m_stateModel->updateData(initialData); // Use your central update method to set the data and emit signals
    updateData(initialData);
}

SystemStateModel::~SystemStateModel() {
    // ✅ CRITICAL FIX: Save zones to file on application shutdown
    // This ensures zones persist even if app closes unexpectedly
    QString zonesPath = QCoreApplication::applicationDirPath() + "/config/zones.json";

    qInfo() << "SystemStateModel: Shutting down, saving zones to" << zonesPath;

    if (saveZonesToFile(zonesPath)) {
        qInfo() << "✓ Zones saved successfully on shutdown";
    } else {
        qWarning() << "✗ Failed to save zones on shutdown!";
    }
}

// --- General Data Update ---
void SystemStateModel::updateData(const SystemStateData &newState) {

    SystemStateData oldData = m_currentStateData;
    static int count = 0;
    /*if (++count % 100 == 0) qDebug() << "dataChanged signals:" << count;
    // Check if anything has actually changed to avoid unnecessary signals/updates
    if (oldData == newState) { // Assumes you have operator== for SystemStateData
        return;
    }*/
    if (m_currentStateData != newState) {
        // Check specifically if gimbal position changed before updating m_currentStateData
        bool gimbalChanged = !qFuzzyCompare(m_currentStateData.gimbalAz, newState.gimbalAz) ||
                             !qFuzzyCompare(m_currentStateData.gimbalEl, newState.gimbalEl);

        // ========================================================================
        // CRITICAL: Detect ballistic drop offset changes (from WeaponController)
        // ========================================================================
        // When gimbal moves, WeaponController recalculates crosswind and ballistic drop.
        // We MUST recalculate reticle position to reflect the new drop offsets!
        // This fixes the bug where reticle only updates on camera switch.
        // ========================================================================
        bool ballisticOffsetsChanged =
            !qFuzzyCompare(m_currentStateData.ballisticDropOffsetAz, newState.ballisticDropOffsetAz) ||
            !qFuzzyCompare(m_currentStateData.ballisticDropOffsetEl, newState.ballisticDropOffsetEl) ||
            (m_currentStateData.ballisticDropActive != newState.ballisticDropActive) ||
            !qFuzzyCompare(m_currentStateData.motionLeadOffsetAz, newState.motionLeadOffsetAz) ||
            !qFuzzyCompare(m_currentStateData.motionLeadOffsetEl, newState.motionLeadOffsetEl);

        m_currentStateData = newState;
        processStateTransitions(oldData, m_currentStateData);

        // Recalculate reticle position if ballistic offsets changed
        if (ballisticOffsetsChanged) {
            recalculateDerivedAimpointData();
        }

        emit dataChanged(m_currentStateData);

        // Emit gimbal position change if it occurred
        if (gimbalChanged) {
            emit gimbalPositionChanged(m_currentStateData.gimbalAz, m_currentStateData.gimbalEl);
        }
    }
}

// --- UI Related Setters Implementation  ---
void SystemStateModel::setColorStyle(const QColor &style)
{
    qDebug() << "SystemStateModel::setColorStyle() called with:" << style;   

    SystemStateData newData = m_currentStateData;
    newData.colorStyle = style;
    newData.osdColorStyle = ColorUtils::fromQColor(style);

    emit colorStyleChanged(style);   
    qDebug() << "SystemStateModel: colorStyleChanged signal emitted";   

    updateData(newData);
}

void SystemStateModel::setReticleStyle(const ReticleType &type)
{
    // 1) set m_stateModel field
    SystemStateData newData = m_currentStateData;
    newData.reticleType = type;
    updateData(newData);

    // 2) Emit a dedicated signal
    emit reticleStyleChanged(type);
}

void SystemStateModel::setDeadManSwitch(bool pressed) {
    if(m_currentStateData.deadManSwitchActive != pressed) {
        m_currentStateData.deadManSwitchActive = pressed;
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::setDownTrack(bool pressed) {
    if(m_currentStateData.downTrack != pressed) {
        m_currentStateData.downTrack = pressed;
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::setDownSw(bool pressed) { if(m_currentStateData.menuDown != pressed) { m_currentStateData.menuDown = pressed; emit dataChanged(m_currentStateData); } }

void SystemStateModel::setUpTrack(bool pressed) {
    if(m_currentStateData.upTrack != pressed) {
        m_currentStateData.upTrack = pressed;
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::setUpSw(bool pressed) { if(m_currentStateData.menuUp != pressed) { m_currentStateData.menuUp = pressed; emit dataChanged(m_currentStateData); } }

void SystemStateModel::setActiveCameraIsDay(bool isDay) {
    // ========================================================================
    // UC5: Camera Switch During Tracking
    // ========================================================================
    // When switching between day/night cameras, FOV changes (different optics)
    // Reticle pixel position must recalculate for new camera's FOV
    // ========================================================================
    if(m_currentStateData.activeCameraIsDay != isDay) {
        m_currentStateData.activeCameraIsDay = isDay;
        qDebug() << "✓ [FIX UC5] Active camera switched to" << (isDay ? "DAY" : "NIGHT")
                 << "- Recalculating reticle for new camera FOV";
        recalculateDerivedAimpointData();  // ← FIX: Trigger reticle recalc on camera switch
        emit dataChanged(m_currentStateData);
        // ✅ LATENCY FIX: Dedicated signal for MainMenuController to reduce event queue load
        emit activeCameraChanged(isDay);
    }
}

void SystemStateModel::setDetectionEnabled(bool enabled)
{
    //QMutexLocker locker(&m_mutex);

    // Safety check: Only allow enabling detection if day camera is active
    if (enabled && !m_currentStateData.activeCameraIsDay) {
        qWarning() << "SystemStateModel: Cannot enable detection - Night camera is active!";
        return;
    }

    if (m_currentStateData.detectionEnabled != enabled) {
        m_currentStateData.detectionEnabled = enabled;
        emit dataChanged(m_currentStateData);
        qInfo() << "SystemStateModel: Detection" << (enabled ? "ENABLED" : "DISABLED");
        // ✅ LATENCY FIX: Dedicated signal for MainMenuController to reduce event queue load
        emit detectionStateChanged(enabled);
    }
}

// --- Area Zone Methods Implementation ---
const std::vector<AreaZone>& SystemStateModel::getAreaZones() const {
    return m_currentStateData.areaZones;
}

AreaZone* SystemStateModel::getAreaZoneById(int id) {
    auto it = std::find_if(m_currentStateData.areaZones.begin(), m_currentStateData.areaZones.end(),
                           [id](const AreaZone& z){ return z.id == id; });
    return (it != m_currentStateData.areaZones.end()) ? &(*it) : nullptr;
}

bool SystemStateModel::addAreaZone(AreaZone zone) {
    zone.id = getNextAreaZoneId(); // Assign next ID
    m_currentStateData.areaZones.push_back(zone);
    qDebug() << "Added AreaZone with ID:" << zone.id;
    emit zonesChanged();
    return true;
}

bool SystemStateModel::modifyAreaZone(int id, const AreaZone& updatedZoneData) {
    AreaZone* zonePtr = getAreaZoneById(id);
    if (zonePtr) {
        *zonePtr = updatedZoneData; // Copy data
        zonePtr->id = id; // Ensure ID remains the same
        qDebug() << "Modified AreaZone with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "modifyAreaZone: ID not found:" << id;
        return false;
    }
}

bool SystemStateModel::deleteAreaZone(int id) {
    auto it = std::remove_if(m_currentStateData.areaZones.begin(), m_currentStateData.areaZones.end(),
                             [id](const AreaZone& z){ return z.id == id; });
    if (it != m_currentStateData.areaZones.end()) {
        m_currentStateData.areaZones.erase(it, m_currentStateData.areaZones.end());
        qDebug() << "Deleted AreaZone with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "deleteAreaZone: ID not found:" << id;
        return false;
    }
}

// --- Auto Sector Scan Zone Methods Implementation ---
const std::vector<AutoSectorScanZone>& SystemStateModel::getSectorScanZones() const {
    return m_currentStateData.sectorScanZones;
}

AutoSectorScanZone* SystemStateModel::getSectorScanZoneById(int id) {
    auto it = std::find_if(m_currentStateData.sectorScanZones.begin(), m_currentStateData.sectorScanZones.end(),
                           [id](const AutoSectorScanZone& z){ return z.id == id; });
    return (it != m_currentStateData.sectorScanZones.end()) ? &(*it) : nullptr;
}

bool SystemStateModel::addSectorScanZone(AutoSectorScanZone zone) {
    zone.id = getNextSectorScanId();
    m_currentStateData.sectorScanZones.push_back(zone);
    qDebug() << "Added SectorScanZone with ID:" << zone.id;
    emit zonesChanged();
    return true;
}

bool SystemStateModel::modifySectorScanZone(int id, const AutoSectorScanZone& updatedZoneData) {
    AutoSectorScanZone* zonePtr = getSectorScanZoneById(id);
    if (zonePtr) {
        *zonePtr = updatedZoneData;
        zonePtr->id = id;
        qDebug() << "Modified SectorScanZone with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "modifySectorScanZone: ID not found:" << id;
        return false;
    }
}

bool SystemStateModel::deleteSectorScanZone(int id) {
    auto it = std::remove_if(m_currentStateData.sectorScanZones.begin(), m_currentStateData.sectorScanZones.end(),
                             [id](const AutoSectorScanZone& z){ return z.id == id; });
    if (it != m_currentStateData.sectorScanZones.end()) {
        m_currentStateData.sectorScanZones.erase(it, m_currentStateData.sectorScanZones.end());
        qDebug() << "Deleted SectorScanZone with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "deleteSectorScanZone: ID not found:" << id;
        return false;
    }
}

// --- Target Reference Point Methods Implementation ---
const std::vector<TargetReferencePoint>& SystemStateModel::getTargetReferencePoints() const {
    return m_currentStateData.targetReferencePoints;
}

TargetReferencePoint* SystemStateModel::getTRPById(int id) {
    auto it = std::find_if(m_currentStateData.targetReferencePoints.begin(), m_currentStateData.targetReferencePoints.end(),
                           [id](const TargetReferencePoint& z){ return z.id == id; });
    return (it != m_currentStateData.targetReferencePoints.end()) ? &(*it) : nullptr;
}

bool SystemStateModel::addTRP(TargetReferencePoint trp) {
    trp.id = getNextTRPId();
    m_currentStateData.targetReferencePoints.push_back(trp);
    qDebug() << "Added TRP with ID:" << trp.id;
    emit zonesChanged();
    return true;
}

bool SystemStateModel::modifyTRP(int id, const TargetReferencePoint& updatedTRPData) {
    TargetReferencePoint* trpPtr = getTRPById(id);
    if (trpPtr) {
        *trpPtr = updatedTRPData;
        trpPtr->id = id;
        qDebug() << "Modified TRP with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "modifyTRP: ID not found:" << id;
        return false;
    }
}

bool SystemStateModel::deleteTRP(int id) {
    auto it = std::remove_if(m_currentStateData.targetReferencePoints.begin(), m_currentStateData.targetReferencePoints.end(),
                             [id](const TargetReferencePoint& z){ return z.id == id; });
    if (it != m_currentStateData.targetReferencePoints.end()) {
        m_currentStateData.targetReferencePoints.erase(it, m_currentStateData.targetReferencePoints.end());
        qDebug() << "Deleted TRP with ID:" << id;
        emit zonesChanged();
        return true;
    } else {
        qWarning() << "deleteTRP: ID not found:" << id;
        return false;
    }
}

// --- Save/Load Zones Implementation ---

bool SystemStateModel::saveZonesToFile(const QString& filePath) {
    QJsonObject rootObject;
    rootObject["zoneFileVersion"] = 1; // Versioning

    // Save next IDs
    rootObject["nextAreaZoneId"] = m_nextAreaZoneId;
    rootObject["nextSectorScanId"] = m_nextSectorScanId;
    rootObject["nextTRPId"] = m_nextTRPId;

    // Save Area Zones
    QJsonArray areaZonesArray;
    for (const auto& zone : m_currentStateData.areaZones) {
        QJsonObject zoneObj;
        zoneObj["id"] = zone.id;
        zoneObj["type"] = static_cast<int>(zone.type); // Assuming type is always AreaZone type
        zoneObj["isEnabled"] = zone.isEnabled;
        zoneObj["isFactorySet"] = zone.isFactorySet;
        zoneObj["isOverridable"] = zone.isOverridable;
        zoneObj["startAzimuth"] = zone.startAzimuth;
        zoneObj["endAzimuth"] = zone.endAzimuth;
        zoneObj["minElevation"] = zone.minElevation;
        zoneObj["maxElevation"] = zone.maxElevation;
        zoneObj["minRange"] = zone.minRange;
        zoneObj["maxRange"] = zone.maxRange;
        zoneObj["name"] = zone.name;
        areaZonesArray.append(zoneObj);
    }
    rootObject["areaZones"] = areaZonesArray;

    // Save Sector Scan Zones
    QJsonArray sectorScanZonesArray;
    for (const auto& zone : m_currentStateData.sectorScanZones) {
        QJsonObject zoneObj;
        zoneObj["id"] = zone.id;
        zoneObj["isEnabled"] = zone.isEnabled;
        zoneObj["az1"] = zone.az1;
        zoneObj["el1"] = zone.el1;
        zoneObj["az2"] = zone.az2;
        zoneObj["el2"] = zone.el2;
        zoneObj["scanSpeed"] = zone.scanSpeed;
        sectorScanZonesArray.append(zoneObj);
    }
    rootObject["sectorScanZones"] = sectorScanZonesArray;

    // Save Target Reference Points
    QJsonArray trpsArray;
    for (const auto& trp : m_currentStateData.targetReferencePoints) {
        QJsonObject trpObj;
        trpObj["id"] = trp.id;
        trpObj["locationPage"] = trp.locationPage;
        trpObj["trpInPage"] = trp.trpInPage;
        trpObj["azimuth"] = trp.azimuth;
        trpObj["elevation"] = trp.elevation;
        trpObj["haltTime"] = trp.haltTime;
        trpsArray.append(trpObj);
    }
    rootObject["targetReferencePoints"] = trpsArray;

    // Write to file
    QJsonDocument doc(rootObject);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing:" << filePath << file.errorString();
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    qDebug() << "Zones saved successfully to" << filePath;
    return true;
}

bool SystemStateModel::loadZonesFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for reading:" << filePath << file.errorString();
        return false; // File doesn't exist or cannot be opened
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse zones file:" << filePath << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Invalid format: Root is not a JSON object in" << filePath;
        return false;
    }

    QJsonObject rootObject = doc.object();

    // Optional: Check version
    int fileVersion = rootObject.value("zoneFileVersion").toInt(0);
    if (fileVersion > 1) {
        qWarning() << "Warning: Loading zones from a newer file version (" << fileVersion << "). Compatibility not guaranteed.";
        // Add specific handling for future versions if needed
    }

    // Clear existing zones before loading
    m_currentStateData.areaZones.clear();
    m_currentStateData.sectorScanZones.clear();
    m_currentStateData.targetReferencePoints.clear();

    // Load next IDs (use defaults if not present for backward compatibility)
    m_nextAreaZoneId = rootObject.value("nextAreaZoneId").toInt(1);
    m_nextSectorScanId = rootObject.value("nextSectorScanId").toInt(1);
    m_nextTRPId = rootObject.value("nextTRPId").toInt(1);

    // Load Area Zones
    if (rootObject.contains("areaZones") && rootObject["areaZones"].isArray()) {
        QJsonArray areaZonesArray = rootObject["areaZones"].toArray();
        for (const QJsonValue &value : areaZonesArray) {
            if (value.isObject()) {
                QJsonObject zoneObj = value.toObject();
                AreaZone zone;
                zone.id = zoneObj.value("id").toInt(-1);
                zone.type = static_cast<ZoneType>(zoneObj.value("type").toInt(static_cast<int>(ZoneType::Safety))); // Type is implicit
                zone.isEnabled = zoneObj.value("isEnabled").toBool(false);
                zone.isFactorySet = zoneObj.value("isFactorySet").toBool(false);
                zone.isOverridable = zoneObj.value("isOverridable").toBool(false);
                zone.startAzimuth = static_cast<float>(zoneObj.value("startAzimuth").toDouble(0.0));
                zone.endAzimuth = static_cast<float>(zoneObj.value("endAzimuth").toDouble(0.0));
                zone.minElevation = static_cast<float>(zoneObj.value("minElevation").toDouble(0.0));
                zone.maxElevation = static_cast<float>(zoneObj.value("maxElevation").toDouble(0.0));
                zone.minRange = static_cast<float>(zoneObj.value("minRange").toDouble(0.0));
                zone.maxRange = static_cast<float>(zoneObj.value("maxRange").toDouble(0.0));
                zone.name = zoneObj.value("name").toString("");

                if (zone.id != -1) { // Basic validation: require an ID
                    m_currentStateData.areaZones.push_back(zone);
                } else {
                    qWarning() << "Skipping invalid AreaZone entry during load (missing or invalid ID).";
                }
            }
        }
    }

    // Load Sector Scan Zones
    if (rootObject.contains("sectorScanZones") && rootObject["sectorScanZones"].isArray()) {
        QJsonArray sectorScanZonesArray = rootObject["sectorScanZones"].toArray();
        for (const QJsonValue &value : sectorScanZonesArray) {
            if (value.isObject()) {
                QJsonObject zoneObj = value.toObject();
                AutoSectorScanZone zone;
                zone.id = zoneObj.value("id").toInt(-1);
                zone.isEnabled = zoneObj.value("isEnabled").toBool(false);
                zone.az1 = static_cast<float>(zoneObj.value("az1").toDouble(0.0));
                zone.el1 = static_cast<float>(zoneObj.value("el1").toDouble(0.0));
                zone.az2 = static_cast<float>(zoneObj.value("az2").toDouble(0.0));
                zone.el2 = static_cast<float>(zoneObj.value("el2").toDouble(0.0));
                zone.scanSpeed = static_cast<float>(zoneObj.value("scanSpeed").toDouble(50.0));

                if (zone.id != -1) {
                    m_currentStateData.sectorScanZones.push_back(zone);
                } else {
                    qWarning() << "Skipping invalid SectorScanZone entry during load (missing or invalid ID).";
                }
            }
        }
    }

    // Load Target Reference Points
    if (rootObject.contains("targetReferencePoints") && rootObject["targetReferencePoints"].isArray()) {
        QJsonArray trpsArray = rootObject["targetReferencePoints"].toArray();
        for (const QJsonValue &value : trpsArray) {
            if (value.isObject()) {
                QJsonObject trpObj = value.toObject();
                TargetReferencePoint trp;
                trp.id = trpObj.value("id").toInt(-1);
                trp.locationPage = trpObj.value("locationPage").toInt(1);
                trp.trpInPage = trpObj.value("trpInPage").toInt(1);
                trp.azimuth = static_cast<float>(trpObj.value("azimuth").toDouble(0.0));
                trp.elevation = static_cast<float>(trpObj.value("elevation").toDouble(0.0));
                trp.haltTime = static_cast<float>(trpObj.value("haltTime").toDouble(0.0));

                if (trp.id != -1) {
                    m_currentStateData.targetReferencePoints.push_back(trp);
                } else {
                    qWarning() << "Skipping invalid TRP entry during load (missing or invalid ID).";
                }
            }
        }
    }

    // Ensure next IDs are correctly set after loading
    updateNextIdsAfterLoad();

    qDebug() << "Zones loaded successfully from" << filePath;
    // ✅ CRITICAL FIX: Emit dataChanged so all controllers know about loaded zones
    emit dataChanged(m_currentStateData);
    emit zonesChanged(); // Notify UI about the loaded zones
    return true;
}

// Helper to update ID counters after loading zones
void SystemStateModel::updateNextIdsAfterLoad() {
    int maxAreaId = 0;
    for(const auto& zone : m_currentStateData.areaZones) {
        maxAreaId = std::max(maxAreaId, zone.id);
    }
    // Ensure next ID is at least one greater than the max loaded ID, or the value read from file
    m_nextAreaZoneId = std::max(m_nextAreaZoneId, maxAreaId + 1);

    int maxSectorId = 0;
    for(const auto& zone : m_currentStateData.sectorScanZones) {
        maxSectorId = std::max(maxSectorId, zone.id);
    }
    m_nextSectorScanId = std::max(m_nextSectorScanId, maxSectorId + 1);

    int maxTRPId = 0;
    for(const auto& trp : m_currentStateData.targetReferencePoints) {
        maxTRPId = std::max(maxTRPId, trp.id);
    }
    m_nextTRPId = std::max(m_nextTRPId, maxTRPId + 1);

    qDebug() << "Next IDs updated after load: AreaZone=" << m_nextAreaZoneId
             << ", SectorScan=" << m_nextSectorScanId << ", TRP=" << m_nextTRPId;
}


void SystemStateModel::onServoAzDataChanged(const ServoDriverData &azData) {
    // ========================================================================
    // AZIMUTH HOME OFFSET CALIBRATION
    // ========================================================================
    // Store raw encoder position (before offset) for calibration procedure
    m_rawAzEncoderSteps = azData.position;

    // Apply home offset if calibration has been performed
    // offset = encoder position at true mechanical home
    // corrected position = raw position - offset
    double correctedPosition = azData.position;
    if (m_currentStateData.azHomeOffsetApplied) {
        correctedPosition = azData.position - m_currentStateData.azHomeOffsetSteps;
    }
    // ========================================================================

    double gearRatio = 174.0/34.0;
    double motorStepDeg = 0.009;
    double degPerSteimbpGal = motorStepDeg / gearRatio;
    double mechAz = correctedPosition * degPerSteimbpGal;    // corrected mechanical angle
    double displayAz = std::fmod(mechAz, 360.0);
    if (displayAz < 0) displayAz += 360.0;                   // keep in [0, 360)

    if (!qFuzzyCompare(m_currentStateData.gimbalAz, displayAz)) {

        m_currentStateData.mechanicalGimbalAz = mechAz;
        m_currentStateData.gimbalAz = displayAz;

        m_currentStateData.azMotorTemp = azData.motorTemp;
        m_currentStateData.azDriverTemp = azData.driverTemp;
        m_currentStateData.azServoConnected = azData.isConnected;
        m_currentStateData.azRpm = azData.rpm / (204.705882353);
        m_currentStateData.azTorque = azData.torque;
        m_currentStateData.azFault = azData.fault;

        emit dataChanged(m_currentStateData);
        emit gimbalPositionChanged(m_currentStateData.gimbalAz,
                                m_currentStateData.gimbalEl);
    }
}

void SystemStateModel::onServoElDataChanged(const ServoDriverData &elData) {

     if (!qFuzzyCompare(m_currentStateData.gimbalEl, elData.position * (-0.0018))) {
        m_currentStateData.gimbalEl = elData.position * (-0.0018);
        m_currentStateData.elMotorTemp = elData.motorTemp;
        m_currentStateData.elDriverTemp = elData.driverTemp;
        m_currentStateData.elServoConnected = elData.isConnected;
        m_currentStateData.elRpm = elData.rpm / (200.0);
        m_currentStateData.elTorque = elData.torque;
        m_currentStateData.elFault = elData.fault;

        emit dataChanged(m_currentStateData); // Emit general data change
        emit gimbalPositionChanged(m_currentStateData.gimbalAz, m_currentStateData.gimbalEl); // Emit specific gimbal change
    }
}

void SystemStateModel::onDayCameraDataChanged(const DayCameraData &dayData)
{
    // ========================================================================
    // UC1: Zoom During Zeroed Operation
    // UC4: Zoom During Active Tracking
    // ========================================================================
    // When day camera FOV changes (zoom in/out), reticle pixel position must
    // be recalculated to maintain correct angular offset on screen.
    // ========================================================================

    bool fovChanged = !qFuzzyCompare(
        static_cast<float>(m_currentStateData.dayCurrentHFOV),
        static_cast<float>(dayData.currentHFOV)
    );

    SystemStateData newData = m_currentStateData;

    newData.dayZoomPosition = dayData.zoomPosition;
    newData.dayCurrentHFOV = dayData.currentHFOV;
    newData.dayCurrentVFOV = dayData.currentVFOV;  // 16:9 aspect ratio (VFOV = HFOV × 9/16)
    newData.dayCameraConnected = dayData.isConnected;
    newData.dayCameraError = dayData.errorState;
    newData.dayCameraStatus = dayData.cameraStatus;
    newData.dayAutofocusEnabled = dayData.autofocusEnabled;
    newData.dayFocusPosition = dayData.focusPosition;

    // ⭐ CRITICAL FIX: Recalculate reticle position when FOV changes
    // This ensures Pixels-Per-Degree (PPD) scaling is correct for new zoom level
    if (fovChanged && m_currentStateData.activeCameraIsDay) {
        qDebug() << "✓ [FIX UC1/UC4] Day camera FOV changed from"
                 << m_currentStateData.dayCurrentHFOV << "to" << dayData.currentHFOV
                 << "- Recalculating reticle aimpoint";

        m_currentStateData = newData;  // Update state first so recalc uses new FOV
        recalculateDerivedAimpointData();  // ← FIX: Trigger reticle recalculation
        emit dataChanged(m_currentStateData);
    } else {
        updateData(newData);
    }
}

// Mode setting slots
void SystemStateModel::setMotionMode(MotionMode newMode) {
    if(m_currentStateData.motionMode != newMode) {
        m_currentStateData.previousMotionMode = m_currentStateData.motionMode;
        if (m_currentStateData.motionMode == MotionMode::AutoSectorScan || m_currentStateData.motionMode == MotionMode::TRPScan) {
        // If exiting a scan mode
            m_currentStateData.currentScanName = "";  // Clear it
        }
        m_currentStateData.motionMode = newMode;

        emit dataChanged(m_currentStateData);
         if (newMode == MotionMode::AutoSectorScan || newMode == MotionMode::TRPScan) {
            updateCurrentScanName(); // Ensure name is updated when entering these modes
        }
    }
}
void SystemStateModel::setOpMode(OperationalMode newOpMode) { if(m_currentStateData.opMode != newOpMode) { m_currentStateData.previousOpMode = m_currentStateData.opMode; m_currentStateData.opMode = newOpMode; emit dataChanged(m_currentStateData); } }
void SystemStateModel::setTrackingRestartRequested(bool restart) { if(m_currentStateData.requestTrackingRestart != restart) { m_currentStateData.requestTrackingRestart = restart; emit dataChanged(m_currentStateData); } }
void SystemStateModel::setTrackingStarted(bool start) { if(m_currentStateData.startTracking != start) { m_currentStateData.startTracking = start; emit dataChanged(m_currentStateData); } }

// TODO Implement other slots similarly, updating relevant parts of m_currentStateData and emitting dataChanged
void SystemStateModel::onGyroDataChanged(const ImuData &gyroData)
{
    SystemStateData newData = m_currentStateData;
    newData.imuConnected = gyroData.isConnected;
    newData.imuRollDeg = gyroData.rollDeg; // ImuData uses rollDeg, not imuRollDeg
    newData.imuPitchDeg = gyroData.pitchDeg; // ImuData uses pitchDeg, not imuPitchDeg
    newData.imuYawDeg = gyroData.yawDeg; // ImuData uses yawDeg, not imuYawDeg
    newData.imuTemp = gyroData.temperature;
    newData.AccelX = gyroData.accelX_g;
    newData.AccelY = gyroData.accelY_g;
    newData.AccelZ = gyroData.accelZ_g;
    newData.GyroX = gyroData.angRateX_dps;
    newData.GyroY = gyroData.angRateY_dps;
    newData.GyroZ = gyroData.angRateZ_dps;

    // Update stationary status
     updateStationaryStatus(newData);
 
    updateData(newData);
}

void SystemStateModel::updateStationaryStatus(SystemStateData& data)
{
    // 1. Calculate the magnitude of the gyroscope vector
    double gyroMagnitude = std::sqrt(data.GyroX * data.GyroX +
                                     data.GyroY * data.GyroY +
                                     data.GyroZ * data.GyroZ);

    // 2. Calculate the magnitude of the accelerometer vector
    double accelMagnitude = std::sqrt(data.AccelX * data.AccelX +
                                      data.AccelY * data.AccelY +
                                      data.AccelZ * data.AccelZ);

    // 3. Calculate the change in acceleration since the last update
    double accelDelta = std::abs(accelMagnitude - data.previousAccelMagnitude);
    data.previousAccelMagnitude = accelMagnitude; // Store for the next cycle

    // 4. Check if the motion is below our thresholds
    if (gyroMagnitude < STATIONARY_GYRO_LIMIT && accelDelta < STATIONARY_ACCEL_DELTA_LIMIT)
    {
        // Motion is low, check how long it's been this way
        if (data.stationaryStartTime.isNull()) {
            // If the timer wasn't running, start it now
            data.stationaryStartTime = QDateTime::currentDateTime();
        }

        // If we have been stationary for long enough, set the flag
        qint64 elapsedMs = data.stationaryStartTime.msecsTo(QDateTime::currentDateTime());
        if (elapsedMs > STATIONARY_TIME_MS) {
            data.isVehicleStationary = true;
        }
    }
    else
    {
        // Motion detected, we are not stationary
        data.isVehicleStationary = false;
        data.stationaryStartTime = QDateTime(); // Reset the timer
    }
}

void SystemStateModel::updateStabilizationDebug(const SystemStateData::StabilizationDebug& debugData)
{
    // Update stabDebug in system state for OSD display
    // This is called from GimbalMotionModeBase::sendStabilizedServoCommands()
    m_currentStateData.stabDebug = debugData;
    // Note: We don't emit dataChanged() here to avoid flooding signals
    // OSD will pick up changes on next regular update cycle
}


void SystemStateModel::onJoystickAxisChanged(int axis, float normalizedValue)
{
 //START_TS_TIMER("SystemStateModel");
    SystemStateData newData = m_currentStateData;

    const float CHANGE_THRESHOLD = 0.05f;
     static int count = 0;
    static auto lastLog = std::chrono::high_resolution_clock::now();
       
    float oldX = m_currentStateData.joystickAzValue;
    float oldY = m_currentStateData.joystickElValue;
    
    // Update axis value
    if (axis == 0) {
        newData.joystickAzValue = - normalizedValue;
    } else if (axis == 1){
        newData.joystickElValue = - normalizedValue;
    }
    
    // Check if change is significant
    float deltaX = std::abs(newData.joystickAzValue - oldX);
    float deltaY = std::abs(newData.joystickElValue - oldY);
    
    if (deltaX > CHANGE_THRESHOLD || deltaY > CHANGE_THRESHOLD) {
        // ✅ Emit SPECIFIC signal for immediate motor response
        updateData(newData);
                if (++count % 100 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLog).count();
            qDebug() << "🕹️ [Joystick] 100 emits in" << elapsed << "ms ="
                     << (100000.0 / elapsed) << "Hz";
            lastLog = now;
        }
    } 
}

void SystemStateModel::onJoystickButtonChanged(int button, bool pressed)
{
    SystemStateData newData = m_currentStateData;
    updateData(newData);
}

void SystemStateModel::onJoystickHatChanged(int hat, int direction)
{
    SystemStateData newData = m_currentStateData;

    if (hat == 0) {
        newData.joystickHatDirection = direction; // Assuming direction is an int representing the hat state
    }

    updateData(newData);
}

void SystemStateModel::onJoystickDataChanged(std::shared_ptr<const JoystickData> joyData)
{
    if (!joyData) return;

    SystemStateData newData = m_currentStateData;
    newData.joystickConnected = joyData->isConnected;

    updateData(newData);
}


void SystemStateModel::onLrfDataChanged(const LrfData &lrfData)
{
    SystemStateData newData = m_currentStateData;
    newData.lrfConnected = lrfData.isConnected;
    newData.lrfDistance = lrfData.lastDistance;
    newData.lrfTemp = lrfData.temperature;
    newData.lrfLaserCount = lrfData.laserCount;
    newData.lrfSystemStatus = lrfData.rawStatusByte;
    newData.lrfFault = lrfData.isFault;
    newData.lrfNoEcho = lrfData.noEcho;
    newData.lrfLaserNotOut = lrfData.laserNotOut;
    newData.lrfOverTemp = lrfData.isOverTemperature;
    newData.isOverTemperature = lrfData.isOverTemperature;

    // Update ballistics target range when LRF measurement is valid
    if (lrfData.isLastRangingValid && lrfData.lastDistance > 0) {
        newData.currentTargetRange = static_cast<float>(lrfData.lastDistance);
    }

    updateData(newData);
}

void SystemStateModel::onNightCameraDataChanged(const NightCameraData &nightData)
{
    // ========================================================================
    // UC1: Zoom During Zeroed Operation
    // UC4: Zoom During Active Tracking
    // UC5: Camera Switch During Tracking
    // ========================================================================
    // When night camera FOV changes (digital zoom), reticle pixel position
    // must be recalculated to maintain correct angular offset on screen.
    // ========================================================================

    bool fovChanged = !qFuzzyCompare(
        static_cast<float>(m_currentStateData.nightCurrentHFOV),
        static_cast<float>(nightData.currentHFOV)
    );

    SystemStateData newData = m_currentStateData;

    newData.nightZoomPosition = nightData.digitalZoomLevel;
    newData.nightCurrentHFOV = nightData.currentHFOV;
    newData.nightCurrentVFOV = nightData.currentVFOV;  // Update VFOV for non-square sensor
    newData.nightCameraConnected = nightData.isConnected;
    newData.nightCameraError = 0; // status never defined in datasheet (nightData.errorState != 0x00);  // Convert errorState byte to boolean
    newData.nightCameraStatus = nightData.cameraStatus;
    newData.nightDigitalZoomLevel = nightData.digitalZoomLevel;
    newData.nightFfcInProgress = nightData.ffcInProgress;
    newData.nightVideoMode = nightData.videoMode;
    newData.nightFpaTemperature = nightData.fpaTemperature;

    // ⭐ CRITICAL FIX: Recalculate reticle position when FOV changes
    // This ensures Pixels-Per-Degree (PPD) scaling is correct for new zoom level
    if (fovChanged && !m_currentStateData.activeCameraIsDay) {
        qDebug() << "✓ [FIX UC1/UC4/UC5] Night camera FOV changed from"
                 << m_currentStateData.nightCurrentHFOV << "to" << nightData.currentHFOV
                 << "- Recalculating reticle aimpoint";

        m_currentStateData = newData;  // Update state first so recalc uses new FOV
        recalculateDerivedAimpointData();  // ← FIX: Trigger reticle recalculation
        emit dataChanged(m_currentStateData);
    } else {
        updateData(newData);
    }
}

void SystemStateModel::onPlc21DataChanged(const Plc21PanelData &pData)
{
    SystemStateData newData = m_currentStateData;

    newData.menuUp = pData.menuUpSW;
    newData.menuDown = pData.menuDownSW;
    newData.menuVal = pData.menuValSw;

    newData.stationEnabled = pData.enableStationSW;
    newData.gunArmed = pData.armGunSW;
    newData.gotoHomePosition = pData.homePositionSW;  // ✓ Home button
    newData.ammoLoadButtonPressed = pData.loadAmmunitionSW;

    newData.authorized = pData.authorizeSw;
    newData.enableStabilization = pData.enableStabilizationSW;
    
    // ⭐ CRITICAL: authorizeSw FALSE = Emergency Stop!
    newData.emergencyStopActive = !pData.authorizeSw;

    switch (pData.fireMode) {
    case 0:
        newData.fireMode = FireMode::SingleShot;
        break;
    case 1:
        newData.fireMode = FireMode::ShortBurst;
        break;
    case 2:
        newData.fireMode = FireMode::LongBurst;
        break;
    default:
        newData.fireMode = FireMode::Unknown;
        break;
    }

    newData.gimbalSpeed = pData.speedSW;
    newData.plc21Connected = pData.isConnected;

    // Auto-disable detection when switching to night camera
    if (!pData.switchCameraSW && m_currentStateData.activeCameraIsDay) {
        bool wasDetectionEnabled = m_currentStateData.detectionEnabled;
        newData.detectionEnabled = false;
        qInfo() << "SystemStateModel: Night camera activated - Detection auto-disabled";
        // ✅ LATENCY FIX: Emit signal only if detection was actually enabled
        if (wasDetectionEnabled) {
            emit detectionStateChanged(false);
        }
    }

    // ✅ LATENCY FIX: Emit dedicated button signal ONLY when buttons actually change
    // PLC21 polls at 20Hz, but buttons change maybe 1-2 times per second
    // This prevents ApplicationController from processing 1,200 events/minute just for button monitoring
    if (m_currentStateData.menuUp != newData.menuUp ||
        m_currentStateData.menuDown != newData.menuDown ||
        m_currentStateData.menuVal != newData.menuVal) {
        emit buttonStateChanged(newData.menuUp, newData.menuDown, newData.menuVal);
    }

    updateData(newData);
    setActiveCameraIsDay(pData.switchCameraSW);
}

// ============================================================================
// UPDATE onPlc42DataChanged() METHOD
// ============================================================================

void SystemStateModel::onPlc42DataChanged(const Plc42Data &pData)
{
    SystemStateData newData = m_currentStateData;
    
    // Limit sensors
    newData.upperLimitSensorActive = pData.stationUpperSensor;
    newData.lowerLimitSensorActive = pData.stationLowerSensor;

    // Station inputs
    newData.stationAmmunitionLevel = pData.ammunitionLevel;
    newData.hatchState = pData.hatchState;
    newData.freeGimbalState = pData.freeGimbalState;  // ✓ FREE toggle
    
    // HOME-END signals (one per axis) ⭐ NEW
    newData.azimuthHomeComplete = pData.azimuthHomeComplete;
    newData.elevationHomeComplete = pData.elevationHomeComplete;

    // Control states
    newData.solenoidMode = pData.solenoidMode;
    newData.gimbalOpMode = pData.gimbalOpMode;
    newData.azimuthSpeed = pData.azimuthSpeed;
    newData.elevationSpeed = pData.elevationSpeed;
    newData.azimuthDirection = pData.azimuthDirection;
    newData.elevationDirection = pData.elevationDirection;
    newData.solenoidState = pData.solenoidState;
    newData.resetAlarm = pData.resetAlarm;
    newData.plc42Connected = pData.isConnected;

    // ========================================================================
    // FREE MODE AUTONOMOUS CONTROL (local toggle has priority)
    // ========================================================================
    // The physical FREE toggle switch at the station controls this
    // MDUINO 42+ monitors I0_3 and autonomously switches gimbalOpMode
    // We just track the state changes here
    
    // Rising edge: FREE toggle turned ON
    if (pData.freeGimbalState && !m_currentStateData.freeGimbalState) {
        qInfo() << "[SystemStateModel] FREE toggle activated - entering MotionFree";
        newData.previousMotionMode = m_currentStateData.motionMode;
        newData.motionMode = MotionMode::MotionFree;
    }
    // Falling edge: FREE toggle turned OFF
    else if (!pData.freeGimbalState && m_currentStateData.freeGimbalState) {
        qInfo() << "[SystemStateModel] FREE toggle deactivated - restoring previous mode";
        newData.motionMode = newData.previousMotionMode;
    }

    // ========================================================================
    // EMERGENCY STOP FROM GIMBAL OP MODE
    // ========================================================================
    // If PLC42 gimbalOpMode == 1, emergency stop is active via hardware
    if (pData.gimbalOpMode == 1 && !m_currentStateData.emergencyStopActive) {
        qCritical() << "[SystemStateModel] Emergency stop detected from PLC42";
        newData.emergencyStopActive = true;
    }

    updateData(newData);
}

void SystemStateModel::onServoActuatorDataChanged(const ServoActuatorData &actuatorData)
{
    SystemStateData newData = m_currentStateData;
    newData.actuatorPosition = actuatorData.position_mm;
    newData.actuatorConnected = actuatorData.isConnected;           
    newData.actuatorVelocity = actuatorData.velocity_mm_s;           
    newData.actuatorTemp = actuatorData.temperature_c;              
    newData.actuatorBusVoltage = actuatorData.busVoltage_v;        
    newData.actuatorTorque = actuatorData.torque_percent;            
    newData.actuatorMotorOff = actuatorData.status.isMotorOff;      
    newData.actuatorFault = actuatorData.status.isLatchingFaultActive;  

    updateData(newData);
}

void SystemStateModel::startZeroingProcedure() {
    if (!m_currentStateData.zeroingModeActive) {
        m_currentStateData.zeroingModeActive = true;

        // ========================================================================
        // BUG FIX #1: CAPTURE INITIAL GIMBAL POSITION
        // ========================================================================
        // Store current gimbal position as reference point
        // When finalized, offset = (final_position - initial_position)
        // This tracks how far the operator moved the gimbal to align with impact
        // ========================================================================
        m_zeroingState.initialAz = m_currentStateData.gimbalAz;
        m_zeroingState.initialEl = m_currentStateData.gimbalEl;
        m_zeroingState.capturedInitialPos = true;

        qInfo() << "[ZEROING] Procedure started";
        qInfo() << "[ZEROING]   Initial gimbal position captured:";
        qInfo() << "[ZEROING]     Azimuth:   " << m_zeroingState.initialAz << "°";
        qInfo() << "[ZEROING]     Elevation: " << m_zeroingState.initialEl << "°";
        qInfo() << "[ZEROING]   Existing offsets:";
        qInfo() << "[ZEROING]     Az offset: " << m_currentStateData.zeroingAzimuthOffset << "°";
        qInfo() << "[ZEROING]     El offset: " << m_currentStateData.zeroingElevationOffset << "°";
        qInfo() << "[ZEROING]   Operator can now move joystick to align reticle with impact point";

        emit dataChanged(m_currentStateData);
        emit zeroingStateChanged(true, m_currentStateData.zeroingAzimuthOffset, m_currentStateData.zeroingElevationOffset);
        // ✅ LATENCY FIX: Dedicated signal for ZeroingController to reduce event queue load
        emit zeroingModeChanged(true);
    }
}

void SystemStateModel::applyZeroingAdjustment(float deltaAz, float deltaEl) {
    if (m_currentStateData.zeroingModeActive) {
        // The PDF says "+/- 3 degree adjustment can be made". This usually means the
        // *total current offset* from the mechanical boreline is within +/-3 degrees,
        // or each individual adjustment step is small. Let's assume total offset.
        // We might need separate "raw mechanical zero" and "user zero offset".
        // For now, these are offsets from a nominal zero.

        // ========================================================================
        // SIGN CONVENTION FIX FOR ELEVATION
        // ========================================================================
        // The gimbal elevation has inverted sign convention:
        //   gimbalEl = servo_position * (-0.0018)
        // This means pointing UP = more negative gimbalEl.
        //
        // When user moves gimbal UP to align with an impact point that was ABOVE:
        //   deltaEl = final_el - initial_el = (more negative) - (less negative) = NEGATIVE
        //
        // But we want POSITIVE zeroingElevationOffset to mean "impact was UP"
        // so the reticle shifts UP to show where bullets hit.
        //
        // Solution: Negate deltaEl to compensate for inverted gimbal coordinates.
        // ========================================================================

        m_currentStateData.zeroingAzimuthOffset -= deltaAz; // NEGATED to fix inverted gimbal
        m_currentStateData.zeroingElevationOffset += deltaEl;  

        // Clamp total offsets if necessary (e.g., to +/- 3 degrees from some baseline)
        // float maxOffset = 3.0f;
        // m_currentStateData.zeroingAzimuthOffset = std::clamp(m_currentStateData.zeroingAzimuthOffset, -maxOffset, maxOffset);
        // m_currentStateData.zeroingElevationOffset = std::clamp(m_currentStateData.zeroingElevationOffset, -maxOffset, maxOffset);

        qDebug() << "Zeroing adjustment applied. New offsets Az:" << m_currentStateData.zeroingAzimuthOffset
                 << "El:" << m_currentStateData.zeroingElevationOffset;
        emit dataChanged(m_currentStateData); // For OSD to potentially show live offset values
        emit zeroingStateChanged(true, m_currentStateData.zeroingAzimuthOffset, m_currentStateData.zeroingElevationOffset);
    }
}

void SystemStateModel::finalizeZeroing() {
    if (m_currentStateData.zeroingModeActive && m_zeroingState.capturedInitialPos) {
        // ========================================================================
        // BUG FIX #1: CALCULATE ZEROING OFFSET FROM GIMBAL MOVEMENT
        // ========================================================================
        // Calculate how far the gimbal moved from initial position
        // This represents the angular offset between weapon bore and camera LOS
        // Positive offset means weapon hits right/high of where camera points
        // ========================================================================

        float currentAz = m_currentStateData.gimbalAz;
        float currentEl = m_currentStateData.gimbalEl;

        float deltaAz = currentAz - m_zeroingState.initialAz;
        float deltaEl = currentEl - m_zeroingState.initialEl;   // Elevation gear ratio is 1:1

        qInfo() << "[ZEROING] Finalizing procedure";
        qInfo() << "[ZEROING]   Initial position:";
        qInfo() << "[ZEROING]     Az: " << m_zeroingState.initialAz << "°  El: " << m_zeroingState.initialEl << "°";
        qInfo() << "[ZEROING]   Final position:";
        qInfo() << "[ZEROING]     Az: " << currentAz << "°  El: " << currentEl << "°";
        qInfo() << "[ZEROING]   Gimbal movement detected:";
        qInfo() << "[ZEROING]     ΔAz: " << deltaAz << "°  ΔEl: " << deltaEl << "° (raw)";
        qInfo() << "[ZEROING]     Note: El sign will be inverted to compensate for gimbal coordinates";

        // Apply the calculated offset (cumulative with existing offsets)
        if (std::abs(deltaAz) > 0.01f || std::abs(deltaEl) > 0.01f) {
            applyZeroingAdjustment(deltaAz, deltaEl);
            qInfo() << "[ZEROING]   ✓ New zeroing offsets applied (El negated for gimbal sign convention)";
        } else {
            qInfo() << "[ZEROING]   ⚠ No significant gimbal movement detected (< 0.01°)";
            qInfo() << "[ZEROING]   ⚠ Keeping existing offsets unchanged";
        }

        qInfo() << "[ZEROING]   Total cumulative offsets:";
        qInfo() << "[ZEROING]     Az: " << m_currentStateData.zeroingAzimuthOffset << "°";
        qInfo() << "[ZEROING]     El: " << m_currentStateData.zeroingElevationOffset << "°";

        // Mark zeroing as complete and active
        m_currentStateData.zeroingModeActive = false;
        m_currentStateData.zeroingAppliedToBallistics = true;
        m_zeroingState.capturedInitialPos = false;

        qInfo() << "[ZEROING] ✓ Procedure complete - Zeroing now ACTIVE";

        // ========================================================================
        // BUG FIX: RECALCULATE RETICLE POSITION WITH NEW ZEROING OFFSETS
        // ========================================================================
        // Without this call, the reticle position doesn't update until another
        // action (zoom, LAC toggle) triggers recalculation.
        // ========================================================================
        recalculateDerivedAimpointData();

        emit dataChanged(m_currentStateData);
        emit zeroingStateChanged(false, m_currentStateData.zeroingAzimuthOffset, m_currentStateData.zeroingElevationOffset);
        // ✅ LATENCY FIX: Dedicated signal for ZeroingController to reduce event queue load
        emit zeroingModeChanged(false);
    }
}

void SystemStateModel::clearZeroing() { // Called on power down, or manually
    m_currentStateData.zeroingModeActive = false;
    m_currentStateData.zeroingAzimuthOffset = 0.0f;
    m_currentStateData.zeroingElevationOffset = 0.0f;
    m_currentStateData.zeroingAppliedToBallistics = false;
    qDebug() << "Zeroing cleared.";
    emit dataChanged(m_currentStateData);
    emit zeroingStateChanged(false, 0.0f, 0.0f);
    // ✅ LATENCY FIX: Dedicated signal for ZeroingController to reduce event queue load
    emit zeroingModeChanged(false);
}

// ============================================================================
// LRF CLEAR (Button 10)
// ============================================================================
void SystemStateModel::clearLRF() {
    // ========================================================================
    // Simple LRF clear: Reset distance to 0 and recalculate reticle
    // Does NOT affect zeroing offsets (separate function for that)
    // ========================================================================

    qInfo() << "[LRF CLEAR] Button 10 pressed - Clearing LRF measurement";

    // Clear LRF measurement
    m_currentStateData.currentTargetRange = 0.0;
    qDebug() << "[LRF CLEAR] ✓ LRF distance reset to:" << m_currentStateData.lrfDistance;

    // Recalculate reticle position (may change due to ballistic calculations)
    recalculateDerivedAimpointData();
    qDebug() << "[LRF CLEAR] ✓ Reticle position recalculated";

    // Emit signal for UI updates
    emit dataChanged(m_currentStateData);

    qInfo() << "[LRF CLEAR] ✓ Clear operation complete";
}

void SystemStateModel::startWindageProcedure() {
    if (!m_currentStateData.windageModeActive) {
        m_currentStateData.windageModeActive = true;
        m_currentStateData.windageDirectionCaptured = false;
        // PDF: "Windage is always zero when CROWS is started."
        // Note: We don't clear existing values here - they persist from previous session
        qDebug() << "Windage procedure started.";
        emit dataChanged(m_currentStateData);
        emit windageStateChanged(true,
                                 m_currentStateData.windageSpeedKnots,
                                 m_currentStateData.windageDirectionDegrees);
        // ✅ LATENCY FIX: Dedicated signal for WindageController to reduce event queue load
        emit windageModeChanged(true);
    }
}

void SystemStateModel::captureWindageDirection(float currentAzimuthDegrees) {
    // Called when user presses MENU SEL in step 5 after aligning WS to wind
    if (m_currentStateData.windageModeActive && !m_currentStateData.windageDirectionCaptured) {
        m_currentStateData.windageDirectionDegrees = currentAzimuthDegrees;
        m_currentStateData.windageDirectionCaptured = true;
        qDebug() << "Windage direction captured:" << m_currentStateData.windageDirectionDegrees << "degrees";
        emit dataChanged(m_currentStateData);
        emit windageStateChanged(true,
                                 m_currentStateData.windageSpeedKnots,
                                 m_currentStateData.windageDirectionDegrees);
    }
}

void SystemStateModel::setWindageSpeed(float knots) {
    // Called during step 6 when user adjusts wind speed with U/D buttons
    if (m_currentStateData.windageModeActive && m_currentStateData.windageDirectionCaptured) {
        m_currentStateData.windageSpeedKnots = qMax(0.0f, knots); // Speed can't be negative
        qDebug() << "Windage speed set to:" << m_currentStateData.windageSpeedKnots << "knots";
        emit dataChanged(m_currentStateData);
        emit windageStateChanged(true,
                                 m_currentStateData.windageSpeedKnots,
                                 m_currentStateData.windageDirectionDegrees);
    }
}

void SystemStateModel::finalizeWindage() {
    // Called when user presses MENU SEL in step 6 to confirm wind speed
    if (m_currentStateData.windageModeActive && m_currentStateData.windageDirectionCaptured) {
        m_currentStateData.windageModeActive = false;
        m_currentStateData.windageAppliedToBallistics = (m_currentStateData.windageSpeedKnots > 0.001f); // Apply if speed > 0
        qDebug() << "Windage procedure finalized."
                 << "Direction:" << m_currentStateData.windageDirectionDegrees << "degrees"
                 << "Speed:" << m_currentStateData.windageSpeedKnots << "knots"
                 << "Applied:" << m_currentStateData.windageAppliedToBallistics;
        emit dataChanged(m_currentStateData);
        emit windageStateChanged(false,
                                 m_currentStateData.windageSpeedKnots,
                                 m_currentStateData.windageDirectionDegrees);
        // ✅ LATENCY FIX: Dedicated signal for WindageController to reduce event queue load
        emit windageModeChanged(false);
    }
}

void SystemStateModel::clearWindage() {
    // Called on startup or when windage needs to be reset
    // PDF: "Windage is always zero when CROWS is started."
    m_currentStateData.windageModeActive = false;
    m_currentStateData.windageSpeedKnots = 0.0f;
    m_currentStateData.windageDirectionDegrees = 0.0f;
    m_currentStateData.windageDirectionCaptured = false;
    m_currentStateData.windageAppliedToBallistics = false;
    qDebug() << "Windage cleared.";
    emit dataChanged(m_currentStateData);
    emit windageStateChanged(false, 0.0f, 0.0f);
    // ✅ LATENCY FIX: Dedicated signal for WindageController to reduce event queue load
    emit windageModeChanged(false);
}

// =================================
// ENVIRONMENTAL CONDITIONS
// =================================

void SystemStateModel::startEnvironmentalProcedure() {
    if (!m_currentStateData.environmentalModeActive) {
        m_currentStateData.environmentalModeActive = true;
        qDebug() << "Environmental procedure started.";
        emit dataChanged(m_currentStateData);
        // ✅ LATENCY FIX: Dedicated signal for EnvironmentalController to reduce event queue load
        emit environmentalModeChanged(true);
    }
}

void SystemStateModel::setEnvironmentalTemperature(float celsius) {
    if (m_currentStateData.environmentalModeActive) {
        m_currentStateData.environmentalTemperatureCelsius = celsius;
        qDebug() << "Environmental temperature set to:" << m_currentStateData.environmentalTemperatureCelsius << "°C";
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::setEnvironmentalAltitude(float meters) {
    if (m_currentStateData.environmentalModeActive) {
        m_currentStateData.environmentalAltitudeMeters = meters;
        qDebug() << "Environmental altitude set to:" << m_currentStateData.environmentalAltitudeMeters << "m";
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::finalizeEnvironmental() {
    if (m_currentStateData.environmentalModeActive) {
        m_currentStateData.environmentalModeActive = false;
        m_currentStateData.environmentalAppliedToBallistics = true;
        qDebug() << "Environmental procedure finalized."
                 << "Temp:" << m_currentStateData.environmentalTemperatureCelsius << "°C"
                 << "Altitude:" << m_currentStateData.environmentalAltitudeMeters << "m"
                 << "Applied:" << m_currentStateData.environmentalAppliedToBallistics
                 << "| NOTE: Crosswind calculated from windage, not environmental menu";
        emit dataChanged(m_currentStateData);
        // ✅ LATENCY FIX: Dedicated signal for EnvironmentalController to reduce event queue load
        emit environmentalModeChanged(false);
    }
}

void SystemStateModel::clearEnvironmental() {
    // Reset to ISA standard atmosphere
    // NOTE: Crosswind is NOT stored here - it's calculated from windage
    m_currentStateData.environmentalModeActive = false;
    m_currentStateData.environmentalTemperatureCelsius = 15.0f;   // ISA standard: 15°C at sea level
    m_currentStateData.environmentalAltitudeMeters = 0.0f;        // Sea level
    m_currentStateData.environmentalAppliedToBallistics = false;
    qDebug() << "Environmental settings cleared (ISA standard atmosphere)."
             << "| NOTE: Use windage menu to set wind conditions";
    emit dataChanged(m_currentStateData);
    // LATENCY FIX: Dedicated signal for EnvironmentalController to reduce event queue load
    emit environmentalModeChanged(false);
}

// ============================================================================
// AZIMUTH HOME OFFSET CALIBRATION
// ============================================================================
// Runtime compensation for ABZO encoder drift in Oriental Motor AZD-KD.
// Drift of ~40,000 steps (~70-80° at gimbal) can occur randomly.
// This system allows field recalibration without hardware modification.
//
// Calibration Procedure:
// 1. Operator commands "Go to Home" - gimbal moves to encoder's 0 reference
// 2. Operator manually slews gimbal to true mechanical home (visual reference)
// 3. Operator clicks "Set as True Home" - system captures encoder position as offset
// 4. System applies offset to all future position reads/writes
// 5. Offset is saved to persistent storage for session persistence
// ============================================================================

void SystemStateModel::startHomeCalibrationProcedure() {
    if (!m_currentStateData.homeCalibrationModeActive) {
        m_currentStateData.homeCalibrationModeActive = true;
        qInfo() << "[HomeCalibration] Procedure started";
        qInfo() << "[HomeCalibration] Current raw encoder position:" << m_rawAzEncoderSteps << "steps";
        qInfo() << "[HomeCalibration] Current offset:" << m_currentStateData.azHomeOffsetSteps << "steps";
        emit dataChanged(m_currentStateData);
        emit homeCalibrationModeChanged(true);
    }
}

double SystemStateModel::getCurrentEncoderPositionSteps() const {
    return m_rawAzEncoderSteps;
}

void SystemStateModel::captureAzHomeOffset() {
    if (m_currentStateData.homeCalibrationModeActive) {
        // The offset is the current raw encoder position
        // After applying this offset, the current position will read as 0° (true home)
        m_currentStateData.azHomeOffsetSteps = m_rawAzEncoderSteps;
        m_currentStateData.azHomeOffsetApplied = true;

        qInfo() << "[HomeCalibration] Home offset CAPTURED";
        qInfo() << "[HomeCalibration] Raw encoder position:" << m_rawAzEncoderSteps << "steps";
        qInfo() << "[HomeCalibration] New offset:" << m_currentStateData.azHomeOffsetSteps << "steps";

        // Recalculate position with new offset (will be done in onServoAzDataChanged)
        emit dataChanged(m_currentStateData);
    } else {
        qWarning() << "[HomeCalibration] Cannot capture offset - calibration mode not active";
    }
}

void SystemStateModel::finalizeHomeCalibration() {
    if (m_currentStateData.homeCalibrationModeActive) {
        m_currentStateData.homeCalibrationModeActive = false;

        // Save to persistent storage
        if (saveHomeCalibration()) {
            qInfo() << "[HomeCalibration] Procedure FINALIZED and SAVED";
            qInfo() << "[HomeCalibration] Offset:" << m_currentStateData.azHomeOffsetSteps << "steps";
            qInfo() << "[HomeCalibration] Applied:" << m_currentStateData.azHomeOffsetApplied;
        } else {
            qWarning() << "[HomeCalibration] Finalized but FAILED to save to persistent storage!";
        }

        emit dataChanged(m_currentStateData);
        emit homeCalibrationModeChanged(false);
    }
}

void SystemStateModel::clearAzHomeOffset() {
    m_currentStateData.azHomeOffsetSteps = 0.0;
    m_currentStateData.azHomeOffsetApplied = false;
    m_currentStateData.homeCalibrationModeActive = false;

    // Remove saved calibration file
    QString calibPath = QCoreApplication::applicationDirPath() + "/config/home_calibration.json";
    if (QFile::exists(calibPath)) {
        QFile::remove(calibPath);
        qInfo() << "[HomeCalibration] Calibration file removed:" << calibPath;
    }

    qInfo() << "[HomeCalibration] Home offset CLEARED - using encoder reference";
    emit dataChanged(m_currentStateData);
    emit homeCalibrationModeChanged(false);
}

bool SystemStateModel::loadHomeCalibration() {
    QString calibPath = QCoreApplication::applicationDirPath() + "/config/home_calibration.json";

    if (!QFile::exists(calibPath)) {
        qInfo() << "[HomeCalibration] No saved calibration file found - using encoder reference";
        return false;
    }

    QFile file(calibPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[HomeCalibration] Failed to open calibration file:" << calibPath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[HomeCalibration] Failed to parse calibration file:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "[HomeCalibration] Invalid calibration file format";
        return false;
    }

    QJsonObject root = doc.object();
    int version = root.value("version").toInt(0);
    if (version != 1) {
        qWarning() << "[HomeCalibration] Unknown calibration file version:" << version;
        return false;
    }

    m_currentStateData.azHomeOffsetSteps = root.value("azHomeOffsetSteps").toDouble(0.0);
    m_currentStateData.azHomeOffsetApplied = root.value("azHomeOffsetApplied").toBool(false);

    qInfo() << "[HomeCalibration] Loaded from file:" << calibPath;
    qInfo() << "[HomeCalibration] Offset:" << m_currentStateData.azHomeOffsetSteps << "steps";
    qInfo() << "[HomeCalibration] Applied:" << m_currentStateData.azHomeOffsetApplied;

    return true;
}

bool SystemStateModel::saveHomeCalibration() {
    QString calibPath = QCoreApplication::applicationDirPath() + "/config/home_calibration.json";

    QJsonObject root;
    root["version"] = 1;
    root["azHomeOffsetSteps"] = m_currentStateData.azHomeOffsetSteps;
    root["azHomeOffsetApplied"] = m_currentStateData.azHomeOffsetApplied;
    root["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["description"] = "Azimuth home offset calibration for ABZO encoder drift compensation";

    QJsonDocument doc(root);
    QFile file(calibPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[HomeCalibration] Failed to save calibration file:" << calibPath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "[HomeCalibration] Saved to file:" << calibPath;
    return true;
}

void SystemStateModel::setLeadAngleCompensationActive(bool active) {
    if (m_currentStateData.leadAngleCompensationActive != active) {
        m_currentStateData.leadAngleCompensationActive = active;
        if (!active) { // When turning off, reset status and offsets
            m_currentStateData.currentLeadAngleStatus = LeadAngleStatus::Off;
            m_currentStateData.leadAngleOffsetAz = 0.0f;
            m_currentStateData.leadAngleOffsetEl = 0.0f;
        } else { // When turning on, initial status is On (BallisticsProcessor will update if LAG/ZOOMOUT)
             m_currentStateData.currentLeadAngleStatus = LeadAngleStatus::On;
        }
        qDebug() << "Lead Angle Compensation active:" << active;
        if (!active) { // When turning OFF LAC
            m_currentStateData.currentLeadAngleStatus = LeadAngleStatus::Off;
            m_currentStateData.leadAngleOffsetAz = 0.0f; // Angular lead is zero
            m_currentStateData.leadAngleOffsetEl = 0.0f;
            // recalculateDerivedAimpointData() will now use these zero lead offsets
        } else { // When turning ON LAC
             // The actual lead offsets will be set by WeaponController via updateCalculatedLeadOffsets.
             // For now, status is On, but offsets might still be 0 until first calculation.
             m_currentStateData.currentLeadAngleStatus = LeadAngleStatus::On;
             // DO NOT set leadAngleOffsetAz/El to 0 here if turning on, let WeaponController populate.
        }

        recalculateDerivedAimpointData();
        updateData(m_currentStateData);
        /*emit leadAngleStateChanged(m_currentStateData.leadAngleCompensationActive,
                                   m_currentStateData.currentLeadAngleStatus,
                                   m_currentStateData.leadAngleOffsetAz,
                                   m_currentStateData.leadAngleOffsetEl);*/
    }
}

void SystemStateModel::recalculateDerivedAimpointData() {
    // ========================================================================
    // CROWS-COMPLIANT COORDINATE SYSTEM (TM 9-1090-225-10-2)
    // ========================================================================
    // 1. RETICLE: Gun boresight with ZEROING ONLY (shows where gun points)
    //    - NO ballistic drop, NO motion lead
    //    - Reticle stays at gun axis (with zeroing offset)
    //
    // 2. CCIP: Impact prediction with ZEROING + DROP + LEAD
    //    - Shows where bullet will actually land
    //    - Operator fires when TARGET overlaps CCIP
    //
    // This is the safer, more certifiable approach per user review:
    // "Reticle = center + zeroing only... slightly less magical but more robust"
    // ========================================================================

    SystemStateData& data = m_currentStateData;

    // Determine active camera's HFOV
    float activeHfov = data.activeCameraIsDay ? static_cast<float>(data.dayCurrentHFOV) : static_cast<float>(data.nightCurrentHFOV);

    // ========================================================================
    // CALCULATION 1: RETICLE Position (ZEROING ONLY - CROWS Doctrine)
    // ========================================================================
    // Reticle = gun boresight = center + zeroing correction
    // NO ballistic drop, NO motion lead
    //
    // Per CROWS doctrine: The reticle shows where the gun is pointing NOW.
    // Ballistic corrections only affect CCIP (impact prediction).
    // ========================================================================
    QPointF newReticlePosPx = ReticleAimpointCalculator::calculateReticleImagePositionPx(
        data.zeroingAzimuthOffset,        // Zeroing correction (camera-gun alignment)
        data.zeroingElevationOffset,      // Zeroing correction
        data.zeroingAppliedToBallistics,  // Apply zeroing?
        0.0f,                             // ← NO ballistic drop for reticle (CROWS)
        0.0f,                             // ← NO ballistic drop for reticle (CROWS)
        false,                            // ← No additional offsets
        LeadAngleStatus::Off,             // ← No lead for reticle
        activeHfov,
        data.currentImageWidthPx,
        data.currentImageHeightPx
    );

    // ========================================================================
    // CALCULATION 2: CCIP Position (ZEROING + DROP + LEAD - Full FCS)
    // ========================================================================
    // CCIP = predicted bullet impact point
    // - Zeroing: camera-gun alignment
    // - Ballistic drop: gravity + wind (auto when LRF valid)
    // - Motion lead: target velocity compensation (when LAC active)
    //
    // Per CROWS doctrine: Operator fires when TARGET overlaps CCIP,
    // not when reticle is on target.
    // ========================================================================
    float ccipTotalAz = data.ballisticDropOffsetAz + data.motionLeadOffsetAz;
    float ccipTotalEl = - data.ballisticDropOffsetEl + data.motionLeadOffsetEl;
    bool ccipActive = data.ballisticDropActive || data.leadAngleCompensationActive;

    QPointF newCcipPosPx = ReticleAimpointCalculator::calculateReticleImagePositionPx(
        data.zeroingAzimuthOffset,        // Zeroing correction
        data.zeroingElevationOffset,      // Zeroing correction
        data.zeroingAppliedToBallistics,  // Apply zeroing?
        ccipTotalAz,                      // ← Ballistic drop + motion lead combined
        ccipTotalEl,                      // ← Ballistic drop + motion lead combined
        ccipActive,                       // ← Active when drop OR motion lead active
        data.currentLeadAngleStatus,      // ← Status for motion lead component
        activeHfov,
        data.currentImageWidthPx,
        data.currentImageHeightPx
    );

    // Update reticle position
    bool reticlePosChanged = false;
    if (!qFuzzyCompare(data.reticleAimpointImageX_px, static_cast<float>(newReticlePosPx.x()))) {
        data.reticleAimpointImageX_px = static_cast<float>(newReticlePosPx.x());
        reticlePosChanged = true;
    }
    if (!qFuzzyCompare(data.reticleAimpointImageY_px, static_cast<float>(newReticlePosPx.y()))) {
        data.reticleAimpointImageY_px = static_cast<float>(newReticlePosPx.y());
        reticlePosChanged = true;
    }

    // Update CCIP position
    bool ccipPosChanged = false;
    if (!qFuzzyCompare(data.ccipImpactImageX_px, static_cast<float>(newCcipPosPx.x()))) {
        data.ccipImpactImageX_px = static_cast<float>(newCcipPosPx.x());
        ccipPosChanged = true;
    }
    if (!qFuzzyCompare(data.ccipImpactImageY_px, static_cast<float>(newCcipPosPx.y()))) {
        data.ccipImpactImageY_px = static_cast<float>(newCcipPosPx.y());
        ccipPosChanged = true;
    }

    // ========================================================================
    // CROWS/SARP ZOOM OUT DETECTION (TM 9-1090-225-10-2)
    // ========================================================================
    // "If the Lead Angle is greater than the selected camera FOV, ZOOM OUT
    //  appears over the on screen reticle. The message remains on screen until
    //  the hit point is no longer outside the viewing area."
    //
    // Check if CCIP is outside the current camera FOV boundaries
    // Also set proper status for ballistic-only mode (when LAC is not active)
    // ========================================================================
    bool ccipOutOfFov = false;
    if (data.leadAngleCompensationActive || data.ballisticDropActive) {
        ccipOutOfFov =
            data.ccipImpactImageX_px < 0 ||
            data.ccipImpactImageX_px > data.currentImageWidthPx ||
            data.ccipImpactImageY_px < 0 ||
            data.ccipImpactImageY_px > data.currentImageHeightPx;

        if (ccipOutOfFov) {
            data.currentLeadAngleStatus = LeadAngleStatus::ZoomOut;
            // ZOOM OUT: Return CCIP to screen center (red diamond at center)
            data.ccipImpactImageX_px = data.currentImageWidthPx / 2.0f;
            data.ccipImpactImageY_px = data.currentImageHeightPx / 2.0f;
            ccipPosChanged = true;

        } else if (!data.leadAngleCompensationActive && data.ballisticDropActive) {
            // Ballistic-only mode with CCIP in FOV - set status to On
            // This ensures consistent status for OSD display
            data.currentLeadAngleStatus = LeadAngleStatus::On;
        }
        // Note: When LAC is active, Lag status is set by WeaponController when lead is clamped
        // On status is the default when LAC active and not ZoomOut/Lag
    }

    // Update status texts
    QString oldLeadStatusText = data.leadStatusText;
    QString oldZeroingStatusText = data.zeroingStatusText;

    if (data.zeroingAppliedToBallistics) data.zeroingStatusText = "Z";
    else if (data.zeroingModeActive) data.zeroingStatusText = "ZEROING";
    else data.zeroingStatusText = "";

    // ========================================================================
    // CROWS/SARP STATUS TEXT LOGIC
    // ========================================================================
    // ZoomOut can occur from ballistic drop alone (extreme range) or LAC
    // Display "ZOOM OUT" in both cases since CCIP is outside FOV
    // LAC-specific status texts only shown when LAC is active
    // ========================================================================
    if (data.currentLeadAngleStatus == LeadAngleStatus::ZoomOut &&
        (data.leadAngleCompensationActive || data.ballisticDropActive)) {
        // ZOOM OUT from either LAC or ballistic drop
        data.leadStatusText = "ZOOM OUT";
    } else if (data.leadAngleCompensationActive) {
        switch(data.currentLeadAngleStatus) {
            case LeadAngleStatus::On: data.leadStatusText = "LEAD ANGLE ON"; break;
            case LeadAngleStatus::Lag: data.leadStatusText = "LEAD ANGLE LAG"; break;
            default: data.leadStatusText = "";
        }
    } else {
        data.leadStatusText = "";
    }

    bool statusTextChanged = (oldLeadStatusText != data.leadStatusText) || (oldZeroingStatusText != data.zeroingStatusText);

    if (reticlePosChanged || ccipPosChanged || statusTextChanged) {
        qDebug() << "SystemStateModel: Recalculated Aimpoints."
                 << "Reticle(zeroing only):" << data.reticleAimpointImageX_px << "," << data.reticleAimpointImageY_px
                 << "CCIP(zeroing+lead):" << data.ccipImpactImageX_px << "," << data.ccipImpactImageY_px
                 << "LeadTxt:" << data.leadStatusText << "ZeroTxt:" << data.zeroingStatusText;
        emit dataChanged(m_currentStateData); // Emit if anything derived changed
    }
}

// Call recalculateDerivedAimpointData() from any method that changes an input to it:
// - setLeadAngleCompensationActive()
// - updateCalculatedLeadOffsets() (from WeaponController)
// - finalizeZeroing(), clearZeroing() (or any method that changes zeroing angular offsets/status)
// - updateCameraOptics(width, height, hfov) // Critical: when FOV or image size changes
// - And when loading from file

void SystemStateModel::updateCameraOpticsAndActivity(int width, int height, float dayHfov, float nightHfov, bool isDayActive) {
    bool changed = false;
    bool cameraChanged = false;
    if (m_currentStateData.currentImageWidthPx != width)   { m_currentStateData.currentImageWidthPx = width; changed=true; }
    if (m_currentStateData.currentImageHeightPx != height) { m_currentStateData.currentImageHeightPx = height; changed=true; }
    if (!qFuzzyCompare(static_cast<float>(m_currentStateData.dayCurrentHFOV), dayHfov)) { m_currentStateData.dayCurrentHFOV = dayHfov; changed=true; }
    if (!qFuzzyCompare(static_cast<float>(m_currentStateData.nightCurrentHFOV), nightHfov)) { m_currentStateData.nightCurrentHFOV = nightHfov; changed=true; }
    if (m_currentStateData.activeCameraIsDay != isDayActive) {
        m_currentStateData.activeCameraIsDay = isDayActive;
        changed=true;
        cameraChanged=true;
    }

    if(changed){
        recalculateDerivedAimpointData();
        emit dataChanged(m_currentStateData);
        // ✅ LATENCY FIX: Emit dedicated signal only if camera actually changed
        if (cameraChanged) {
            emit activeCameraChanged(isDayActive);
        }
    }
}

void SystemStateModel::updateCalculatedLeadOffsets(float angularLeadAz, float angularLeadEl, LeadAngleStatus statusFromCalc) {
    // This method is called by the WeaponController/BallisticsProcessor with new calculations
    bool changed = false;

    // These are the raw calculated angular leads from BallisticsComputer
    if (!qFuzzyCompare(m_currentStateData.leadAngleOffsetAz, angularLeadAz)) {
        m_currentStateData.leadAngleOffsetAz = angularLeadAz;
        changed = true;
    }
    if (!qFuzzyCompare(m_currentStateData.leadAngleOffsetEl, angularLeadEl)) {
        m_currentStateData.leadAngleOffsetEl = angularLeadEl;
        changed = true;
    }
    if (m_currentStateData.currentLeadAngleStatus != statusFromCalc) {
        m_currentStateData.currentLeadAngleStatus = statusFromCalc;
        changed = true;
    }

    // If LAC is active, and any of the core lead parameters changed, then recalculate.
    // If LAC is NOT active, WeaponController should have passed 0s, which should also trigger a recalc if different from current.
    if (changed) {
        qDebug() << "SystemStateModel: Angular Lead Offsets received: Az" << angularLeadAz
                 << "El" << angularLeadEl << "Status:" << static_cast<int>(statusFromCalc)
                 << "LAC Active in model:" << m_currentStateData.leadAngleCompensationActive;

        // Recalculate will use m_currentStateData.leadAngleCompensationActive to decide if these
        // angularLeadOffsets are actually applied to the final reticle position.
        recalculateDerivedAimpointData(); // This will update derived pixel offsets and status texts
    }
    updateData(m_currentStateData);
}

void SystemStateModel::updateTargetAngularRates(float rateAzDegS, float rateElDegS) {
    // ========================================================================
    // BUG FIX #1: Target Angular Rates for LAC Motion Lead Calculation
    // ========================================================================
    // This method is called by GimbalController when tracking is active.
    // It provides the target's angular velocity (deg/s) to WeaponController
    // for calculating motion lead compensation.
    //
    // Data Flow:
    // 1. Tracker provides pixel velocity (px/s) from visual tracking
    // 2. GimbalController converts to angular velocity (deg/s) using FOV
    // 3. This method stores the angular rates in SystemStateData
    // 4. WeaponController reads rates and calculates motion lead = rate × TOF
    //
    // Critical: Without this update, currentTargetAngularRateAz/El remain 0,
    // and LAC motion lead will never be calculated!
    // ========================================================================

    bool changed = false;

    // Only update if values actually changed (avoid unnecessary dataChanged signals)
    if (!qFuzzyCompare(m_currentStateData.currentTargetAngularRateAz, rateAzDegS)) {
        m_currentStateData.currentTargetAngularRateAz = rateAzDegS;
        changed = true;
    }
    if (!qFuzzyCompare(m_currentStateData.currentTargetAngularRateEl, rateElDegS)) {
        m_currentStateData.currentTargetAngularRateEl = rateElDegS;
        changed = true;
    }

    if (changed) {
        /*qDebug() << "[SystemStateModel] Target angular rates updated:"
                 << "Az:" << rateAzDegS << "°/s"
                 << "El:" << rateElDegS << "°/s";*/
        emit dataChanged(m_currentStateData);
    }
}

// =============================================================================
// CROWS-COMPLIANT LAC LATCHING (TM 9-1090-225-10-2)
// =============================================================================

void SystemStateModel::armLAC(float azRate_dps, float elRate_dps) {
    // ========================================================================
    // CROWS/SARP LAC WORKFLOW - ARM STEP (TM 9-1090-225-10-2)
    // ========================================================================
    // When operator presses LAC button:
    // 1. Latch current angular rates (capture target motion)
    // 2. Display "LAC ARMED" on OSD
    // 3. DO NOT apply lead yet - wait for fire trigger
    //
    // Per TM 9-1090-225-10-2:
    // "The computer is constantly monitoring the change in rotation of the
    //  elevation and azimuth axes and measuring the speed."
    //
    // WARNING from manual:
    // "If target #2 is not properly acquired, the WS will fire outside the
    //  desired engagement area by continuing to apply the lead angle acquired
    //  from target #1"
    //
    // NOTE: leadAngleCompensationActive is set TRUE only when fire trigger
    // is pressed. This ensures joystick motion NEVER produces lead unless
    // both LAC is armed AND fire trigger is active.
    // ========================================================================

    SystemStateData& data = m_currentStateData;

    // ARM LAC - latch rates but do NOT apply lead yet
    data.lacArmed = true;
    data.lacLatchedAzRate_dps = azRate_dps;
    data.lacLatchedElRate_dps = elRate_dps;
    data.lacArmTimestampMs = QDateTime::currentMSecsSinceEpoch();

    // NOTE: Do NOT set leadAngleCompensationActive here!
    // Lead is only applied when fire trigger is pressed (engageLAC).
    // This separation allows gimbal movement while LAC is armed but not engaged.

    qInfo() << "";
    qInfo() << "========================================";
    qInfo() << "  LAC ARMED (Rates Latched)";
    qInfo() << "========================================";
    qInfo() << "  Latched rates: Az=" << azRate_dps << "°/s, El=" << elRate_dps << "°/s";
    qInfo() << "  Lead NOT applied - waiting for fire trigger";
    qInfo() << "";
    qWarning() << "[CROWS WARNING] LAC armed with latched rates.";
    qWarning() << "[CROWS WARNING] Lead will be applied when fire trigger pressed.";
    qWarning() << "[CROWS WARNING] Minimum 2 seconds between LAC toggles.";

    emit dataChanged(m_currentStateData);
}

bool SystemStateModel::disarmLAC() {
    // ========================================================================
    // Per TM 9-1090-225-10-2:
    // "Cancel Lead Angle Compensation by pressing the Palm Switch with the
    //  CG in the neutral position."
    //
    // "A minimum of 2 seconds must be waited before reuse of lead angle
    //  compensation feature."
    // ========================================================================

    SystemStateData& data = m_currentStateData;

    // Check 2-second minimum reset interval
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSinceArm = now - data.lacArmTimestampMs;

    if (data.lacArmed && timeSinceArm < SystemStateData::LAC_MIN_RESET_INTERVAL_MS) {
        qWarning() << "[CROWS] LAC disarm BLOCKED - only" << timeSinceArm
                   << "ms since arm (min:" << SystemStateData::LAC_MIN_RESET_INTERVAL_MS << "ms)";
        return false;
    }

    data.leadAngleCompensationActive = false;
    data.lacArmed = false;
    data.lacLatchedAzRate_dps = 0.0f;
    data.lacLatchedElRate_dps = 0.0f;
    // Note: Keep lacArmTimestampMs for next canRearmLAC() check

    // Clear motion lead offsets
    data.motionLeadOffsetAz = 0.0f;
    data.motionLeadOffsetEl = 0.0f;
    data.currentLeadAngleStatus = LeadAngleStatus::Off;

    qInfo() << "";
    qInfo() << "========================================";
    qInfo() << "  LAC DISARMED";
    qInfo() << "========================================";
    qInfo() << "";

    emit dataChanged(m_currentStateData);
    return true;
}

bool SystemStateModel::canRearmLAC() const {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSinceArm = now - m_currentStateData.lacArmTimestampMs;
    return timeSinceArm >= SystemStateData::LAC_MIN_RESET_INTERVAL_MS;
}

void SystemStateModel::engageLAC() {
    // ========================================================================
    // CROWS/SARP LAC WORKFLOW - ENGAGE STEP (Fire Trigger Pressed)
    // ========================================================================
    // When fire trigger is pressed and LAC is armed:
    // 1. Set leadAngleCompensationActive = true
    // 2. CCIP now includes motion lead (using latched rates)
    // 3. Display "LEAD ANGLE ON/LAG/ZOOM OUT"
    //
    // This is the moment when lead is actually applied to the firing solution.
    // Lead is calculated using the latched rates from armLAC().
    // ========================================================================

    if (!m_currentStateData.lacArmed) {
        qDebug() << "[LAC] engageLAC called but LAC is not armed - ignoring";
        return;
    }

    if (m_currentStateData.leadAngleCompensationActive) {
        // Already engaged - no change needed
        return;
    }

    m_currentStateData.leadAngleCompensationActive = true;
    m_currentStateData.currentLeadAngleStatus = LeadAngleStatus::On;

    qInfo() << "[LAC] ENGAGED - Lead compensation now active"
            << "| Latched rates: Az=" << m_currentStateData.lacLatchedAzRate_dps
            << "°/s, El=" << m_currentStateData.lacLatchedElRate_dps << "°/s";

    emit dataChanged(m_currentStateData);
}

void SystemStateModel::disengageLAC() {
    // ========================================================================
    // CROWS/SARP LAC WORKFLOW - DISENGAGE STEP (Fire Trigger Released)
    // ========================================================================
    // When fire trigger is released:
    // 1. Set leadAngleCompensationActive = false
    // 2. CCIP returns to ballistic-only (no motion lead)
    // 3. LAC remains armed (latched rates preserved)
    //
    // Lead is NOT applied outside of firing. Operator can re-engage
    // by pressing fire trigger again (with same latched rates).
    // ========================================================================

    if (!m_currentStateData.leadAngleCompensationActive) {
        // Already disengaged - no change needed
        return;
    }

    m_currentStateData.leadAngleCompensationActive = false;
    // Note: Do NOT clear motionLeadOffsets - let WeaponController handle that
    // Also keep lacArmed = true so operator can re-engage

    qInfo() << "[LAC] DISENGAGED - Lead compensation inactive, LAC remains armed";

    emit dataChanged(m_currentStateData);
}

// =============================================================================
// DEAD RECKONING (Firing during tracking)
// =============================================================================

void SystemStateModel::enterDeadReckoning(float azVel_dps, float elVel_dps) {
    // ========================================================================
    // Per TM 9-1090-225-10-2 page 38:
    // "When firing is initiated, CROWS aborts Target Tracking. Instead the
    //  system moves according to the speed and direction of the WS just prior
    //  to pulling the trigger. CROWS will not automatically compensate for
    //  changes in speed or direction of the tracked target during firing."
    // ========================================================================

    SystemStateData& data = m_currentStateData;

    data.deadReckoningActive = true;
    data.deadReckoningAzVel_dps = azVel_dps;
    data.deadReckoningElVel_dps = elVel_dps;

    // Transition tracking phase to Firing (dashed green box)
    if (data.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
        data.currentTrackingPhase == TrackingPhase::Tracking_Coast) {
        data.currentTrackingPhase = TrackingPhase::Tracking_Firing;
    }

    qInfo() << "[CROWS] DEAD RECKONING ACTIVE - Tracking aborted during fire";
    qInfo() << "[CROWS] Holding velocity: Az=" << azVel_dps << "°/s, El=" << elVel_dps << "°/s";

    emit dataChanged(m_currentStateData);
}

void SystemStateModel::exitDeadReckoning() {
    // Called when firing stops

    SystemStateData& data = m_currentStateData;

    if (!data.deadReckoningActive) {
        return; // Not in dead reckoning
    }

    data.deadReckoningActive = false;
    data.deadReckoningAzVel_dps = 0.0f;
    data.deadReckoningElVel_dps = 0.0f;

    // Return to Manual mode after firing (tracking was aborted)
    // Per CROWS: "Firing terminates Target Tracking"
    data.currentTrackingPhase = TrackingPhase::Off;
    data.motionMode = MotionMode::Manual;
    data.opMode = OperationalMode::Surveillance;

    qInfo() << "[CROWS] DEAD RECKONING ENDED - Returning to Manual mode";
    qInfo() << "[CROWS] Operator must re-acquire target to resume tracking";

    emit dataChanged(m_currentStateData);
}

// Helper for Azimuth checks considering wrap-around
bool isAzimuthInRange(float targetAz, float startAz, float endAz) {
    // Normalize all to 0-360
    targetAz = std::fmod(targetAz + 360.0f, 360.0f);
    startAz = std::fmod(startAz + 360.0f, 360.0f);
    endAz = std::fmod(endAz + 360.0f, 360.0f);

    if (startAz <= endAz) { // Normal case, e.g., 30 to 60
        return targetAz >= startAz && targetAz <= endAz;
    } else { // Wraps around 360, e.g., 350 to 10
        return targetAz >= startAz || targetAz <= endAz;
    }
}

bool SystemStateModel::isPointInNoFireZone(float targetAz, float targetEl, float targetRange) const {
    for (const auto& zone : m_currentStateData.areaZones) {
        if (zone.isEnabled && zone.type == ZoneType::NoFire) {
            bool azMatch = isAzimuthInRange(targetAz, zone.startAzimuth, zone.endAzimuth);
            bool elMatch = (targetEl >= zone.minElevation && targetEl <= zone.maxElevation);
            bool rangeMatch = true; // Assume range matches if not specified or zone has no range limits
            /*if (targetRange != -1.0f && (zone.minRange > 0 || zone.maxRange > 0)) {
                rangeMatch = (targetRange >= zone.minRange && (zone.maxRange == 0 || targetRange <= zone.maxRange));
            }*/

            if (azMatch && elMatch && rangeMatch) {
                // TODO: Consider 'isOverridable' if you have an override switch state
                return true;
            }
        }
    }
    return false;
}

void SystemStateModel::setPointInNoFireZone(bool inZone) {
    // This method is not strictly necessary, but can be used to set a flag
    // if you want to track whether the current point is in a No Fire Zone.
    // It could be used for UI updates or other logic.
    m_currentStateData.isReticleInNoFireZone = inZone;
    emit dataChanged(m_currentStateData);
}

bool SystemStateModel::isPointInNoTraverseZone(float targetAz, float currentEl) const {
    for (const auto& zone : m_currentStateData.areaZones) {
        if (zone.isEnabled && zone.type == ZoneType::NoTraverse) {
            // No Traverse Zones often apply across all elevations or a very wide range
            // For simplicity, let's assume they apply if currentEl is within zone's El range
            bool elInRange = (currentEl >= zone.minElevation && currentEl <= zone.maxElevation);
            if (elInRange && isAzimuthInRange(targetAz, zone.startAzimuth, zone.endAzimuth)) {
                // TODO: Consider 'isOverridable'
                return true;
            }
        }
    }
    return false;
}
void SystemStateModel::setPointInNoTraverseZone(bool inZone) {
    // ✅ CRITICAL FIX: Only emit if value actually changed
    // This prevents signal feedback loop that was causing event queue saturation
    if (m_currentStateData.isReticleInNoTraverseZone != inZone) {
        m_currentStateData.isReticleInNoTraverseZone = inZone;
        emit dataChanged(m_currentStateData);
    }
}

bool SystemStateModel::isAtNoTraverseZoneLimit(float currentAz, float currentEl, float intendedMoveAz) const
{
    // Normalize current az and compute new az after intended move (intendedMoveAz is delta in degrees)
    auto normalize360 = [](double a)->double {
        double v = std::fmod(a, 360.0);
        if (v < 0) v += 360.0;
        return v;
    };

    double cur = normalize360(currentAz);
    double next = normalize360(cur + intendedMoveAz);

    // If there's no movement, there's no crossing
    if (qFuzzyCompare(static_cast<float>(cur), static_cast<float>(next))) return false;

    // For each enabled NoTraverse zone, check if current is outside and next is inside (i.e., crossing into zone)
    for (const auto &zone : m_currentStateData.areaZones) {
        if (!zone.isEnabled) continue;
        if (zone.type != ZoneType::NoTraverse) continue;

        // elevation check: if current elevation not in zone's elevation band, skip
        if (currentEl < zone.minElevation || currentEl > zone.maxElevation) continue;

        // Use the existing az range helper (handles wrap-around)
        bool curInside = isAzimuthInRange(static_cast<float>(cur), zone.startAzimuth, zone.endAzimuth);
        bool nextInside = isAzimuthInRange(static_cast<float>(next), zone.startAzimuth, zone.endAzimuth);

        // We are at the limit if movement pushes us from outside->inside (penetration)
        if (!curInside && nextInside) {
            return true;
        }

        // Also treat the case where we are inside and the intended move would push us further inside
        // (rare), but main focus is preventing outside->inside.
        if (curInside && nextInside) {
            // Not strictly a crossing; not an entering movement — not considered a new limit hit.
            continue;
        }
    }

    return false;
}


// ----------------------------------------------------------------------------
// MATH HELPERS
// ----------------------------------------------------------------------------
double SystemStateModel::normalize360(double a) {
    a = std::fmod(a, 360.0);
    return (a < 0.0) ? a + 360.0 : a;
}

double SystemStateModel::shortestSignedDelta(double from, double to) {
    double d = normalize360(to) - normalize360(from);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
}

bool SystemStateModel::isInsideAz(double az, double start, double end) {
    double s = normalize360(start);
    double e = normalize360(end);
    double x = normalize360(az);
    return (s <= e) ? (x >= s && x <= e) : (x >= s || x <= e);
}

// ----------------------------------------------------------------------------
// RAY CASTER: Returns fraction [0.0 - 1.0] of delta allowed before impact
// ----------------------------------------------------------------------------
double SystemStateModel::getCollisionFraction(double current, double boundary, double delta, bool isAzimuth) {
    if (std::abs(delta) < 1e-6) return 2.0;

    double distToWall;
    if (isAzimuth) {
        distToWall = shortestSignedDelta(current, boundary);
    } else {
        distToWall = boundary - current;
    }

    double t = distToWall / delta;

    // Standard Ray Cast
    if (t < 0.0) return 2.0; // Moving away

    double effectiveDist = std::abs(distToWall) - NTZ_EPS; 
    if (effectiveDist < 0) effectiveDist = 0;

    return effectiveDist / std::abs(delta);
}

void SystemStateModel::resetNtzStates() {
    m_ntzStates.clear();
}

// ----------------------------------------------------------------------------
// MAIN PHYSICS LOOP
// ----------------------------------------------------------------------------
void SystemStateModel::computeAllowedDeltas(
    float currentAz, float currentEl,
    float intendedAzDelta, float intendedElDelta,
    float& allowedAzDelta, float& allowedElDelta,
    double dt)
{
    Q_UNUSED(dt);
    
    allowedAzDelta = intendedAzDelta;
    allowedElDelta = intendedElDelta;

    if (qFuzzyIsNull(intendedAzDelta) && qFuzzyIsNull(intendedElDelta)) return;

    double curAz = normalize360(currentAz);
    double curEl = currentEl;
    double nextAz = normalize360(curAz + intendedAzDelta);
    double nextEl = curEl + intendedElDelta;

    for (const auto& zone : m_currentStateData.areaZones) {
        if (!zone.isEnabled || zone.type != ZoneType::NoTraverse) continue;

        double zStart = normalize360(zone.startAzimuth);
        double zEnd   = normalize360(zone.endAzimuth);
        double zMinEl = zone.minElevation;
        double zMaxEl = zone.maxElevation;

        bool azInCur = isInsideAz(curAz, zStart, zEnd);
        bool elInCur = (curEl >= zMinEl && curEl <= zMaxEl);
        bool physicallyInside = azInCur && elInCur;

        if (!m_ntzStates.contains(zone.id)) {
            m_ntzStates[zone.id] = NtzState{zone.id, false, false, 0.0, false};
        }
        NtzState& state = m_ntzStates[zone.id];

        // ====================================================================
        // PRIORITY 1: IMMEDIATE LATCH
        // ====================================================================
        if (physicallyInside && !state.isInside) {
             state.isInside = true;
             state.isInitialized = true;
             
             double dStart = std::abs(shortestSignedDelta(curAz, zStart));
             double dEnd   = std::abs(shortestSignedDelta(curAz, zEnd));
             double dMin   = std::abs(curEl - zMinEl);
             double dMax   = std::abs(curEl - zMaxEl);
             
             double minAz = std::min(dStart, dEnd);
             double minEl = std::min(dMin, dMax);

             // LATCH THE EXACT WALL
             if (minAz < minEl) {
                 state.enteredViaAz = true;
                 state.entryBoundary = (dStart < dEnd) ? zStart : zEnd;
                 qWarning() << "[NTZ] Latch AZ:" << state.entryBoundary;
             } else {
                 state.enteredViaAz = false;
                 state.entryBoundary = (dMin < dMax) ? zMinEl : zMaxEl;
                 qWarning() << "[NTZ] Latch EL:" << state.entryBoundary;
             }
        }

        // ====================================================================
        // CASE A: PREDICTIVE COLLISION
        // ====================================================================
        if (!state.isInside) {
            bool azInNext = isInsideAz(nextAz, zStart, zEnd);
            bool elInNext = (nextEl >= zMinEl && nextEl <= zMaxEl);

            if (azInNext && elInNext) {
                double tStart = getCollisionFraction(curAz, zStart, intendedAzDelta, true);
                double tEnd   = getCollisionFraction(curAz, zEnd, intendedAzDelta, true);
                double tAz    = std::min(tStart, tEnd);

                double tMin   = getCollisionFraction(curEl, zMinEl, intendedElDelta, false);
                double tMax   = getCollisionFraction(curEl, zMaxEl, intendedElDelta, false);
                double tEl    = std::min(tMin, tMax);

                if (tAz < tEl) {
                    if (tAz < 1.0) allowedAzDelta = intendedAzDelta * tAz;
                } else {
                    if (tEl < 1.0) allowedElDelta = intendedElDelta * tEl;
                }
            }
        }
        
        // ====================================================================
        // CASE B: RECOVERY (STATIC WALL LOGIC)
        // ====================================================================
        else {
            double dStart = std::abs(shortestSignedDelta(curAz, zStart));
            double dEnd   = std::abs(shortestSignedDelta(curAz, zEnd));
            double dMin   = std::abs(curEl - zMinEl);
            double dMax   = std::abs(curEl - zMaxEl);
            double minDist = std::min({dStart, dEnd, dMin, dMax});

            if (!physicallyInside && minDist > NTZ_HYSTERESIS) {
                state.isInside = false;
                continue;
            }

            // --- FIXED DIRECTION CHECK ---
            if (state.enteredViaAz) {
                // Determine if we are at Start or End wall
                bool isStartWall = std::abs(shortestSignedDelta(state.entryBoundary, zStart)) < 0.1;
                
                if (isStartWall) {
                    // Start Wall (Left/CCW edge of zone). Zone is to the Right (+).
                    // BLOCK Positive (CW) motion.
                    // ALLOW Negative (CCW) motion.
                    if (intendedAzDelta > 0) allowedAzDelta = 0.0f;
                } else {
                    // End Wall (Right/CW edge of zone). Zone is to the Left (-).
                    // BLOCK Negative (CCW) motion.
                    // ALLOW Positive (CW) motion.
                    if (intendedAzDelta < 0) allowedAzDelta = 0.0f;
                }
                
                // ELEVATION IS FREE
            }
            else {
                bool isFloor = std::abs(state.entryBoundary - zMinEl) < 0.1;

                if (isFloor) {
                    // Floor (Bottom edge). Zone is Up (+).
                    // BLOCK Positive (Up) motion.
                    // ALLOW Negative (Down) motion.
                    if (intendedElDelta < 0) allowedElDelta = 0.0f;
                } else {
                    // Ceiling (Top edge). Zone is Down (-).
                    // BLOCK Negative (Down) motion.
                    // ALLOW Positive (Up) motion.
                    if (intendedElDelta > 0) allowedElDelta = 0.0f;
                }

                // SAFETY: Azimuth BLOCKED
                allowedAzDelta = 0.0f;
            }
        }
    }
}

void SystemStateModel::updateCurrentScanName() {
    SystemStateData& data = m_currentStateData; // Work on member
    QString newScanName = "";

    if (data.motionMode == MotionMode::AutoSectorScan) {
        auto it = std::find_if(data.sectorScanZones.begin(), data.sectorScanZones.end(),
                               [&](const AutoSectorScanZone& z){ return z.id == data.activeAutoSectorScanZoneId && z.isEnabled; });
        if (it != data.sectorScanZones.end()) {
            newScanName = QString("SCAN: SECTOR %1").arg(QString::number(it->id));
        } else {
            newScanName = "SCAN: SECTOR (none)";
        }
    } else if (data.motionMode == MotionMode::TRPScan) {
        newScanName = QString("SCAN: TRP PAGE %1").arg(data.activeTRPLocationPage);
    } else {
        newScanName = ""; // No scan active or selected for scan mode
    }

    if (data.currentScanName != newScanName) {
        data.currentScanName = newScanName;
        // dataChanged will be emitted by the calling function after all updates
    }
}


// --- Auto Sector Scan Selection ---
void SystemStateModel::selectNextAutoSectorScanZone() {
    SystemStateData& data = m_currentStateData;
    if (data.sectorScanZones.empty()) {
        data.activeAutoSectorScanZoneId = -1;
        updateCurrentScanName(); // Update display name
        emit dataChanged(data);
        return;
    }

    // Get a sorted list of enabled zone IDs
    std::vector<int> enabledZoneIds;
    for (const auto& zone : data.sectorScanZones) {
        if (zone.isEnabled) {
            enabledZoneIds.push_back(zone.id);
        }
    }
    if (enabledZoneIds.empty()) {
        data.activeAutoSectorScanZoneId = -1;
        updateCurrentScanName();
        emit dataChanged(data);
        return;
    }
    std::sort(enabledZoneIds.begin(), enabledZoneIds.end());

    auto it = std::find(enabledZoneIds.begin(), enabledZoneIds.end(), data.activeAutoSectorScanZoneId);

    if (it == enabledZoneIds.end() || std::next(it) == enabledZoneIds.end()) {
        // If current not found or is the last, wrap to the first
        data.activeAutoSectorScanZoneId = enabledZoneIds.front();
    } else {
        // Move to the next
        data.activeAutoSectorScanZoneId = *std::next(it);
    }
    qDebug() << "Selected next Auto Sector Scan Zone ID:" << data.activeAutoSectorScanZoneId;

    updateCurrentScanName();
    emit dataChanged(data);
}

void SystemStateModel::selectPreviousAutoSectorScanZone() {
    SystemStateData& data = m_currentStateData;
    if (data.sectorScanZones.empty()) {
        data.activeAutoSectorScanZoneId = -1;
        updateCurrentScanName();
        emit dataChanged(data);
        return;
    }

    std::vector<int> enabledZoneIds;
    for (const auto& zone : data.sectorScanZones) {
        if (zone.isEnabled) {
            enabledZoneIds.push_back(zone.id);
        }
    }
    if (enabledZoneIds.empty()) {
        data.activeAutoSectorScanZoneId = -1;
        updateCurrentScanName();
        emit dataChanged(data);
        return;
    }
    std::sort(enabledZoneIds.begin(), enabledZoneIds.end());

    auto it = std::find(enabledZoneIds.begin(), enabledZoneIds.end(), data.activeAutoSectorScanZoneId);

    if (it == enabledZoneIds.end() || it == enabledZoneIds.begin()) {
        // If current not found or is the first, wrap to the last
        data.activeAutoSectorScanZoneId = enabledZoneIds.back();
    } else {
        // Move to the previous
        data.activeAutoSectorScanZoneId = *std::prev(it);
    }
    qDebug() << "Selected previous Auto Sector Scan Zone ID:" << data.activeAutoSectorScanZoneId;
    updateCurrentScanName();
    emit dataChanged(data);
        updateData(data);
}


// --- TRP Location Page Selection ---
void SystemStateModel::selectNextTRPLocationPage() {
    SystemStateData& data = m_currentStateData;

    // 1. Find all unique page numbers that have at least one TRP defined.
    std::set<int> definedPagesSet;
    for (const auto& trp : data.targetReferencePoints) {
        definedPagesSet.insert(trp.locationPage);
    }

    if (definedPagesSet.empty()) {
        qDebug() << "selectNextTRPLocationPage: No TRP pages defined at all.";
        // data.activeTRPLocationPage might remain, or you could set to a default like 1
        updateCurrentScanName(); // Update OSD text if any
        emit dataChanged(data);
        return;
    }

    // 2. Convert to a sorted vector for easy cycling
    std::vector<int> sortedDefinedPages(definedPagesSet.begin(), definedPagesSet.end());
    // std::sort(sortedDefinedPages.begin(), sortedDefinedPages.end()); // Set already keeps them sorted

    // 3. Find the current active page in the list of defined pages
    auto it = std::find(sortedDefinedPages.begin(), sortedDefinedPages.end(), data.activeTRPLocationPage);

    if (it == sortedDefinedPages.end() || std::next(it) == sortedDefinedPages.end()) {
        // If current active page isn't found among defined pages (e.g., it was deleted or never existed with TRPs)
        // OR if it's the last defined page in the sorted list,
        // then wrap around to the FIRST defined page.
        data.activeTRPLocationPage = sortedDefinedPages.front();
    } else {
        // Move to the next defined page in the sorted list
        data.activeTRPLocationPage = *std::next(it);
    }

    qDebug() << "Selected next TRP Location Page:" << data.activeTRPLocationPage;
    updateCurrentScanName(); // Update m_currentStateData.currentScanName
    emit dataChanged(data);
}

void SystemStateModel::selectPreviousTRPLocationPage() {
    SystemStateData& data = m_currentStateData;

    std::set<int> definedPagesSet;
    for (const auto& trp : data.targetReferencePoints) {
        definedPagesSet.insert(trp.locationPage);
    }

    if (definedPagesSet.empty()) {
        qDebug() << "selectPreviousTRPLocationPage: No TRP pages defined at all.";
        updateCurrentScanName();
        emit dataChanged(data);
        return;
    }

    std::vector<int> sortedDefinedPages(definedPagesSet.begin(), definedPagesSet.end());

    auto it = std::find(sortedDefinedPages.begin(), sortedDefinedPages.end(), data.activeTRPLocationPage);

    if (it == sortedDefinedPages.end() || it == sortedDefinedPages.begin()) {
        // If current active page isn't found OR it's the first defined page,
        // wrap around to the LAST defined page.
        data.activeTRPLocationPage = sortedDefinedPages.back();
    } else {
        // Move to the previous defined page
        data.activeTRPLocationPage = *std::prev(it);
    }

    qDebug() << "Selected previous TRP Location Page:" << data.activeTRPLocationPage;
    updateCurrentScanName();
    emit dataChanged(data);
}

 
void SystemStateModel::processStateTransitions(const SystemStateData& oldData,
                                                SystemStateData& newData)
{
    // ========================================================================
    // PRIORITY 1: Emergency Stop Check (HIGHEST PRIORITY!)
    // ========================================================================
    if (newData.emergencyStopActive && !oldData.emergencyStopActive) {
        enterEmergencyStopMode();
        return;  // E-Stop overrides all other transitions
    }

    if (!newData.emergencyStopActive && oldData.emergencyStopActive) {
        enterIdleMode();
        return;
    }

    // Do not allow any other transitions if E-Stop is active
    if (newData.emergencyStopActive) {
        return;
    }

    // ========================================================================
    // PRIORITY 2: HOMING SEQUENCE MANAGEMENT
    // ========================================================================
    processHomingStateMachine(oldData, newData);

    // ========================================================================
    // PRIORITY 3: Station Power Check
    // ========================================================================
    if (!newData.stationEnabled && oldData.stationEnabled) {
        enterIdleMode();
        return;
    }

    // ⭐ BUG FIX: Handle startup case where station is already enabled
    // Original code only detected rising edge (false→true transition)
    // This fix ensures that if we're in Idle mode and station is enabled,
    // we transition to Surveillance mode even at startup
    if (newData.stationEnabled && !oldData.stationEnabled) {
        // Rising edge detected - normal transition
        if (newData.opMode == OperationalMode::Idle) {
            enterSurveillanceMode();
        }
    } else if (newData.stationEnabled && oldData.stationEnabled &&
               newData.opMode == OperationalMode::Idle) {
        // ⭐ STARTUP FIX: Station already enabled at startup
        // This happens when application starts with physical switch already ON
        qDebug() << "[BUG FIX] Station already enabled at startup - entering Surveillance mode";
        enterSurveillanceMode();
    }

    // Add any other automatic state transition rules here...
    // For example, if a fault is detected, etc.
}

void SystemStateModel::enterSurveillanceMode() {
    SystemStateData& data = m_currentStateData;
    if (!data.stationEnabled || data.opMode == OperationalMode::Surveillance) return;

    qDebug() << "[MODEL] Transitioning to Surveillance Mode.";
    data.opMode = OperationalMode::Surveillance;
    data.motionMode = MotionMode::Manual;
    // Any other setup for entering surveillance
    emit dataChanged(m_currentStateData);
}

void SystemStateModel::enterIdleMode() {
    SystemStateData& data = m_currentStateData;
    if (data.opMode == OperationalMode::Idle) return;

    qDebug() << "[MODEL] Transitioning to Idle Mode.";
    data.opMode = OperationalMode::Idle;
    data.motionMode = MotionMode::Idle;
    // Stop tracking if it was active
    if (data.currentTrackingPhase != TrackingPhase::Off) {
        stopTracking(); // Use your existing stopTracking method
    }
    // Note: stopTracking will emit dataChanged, so we might not need another emit here.
    // It's safer to ensure one is called.
    emit dataChanged(m_currentStateData);
}

void SystemStateModel::commandEngagement(bool start) {
    SystemStateData& data = m_currentStateData;
    if (start) {
        if (data.opMode == OperationalMode::Engagement || !data.gunArmed) {
            // Cannot enter engagement if already in it or not armed
            return;
        }
        qDebug() << "[MODEL] Entering Engagement Mode.";
        // Store previous state for reversion
        data.previousOpMode = data.opMode;
        data.previousMotionMode = data.motionMode;
        data.opMode = OperationalMode::Engagement;
        // The weapon controller will now act based on this mode
    } else { // stop engagement
        if (data.opMode != OperationalMode::Engagement) return;
        qDebug() << "[MODEL] Exiting Engagement Mode, reverting to previous state.";
        // Revert to the state before engagement
        data.opMode = data.previousOpMode;
        data.motionMode = data.previousMotionMode;
    }
    emit dataChanged(m_currentStateData);
}

void SystemStateModel::processHomingStateMachine(const SystemStateData& oldData,
                                                  SystemStateData& newData)
{
    // ========================================================================
    // HOMING BUTTON PRESSED (rising edge)
    // ========================================================================
    if (newData.gotoHomePosition && !oldData.gotoHomePosition) {
        // Home button just pressed
        if (newData.homingState == HomingState::Idle) {
            qInfo() << "[SystemStateModel] Home button pressed - initiating homing";
            newData.homingState = HomingState::Requested;
            newData.motionMode = MotionMode::Idle;  // Suspend motion during homing
            // GimbalController will send HOME command in next cycle
        }
    }

    // ========================================================================
    // HOMING REQUESTED → TRANSITION TO IN PROGRESS
    // ========================================================================
    // ⭐ BUG FIX: Auto-transition from Requested to InProgress
    // GimbalController sets its own m_currentHomingState but doesn't propagate
    // back to SystemStateModel. We auto-transition after one cycle to sync states.
    if (newData.homingState == HomingState::Requested &&
        oldData.homingState == HomingState::Requested) {
        // We've been in Requested for at least one cycle - GimbalController should
        // have sent the HOME command by now. Transition to InProgress.
        qInfo() << "[SystemStateModel] Auto-transitioning from Requested → InProgress";
        newData.homingState = HomingState::InProgress;
    }

    // ========================================================================
    // HOMING IN PROGRESS → CHECK FOR COMPLETION (REQUIRE BOTH AXES)
    // ========================================================================
    if (newData.homingState == HomingState::InProgress) {
        // Check if BOTH HOME-END signals received
        // We need both azimuth AND elevation to complete homing
        bool azHomeDone = newData.azimuthHomeComplete;
        bool elHomeDone = newData.elevationHomeComplete;

        // Log individual axis completion (rising edge detection for logging only)
        if (azHomeDone && !oldData.azimuthHomeComplete) {
            qInfo() << "[SystemStateModel] ✓ Azimuth HOME-END received";
        }
        if (elHomeDone && !oldData.elevationHomeComplete) {
            qInfo() << "[SystemStateModel] ✓ Elevation HOME-END received";
        }

        // ⭐ BUG FIX: Complete homing when BOTH axes report HOME-END
        // Original code: Required rising edge detection, preventing completion if flags were already set
        // New code: Complete homing whenever both flags are true, regardless of previous state
        if (azHomeDone && elHomeDone) {
            // Only log and transition if we weren't already Completed
            if (newData.homingState == HomingState::InProgress) {
                qInfo() << "[SystemStateModel] ✓✓ BOTH axes homed - homing complete";
                newData.homingState = HomingState::Completed;
                // GimbalController will restore motion mode
            }
        }
    }

    // ========================================================================
    // HOMING COMPLETED → AUTO-CLEAR FLAGS
    // ========================================================================
    if (newData.homingState == HomingState::Completed &&
        oldData.homingState == HomingState::InProgress) {
        // Homing just completed - clear flags after one cycle
        qDebug() << "[SystemStateModel] Clearing homing flags";
        newData.gotoHomePosition = false;
        newData.azimuthHomeComplete = false;
        newData.elevationHomeComplete = false;
        // Will transition to Idle on next cycle
    }

    // ========================================================================
    // AUTO-TRANSITION COMPLETED/FAILED/ABORTED → IDLE
    // ========================================================================
    if ((newData.homingState == HomingState::Completed ||
         newData.homingState == HomingState::Failed ||
         newData.homingState == HomingState::Aborted) &&
        !newData.gotoHomePosition) {
        // Flags cleared, transition back to idle
        newData.homingState = HomingState::Idle;
    }
}

// ============================================================================
// UPDATE enterEmergencyStopMode() - ADD HOMING ABORT
// ============================================================================

void SystemStateModel::enterEmergencyStopMode() {
    SystemStateData& data = m_currentStateData;
    if (data.opMode == OperationalMode::EmergencyStop) return;

    qCritical() << "[MODEL] ENTERING EMERGENCY STOP MODE!";

    // Abort homing if in progress
    if (data.homingState == HomingState::InProgress ||
        data.homingState == HomingState::Requested) {
        qWarning() << "[MODEL] Aborting in-progress homing sequence";
        data.homingState = HomingState::Aborted;
        data.gotoHomePosition = false;
    }

    data.opMode = OperationalMode::EmergencyStop;
    data.motionMode = MotionMode::Idle;
    data.trackingActive = false;
    data.currentTrackingPhase = TrackingPhase::Off;
    data.trackerHasValidTarget = false;
    data.leadAngleCompensationActive = false;

    emit dataChanged(m_currentStateData);
}

void SystemStateModel::updateTrackingResult(
    int cameraIndex,
    bool hasLock, // This parameter might become less relevant as we use VPITrackingState directly
    float centerX_px, float centerY_px,
    float width_px, float height_px,
    float velocityX_px_s, float velocityY_px_s,
    VPITrackingState trackerState,
    float confidence)
{
    //QMutexLocker locker(&m_mutex); // Protect shared state

    // 1. Determine if this camera is the active one for tracking
    int activeCameraIndex = m_currentStateData.activeCameraIsDay ? 0 : 1;
    if (cameraIndex != activeCameraIndex) {
        // qDebug() << "[MODEL-REJECT] IGNORING update from INACTIVE Cam" << cameraIndex;
        return; // Ignore tracking updates from inactive cameras
    }

    SystemStateData& data = m_currentStateData;
    bool stateDataChanged = false;

    // --- 1. Update the raw tracked target data fields ---
    // The 'hasLock' parameter from CameraVideoStreamDevice is derived from its internal logic.
    // We will primarily rely on 'trackerState' for the model's state machine.
    bool newTrackerHasValidTarget = (trackerState == VPI_TRACKING_STATE_TRACKED);

    if (data.trackerHasValidTarget != newTrackerHasValidTarget) { data.trackerHasValidTarget = newTrackerHasValidTarget; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetCenterX_px, centerX_px)) { data.trackedTargetCenterX_px = centerX_px; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetCenterY_px, centerY_px)) { data.trackedTargetCenterY_px = centerY_px; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetWidth_px, width_px)) { data.trackedTargetWidth_px = width_px; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetHeight_px, height_px)) { data.trackedTargetHeight_px = height_px; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetVelocityX_px_s, velocityX_px_s)) { data.trackedTargetVelocityX_px_s = velocityX_px_s; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackedTargetVelocityY_px_s, velocityY_px_s)) { data.trackedTargetVelocityY_px_s = velocityY_px_s; stateDataChanged = true; }
    if (data.trackedTargetState != trackerState) { data.trackedTargetState = trackerState; stateDataChanged = true; }
    if (!qFuzzyCompare(data.trackingConfidence, confidence)) { data.trackingConfidence = confidence; stateDataChanged = true; }

    // --- 2. REFINED High-Level TrackingPhase state machine ---
    TrackingPhase oldPhase = data.currentTrackingPhase;

    switch (data.currentTrackingPhase) {
        case TrackingPhase::Off:
            // If we are in Off state, and suddenly receive a NEW or TRACKED state from the active camera,
            // it implies a command was issued (e.g., TRACK button pressed, leading to Acquisition then LockPending).
            // The transition from Off to Acquisition is typically triggered by a UI event (e.g., TRACK button press),
            // not directly by the CameraVideoStreamDevice reporting a state.
            // This block should primarily handle resetting if we somehow get tracking data while Off.
            if (trackerState != VPI_TRACKING_STATE_LOST) {
                qWarning() << "[MODEL] Received tracking data while in Off phase. Resetting model tracking state.";
                data.trackerHasValidTarget = false;
                data.trackedTargetState = VPI_TRACKING_STATE_LOST;
                data.motionMode = MotionMode::Manual; // Ensure gimbal is manual
            }
            break;

        case TrackingPhase::Acquisition:
            // In Acquisition phase, the OSD displays the box. The VPI tracker is NOT yet initialized.
            // The CameraVideoStreamDevice should NOT be reporting NEW or TRACKED states here.
            // If it does, it's an anomaly or a timing issue.
            // The transition from Acquisition to LockPending is triggered by a UI event (TRACK button press).
            // This model should primarily update the OSD box based on user input (if any) during this phase.
            // No direct VPI tracker state handling here for phase transition.
            if (trackerState != VPI_TRACKING_STATE_LOST) {
                qWarning() << "[MODEL] Received tracking data (" << static_cast<int>(trackerState) << ") while in Acquisition phase. Ignoring for phase transition.";
            }
            break;

        case TrackingPhase::Tracking_LockPending:
         qDebug() << "Ttracker State " << static_cast<int>(trackerState) << " in LockPending phase.";
            // This is the critical phase where we wait for the tracker to lock.
            if (trackerState == VPI_TRACKING_STATE_TRACKED) {
                // Success! Tracker has locked onto the target.
                data.currentTrackingPhase = TrackingPhase::Tracking_ActiveLock;
                data.opMode = OperationalMode::Tracking;
                data.motionMode = MotionMode::AutoTrack; // Activate gimbal tracking
                qInfo() << "[MODEL] Valid Lock Acquired! Phase -> ActiveLock (" << static_cast<int>(data.currentTrackingPhase) << ")";
            } else if (trackerState == VPI_TRACKING_STATE_LOST) {
                // Tracker failed to lock or lost target immediately after initialization.
                // This can happen if the initial box was bad or target moved too fast.
                data.currentTrackingPhase = TrackingPhase::Off; // Go back to Off
                data.opMode = OperationalMode::Idle;
                data.motionMode = MotionMode::Manual; // Deactivate gimbal tracking
                data.trackerHasValidTarget = false; // Ensure model reflects no valid target
                qWarning() << "[MODEL] Tracker failed to acquire lock (LOST). Returning to Off (" << static_cast<int>(data.currentTrackingPhase) << ").";
            } else if (trackerState == VPI_TRACKING_STATE_NEW) {
                // Tracker is initialized and attempting to lock. This is expected.
                // Stay in LockPending and wait for TRACKED or LOST.
                qDebug() << "[MODEL] In LockPending, tracker initialized (NEW). Waiting for lock.";
            } else {
                // Any other unexpected state during LockPending, log and stay in LockPending.
                qWarning() << "[MODEL] In LockPending, received unexpected VPI state: " << static_cast<int>(trackerState) << ". Staying in LockPending.";
            }
            break;

        case TrackingPhase::Tracking_ActiveLock:
            // We are actively tracking. Monitor the tracker's state.
            if (trackerState == VPI_TRACKING_STATE_LOST) {
                // Target lost during active tracking.
                data.currentTrackingPhase = TrackingPhase::Tracking_Coast; // Transition to Coast
                data.opMode = OperationalMode::Tracking; // Still in tracking op mode
                data.motionMode = MotionMode::Manual; // Gimbal goes to manual in Coast
                data.trackerHasValidTarget = false; // Ensure model reflects no valid target
                qWarning() << "[MODEL] Target lost during active tracking. Transitioning to Coast (" << static_cast<int>(data.currentTrackingPhase) << ").";
            } else if (trackerState == VPI_TRACKING_STATE_TRACKED) {
                // All good, continue tracking.
               // qDebug() << "[MODEL] ActiveLock: Target still tracked.";
            } else {
                // Unexpected state during ActiveLock. Log and potentially reset.
                //qWarning() << "[MODEL] In ActiveLock, received unexpected VPI state: " << static_cast<int>(trackerState) << ". Staying in ActiveLock but might indicate issue.";
            }
            break;

        case TrackingPhase::Tracking_Coast:
            // In Coast phase, we are trying to re-acquire or waiting for user input.
            if (trackerState == VPI_TRACKING_STATE_TRACKED) {
                // Target re-acquired!
                data.currentTrackingPhase = TrackingPhase::Tracking_ActiveLock;
                data.opMode = OperationalMode::Tracking;
                data.motionMode = MotionMode::AutoTrack;
                qInfo() << "[MODEL] Target Re-acquired! Phase -> ActiveLock (" << static_cast<int>(data.currentTrackingPhase) << ")";
            } else if (trackerState == VPI_TRACKING_STATE_LOST) {
                // Still lost, remain in Coast.
                qDebug() << "[MODEL] In Coast: Target still lost.";
            } else if (trackerState == VPI_TRACKING_STATE_NEW) {
                // If we get NEW in Coast, it means a re-initialization happened. Stay in Coast and wait.
                qDebug() << "[MODEL] In Coast: Tracker re-initialized (NEW). Waiting for re-acquisition.";
            }
            break;

        case TrackingPhase::Tracking_Firing:
            // In Firing phase, the system holds position. Tracking updates might still come in,
            // but the phase should not change based on them. The phase changes based on weapon state.
            qDebug() << "[MODEL] In Firing phase. Ignoring tracking state for phase transition.";
            break;

        default:
            qWarning() << "[MODEL] Unknown TrackingPhase: " << static_cast<int>(data.currentTrackingPhase);
            break;
    }

    if (oldPhase != data.currentTrackingPhase) {
        stateDataChanged = true;
    }

    // Only emit dataChanged if something actually changed (raw data or phase)
    if (stateDataChanged) {
        /*qDebug() << "[MODEL-OUT] Emitting dataChanged. New Phase:" << static_cast<int>(data.currentTrackingPhase)
                 << "Valid Target:" << data.trackerHasValidTarget;
         qDebug() << "trackedTarget_position: (" << data.trackedTargetCenterX_px << ", " << data.trackedTargetCenterY_px << ")";*/
         
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::startTrackingAcquisition() {
    // ========================================================================
    // UC2: Initiate Tracking on Moving Target
    // UC3: Track Target with Lead Angle Active
    // ========================================================================
    // CRITICAL ARCHITECTURAL PRINCIPLE:
    // • Visual tracking (acquisition box) operates in IMAGE COORDINATES
    // • Ballistic compensation (reticle position) operates in ANGULAR COORDINATES
    // • These TWO COORDINATE SYSTEMS must remain SEPARATE
    //
    // The acquisition box marks WHERE THE TARGET APPEARS VISUALLY
    // The reticle marks WHERE TO AIM TO HIT (with ballistic corrections)
    // ========================================================================

    SystemStateData& data = m_currentStateData;
    if (data.currentTrackingPhase == TrackingPhase::Off) {
        data.currentTrackingPhase = TrackingPhase::Acquisition;

        // ✅ FIX: Center acquisition box on RETICLE position (where gun is pointing)
        // This provides better UX - when operator aims at target and clicks to track,
        // the yellow box appears exactly where they're aiming (reticle position)
        // Reticle position includes zeroing offsets, which is correct because:
        // - The operator has zeroed the weapon to align gun with camera
        // - They are aiming AT THE TARGET with the reticle
        // - The acquisition box should appear WHERE THEY ARE AIMING
        float reticleCenterX = data.reticleAimpointImageX_px;
        float reticleCenterY = data.reticleAimpointImageY_px;

        qDebug() << "========================================";
        qDebug() << "STARTING TRACKING ACQUISITION";
        qDebug() << "========================================";
        qDebug() << "Screen Size:" << data.currentImageWidthPx << "x" << data.currentImageHeightPx;
        qDebug() << "Screen Center:" << (data.currentImageWidthPx/2) << "," << (data.currentImageHeightPx/2);
        qDebug() << "Zeroing Status:";
        qDebug() << "  - Applied:" << data.zeroingAppliedToBallistics;
        qDebug() << "  - Az Offset:" << data.zeroingAzimuthOffset << "deg";
        qDebug() << "  - El Offset:" << data.zeroingElevationOffset << "deg";
        qDebug() << "Reticle Position (from SystemStateData):";
        qDebug() << "  - X:" << reticleCenterX << "px";
        qDebug() << "  - Y:" << reticleCenterY << "px";
        qDebug() << "Centering acquisition box on RETICLE position";

        // Initialize acquisition box centered on RETICLE (where gun is pointing)
        float defaultBoxW = 100.0f;
        float defaultBoxH = 100.0f;
        data.acquisitionBoxW_px = defaultBoxW;
        data.acquisitionBoxH_px = defaultBoxH;
        data.acquisitionBoxX_px = reticleCenterX - (defaultBoxW / 2.0f);
        data.acquisitionBoxY_px = reticleCenterY - (defaultBoxH / 2.0f);

        // Safety: Clamp to screen bounds (should already be centered, but ensure safety)
        data.acquisitionBoxX_px = qBound(0.0f, data.acquisitionBoxX_px,
                                         static_cast<float>(data.currentImageWidthPx) - data.acquisitionBoxW_px);
        data.acquisitionBoxY_px = qBound(0.0f, data.acquisitionBoxY_px,
                                         static_cast<float>(data.currentImageHeightPx) - data.acquisitionBoxH_px);

        qDebug() << "Acquisition Box Position:";
        qDebug() << "  - Top-Left: (" << data.acquisitionBoxX_px << "," << data.acquisitionBoxY_px << ")";
        qDebug() << "  - Size:" << data.acquisitionBoxW_px << "x" << data.acquisitionBoxH_px;
        qDebug() << "  - Center: (" << (data.acquisitionBoxX_px + defaultBoxW/2.0f) << ","
                 << (data.acquisitionBoxY_px + defaultBoxH/2.0f) << ")";
        qDebug() << "========================================";

        // We are still in Surveillance and Manual motion
        data.opMode = OperationalMode::Surveillance;
        data.motionMode = MotionMode::Manual;

        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::requestTrackerLockOn() {
    SystemStateData& data = m_currentStateData;
    if (data.currentTrackingPhase == TrackingPhase::Acquisition) {
        data.currentTrackingPhase = TrackingPhase::Tracking_LockPending;
        // Motion mode is still Manual here. GimbalController will switch it to AutoTrack
        // only AFTER CameraVideoStreamDevice confirms a lock via updateTrackingResult.
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::stopTracking() {
    SystemStateData& data = m_currentStateData;
    if (data.currentTrackingPhase != TrackingPhase::Off) {
        data.currentTrackingPhase = TrackingPhase::Off;
        data.trackerHasValidTarget = false;
        // Revert to Surveillance/Manual modes
        data.opMode = OperationalMode::Surveillance;
        data.motionMode = MotionMode::Manual;
        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::adjustAcquisitionBoxSize(float dW, float dH) {
    // ========================================================================
    // SAFETY: Acquisition Box Boundary Validation
    // ========================================================================
    // Ensures acquisition box stays within valid bounds during user adjustment
    // Maintains minimum size for VPI tracker initialization requirements
    // Prevents box from exceeding screen dimensions
    // ========================================================================

    SystemStateData& data = m_currentStateData;
    if (data.currentTrackingPhase == TrackingPhase::Acquisition) {
        // Apply size adjustment
        float newWidth = data.acquisitionBoxW_px + dW;
        float newHeight = data.acquisitionBoxH_px + dH;

        // Define safety constraints
        const float MIN_BOX_SIZE = 20.0f;   // VPI tracker minimum patch size requirement
        const float MAX_BOX_RATIO = 0.8f;   // Maximum 80% of screen dimension

        // Clamp to valid range
        data.acquisitionBoxW_px = qBound(MIN_BOX_SIZE, newWidth,
                                         static_cast<float>(data.currentImageWidthPx * MAX_BOX_RATIO));
        data.acquisitionBoxH_px = qBound(MIN_BOX_SIZE, newHeight,
                                         static_cast<float>(data.currentImageHeightPx * MAX_BOX_RATIO));

        // Recenter box after resizing (maintains screen-centered position)
        data.acquisitionBoxX_px = (data.currentImageWidthPx / 2.0f) - (data.acquisitionBoxW_px / 2.0f);
        data.acquisitionBoxY_px = (data.currentImageHeightPx / 2.0f) - (data.acquisitionBoxH_px / 2.0f);

        qDebug() << "   Acquisition box resized to"
                 << data.acquisitionBoxW_px << "x" << data.acquisitionBoxH_px
                 << "at [" << data.acquisitionBoxX_px << "," << data.acquisitionBoxY_px << "]";

        emit dataChanged(m_currentStateData);
    }
}

void SystemStateModel::onRadarPlotsUpdated(const QVector<RadarData> &plots) {
QVector<SimpleRadarPlot> converted;
converted.reserve(plots.size());

    for (const RadarData &p : plots) {
        SimpleRadarPlot s;
        s.id = p.id;
        s.azimuth = p.azimuthDegrees;
        s.range = p.rangeMeters;
        s.relativeCourse = p.relativeCourseDegrees;
        s.relativeSpeed = p.relativeSpeedMPS;
        converted.append(s);
    }
    if (m_currentStateData.radarPlots != converted) {
        m_currentStateData.radarPlots = converted;
        updateData(m_currentStateData);
    }
    //}
}

void SystemStateModel::selectNextRadarTrack() {
    SystemStateData& data = m_currentStateData;
    if (data.radarPlots.isEmpty()) return;

    // Find the index of the currently selected track ID
    auto it = std::find_if(data.radarPlots.begin(), data.radarPlots.end(),
                           [&](const SimpleRadarPlot& p){
                               return p.id == data.selectedRadarTrackId;
                            }
                           );

    if (it == data.radarPlots.end() || std::next(it) == data.radarPlots.end()) {
        // Not found or is the last one, wrap to the first
        data.selectedRadarTrackId = data.radarPlots.front().id;
    } else {
        // Move to the next
        data.selectedRadarTrackId = (*std::next(it)).id;
    }
    qDebug() << "[MODEL] Selected Radar Track ID:" << data.selectedRadarTrackId;
    emit dataChanged(data);
}

void SystemStateModel::selectPreviousRadarTrack() {
    SystemStateData& data = m_currentStateData;
    if (data.radarPlots.isEmpty()) return;

    // Find the index of the currently selected track ID
    auto it = std::find_if(data.radarPlots.begin(), data.radarPlots.end(),
                           [&](const SimpleRadarPlot& p){
                               return p.id == data.selectedRadarTrackId;
                           }
                           );

    if (it == data.radarPlots.end() || it == data.radarPlots.begin()) {
        // Not found or is the first one, wrap to the last
        data.selectedRadarTrackId = data.radarPlots.back().id;
    } else {
        // Move to the previous
        data.selectedRadarTrackId = (*std::prev(it)).id;
    }
    qDebug() << "[MODEL] Selected Radar Track ID:" << data.selectedRadarTrackId;
    emit dataChanged(data);
}

void SystemStateModel::commandSlewToSelectedRadarTrack() {
    SystemStateData& data = m_currentStateData;
    // Check if we are in a mode that allows radar slewing
    if (data.opMode != OperationalMode::Surveillance) return;

    if (data.selectedRadarTrackId != 0) {
        qDebug() << "[MODEL] Commanding gimbal to slew to Radar Track ID:" << data.selectedRadarTrackId;
        // The responsibility of moving the gimbal is NOT here.
        // We set the MOTION mode. The GimbalController will react to it.
        //data.motionMode = MotionMode::RadarSlew; // << NEW MOTION MODE
        emit dataChanged(data);
    }
}

/*
Ammo Feed Cycle State Management
*/
void SystemStateModel::setAmmoFeedState(AmmoFeedState state)
{
    if (m_ammoFeedState == state) {
        return;
    }
    m_ammoFeedState = state;

    // Update the central data store
    SystemStateData data = m_currentStateData;
    data.ammoFeedState = state;
    // Also update backward compat flag
    data.ammoFeedCycleInProgress = (state == AmmoFeedState::Extending ||
                                     state == AmmoFeedState::Retracting);
    m_currentStateData = data;

    emit ammoFeedStateChanged(state);
    emit dataChanged(data);

    // Log state name for debugging
    QString stateName;
    switch (state) {
        case AmmoFeedState::Idle: stateName = "IDLE"; break;
        case AmmoFeedState::Extending: stateName = "EXTENDING"; break;
        case AmmoFeedState::Retracting: stateName = "RETRACTING"; break;
        case AmmoFeedState::Fault: stateName = "FAULT"; break;
    }
    qDebug() << "[SystemStateModel] Ammo feed state:" << stateName;
}

void SystemStateModel::setAmmoFeedCycleInProgress(bool inProgress)
{
    if (m_ammoFeedCycleInProgress == inProgress) {
        return;
    }
    m_ammoFeedCycleInProgress = inProgress;
    emit ammoFeedCycleInProgressChanged(inProgress);
    
    qDebug() << "[SystemStateModel] Ammo feed cycle in progress:" << inProgress;
}

void SystemStateModel::setAmmoLoaded(bool loaded)
{
    if (m_ammoLoaded == loaded) {
        return;
    }
    m_ammoLoaded = loaded;
    emit ammoLoadedChanged(loaded);
    
    qDebug() << "[SystemStateModel] Ammo loaded:" << loaded;
}


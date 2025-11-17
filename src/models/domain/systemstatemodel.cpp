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

    // Connect signals from sub-models to slots here (as was likely intended)
    loadZonesFromFile("zones.json"); // Load initial zones from file if exists

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

// --- General Data Update ---
void SystemStateModel::updateData(const SystemStateData &newState) {

    SystemStateData oldData = m_currentStateData;
    static int count = 0;
    if (++count % 100 == 0) qDebug() << "dataChanged signals:" << count;
    // Check if anything has actually changed to avoid unnecessary signals/updates
    if (oldData == newState) { // Assumes you have operator== for SystemStateData
        return;
    }
    if (m_currentStateData != newState) {
        // Check specifically if gimbal position changed before updating m_currentStateData
        bool gimbalChanged = !qFuzzyCompare(m_currentStateData.gimbalAz, newState.gimbalAz) ||
                             !qFuzzyCompare(m_currentStateData.gimbalEl, newState.gimbalEl);

        m_currentStateData = newState;
        processStateTransitions(oldData, m_currentStateData);
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

void SystemStateModel::setDeadManSwitch(bool pressed) { if(m_currentStateData.deadManSwitchActive != pressed) { m_currentStateData.deadManSwitchActive = pressed; emit dataChanged(m_currentStateData); } }
void SystemStateModel::setDownTrack(bool pressed) { if(m_currentStateData.downTrack != pressed) { m_currentStateData.downTrack = pressed; emit dataChanged(m_currentStateData); } }
void SystemStateModel::setDownSw(bool pressed) { if(m_currentStateData.menuDown != pressed) { m_currentStateData.menuDown = pressed; emit dataChanged(m_currentStateData); } }
void SystemStateModel::setUpTrack(bool pressed) { if(m_currentStateData.upTrack != pressed) { m_currentStateData.upTrack = pressed; emit dataChanged(m_currentStateData); } }
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
    // Assuming ServoDriverData contains azimuth position
    //if (!qFuzzyCompare(m_currentStateData.gimbalAz, azData.position * 0.0016179775280)) { // Check if position actually changed
        m_currentStateData.gimbalAz = azData.position* 0.0016179775280;;
        m_currentStateData.azMotorTemp = azData.motorTemp;
        m_currentStateData.azDriverTemp = azData.driverTemp;
        m_currentStateData.azServoConnected = azData.isConnected;
        m_currentStateData.azRpm = azData.rpm;
        m_currentStateData.azTorque = azData.torque;
        m_currentStateData.azFault = azData.fault;

        emit dataChanged(m_currentStateData); // Emit general data change
        emit gimbalPositionChanged(m_currentStateData.gimbalAz, m_currentStateData.gimbalEl); // Emit specific gimbal change
    //}
}

void SystemStateModel::onServoElDataChanged(const ServoDriverData &elData) {
    // Assuming ServoData contains elevation position
    // if (!qFuzzyCompare(m_currentStateData.gimbalEl, elData.position * (-0.0018))) { // Check if position actually changed
        m_currentStateData.gimbalEl = elData.position * (-0.0018);
        m_currentStateData.elMotorTemp = elData.motorTemp;
        m_currentStateData.elDriverTemp = elData.driverTemp;
        m_currentStateData.elServoConnected = elData.isConnected;
        m_currentStateData.elRpm = elData.rpm;            
        m_currentStateData.elTorque = elData.torque;      
        m_currentStateData.elFault = elData.fault;        
 
        emit dataChanged(m_currentStateData); // Emit general data change
        emit gimbalPositionChanged(m_currentStateData.gimbalAz, m_currentStateData.gimbalEl); // Emit specific gimbal change
    //}
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


void SystemStateModel::onJoystickAxisChanged(int axis, float normalizedValue)
{
    //START_TS_TIMER("SystemStateModel");
    SystemStateData newData = m_currentStateData;

    if (axis == 0){
        newData.joystickAzValue = normalizedValue;
    } else if (axis == 1){
        newData.joystickElValue = normalizedValue;
    }

    updateData(newData);
    //LOG_TS_ELAPSED("SystemStateModel", "Processed model data");
}

void SystemStateModel::onJoystickButtonChanged(int button, bool pressed)
{
    //START_TS_TIMER("SystemStateModel");
    SystemStateData newData = m_currentStateData;

    updateData(newData);
    //LOG_TS_ELAPSED("SystemStateModel", "Processed model data");
}

void SystemStateModel::onJoystickHatChanged(int hat, int direction)
{
    //START_TS_TIMER("SystemStateModel");
    SystemStateData newData = m_currentStateData;

    if (hat == 0) {
        newData.joystickHatDirection = direction; // Assuming direction is an int representing the hat state
    }


    updateData(newData);
    //LOG_TS_ELAPSED("SystemStateModel", "Processed model data");
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

    newData.stationEnabled =  pData.enableStationSW;
    newData.gunArmed = pData.armGunSW;
    newData.gotoHomePosition = pData.homePositionSW;
    newData.ammoLoaded = pData.loadAmmunitionSW;

    newData.authorized = pData.authorizeSw;
    newData.enableStabilization = pData.enableStabilizationSW;
    // DON'T set activeCameraIsDay here - let setActiveCameraIsDay() handle it!
    newData.emergencyStopActive = pData.authorizeSw;

    switch (pData.fireMode) {
    case 0:
        newData.fireMode =FireMode::SingleShot;
        break;
    case 1:
        newData.fireMode =FireMode::ShortBurst;
        break;
    case 2:
        newData.fireMode =FireMode::LongBurst;
        break;
    default:
        newData.fireMode =FireMode::Unknown;
        break;
    }

    newData.gimbalSpeed = pData.speedSW;
    newData.plc21Connected = pData.isConnected;

    // Auto-disable detection when switching to night camera
    if (!pData.switchCameraSW && m_currentStateData.activeCameraIsDay) {
        // Switching from day to night camera
        newData.detectionEnabled = false;
        qInfo() << "SystemStateModel: Night camera activated - Detection auto-disabled";
    }

    updateData(newData);

    // ✅ Use setActiveCameraIsDay() - it checks if changed and triggers CCIP recalculation!
    setActiveCameraIsDay(pData.switchCameraSW);
}

void SystemStateModel::onPlc42DataChanged(const Plc42Data &pData)
{
    SystemStateData newData = m_currentStateData;
    newData.upperLimitSensorActive = pData.stationUpperSensor;        // DataModel::m_stationUpperSensor
    newData.lowerLimitSensorActive = pData.stationLowerSensor;        // DataModel::m_stationLowerSensor

    // Additional station inputs (if needed)
    newData.stationAmmunitionLevel = pData.ammunitionLevel;        // DataModel::m_stationAmmunitionLevel
    newData.hatchState = pData.hatchState;                 // DataModel::m_hatchState
    newData.stationInput2 = pData.stationInput2;                 // DataModel::m_stationInput2
    newData.stationInput3 = pData.stationInput3;                 // DataModel::m_stationInput3

    newData.solenoidMode     = pData.solenoidMode;
    newData.gimbalOpMode     = pData.gimbalOpMode;
    newData.azimuthSpeed     = pData.azimuthSpeed;
    newData.elevationSpeed   = pData.elevationSpeed;
    newData.azimuthDirection = pData.azimuthDirection;
    newData.elevationDirection = pData.elevationDirection;
    newData.solenoidState     = pData.solenoidState;


    newData.solenoidState = pData.solenoidState;
    newData.resetAlarm = pData.resetAlarm;

    newData.plc42Connected = pData.isConnected;

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
        // Don't reset offsets here, user might be re-doing it or making cumulative adjustments
        qDebug() << "Zeroing procedure started.";
        emit dataChanged(m_currentStateData);
        emit zeroingStateChanged(true, m_currentStateData.zeroingAzimuthOffset, m_currentStateData.zeroingElevationOffset);
    }
}

void SystemStateModel::applyZeroingAdjustment(float deltaAz, float deltaEl) {
    if (m_currentStateData.zeroingModeActive) {
        // The PDF says "+/- 3 degree adjustment can be made". This usually means the
        // *total current offset* from the mechanical boreline is within +/-3 degrees,
        // or each individual adjustment step is small. Let's assume total offset.
        // We might need separate "raw mechanical zero" and "user zero offset".
        // For now, these are offsets from a nominal zero.

        m_currentStateData.zeroingAzimuthOffset += deltaAz;
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
    if (m_currentStateData.zeroingModeActive) {
        m_currentStateData.zeroingModeActive = false;
        m_currentStateData.zeroingAppliedToBallistics = true; // Zeroing is now active
        qDebug() << "Zeroing procedure finalized. Offsets Az:" << m_currentStateData.zeroingAzimuthOffset
                 << "El:" << m_currentStateData.zeroingElevationOffset;
        emit dataChanged(m_currentStateData);
        emit zeroingStateChanged(false, m_currentStateData.zeroingAzimuthOffset, m_currentStateData.zeroingElevationOffset);
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
}

// =================================
// ENVIRONMENTAL CONDITIONS
// =================================

void SystemStateModel::startEnvironmentalProcedure() {
    if (!m_currentStateData.environmentalModeActive) {
        m_currentStateData.environmentalModeActive = true;
        qDebug() << "Environmental procedure started.";
        emit dataChanged(m_currentStateData);
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
    // ARCHITECTURAL PRINCIPLE: Two Separate Coordinate Systems
    // ========================================================================
    // 1. RETICLE: Gun boresight with ZEROING ONLY (shows where gun points)
    // 2. CCIP:    Impact prediction with ZEROING + LEAD (shows where bullets hit)
    // ========================================================================

    SystemStateData& data = m_currentStateData;

    // Determine active camera's HFOV
    float activeHfov = data.activeCameraIsDay ? static_cast<float>(data.dayCurrentHFOV) : static_cast<float>(data.nightCurrentHFOV);

    // ========================================================================
    // CALCULATION 1: RETICLE Position (Zeroing ONLY)
    // ========================================================================
    // Main reticle shows gun boresight with zeroing correction
    // NO lead angle applied - this is pure gun pointing direction
    // ========================================================================
    QPointF newReticlePosPx = ReticleAimpointCalculator::calculateReticleImagePositionPx(
        data.zeroingAzimuthOffset,        // Zeroing correction
        data.zeroingElevationOffset,      // Zeroing correction
        data.zeroingAppliedToBallistics,  // Apply zeroing?
        0.0f,                             // ← NO LEAD ANGLE for reticle!
        0.0f,                             // ← NO LEAD ANGLE for reticle!
        false,                            // ← LAC not applied to reticle
        LeadAngleStatus::Off,             // ← Status irrelevant for reticle
        activeHfov,
        data.currentImageWidthPx,
        data.currentImageHeightPx
    );

    // ========================================================================
    // CALCULATION 2: CCIP Position (Zeroing + Lead)
    // ========================================================================
    // CCIP pipper shows bullet impact point with full ballistic compensation
    // Includes BOTH zeroing AND lead angle
    // Only visible when LAC is active
    // ========================================================================
    QPointF newCcipPosPx = ReticleAimpointCalculator::calculateReticleImagePositionPx(
        data.zeroingAzimuthOffset,        // Zeroing correction
        data.zeroingElevationOffset,      // Zeroing correction
        data.zeroingAppliedToBallistics,  // Apply zeroing?
        data.leadAngleOffsetAz,           // ← WITH lead angle!
        data.leadAngleOffsetEl,           // ← WITH lead angle!
        data.leadAngleCompensationActive, // ← LAC applied to CCIP
        data.currentLeadAngleStatus,      // ← Status matters for CCIP
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

    // Update status texts
    QString oldLeadStatusText = data.leadStatusText;
    QString oldZeroingStatusText = data.zeroingStatusText;

    if (data.zeroingAppliedToBallistics) data.zeroingStatusText = "Z";
    else if (data.zeroingModeActive) data.zeroingStatusText = "ZEROING";
    else data.zeroingStatusText = "";

    if (data.leadAngleCompensationActive) {
        switch(data.currentLeadAngleStatus) {
            case LeadAngleStatus::On: data.leadStatusText = "LEAD ANGLE ON"; break;
            case LeadAngleStatus::Lag: data.leadStatusText = "LEAD ANGLE LAG"; break;
            case LeadAngleStatus::ZoomOut: data.leadStatusText = "ZOOM OUT"; break;
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
    if (m_currentStateData.currentImageWidthPx != width)   { m_currentStateData.currentImageWidthPx = width; changed=true; }
    if (m_currentStateData.currentImageHeightPx != height) { m_currentStateData.currentImageHeightPx = height; changed=true; }
    if (!qFuzzyCompare(static_cast<float>(m_currentStateData.dayCurrentHFOV), dayHfov)) { m_currentStateData.dayCurrentHFOV = dayHfov; changed=true; }
    if (!qFuzzyCompare(static_cast<float>(m_currentStateData.nightCurrentHFOV), nightHfov)) { m_currentStateData.nightCurrentHFOV = nightHfov; changed=true; }
    if (m_currentStateData.activeCameraIsDay != isDayActive) {m_currentStateData.activeCameraIsDay = isDayActive; changed=true;}

    if(changed){
        recalculateDerivedAimpointData();
        emit dataChanged(m_currentStateData);
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
    // Similar to No Fire Zone, this can be used to track if the current azimuth is in a No Traverse Zone
    m_currentStateData.isReticleInNoTraverseZone = inZone;
    emit dataChanged(m_currentStateData);
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

void SystemStateModel::processStateTransitions(const SystemStateData& oldData, SystemStateData& newData)
{
    // This function takes newData by reference, so it can modify it directly.

    // PRIORITY 1: Emergency Stop Check
    // If the E-Stop has just been activated
    if (newData.emergencyStopActive && !oldData.emergencyStopActive) {
        // We pass the reference 'newData' to be modified
        enterEmergencyStopMode(); // Modify enterEmergencyStopMode to take a reference
        return; // E-Stop overrides all other transitions
    }

    // If E-Stop has been released
    if (!newData.emergencyStopActive && oldData.emergencyStopActive) {
        // Go to a safe, idle state. Operator must re-enable the station.
        enterIdleMode(); // Modify enterIdleMode to take a reference
        return;
    }

    // Do not allow any other transitions if E-Stop is active
    if (newData.emergencyStopActive) {
        return;
    }

    // PRIORITY 2: Station Power Check
    // If station was just disabled
    if (!newData.stationEnabled && oldData.stationEnabled) {
        enterIdleMode();
        return;
    }
    // If station was just enabled
    if (newData.stationEnabled && !oldData.stationEnabled) {
        // If we were Idle, transition to Surveillance
        if (newData.opMode == OperationalMode::Idle) {
            enterSurveillanceMode();
        }
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

 

void SystemStateModel::enterEmergencyStopMode() {
    SystemStateData& data = m_currentStateData;
    if (data.opMode == OperationalMode::EmergencyStop) return; // Already in this state

    qCritical() << "[MODEL] ENTERING EMERGENCY STOP MODE!";

    // Set the high-level operational mode
    data.opMode = OperationalMode::EmergencyStop;
    // Set the gimbal motion mode to Idle to ensure no commands are being calculated
    data.motionMode = MotionMode::Idle;
    // Set all "action" flags to false
    data.trackingActive = false; // Use the old one if still present, or better:
    data.currentTrackingPhase = TrackingPhase::Off;
    data.trackerHasValidTarget = false;
    data.leadAngleCompensationActive = false;
    // Do NOT clear zeroing/windage settings, as they might be needed after reset.
    // The E-Stop is about stopping motion and firing, not erasing calibration.

    // Emit the state change so all components react
    emit dataChanged(m_currentStateData);
}

/*void SystemStateModel::updateTrackedTargetInfo(int cameraIndex, bool isValid, float centerX_px, float centerY_px,
                                 float width_px, float height_px,
                                 float velocityX_px_s, float velocityY_px_s,
                                 VPITrackingState state)
{
    // Check which camera is supposed to be active (e.g., 0 for Day, 1 for Night)
    int activeCameraIndex = m_currentStateData.activeCameraIsDay ? 0 : 1;

    // If the update is coming from an INACTIVE camera, simply ignore it.
    if (cameraIndex != activeCameraIndex) {
        return; // Do nothing.
    }

    // --- If we get here, the update is from the ACTIVE camera. Proceed as before. ---

    SystemStateData& data = m_currentStateData; // Get a reference to modify
    bool changed = false;

    if (data.trackerHasValidTarget != isValid) {
        data.trackerHasValidTarget = isValid;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetCenterX_px, centerX_px)) {
        data.trackedTargetCenterX_px = centerX_px;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetCenterY_px, centerY_px)) {
        data.trackedTargetCenterY_px = centerY_px;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetWidth_px, width_px)) {
        data.trackedTargetWidth_px = width_px;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetHeight_px, height_px)) {
        data.trackedTargetHeight_px = height_px;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetVelocityX_px_s, velocityX_px_s)) {
        data.trackedTargetVelocityX_px_s = velocityX_px_s;
        changed = true;
    }
    if (!qFuzzyCompare(data.trackedTargetVelocityY_px_s, velocityY_px_s)) {
        data.trackedTargetVelocityY_px_s = velocityY_px_s;
        changed = true;
    }

    if (data.trackedTargetState != state) {
        data.trackedTargetState = state;
        changed = true;
    }

    if (changed) {
        //qDebug() << "SystemStateModel: Tracked target info updated - Valid:" << isValid
        //         << "CenterPx: (" << centerX_px << "," << centerY_px << ") State:" << static_cast<int>(state);
        emit dataChanged(m_currentStateData); // Emit the signal with the entire updated state
    }
}*/

// systemstatemodel.cpp

// Rename/replace updateTrackedTargetInfo with this one.
void SystemStateModel::updateTrackingResult(
    int cameraIndex,
    bool hasLock, // This parameter might become less relevant as we use VPITrackingState directly
    float centerX_px, float centerY_px,
    float width_px, float height_px,
    float velocityX_px_s, float velocityY_px_s,
    VPITrackingState trackerState)
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
                qDebug() << "[MODEL] ActiveLock: Target still tracked.";
            } else {
                // Unexpected state during ActiveLock. Log and potentially reset.
                qWarning() << "[MODEL] In ActiveLock, received unexpected VPI state: " << static_cast<int>(trackerState) << ". Staying in ActiveLock but might indicate issue.";
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
        qDebug() << "[MODEL-OUT] Emitting dataChanged. New Phase:" << static_cast<int>(data.currentTrackingPhase)
                 << "Valid Target:" << data.trackerHasValidTarget;
         qDebug() << "trackedTarget_position: (" << data.trackedTargetCenterX_px << ", " << data.trackedTargetCenterY_px << ")";
         
        emit dataChanged(m_currentStateData);
    }
}

// Dummy method to simulate external state changes for testing
/*void SystemStateModel::simulateTrackingPhaseChange(TrackingPhase newPhase, int cameraIndex, float acqX, float acqY, float acqW, float acqH)
{
    QMutexLocker locker(&m_mutex);
    m_currentStateData.currentTrackingPhase = newPhase;
    m_currentStateData.activeCameraIsDay = (cameraIndex == 0); // Set active camera for simulation

    // Update acquisition box if provided
    if (acqW > 0 && acqH > 0) {
        m_currentStateData.acquisitionBoxX_px = acqX;
        m_currentStateData.acquisitionBoxY_px = acqY;
        m_currentStateData.acquisitionBoxW_px = acqW;
        m_currentStateData.acquisitionBoxH_px = acqH;
    }

    // Adjust opMode and motionMode based on newPhase for simulation clarity
    if (newPhase == TrackingPhase::Off) {
        m_currentStateData.opMode = OperationalMode::Idle;
        m_currentStateData.motionMode = MotionMode::Manual;
    } else if (newPhase == TrackingPhase::Acquisition) {
        m_currentStateData.opMode = OperationalMode::Surveillance;
        m_currentStateData.motionMode = MotionMode::Manual;
    } else if (newPhase == TrackingPhase::Tracking_LockPending || newPhase == TrackingPhase::Tracking_ActiveLock || newPhase == TrackingPhase::Tracking_Coast || newPhase == TrackingPhase::Tracking_Firing) {
        m_currentStateData.opMode = OperationalMode::Tracking;
        // MotionMode will be set by updateTrackingResult or specific logic for Firing
    }

    qDebug() << "[MODEL-SIMULATE] Phase changed to:" << static_cast<int>(newPhase) << "for camera:" << cameraIndex;
    emit dataChanged(m_currentStateData);
}*/


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

/*void SystemStateModel::updateTrackingResult(int camIndex, bool hasLock, ...) {
    SystemStateData& data = m_currentStateData;
    // ... (update trackerHasValidTarget, trackedTargetCenterX_px, etc. as before) ...

    // --- State Machine Logic ---
    if (data.currentTrackingPhase == TrackingPhase::Tracking_LockPending ||
        data.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
        data.currentTrackingPhase == TrackingPhase::Tracking_Coast)
    {
        if (hasLock) {
            data.currentTrackingPhase = TrackingPhase::Tracking_ActiveLock;
            // Now that we have a lock, system can enter tracking modes
            data.opMode = OperationalMode::Tracking;
            data.motionMode = MotionMode::AutoTrack; // Or just 'Tracking'
        } else {
            // Target was lost or lock failed
            data.currentTrackingPhase = TrackingPhase::Tracking_Coast; // Or back to LockPending if it never locked
            // Revert to manual control but keep trying to re-acquire (or show 'lost' status)
            data.opMode = OperationalMode::Surveillance; // Or stay in Tracking op mode with a "COAST" status
            data.motionMode = MotionMode::Manual;
        }
        emit dataChanged(m_currentStateData);
    }
}
*/
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

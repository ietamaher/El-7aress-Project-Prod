#include "ledcontroller.h"
#include "hardware/devices/plc21device.h"

LedController::LedController(SystemStateModel* systemStateModel, Plc21Device* plc21Device, QObject *parent)
    : QObject(parent),
    m_systemStateModel(systemStateModel),
    m_plc21Device(plc21Device)
{
    // ✅ LATENCY FIX: Queued connection prevents LED updates from blocking device I/O
    connect(m_systemStateModel, &SystemStateModel::dataChanged,
            this, &LedController::onSystemStateChanged,
            Qt::QueuedConnection);  // Non-blocking signal delivery
}

void LedController::onSystemStateChanged(const SystemStateData &data)
{
    if (!m_plc21Device) return;

    // Only write to PLC21 when LED states actually change
    // This prevents flooding PLC21 with redundant Modbus writes (was 80 writes/sec!)

    if (data.gunArmed != m_cachedGunArmed) {
        m_cachedGunArmed = data.gunArmed;
        m_plc21Device->setGunArmedLed(data.gunArmed);
    }

    if (data.stationEnabled != m_cachedStationEnabled) {
        m_cachedStationEnabled = data.stationEnabled;
        m_plc21Device->setStationEnabledLed(data.stationEnabled);
    }

    if (data.hatchState != m_cachedhatchState) {
        m_cachedhatchState = data.hatchState;
        m_plc21Device->sethatchStateLed(data.hatchState);
    }

    bool panelBacklight = (data.osdColorStyle == ColorStyle::Red);
    if (panelBacklight != m_cachedPanelBacklight) {
        m_cachedPanelBacklight = panelBacklight;
        m_plc21Device->setPanelBacklight(panelBacklight);
    }
}


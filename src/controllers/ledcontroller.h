#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include <QObject>
#include "models/domain/systemstatemodel.h"

class Plc21Device;

class LedController : public QObject {
    Q_OBJECT

public:
    explicit LedController(SystemStateModel* systemStateModel, Plc21Device* plc21Device,
                           QObject* parent = nullptr);

private slots:
    void onSystemStateChanged(const SystemStateData& data);

private:
    SystemStateModel* m_systemStateModel;
    Plc21Device* m_plc21Device;

    // Cache LED states to prevent flooding PLC21 with redundant writes
    bool m_cachedGunArmed = false;
    bool m_cachedStationEnabled = false;
    bool m_cachedhatchState = false;
    bool m_cachedPanelBacklight = false;
};

#endif  // LEDCONTROLLER_H

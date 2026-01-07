#ifndef SERVODRIVERDATAMODEL_H
#define SERVODRIVERDATAMODEL_H


#include <QObject>
#include "hardware/data/DataTypes.h"


class ServoDriverDataModel : public QObject {
    Q_OBJECT

public:
    explicit ServoDriverDataModel(QObject* parent = nullptr) : QObject(parent) {}

    ServoDriverData data() const {
        return m_data;
    }


signals:
    void dataChanged(const ServoDriverData& newData);

public slots:
    void updateData(const ServoDriverData& newData) {
        if (m_data != newData) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    ServoDriverData m_data;
};

#endif  // SERVODRIVERDATAMODEL_H

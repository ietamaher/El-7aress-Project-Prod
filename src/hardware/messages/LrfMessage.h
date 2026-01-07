#ifndef LRFMESSAGE_H
#define LRFMESSAGE_H

#include "hardware/interfaces/Message.h"
#include "hardware/data/DataTypes.h"
#include <QString>

/**
 * @brief Message carrying LRF measurement data
 */
class LrfDataMessage : public Message {
public:
    explicit LrfDataMessage(const LrfData& data) : m_data(data) {}
    Type typeId() const override {
        return Type::LrfDataType;
    }
    const LrfData& data() const {
        return m_data;
    }

private:
    LrfData m_data;
};

/**
 * @brief Message carrying LRF product information
 */
class LrfInfoMessage : public Message {
public:
    LrfInfoMessage(quint8 id, const QString& version) : m_id(id), m_version(version) {}
    Type typeId() const override {
        return Type::LrfInfoType;
    }
    quint8 productId() const {
        return m_id;
    }
    QString softwareVersion() const {
        return m_version;
    }

private:
    quint8 m_id;
    QString m_version;
};

#endif  // LRFMESSAGE_H

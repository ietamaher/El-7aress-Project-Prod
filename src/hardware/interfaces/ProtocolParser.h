#pragma once
#include <QObject>
#include <QByteArray>
#include <QList>
#include "hardware/interfaces/Message.h"
#include <vector>

class QModbusReply;

class ProtocolParser : public QObject {
    Q_OBJECT

public:
    explicit ProtocolParser(QObject* parent = nullptr) : QObject(parent) {}

    // Parses a raw chunk of data and returns a list of fully-formed messages.
    // Using unique_ptr to manage memory and ownership automatically.
    // FIX: Changed return type to std::vector to correctly handle move-only MessagePtr.
    virtual std::vector<MessagePtr> parse(const QByteArray& rawData) = 0;

    virtual std::vector<MessagePtr> parse(QModbusReply* reply) {
        Q_UNUSED(reply);
        return {};
    }
};

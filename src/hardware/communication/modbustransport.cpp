#include "modbustransport.h"
#include "qserialport.h"
#include <QDebug>

ModbusTransport::ModbusTransport(QObject* parent)
    : Transport(parent),
    m_client(new QModbusRtuSerialClient(this)),
    m_slaveId(1) // Default value
{
    connect(m_client, &QModbusClient::stateChanged, this, &ModbusTransport::onStateChanged);
    connect(m_client, &QModbusClient::errorOccurred, this, &ModbusTransport::onModbusError);
}

ModbusTransport::~ModbusTransport() = default;

bool ModbusTransport::open(const QJsonObject& config) {
    m_config = config;

    // FIXED: Read slave ID from config
    m_slaveId = config["slaveId"].toInt(1);
    QString port = config["port"].toString();
    int baudRate = config["baudRate"].toInt(9600);

    qDebug() << "ModbusTransport: Setting slave ID to" << m_slaveId;
    qDebug() << "ModbusTransport: Opening port" << port << "at" << baudRate << "baud";

    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter, port);
    m_client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
    m_client->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_client->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
    m_client->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                     static_cast<QSerialPort::Parity>(config["parity"].toInt(QSerialPort::NoParity)));

    m_client->setTimeout(config["timeoutMs"].toInt(500));
    m_client->setNumberOfRetries(config["retries"].toInt(3));

    if (!m_client->connectDevice()) {
        QString error = QString("ModbusTransport: Failed to connect to %1 (slave %2) - %3")
                        .arg(port).arg(m_slaveId).arg(m_client->errorString());
        qCritical() << error;
        emit linkError(error);
        return false;
    }

    qDebug() << "ModbusTransport: Connected successfully to" << port << "with slave ID" << m_slaveId;
    return true;
}

void ModbusTransport::close() {
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();
    emit connectionStateChanged(false);
}

// FIXED: Use stored slave ID
QModbusReply* ModbusTransport::sendReadRequest(const QModbusDataUnit &unit) {
    if (m_client->state() != QModbusDevice::ConnectedState) {
        emit linkError("ModbusTransport: client not connected");
        return nullptr;
    }

    //qDebug() << "ModbusTransport: Sending read request to slave" << m_slaveId
    //         << "address" << unit.startAddress() << "count" << unit.valueCount();

    QModbusReply *reply = m_client->sendReadRequest(unit, m_slaveId);
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusDevice::NoError) {
                //qDebug() << "ModbusTransport: Read reply received successfully from slave" << m_slaveId;
            } else {
                //qWarning() << "ModbusTransport: Read reply error from slave" << m_slaveId << ":" << reply->errorString();
            }
            emit modbusReplyReady(reply);
        });
    } else {
        qWarning() << "ModbusTransport: Failed to create read request for slave" << m_slaveId;
    }
    return reply;
}

// FIXED: Use stored slave ID
QModbusReply* ModbusTransport::sendWriteRequest(const QModbusDataUnit &unit) {
    if (m_client->state() != QModbusDevice::ConnectedState) {
        emit linkError("ModbusTransport: client not connected");
        return nullptr;
    }

    //qDebug() << "ModbusTransport: Sending write request to slave" << m_slaveId
    //         << "address" << unit.startAddress() << "count" << unit.valueCount();

    QModbusReply *reply = m_client->sendWriteRequest(unit, m_slaveId);
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusDevice::NoError) {
                //qDebug() << "ModbusTransport: Write reply received successfully from slave" << m_slaveId;
            } else {
                //qWarning() << "ModbusTransport: Write reply error from slave" << m_slaveId << ":" << reply->errorString();
            }
            emit modbusReplyReady(reply);
        });
    } else {
        qWarning() << "ModbusTransport: Failed to create write request for slave" << m_slaveId;
    }
    return reply;
}

void ModbusTransport::onStateChanged(QModbusDevice::State state) {
    bool connected = (state == QModbusDevice::ConnectedState);

    QString stateStr;
    switch (state) {
        case QModbusDevice::UnconnectedState: stateStr = "Unconnected"; break;
        case QModbusDevice::ConnectingState: stateStr = "Connecting"; break;
        case QModbusDevice::ConnectedState: stateStr = "Connected"; break;
        case QModbusDevice::ClosingState: stateStr = "Closing"; break;
        default: stateStr = QString("Unknown(%1)").arg(state);
    }

    qDebug() << "ModbusTransport: State changed to" << stateStr << "for slave" << m_slaveId;

    // If we failed to connect, log the error
    if (state == QModbusDevice::UnconnectedState && m_client->error() != QModbusDevice::NoError) {
        qWarning() << "ModbusTransport: Slave" << m_slaveId << "connection failed:"
                   << m_client->errorString();
    }

    emit connectionStateChanged(connected);
}

void ModbusTransport::onModbusError(QModbusDevice::Error err) {
    if (err != QModbusDevice::NoError) {
        QString errorMsg = QString("ModbusTransport slave %1: %2").arg(m_slaveId).arg(m_client->errorString());
        emit linkError(errorMsg);
    }
}

#include "serialporttransport.h"
#include <QDebug>

SerialPortTransport::SerialPortTransport(QObject* parent) : Transport(parent) {
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SerialPortTransport::attemptReconnect);
}

bool SerialPortTransport::open(const QJsonObject& config) {
    m_config = config;
    QString port = config["port"].toString();
    int baud = config["baudRate"].toInt(9600);
    m_maxRetries = config["maxRetries"].toInt(5);
    m_baseDelayMs = config["reconnectBaseDelayMs"].toInt(1000);

    m_port.setPortName(port);
    m_port.setBaudRate(baud);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit linkError(m_port.errorString());
        return false;
    }

    // CRITICAL FIX: Flush serial buffers to eliminate stale data from previous runs
    // or device boot-up sequences that cause parser desynchronization
    m_port.clear(QSerialPort::AllDirections);

    connect(&m_port, &QSerialPort::readyRead, this, &SerialPortTransport::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialPortTransport::onError);

    emit connectionStateChanged(true);
    m_retryCount = 0;
    return true;
}

void SerialPortTransport::close() {
    if (m_port.isOpen())
        m_port.close();
    m_reconnectTimer.stop();
    emit connectionStateChanged(false);
}

void SerialPortTransport::sendFrame(const QByteArray& frame) {
    if (m_port.isOpen())
        m_port.write(frame);
}

void SerialPortTransport::onReadyRead() {
    QByteArray chunk = m_port.readAll();
    emit frameReceived(chunk);
}

void SerialPortTransport::onError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError)
        return;
    QString err = m_port.errorString();
    emit linkError(err);

    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        int delay = m_baseDelayMs * (1 << (m_retryCount - 1));
        m_reconnectTimer.start(delay);
    } else {
        emit linkError(QString("SerialPortTransport: max retries reached (%1)").arg(m_maxRetries));
    }
}

void SerialPortTransport::attemptReconnect() {
    if (m_port.isOpen())
        m_port.close();
    // Try reopen with stored config
    open(m_config);
}

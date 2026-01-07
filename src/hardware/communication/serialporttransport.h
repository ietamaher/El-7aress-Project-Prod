#pragma once
#include "hardware/interfaces/Transport.h"
#include <QtSerialPort/QSerialPort>
#include <QJsonObject>
#include <QTimer>

class SerialPortTransport : public Transport {
    Q_OBJECT

public:
    explicit SerialPortTransport(QObject* parent = nullptr);
    bool open(const QJsonObject& config) override;
    void close() override;
    void sendFrame(const QByteArray& frame) override;

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);
    void attemptReconnect();

private:
    QSerialPort m_port;
    QTimer m_reconnectTimer;
    QJsonObject m_config;
    int m_maxRetries = 5;
    int m_retryCount = 0;
    int m_baseDelayMs = 1000;
};

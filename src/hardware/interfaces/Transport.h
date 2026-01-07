#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>

class Transport : public QObject {
    Q_OBJECT
public:
    explicit Transport(QObject* parent = nullptr) : QObject(parent) {}
    virtual bool open(const QJsonObject& config) = 0;
    virtual void close() = 0;
    virtual void sendFrame(const QByteArray& frame) = 0;

signals:
    void frameReceived(const QByteArray& frame);
    void linkError(const QString& error);
    void connectionStateChanged(bool connected);
};

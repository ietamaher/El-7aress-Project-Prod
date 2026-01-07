#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>

// ============================================================================
// CLASS DECLARATION
// ============================================================================
class VideoFrameNotifier : public QObject {
    Q_OBJECT

public:
    explicit VideoFrameNotifier(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void frameUpdated();
};

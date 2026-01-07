#include "aboutviewmodel.h"
#include <QCoreApplication>
#include <QDate>
#include <QSysInfo>

AboutViewModel::AboutViewModel(QObject* parent) : QObject(parent), m_visible(false) {
    // Application Name
    m_appName = "El 7arress RCWS";

    // Application Version
    m_appVersion = QCoreApplication::applicationVersion();
    if (m_appVersion.isEmpty()) {
        m_appVersion = "4.5 (Development Build)";
    }

    // Build Date & Time
    m_buildDate = QString("Built: %1 %2").arg(__DATE__).arg(__TIME__);

    // Qt Version
    m_qtVersion = QString("Qt Version: %1").arg(qVersion());

    // Credits
    m_credits = QString("<b>Lead Developer:</b> Captain Maher BOUZAIEN<br>"
                        "<b>Organization:</b> Tunisian Ministry of Defense<br>"
                        "<b>Special Thanks:</b> EMAM, CRM<br>"
                        "<br>"
                        "<b>Technologies Used:</b><br>"
                        "• Qt %1 Framework<br>"
                        "• NVIDIA VPI (Computer Vision)<br>"
                        "• GStreamer (Video Processing)<br>"
                        "• OpenCV (Image Processing)<br>"
                        "• YOLO v8 (Object Detection)<br>"
                        "• Modbus RTU (Device Communication)")
                    .arg(qVersion());

    // Copyright
    int currentYear = QDate::currentDate().year();
    m_copyright = QString("Copyright © 2022-%1 Tunisian Ministry of Defense.<br>"
                          "All rights reserved.")
                      .arg(currentYear);

    // License
    m_license = QString("<b>License Information:</b><br>"
                        "This software is proprietary and confidential.<br>"
                        "Unauthorized copying, distribution, or use is strictly prohibited.<br>"
                        "<br>"
                        "<b>System Information:</b><br>"
                        "• OS: %1<br>"
                        "• Kernel: %2<br>"
                        "• Architecture: %3")
                    .arg(QSysInfo::prettyProductName())
                    .arg(QSysInfo::kernelVersion())
                    .arg(QSysInfo::currentCpuArchitecture());
}

void AboutViewModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void AboutViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}

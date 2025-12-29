#include "brightnesscontroller.h"
#include "models/brightnessviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QProcess>

BrightnessController::BrightnessController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_currentState(BrightnessState::Idle)
    , m_currentBrightnessEdit(DEFAULT_BRIGHTNESS)
    , m_initialBrightness(DEFAULT_BRIGHTNESS)
{
    // Detect display output on construction
    m_displayOutput = detectDisplayOutput();
    if (m_displayOutput.isEmpty()) {
        qWarning() << "BrightnessController: Could not detect display output, using default";
        m_displayOutput = "HDMI-0"; // Fallback default
    }
    qDebug() << "BrightnessController: Using display output:" << m_displayOutput;
}

void BrightnessController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &BrightnessController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);

    // Apply default brightness on startup
    applyBrightness(DEFAULT_BRIGHTNESS);
}

QString BrightnessController::detectDisplayOutput()
{
    QProcess process;
    process.start("xrandr", QStringList() << "--query");
    process.waitForFinished(2000);

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n');

    for (const QString& line : lines) {
        // Look for connected displays (e.g., "HDMI-0 connected" or "DP-0 connected")
        if (line.contains(" connected")) {
            QString displayName = line.split(' ').first();
            qDebug() << "BrightnessController: Detected connected display:" << displayName;
            return displayName;
        }
    }

    return QString();
}

void BrightnessController::show()
{
    // Store initial brightness to allow cancel
    m_initialBrightness = m_currentBrightnessEdit;
    transitionToState(BrightnessState::Adjusting);
    m_viewModel->setVisible(true);
}

void BrightnessController::hide()
{
    m_viewModel->setVisible(false);
    transitionToState(BrightnessState::Idle);
}

void BrightnessController::transitionToState(BrightnessState newState)
{
    m_currentState = newState;
    updateUI();
}

void BrightnessController::updateUI()
{
    switch (m_currentState) {
    case BrightnessState::Adjusting:
        m_viewModel->setTitle("Display Brightness");
        m_viewModel->setInstruction(
            "Adjust display brightness.\n"
            "Use UP/DOWN to change. Press SELECT to apply."
        );
        m_viewModel->setBrightness(m_currentBrightnessEdit);
        m_viewModel->setShowParameters(true);
        m_viewModel->setParameterLabel(
            QString("Brightness: %1%").arg(m_currentBrightnessEdit)
        );
        break;

    case BrightnessState::Applied:
        m_viewModel->setTitle("Brightness Applied");
        m_viewModel->setInstruction(
            QString("Display brightness set to %1%.\n\n"
                    "Press SELECT to return.")
                .arg(m_currentBrightnessEdit)
        );
        m_viewModel->setShowParameters(true);
        m_viewModel->setParameterLabel(
            QString("Brightness: %1% (APPLIED)").arg(m_currentBrightnessEdit)
        );
        break;

    case BrightnessState::Idle:
    default:
        m_viewModel->setTitle("Display Brightness");
        m_viewModel->setInstruction("");
        m_viewModel->setShowParameters(false);
        break;
    }
}

void BrightnessController::onSelectButtonPressed()
{
    switch (m_currentState) {
    case BrightnessState::Adjusting:
        // Apply the brightness
        applyBrightness(m_currentBrightnessEdit);
        qDebug() << "BrightnessController: Brightness applied:" << m_currentBrightnessEdit << "%";
        transitionToState(BrightnessState::Applied);
        break;

    case BrightnessState::Applied:
        hide();
        emit returnToMainMenu();
        emit brightnessFinished();
        break;

    default:
        break;
    }
}

void BrightnessController::onBackButtonPressed()
{
    // Restore initial brightness if cancelled
    if (m_currentState == BrightnessState::Adjusting) {
        m_currentBrightnessEdit = m_initialBrightness;
        applyBrightness(m_initialBrightness);
        qDebug() << "BrightnessController: Brightness change cancelled, restored to:" << m_initialBrightness << "%";
    }

    hide();
    emit returnToMainMenu();
    emit brightnessFinished();
}

void BrightnessController::onUpButtonPressed()
{
    if (m_currentState == BrightnessState::Adjusting) {
        m_currentBrightnessEdit += BRIGHTNESS_STEP;
        if (m_currentBrightnessEdit > MAX_BRIGHTNESS) {
            m_currentBrightnessEdit = MAX_BRIGHTNESS;
        }
        // Apply brightness in real-time for preview
        applyBrightness(m_currentBrightnessEdit);
        updateUI();
    }
}

void BrightnessController::onDownButtonPressed()
{
    if (m_currentState == BrightnessState::Adjusting) {
        m_currentBrightnessEdit -= BRIGHTNESS_STEP;
        if (m_currentBrightnessEdit < MIN_BRIGHTNESS) {
            m_currentBrightnessEdit = MIN_BRIGHTNESS;
        }
        // Apply brightness in real-time for preview
        applyBrightness(m_currentBrightnessEdit);
        updateUI();
    }
}

void BrightnessController::applyBrightness(int percentage)
{
    // Clamp to valid range
    if (percentage < MIN_BRIGHTNESS) percentage = MIN_BRIGHTNESS;
    if (percentage > MAX_BRIGHTNESS) percentage = MAX_BRIGHTNESS;

    // Convert percentage to decimal (xrandr uses 0.0-1.0)
    double brightnessValue = percentage / 100.0;

    // Set brightness using xrandr
    QProcess process;
    process.start("xrandr", QStringList()
                  << "--output" << m_displayOutput
                  << "--brightness" << QString::number(brightnessValue, 'f', 2));
    process.waitForFinished(1000);

    if (process.exitCode() != 0) {
        qWarning() << "BrightnessController: xrandr failed with exit code:" << process.exitCode();
        qWarning() << "BrightnessController: stderr:" << process.readAllStandardError();
    } else {
        qDebug() << "BrightnessController: Brightness set to" << percentage << "% on" << m_displayOutput;
    }
}

void BrightnessController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "BrightnessController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

void BrightnessController::setViewModel(BrightnessViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void BrightnessController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}

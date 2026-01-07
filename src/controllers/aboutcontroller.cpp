#include "aboutcontroller.h"
#include "models/aboutviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

AboutController::AboutController(QObject* parent)
    : QObject(parent), m_viewModel(nullptr), m_stateModel(nullptr) {
    qDebug() << "AboutController: Constructor";
}

AboutController::~AboutController() {
    qDebug() << "AboutController: Destructor";
}

void AboutController::setViewModel(AboutViewModel* viewModel) {
    m_viewModel = viewModel;
    qDebug() << "AboutController: ViewModel set";
}

void AboutController::setStateModel(SystemStateModel* stateModel) {
    m_stateModel = stateModel;
    qDebug() << "AboutController: StateModel set";
}

void AboutController::initialize() {
    qDebug() << "AboutController::initialize()";

    if (!m_viewModel) {
        qCritical() << "AboutController: ViewModel is null!";
        return;
    }

    if (!m_stateModel) {
        qCritical() << "AboutController: StateModel is null!";
        return;
    }

    // Connect to color style changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged, this,
            &AboutController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);

    qDebug() << "AboutController initialized successfully";
}

void AboutController::show() {
    if (m_viewModel) {
        m_viewModel->setVisible(true);
        qDebug() << "AboutController: Showing About/Help dialog";
    }
}

void AboutController::hide() {
    if (m_viewModel) {
        m_viewModel->setVisible(false);
        qDebug() << "AboutController: Hiding About/Help dialog";
    }
}

void AboutController::onColorStyleChanged(const QColor& color) {
    qDebug() << "AboutController: Color changed to" << color;
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}

void AboutController::onSelectButtonPressed() {
    qDebug() << "AboutController: Select button pressed - closing dialog";
    hide();
    emit returnToMainMenu();
    emit aboutFinished();
}

void AboutController::onBackButtonPressed() {
    qDebug() << "AboutController: Back button pressed - closing dialog";
    hide();
    emit aboutFinished();
}

void AboutController::onUpButtonPressed() {
    // Could be used for scrolling if content is long
    // For now, no action
    qDebug() << "AboutController: Up button pressed (no action)";
}

void AboutController::onDownButtonPressed() {
    // Could be used for scrolling if content is long
    // For now, no action
    qDebug() << "AboutController: Down button pressed (no action)";
}

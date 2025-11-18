#include "joystickdevice.h"
#include "../protocols/JoystickProtocolParser.h"
#include "../messages/JoystickMessage.h"
#include <QDebug>
#include <cstring>

JoystickDevice::JoystickDevice(QObject* parent)
    : TemplatedDevice<JoystickData>(parent)
    , m_joystick(nullptr)
    , m_pollTimer(new QTimer(this))
    , m_parser(nullptr)
    , m_targetGUID("030000004f0400000204000011010000")  // Thrustmaster HOTAS Warthog
    , m_pollInterval(16)  // ~60Hz
    , m_sdlInitialized(false)
{
    connect(m_pollTimer, &QTimer::timeout, this, &JoystickDevice::pollJoystick);
}

JoystickDevice::~JoystickDevice()
{
    shutdown();
}

void JoystickDevice::setParser(JoystickProtocolParser* parser)
{
    m_parser = parser;
}

void JoystickDevice::setTargetGUID(const QString& guid)
{
    if (state() == DeviceState::Offline) {
        m_targetGUID = guid;
    } else {
        qWarning() << "Cannot change target GUID while device is not offline";
    }
}

void JoystickDevice::setPollInterval(int intervalMs)
{
    m_pollInterval = intervalMs;
    if (m_pollTimer->isActive()) {
        m_pollTimer->setInterval(m_pollInterval);
    }
}

bool JoystickDevice::initialize()
{
    if (state() != DeviceState::Offline) {
        qWarning() << "JoystickDevice: Already initialized";
        return false;
    }

    if (!m_parser) {
        qWarning() << "JoystickDevice: Parser not set. Call setParser() first.";
        return false;
    }

    setState(DeviceState::Initializing);

    // Initialize SDL joystick subsystem
    if (!initializeSDL()) {
        setState(DeviceState::Error);
        return false;
    }

    // Open the target joystick
    if (!openJoystick()) {
        setState(DeviceState::Error);
        return false;
    }

    // Initialize parser state
    m_parser->reset();

    // Create initial data
    auto initialData = std::make_shared<JoystickData>();
    initialData->isConnected = true;
    updateData(initialData);
    emit dataChanged(initialData);

    // Start polling
    m_pollTimer->start(m_pollInterval);

    setState(DeviceState::Online);
    qDebug() << "JoystickDevice: Initialized successfully";
    return true;
}

void JoystickDevice::shutdown()
{
    if (state() == DeviceState::Offline) {
        return;
    }

    // Stop polling
    m_pollTimer->stop();

    // Close joystick
    if (m_joystick) {
        SDL_JoystickClose(m_joystick);
        m_joystick = nullptr;
        qDebug() << "JoystickDevice: Joystick closed";
    }

    // Shutdown SDL joystick subsystem
    if (m_sdlInitialized) {
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        m_sdlInitialized = false;
        qDebug() << "JoystickDevice: SDL subsystem shut down";
    }

    // Update connection state
    auto disconnectedData = std::make_shared<JoystickData>();
    disconnectedData->isConnected = false;
    updateData(disconnectedData);
    emit dataChanged(disconnectedData);

    setState(DeviceState::Offline);
    qDebug() << "JoystickDevice: Shutdown complete";
}

void JoystickDevice::pollJoystick()
{
    if (!m_joystick || !m_parser) {
        return;
    }

    SDL_Event event;
    bool dataUpdated = false;

    // Process all pending SDL events
    while (SDL_PollEvent(&event)) {
        // Let the parser process the event
        MessagePtr message = m_parser->processEvent(event);

        if (message && message->typeId() == Message::Type::JoystickDataType) {
            auto* joystickMsg = static_cast<JoystickDataMessage*>(message.get());

            // Update device data with the new state (axis/button values)
            // Note: We do NOT emit dataChanged here because connection status hasn't changed
            // dataChanged is only for connection status tracking, not for every axis/button update
            auto newData = std::make_shared<JoystickData>(joystickMsg->data());
            newData->isConnected = true;
            updateData(newData);

            dataUpdated = true;

            // Emit backward-compatible signals for axis/button events
            emitEventSignals(event);
        }
    }
}

bool JoystickDevice::initializeSDL()
{
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        qCritical() << "JoystickDevice: Failed to initialize SDL joystick subsystem:" 
                    << SDL_GetError();
        emit deviceError(QString("SDL initialization failed: %1").arg(SDL_GetError()));
        return false;
    }

    m_sdlInitialized = true;
    qDebug() << "JoystickDevice: SDL joystick subsystem initialized";
    return true;
}

bool JoystickDevice::openJoystick()
{
    int numJoysticks = SDL_NumJoysticks();
    qDebug() << "JoystickDevice: Found" << numJoysticks << "joystick(s)";

    if (numJoysticks == 0) {
        qWarning() << "JoystickDevice: No joysticks connected";
        emit deviceError("No joysticks found");
        return false;
    }

    // Search for the target joystick by GUID
    QByteArray targetGuidBytes = m_targetGUID.toLatin1();
    const char* targetGUID = targetGuidBytes.constData();

    for (int i = 0; i < numJoysticks; ++i) {
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guidStr[33];  // 32 hex characters + null terminator
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        
        qDebug() << "JoystickDevice: Index" << i << "GUID:" << guidStr;

        // Compare with target GUID
        if (strcmp(guidStr, targetGUID) == 0) {
            m_joystick = SDL_JoystickOpen(i);
            if (!m_joystick) {
                qCritical() << "JoystickDevice: Failed to open joystick:" << SDL_GetError();
                emit deviceError(QString("Failed to open joystick: %1").arg(SDL_GetError()));
                return false;
            }

            qDebug() << "JoystickDevice: Opened joystick:" << SDL_JoystickName(m_joystick);
            qDebug() << "  Axes:" << SDL_JoystickNumAxes(m_joystick);
            qDebug() << "  Buttons:" << SDL_JoystickNumButtons(m_joystick);
            qDebug() << "  Hats:" << SDL_JoystickNumHats(m_joystick);
            return true;
        }
    }

    qWarning() << "JoystickDevice: No joystick with GUID" << m_targetGUID << "found";
    emit deviceError(QString("Target joystick not found (GUID: %1)").arg(m_targetGUID));
    return false;
}

void JoystickDevice::emitEventSignals(const SDL_Event& event)
{
    // Emit backward-compatible signals for QML/UI integration
    switch (event.type) {
        case SDL_JOYAXISMOTION:
            emit axisMoved(event.jaxis.axis, 
                          static_cast<float>(event.jaxis.value) / 32768.0f);
            break;

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            emit buttonPressed(event.jbutton.button, 
                             event.type == SDL_JOYBUTTONDOWN);
            break;

        case SDL_JOYHATMOTION:
            emit hatMoved(event.jhat.hat, event.jhat.value);
            break;

        default:
            break;
    }
}

void JoystickDevice::printJoystickGUIDs()
{
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        qCritical() << "Failed to initialize SDL:" << SDL_GetError();
        return;
    }

    int numJoysticks = SDL_NumJoysticks();
    qDebug() << "=== Connected Joysticks ===" << numJoysticks << "found";

    for (int i = 0; i < numJoysticks; ++i) {
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guidStr[33];
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        
        SDL_Joystick* joy = SDL_JoystickOpen(i);
        if (joy) {
            qDebug() << "Index:" << i;
            qDebug() << "  Name:" << SDL_JoystickName(joy);
            qDebug() << "  GUID:" << guidStr;
            qDebug() << "  Axes:" << SDL_JoystickNumAxes(joy);
            qDebug() << "  Buttons:" << SDL_JoystickNumButtons(joy);
            qDebug() << "  Hats:" << SDL_JoystickNumHats(joy);
            SDL_JoystickClose(joy);
        }
    }

    qDebug() << "===========================";
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

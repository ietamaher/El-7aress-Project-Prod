#ifndef TRPSCANMOTIONMODE_H
#define TRPSCANMOTIONMODE_H


#include "gimbalmotionmodebase.h"
#include "models/domain/systemstatemodel.h"
#include <QVector>

/*struct TargetReferencePoint {
    int id = 0;
    int locationPage = 0;
    int trpInPage = 0;
    double azimuth = 0.0;
    double elevation = 0.0;
    double haltTime = 0.0;
};
*/
class TRPScanMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT

public:
    explicit TRPScanMotionMode(QObject* parent = nullptr);
    ~TRPScanMotionMode() override = default;

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;

    void setTrpList(const QVector<TargetReferencePoint>& trps);

protected:
    /**
     * @brief TRP scan motion mode implementation.
     *
     * Implements Target Reference Point scanning with hold times.
     * Safety checks are handled by base class updateWithSafety().
     *
     * @param controller Pointer to the GimbalController
     * @param dt Time delta in seconds since last update
     */
    void updateImpl(GimbalController* controller, double dt) override;
    void setTrpList(const std::vector<TargetReferencePoint>& trps);
    bool selectPage(int locationPage);

private:
    enum State {
        SlewToPoint,
        HoldPoint
    };

    State m_state = SlewToPoint;

    QVector<TargetReferencePoint> m_trps;
    QVector<int> m_pageOrder;   // indexes of TRPs in this page

    // PID controllers
    PIDController m_azPid, m_elPid;

    int m_currentIndex = 0;     // index inside pageOrder
    double m_targetAz = 0.0;
    double m_targetEl = 0.0;
    double m_holdRemaining = 0.0;

    double m_prevAzVel = 0.0;

    static double norm360(double a);
    static double shortestDiff(double t, double c);

    void startCurrentTarget();
    void advanceToNextPoint();
};

#endif // TRPSCANMOTIONMODE_H

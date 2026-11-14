#pragma once

#include <QWidget>
#include <QString>

// Qt forward declarations
class QLabel;
class QPushButton;
class QFrame;

class CommandCenterWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CommandCenterWidget(QWidget *parent = nullptr);

    void setNetworkState(bool online,
                         const QString &adapter,
                         const QString &ip,
                         const QString &gateway);

    void setDeviceCount(int count);

    void setProfileCount(int count);

    void setRouterState(bool inspected,
                        bool authenticated,
                        const QString &manufacturer,
                        const QString &model,
                        const QString &message);

    void setCaptureState(const QString &message);

    void addActivity(const QString &message);

signals:
    void discoverNetworkRequested();
    void scanDevicesRequested();
    void openRouterSettingsRequested();
    void openDevicesRequested();
    void openProfilesRequested();
    void openCaptureRequested();
    void openLiveMonitorRequested();
    void openWirelessIntelligenceRequested();

private:
    QLabel *networkValue = nullptr;
    QLabel *devicesValue = nullptr;
    QLabel *routerValue = nullptr;
    QLabel *profilesValue = nullptr;

    QLabel *networkDetails = nullptr;
    QLabel *routerDetails = nullptr;
    QLabel *captureDetails = nullptr;
    QLabel *activityDetails = nullptr;

    QPushButton *discoverButton = nullptr;

    QFrame *createMetricCard(const QString &title,
                             const QString &value,
                             const QString &description,
                             QLabel **valueLabel);

    void buildInterface();
};
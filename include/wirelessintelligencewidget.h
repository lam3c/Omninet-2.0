#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

class WirelessIntelligenceWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WirelessIntelligenceWidget(QWidget *parent = nullptr);

public slots:
    void setRouterSettings(const QJsonObject &settings);
    void setRouterAuthenticated(bool authenticated);

private slots:
    void scanNetworks();
    void refreshConnection();

private:
    void buildInterface();
    void loadAvailableNetworks();

    QString estimateDistance(double rssiDbm,
                             double frequencyMHz) const;

    void inspectRouterSettings(const QJsonObject &settings);

    QLabel *modeValue = nullptr;
    QLabel *masterSsidValue = nullptr;
    QLabel *masterSignalValue = nullptr;
    QLabel *masterRateValue = nullptr;
    QLabel *connectedSsidValue = nullptr;
    QLabel *bssidValue = nullptr;
    QLabel *signalValue = nullptr;
    QLabel *rxRateValue = nullptr;
    QLabel *txRateValue = nullptr;
    QLabel *distanceValue = nullptr;
    QLabel *frequencyValue = nullptr;
    QLabel *scanStatus = nullptr;
    QLabel *routerApiStatus = nullptr;

    QPushButton *scanButton = nullptr;
    QTableWidget *networksTable = nullptr;
    QTimer *refreshTimer = nullptr;

    QString currentSsid;
    double currentRssi = -100.0;
    double currentFrequencyMHz = 0.0;

    bool routerAuthenticated = false;
};
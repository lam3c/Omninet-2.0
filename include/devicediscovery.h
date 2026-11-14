#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QThreadPool>
#include <QtGlobal>

struct NetworkDevice
{
    QString ipAddress;
    QString macAddress;
    QString hostName;
    QString status;
};

class DeviceDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDiscovery(QObject *parent = nullptr);
    ~DeviceDiscovery() override;

    void scanNetwork(const QString &localIp,
                     const QString &subnetMask);

    void cancelScan();

signals:
    void scanStarted();
    void deviceFound(const NetworkDevice &device);
    void scanFinished(const QList<NetworkDevice> &devices);
    void scanProgress(const QString &message);

private:
    QThreadPool threadPool;

    QList<NetworkDevice> discoveredDevices;

    quint64 currentScanId = 0;
    int pendingTasks = 0;

    bool scanActive = false;

    void finishScanIfReady();
};
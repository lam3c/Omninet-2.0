#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkAccessManager>

#include "authenticationdetector.h"
#include "networkdetector.h"

// NetworkDetails was identical to NetworkInformation, so it is now an alias.
// Existing code that uses NetworkDetails keeps compiling.
using NetworkDetails = NetworkInformation;

struct RouterPageResult
{
    QString url;
    QString title;
    QString server;
    int statusCode = 0;
    bool reachable = false;
    bool loginFormDetected = false;
    QString error;

    // Authentication details extracted from the login page.
    AuthenticationInfo authentication;
};

class RouterDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit RouterDiscovery(QObject *parent = nullptr);

    // Finds the active adapter, local IPv4 address and default gateway.
    bool discoverNetwork(NetworkDetails &details, QString &error);

    // Inspects HTTP and HTTPS management pages without logging in.
    void inspectGateway(const QString &gatewayIp);

signals:
    void networkDiscovered(const NetworkDetails &details);

    void inspectionFinished(const QList<RouterPageResult> &results);

    void inspectionProgress(const QString &message);

private:
    QNetworkAccessManager networkManager;

    QList<RouterPageResult> pageResults;
    int pendingRequests = 0;

    void inspectUrl(const QString &url);
};
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct GatewayInformation
{
    QString gatewayIp;
    QString url;
    QString pageTitle;
    QString server;
    QString html;
    int httpStatus = 0;
    bool reachable = false;
};

class GatewayInspector : public QObject
{
    Q_OBJECT

public:
    explicit GatewayInspector(QObject *parent = nullptr);

    void inspect(const QString &gatewayIp);

signals:
    void inspectionProgress(const QString &message);
    void inspectionFinished(const GatewayInformation &information);

private:
    QNetworkAccessManager *networkManager = nullptr;

    void inspectUrl(const QString &url);
    void processReply(QNetworkReply *reply, const QString &url);
};
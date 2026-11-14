
#include "gatewayinspector.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QRegularExpression>

GatewayInspector::GatewayInspector(QObject *parent)
    : QObject(parent),
    networkManager(new QNetworkAccessManager(this))
{
}

void GatewayInspector::inspect(const QString &gatewayIp)
{
    QUrl testUrl("http://" + gatewayIp);

    if (!testUrl.isValid() || testUrl.host().isEmpty())
    {
        GatewayInformation information;
        information.gatewayIp = gatewayIp;

        emit inspectionFinished(information);
        return;
    }

    emit inspectionProgress(
        "Checking router management page at " + gatewayIp);

    inspectUrl("http://" + gatewayIp);
}

void GatewayInspector::inspectUrl(const QString &url)
{
    QNetworkRequest request{QUrl(url)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0");

    request.setTransferTimeout(8000);

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url]()
            {
                processReply(reply, url);
            });
}

void GatewayInspector::processReply(
    QNetworkReply *reply,
    const QString &url)
{
    GatewayInformation information;

    information.url = url;
    information.gatewayIp = reply->url().host();

    information.httpStatus =
        reply->attribute(
                 QNetworkRequest::HttpStatusCodeAttribute).toInt();

    information.server =
        reply->header(QNetworkRequest::ServerHeader).toString();

    information.html =
        QString::fromUtf8(reply->readAll());

    information.reachable =
        reply->error() == QNetworkReply::NoError;

    // Extract the HTML page title.
    QRegularExpression titleRegex(
        "<title[^>]*>(.*?)</title>",
        QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch match =
        titleRegex.match(information.html);

    if (match.hasMatch())
    {
        information.pageTitle =
            match.captured(1).trimmed();
    }

    if (information.reachable)
    {
        emit inspectionProgress(
            "Router management page responded successfully.");
    }
    else
    {
        emit inspectionProgress(
            "HTTP inspection failed: " +
            reply->errorString());
    }

    emit inspectionFinished(information);

    reply->deleteLater();
}

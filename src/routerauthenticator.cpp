
#include "routerauthenticator.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>

// --------------------------------------------------
// Constructor
// --------------------------------------------------

RouterAuthenticator::RouterAuthenticator(QObject *parent)
    : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
}

// --------------------------------------------------
// Authentication
// --------------------------------------------------

void RouterAuthenticator::authenticate(
    const QUrl &routerUrl,
    const QString &password)
{
    if (authenticationInProgress) {
        emit authenticationFinished(
            false,
            "An authentication attempt is already running."
            );
        return;
    }

    if (!routerUrl.isValid() || routerUrl.host().isEmpty()) {
        emit authenticationFinished(
            false,
            "The router address is invalid."
            );
        return;
    }

    if (password.isEmpty()) {
        emit authenticationFinished(
            false,
            "Enter the router password."
            );
        return;
    }

    baseUrl = routerUrl;
    baseUrl.setPath("/");
    baseUrl.setQuery(QString());
    baseUrl.setFragment({});

    sessionAuthenticated = false;

    pendingPassword = password;
    randomKey.clear();

    authenticationInProgress = true;

    emit progressChanged(
        "Requesting a fresh authentication key..."
        );

    requestRandomKey();
}

// --------------------------------------------------
// Request authentication key
// --------------------------------------------------

void RouterAuthenticator::requestRandomKey()
{
    QUrl endpoint = baseUrl;
    endpoint.setPath("/goform/getstok");

    QNetworkRequest request(endpoint);

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0"
        );

    request.setTransferTimeout(10000);

    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    activeReply = networkManager->get(request);

    connect(activeReply, &QNetworkReply::finished,
            this, [this]() {

                QNetworkReply *reply = activeReply;
                activeReply = nullptr;

                if (!reply) {
                    finishAuthentication(
                        false,
                        "The authentication-key request failed."
                        );
                    return;
                }

                const QByteArray response = reply->readAll();

                const QNetworkReply::NetworkError networkError =
                    reply->error();

                const int statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute
                             ).toInt();

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    finishAuthentication(
                        false,
                        "Could not retrieve the router authentication key: "
                            + QString::number(statusCode)
                        );
                    return;
                }

                QJsonParseError parseError;

                const QJsonDocument document =
                    QJsonDocument::fromJson(response, &parseError);

                if (parseError.error != QJsonParseError::NoError ||
                    !document.isObject()) {

                    finishAuthentication(
                        false,
                        "The router returned an unexpected key response."
                        );
                    return;
                }

                randomKey =
                    document.object().value("random").toString();

                if (randomKey.isEmpty()) {
                    finishAuthentication(
                        false,
                        "The router response did not contain a random key."
                        );
                    return;
                }

                emit progressChanged(
                    "Authentication key received."
                    );

                submitPasswordHash();
            });
}

// --------------------------------------------------
// Submit password hash
// --------------------------------------------------

void RouterAuthenticator::submitPasswordHash()
{
    emit progressChanged(
        "Preparing the router password response..."
        );

    const QByteArray passwordBytes =
        pendingPassword.toUtf8();

    const QByteArray encodedPassword =
        passwordBytes.toBase64();

    const QByteArray hashInput =
        encodedPassword + randomKey.toUtf8();

    const QByteArray passwordHash =
        QCryptographicHash::hash(
            hashInput,
            QCryptographicHash::Md5
            ).toHex();

    pendingPassword.clear();

    QUrl endpoint = baseUrl;
    endpoint.setPath("/login/Auth");

    QNetworkRequest request(endpoint);

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/x-www-form-urlencoded"
        );

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0"
        );

    request.setTransferTimeout(10000);

    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    const QByteArray postData =
        QByteArray("password=") + passwordHash;

    emit progressChanged(
        "Submitting authentication response..."
        );

    activeReply = networkManager->post(
        request,
        postData
        );

    connect(activeReply, &QNetworkReply::finished,
            this, [this]() {

                QNetworkReply *reply = activeReply;
                activeReply = nullptr;

                if (!reply) {
                    finishAuthentication(
                        false,
                        "The router login request failed."
                        );
                    return;
                }

                const QNetworkReply::NetworkError networkError =
                    reply->error();

                const int statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute
                             ).toInt();

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    finishAuthentication(
                        false,
                        "The router rejected or could not process the login request. "
                        "HTTP status: " + QString::number(statusCode)
                        );
                    return;
                }

                emit progressChanged(
                    "Login response received. Checking the session..."
                    );

                verifySession();
            });
}

// --------------------------------------------------
// Verify authenticated session
// --------------------------------------------------

void RouterAuthenticator::verifySession()
{
    QUrl endpoint = baseUrl;
    endpoint.setPath("/index.html");

    QNetworkRequest request(endpoint);

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0"
        );

    request.setTransferTimeout(10000);

    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    activeReply = networkManager->get(request);

    connect(activeReply, &QNetworkReply::finished,
            this, [this]() {

                QNetworkReply *reply = activeReply;
                activeReply = nullptr;

                if (!reply) {
                    finishAuthentication(
                        false,
                        "Could not verify the router session."
                        );
                    return;
                }

                const QByteArray page = reply->readAll();

                const QNetworkReply::NetworkError networkError =
                    reply->error();

                const int statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute
                             ).toInt();

                const QUrl finalUrl = reply->url();

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    finishAuthentication(
                        false,
                        "The login response was received, but session verification "
                        "failed. HTTP status: " + QString::number(statusCode)
                        );
                    return;
                }

                if (isLoginPage(page) ||
                    finalUrl.path().contains(
                        "login",
                        Qt::CaseInsensitive
                        )) {

                    finishAuthentication(
                        false,
                        "The router is still showing its login page. "
                        "Check the password and try again."
                        );
                    return;
                }

                if (page.isEmpty()) {
                    finishAuthentication(
                        false,
                        "The router returned an empty verification page. "
                        "Authentication could not be confirmed."
                        );
                    return;
                }

                finishAuthentication(
                    true,
                    "The router session appears to be authenticated."
                    );
            });
}

// --------------------------------------------------
// Check for login page
// --------------------------------------------------

bool RouterAuthenticator::isLoginPage(
    const QByteArray &html)
{
    const QByteArray lowerHtml = html.toLower();

    const bool hasLoginForm =
        lowerHtml.contains("<form") &&
        lowerHtml.contains("login-password");

    const bool hasLoginTitle =
        lowerHtml.contains("tenda | login");

    return hasLoginForm || hasLoginTitle;
}

// --------------------------------------------------
// Finish authentication
// --------------------------------------------------

void RouterAuthenticator::finishAuthentication(
    bool success,
    const QString &message)
{
    authenticationInProgress = false;

    sessionAuthenticated = success;

    pendingPassword.clear();
    randomKey.clear();

    emit progressChanged(message);

    emit authenticationFinished(
        success,
        message
        );
}

// --------------------------------------------------
// Retrieve Wi-Fi settings
// --------------------------------------------------

void RouterAuthenticator::getWifiSettings()
{
    if (!sessionAuthenticated) {
        emit wifiSettingsFailed(
            "Authenticate with the router before requesting Wi-Fi settings."
            );
        return;
    }

    if (activeReply) {
        emit wifiSettingsFailed(
            "Another router request is already running."
            );
        return;
    }

    QUrl endpoint = baseUrl;
    endpoint.setPath("/goform/getWifi");

    QUrlQuery query;

    // Cache-busting parameter used by the observed router endpoint.
    query.addQueryItem(
        "random",
        QString::number(QDateTime::currentMSecsSinceEpoch())
        );

    // Request the wireless configuration modules.
    query.addQueryItem(
        "modules",
        "wifiEn,wifiBasicCfg,wifiAdvCfg,wifiPower,"
        "wifiTime,wifiWPS,wifiVirSsid"
        );

    endpoint.setQuery(query);

    QNetworkRequest request(endpoint);

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0"
        );

    request.setRawHeader(
        "Accept",
        "application/json"
        );

    request.setTransferTimeout(15000);

    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    emit progressChanged(
        "Retrieving router Wi-Fi settings..."
        );

    activeReply = networkManager->get(request);

    connect(activeReply, &QNetworkReply::finished,
            this, [this]() {

                QNetworkReply *reply = activeReply;
                activeReply = nullptr;

                if (!reply) {
                    emit wifiSettingsFailed(
                        "The Wi-Fi settings request failed."
                        );
                    return;
                }

                const QByteArray response = reply->readAll();

                const QNetworkReply::NetworkError networkError =
                    reply->error();

                const int statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute
                             ).toInt();

                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    emit wifiSettingsFailed(
                        "Could not retrieve Wi-Fi settings. HTTP status: "
                        + QString::number(statusCode)
                        );
                    return;
                }

                QJsonParseError parseError;

                const QJsonDocument document =
                    QJsonDocument::fromJson(
                        response,
                        &parseError
                        );

                if (parseError.error != QJsonParseError::NoError ||
                    !document.isObject()) {

                    emit wifiSettingsFailed(
                        "The router returned an invalid Wi-Fi settings response."
                        );
                    return;
                }

                const QJsonObject settings =
                    document.object();

                if (settings.isEmpty()) {
                    emit wifiSettingsFailed(
                        "The router returned empty Wi-Fi settings."
                        );
                    return;
                }

                emit progressChanged(
                    "Wi-Fi settings retrieved successfully."
                    );

                emit wifiSettingsReceived(settings);
            });
}

// --------------------------------------------------
// Cancel
// --------------------------------------------------

void RouterAuthenticator::cancel()
{
    if (activeReply) {
        activeReply->abort();
        activeReply = nullptr;
    }

    pendingPassword.clear();
    randomKey.clear();

    authenticationInProgress = false;
    sessionAuthenticated = false;

    emit progressChanged(
        "Authentication cancelled."
        );
}

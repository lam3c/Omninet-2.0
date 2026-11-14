#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QByteArray>
#include <QJsonObject>

#include <QNetworkAccessManager>
#include <QNetworkReply>

class RouterAuthenticator : public QObject
{
    Q_OBJECT

public:
    explicit RouterAuthenticator(QObject *parent = nullptr);

    void authenticate(const QUrl &routerUrl,
                      const QString &password);

    void cancel();

    // Retrieves Wi-Fi settings using the authenticated session.
    void getWifiSettings();

    QNetworkAccessManager *sessionManager() const { return networkManager; }

signals:
    void progressChanged(const QString &message);

    void authenticationFinished(bool success,
                                const QString &message);

    // Emitted when Wi-Fi settings are successfully retrieved.
    void wifiSettingsReceived(const QJsonObject &settings);

    // Emitted when Wi-Fi settings cannot be retrieved.
    void wifiSettingsFailed(const QString &message);

private:
    QNetworkAccessManager *networkManager = nullptr;

    QUrl baseUrl;

    QString pendingPassword;
    QString randomKey;

    QNetworkReply *activeReply = nullptr;

    bool authenticationInProgress = false;
    bool sessionAuthenticated = false;

    void requestRandomKey();
    void submitPasswordHash();
    void verifySession();

    void finishAuthentication(bool success,
                              const QString &message);

    static bool isLoginPage(const QByteArray &html);
};
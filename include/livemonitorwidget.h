#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QWidget>

class QLabel;
class QFrame;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;

class SpeedGraph;

// Live traffic dashboard:
//  - graphs of router download/upload (sum of all devices)
//  - graph of this computer's download/upload
//  - per-device speed, session usage and bandwidth share
class LiveMonitorWidget : public QWidget
{
public:
    explicit LiveMonitorWidget(QWidget *parent = nullptr);

    // Call after the router login succeeds. The manager must be the
    // authenticated one (RouterAuthenticator::sessionManager()).
    void setSession(QNetworkAccessManager *sessionManager,
                    const QUrl &routerBaseUrl);

    void start();
    void stop();

private:
    struct DeviceSpeed
    {
        QString name;
        QString ip;
        QString mac;
        double down = 0;   // bytes per second
        double up = 0;     // bytes per second
    };

    void poll();
    void pollLocal();
    void pollRouter();
    void handleRouterReply(QNetworkReply *reply);
    void applyDevices(QVector<DeviceSpeed> devices);

    void setStatus(const QString &text, bool warning);
    void setCell(int row, int column, const QString &text);

    QFrame *makeCard(const QString &title, QLabel *&value, QLabel *&note);
    QFrame *makeGraphCard(const QString &titleHtml, SpeedGraph *graph);

    QNetworkAccessManager *manager = nullptr;
    QUrl baseUrl;
    QTimer *timer = nullptr;

    bool routerRequestActive = false;
    int routerFailures = 0;

    QElapsedTimer routerClock;
    bool routerClockStarted = false;

    QElapsedTimer localClock;
    bool haveLocal = false;
    quint64 lastIn = 0;
    quint64 lastOut = 0;

    double peakDown = 0;
    double peakUp = 0;
    QHash<QString, double> sessionUsed;   // bytes per device this session

    SpeedGraph *routerGraph = nullptr;
    SpeedGraph *localGraph = nullptr;

    QLabel *downValue = nullptr;
    QLabel *downNote = nullptr;
    QLabel *upValue = nullptr;
    QLabel *upNote = nullptr;
    QLabel *devicesValue = nullptr;
    QLabel *devicesNote = nullptr;
    QLabel *pcValue = nullptr;
    QLabel *pcNote = nullptr;
    QLabel *statusLabel = nullptr;

    QTableWidget *table = nullptr;
    QPlainTextEdit *rawView = nullptr;
    QPushButton *rawButton = nullptr;
};
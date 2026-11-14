#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QSet>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class RouterCapture : public QObject
{
    Q_OBJECT

public:
    explicit RouterCapture(QObject *parent = nullptr);

    void start(QNetworkAccessManager *manager,
               const QUrl &url,
               bool maskMacs = true);

    void cancel();

    bool isRunning() const;

signals:
    void progress(int done,
                  int total,
                  const QString &message);

    void finished(const QString &report,
                  bool cancelled);

private slots:
    void processNext();
    void onReplyFinished();

private:
    struct Task
    {
        QUrl url;
        int depth = 0;
    };

    void enqueue(const QUrl &url, int depth);
    void analyseHtml(const QString &html, const QUrl &pageUrl, int depth);
    void scanEndpoints(const QString &html, const QUrl &pageUrl, int depth);
    bool isSafeUrl(const QUrl &url) const;
    bool isSensitiveUrl(const QUrl &url) const;
    QString safeUrl(const QUrl &url) const;
    QString redact(const QString &text) const;
    void finish();
    QString buildReport() const;

    QNetworkAccessManager *networkManager = nullptr;
    QNetworkReply *activeReply = nullptr;

    QUrl baseUrl;

    QQueue<Task> queue;
    QSet<QString> visited;

    QStringList findings;
    QStringList endpoints;

    int completed = 0;
    int total = 0;

    static constexpr int MaxPages = 40;
    static constexpr int MaxDepth = 2;

    bool maskMacAddresses = true;
    bool running = false;
    bool cancelled = false;
    int currentDepth = 0;
};
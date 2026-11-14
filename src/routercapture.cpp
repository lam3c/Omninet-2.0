#include "routercapture.h"

#include <QDateTime>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrlQuery>

RouterCapture::RouterCapture(QObject *parent)
    : QObject(parent)
{
}

bool RouterCapture::isRunning() const
{
    return running;
}

// --------------------------------------------------
// START CAPTURE
// --------------------------------------------------

void RouterCapture::start(QNetworkAccessManager *manager,
                          const QUrl &url,
                          bool maskMacs)
{
    if (running)
        return;

    if (!manager || !url.isValid() || url.host().isEmpty()) {
        findings.clear();
        findings.append(
            "Capture could not start: invalid network manager or router URL."
            );

        baseUrl = url;
        cancelled = false;
        completed = 0;
        total = 0;

        emit finished(buildReport(), false);
        return;
    }

    networkManager = manager;
    baseUrl = url.adjusted(QUrl::RemoveFragment);
    maskMacAddresses = maskMacs;

    queue.clear();
    visited.clear();
    findings.clear();
    endpoints.clear();

    completed = 0;
    total = 0;

    cancelled = false;
    running = true;

    // Initial pages commonly used by router interfaces.
    const QStringList commonPaths = {
        "/",
        "/index.html",
        "/login.html",
        "/login.htm",
        "/status.html",
        "/status.htm",
        "/system.html",
        "/network.html",
        "/wireless.html",
        "/wan.html",
        "/lan.html",
        "/dhcp.html",
        "/device.html",
        "/administration.html",
        "/management.html"
    };

    enqueue(baseUrl, 0);

    for (const QString &path : commonPaths) {
        enqueue(baseUrl.resolved(QUrl(path)), 1);
    }

    if (queue.isEmpty()) {
        findings.append("No eligible pages were found.");
        finish();
        return;
    }

    processNext();
}

// --------------------------------------------------
// CANCEL
// --------------------------------------------------

void RouterCapture::cancel()
{
    if (!running)
        return;

    cancelled = true;

    if (activeReply) {
        activeReply->abort();
    } else {
        finish();
    }
}

// --------------------------------------------------
// URL VALIDATION
// --------------------------------------------------

bool RouterCapture::isSafeUrl(const QUrl &url) const
{
    if (!url.isValid())
        return false;

    if (url.host().isEmpty())
        return false;

    // Stay on the same router host and protocol.
    if (url.host().compare(baseUrl.host(),
                           Qt::CaseInsensitive) != 0)
        return false;

    if (url.scheme().compare(baseUrl.scheme(),
                             Qt::CaseInsensitive) != 0)
        return false;

    const int basePort = baseUrl.port(
        baseUrl.scheme() == "https" ? 443 : 80
        );

    const int targetPort = url.port(
        url.scheme() == "https" ? 443 : 80
        );

    if (basePort != targetPort)
        return false;

    if (url.scheme() != "http" && url.scheme() != "https")
        return false;

    return true;
}

bool RouterCapture::isSensitiveUrl(const QUrl &url) const
{
    const QString path = url.path().toLower();

    static const QRegularExpression dangerousPath(
        R"((logout|log-out|reboot|restart|reset|restore|factory|upgrade|firmware|upload|delete|remove|apply|submit))",
        QRegularExpression::CaseInsensitiveOption
        );

    if (dangerousPath.match(path).hasMatch())
        return true;

    // Avoid URLs carrying authentication or session parameters.
    const QUrlQuery query(url);

    const auto items = query.queryItems(
        QUrl::FullyDecoded
        );

    static const QRegularExpression sensitiveParameter(
        R"((password|passwd|token|session|cookie|secret|auth|csrf|key))",
        QRegularExpression::CaseInsensitiveOption
        );

    for (const auto &item : items) {
        if (sensitiveParameter.match(item.first).hasMatch())
            return true;
    }

    return false;
}

QString RouterCapture::safeUrl(const QUrl &url) const
{
    QUrl safe = url;

    safe.setQuery(QString());
    safe.setFragment(QString());

    return safe.toString();
}

// --------------------------------------------------
// QUEUE MANAGEMENT
// --------------------------------------------------

void RouterCapture::enqueue(const QUrl &url, int depth)
{
    if (queue.size() + completed >= MaxPages)
        return;

    if (depth > MaxDepth)
        return;

    if (!isSafeUrl(url))
        return;

    if (isSensitiveUrl(url))
        return;

    QUrl cleanUrl = url.adjusted(QUrl::RemoveFragment);

    const QString key = cleanUrl.toString();

    if (visited.contains(key))
        return;

    visited.insert(key);

    Task task;
    task.url = cleanUrl;
    task.depth = depth;

    queue.enqueue(task);

    ++total;
}

// --------------------------------------------------
// PROCESS QUEUE
// --------------------------------------------------

void RouterCapture::processNext()
{
    if (!running)
        return;

    if (cancelled || queue.isEmpty() ||
        completed >= MaxPages) {
        finish();
        return;
    }

    const Task task = queue.dequeue();

    QNetworkRequest request(task.url);

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet Router Diagnostics"
        );

    request.setTransferTimeout(10000);

    emit progress(
        completed,
        total,
        "Requesting " + safeUrl(task.url)
        );

    // Read-only GET request. No POST, PUT or DELETE requests.
    activeReply = networkManager->get(request);

    connect(
        activeReply,
        &QNetworkReply::finished,
        this,
        &RouterCapture::onReplyFinished
        );

    // Store depth for the page currently being processed.
    currentDepth = task.depth;
}

// --------------------------------------------------
// HANDLE RESPONSE
// --------------------------------------------------

void RouterCapture::onReplyFinished()
{
    if (!activeReply)
        return;

    QNetworkReply *reply = activeReply;
    activeReply = nullptr;

    if (reply->error() == QNetworkReply::NoError) {

        const QByteArray response = reply->readAll();

        const QString html = QString::fromUtf8(response);

        const QUrl responseUrl = reply->url();

        findings.append(
            "Captured page: " + safeUrl(responseUrl)
            );

        const QString server = reply->header(
                                        QNetworkRequest::ServerHeader
                                        ).toString();

        if (!server.isEmpty()) {
            findings.append(
                "HTTP server: " + redact(server)
                );
        }

        analyseHtml(html, responseUrl, currentDepth);
    }
    else if (!cancelled) {

        findings.append(
            "Request failed: " +
            safeUrl(reply->url()) +
            " | " +
            reply->errorString()
            );
    }

    reply->deleteLater();

    ++completed;

    emit progress(
        completed,
        total,
        cancelled
            ? "Cancelling capture..."
            : "Processed " + QString::number(completed) + " page(s)"
        );

    if (cancelled) {
        finish();
        return;
    }

    processNext();
}

// --------------------------------------------------
// HTML ANALYSIS
// --------------------------------------------------

void RouterCapture::analyseHtml(const QString &html,
                                const QUrl &pageUrl,
                                int depth)
{
    if (html.isEmpty())
        return;

    static const QRegularExpression titleRegex(
        R"(<title[^>]*>(.*?)</title>)",
        QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::DotMatchesEverythingOption
        );

    const auto titleMatch = titleRegex.match(html);

    if (titleMatch.hasMatch()) {

        const QString title =
            titleMatch.captured(1).simplified();

        if (!title.isEmpty()) {
            findings.append(
                "Page title: " + redact(title)
                );
        }
    }

    const QStringList keywords = {
        "wireless",
        "ssid",
        "gateway",
        "dhcp",
        "wan",
        "lan",
        "firmware",
        "administration",
        "management",
        "network",
        "device"
    };

    for (const QString &keyword : keywords) {

        if (html.contains(keyword, Qt::CaseInsensitive)) {

            const QString finding =
                "Interface reference detected: " + keyword;

            if (!findings.contains(finding))
                findings.append(finding);
        }
    }

    scanEndpoints(html, pageUrl, depth);
}

// --------------------------------------------------
// ENDPOINT DISCOVERY
// --------------------------------------------------

void RouterCapture::scanEndpoints(const QString &html,
                                  const QUrl &pageUrl,
                                  int depth)
{
    static const QRegularExpression linkRegex(
        R"((?:href|src)\s*=\s*["']([^"']+)["'])",
        QRegularExpression::CaseInsensitiveOption
        );

    auto iterator = linkRegex.globalMatch(html);

    while (iterator.hasNext()) {

        const auto match = iterator.next();

        const QString value =
            match.captured(1).trimmed();

        if (value.isEmpty())
            continue;

        if (value.startsWith("#") ||
            value.startsWith("javascript:", Qt::CaseInsensitive) ||
            value.startsWith("mailto:", Qt::CaseInsensitive) ||
            value.startsWith("data:", Qt::CaseInsensitive)) {
            continue;
        }

        const QUrl discovered =
            pageUrl.resolved(QUrl(value));

        if (!isSafeUrl(discovered))
            continue;

        if (isSensitiveUrl(discovered))
            continue;

        const QString endpoint = safeUrl(discovered);

        if (!endpoints.contains(endpoint))
            endpoints.append(endpoint);

        enqueue(discovered, depth + 1);
    }
}

// --------------------------------------------------
// REPORT REDACTION
// --------------------------------------------------

QString RouterCapture::redact(const QString &text) const
{
    QString result = text;

    // Mask MAC addresses when requested.
    if (maskMacAddresses) {

        static const QRegularExpression macRegex(
            R"(\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b)"
            );

        result.replace(
            macRegex,
            "[REDACTED MAC ADDRESS]"
            );
    }

    // Redact common credential assignments in captured text.
    static const QRegularExpression credentialRegex(
        R"((password|passwd|token|secret|authorization|cookie)\s*[:=]\s*["']?[^&\s"'<>]+)",
        QRegularExpression::CaseInsensitiveOption
        );

    result.replace(
        credentialRegex,
        "\\1=[REDACTED]"
        );

    return result;
}

// --------------------------------------------------
// REPORT GENERATION
// --------------------------------------------------

QString RouterCapture::buildReport() const
{
    QString report;

    QTextStream stream(&report);

    stream << "============================================\n";
    stream << "                 OMNINET\n";
    stream << "             ROUTER CAPTURE REPORT\n";
    stream << "============================================\n\n";

    stream << "Capture date: "
           << QDateTime::currentDateTime().toString()
           << "\n";

    stream << "Router address: "
           << safeUrl(baseUrl)
           << "\n";

    stream << "Capture status: "
           << (cancelled ? "Cancelled" : "Completed")
           << "\n";

    stream << "Pages processed: "
           << completed
           << " / "
           << total
           << "\n";

    stream << "MAC masking: "
           << (maskMacAddresses ? "Enabled" : "Disabled")
           << "\n\n";

    stream << "--------------------------------------------\n";
    stream << "CAPTURE FINDINGS\n";
    stream << "--------------------------------------------\n";

    if (findings.isEmpty()) {
        stream << "No findings recorded.\n";
    }

    for (const QString &finding : findings) {
        stream << "- " << redact(finding) << "\n";
    }

    stream << "\n--------------------------------------------\n";
    stream << "DISCOVERED ENDPOINTS\n";
    stream << "--------------------------------------------\n";

    if (endpoints.isEmpty()) {
        stream << "No endpoints discovered.\n";
    }

    for (const QString &endpoint : endpoints) {
        stream << "- " << redact(endpoint) << "\n";
    }

    stream << "\n============================================\n";
    stream << "             END OF REPORT\n";
    stream << "============================================\n";

    return report;
}

// --------------------------------------------------
// FINISH
// --------------------------------------------------

void RouterCapture::finish()
{
    if (!running)
        return;

    running = false;

    emit finished(
        buildReport(),
        cancelled
        );
}
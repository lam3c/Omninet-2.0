#include "livemonitorwidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#endif

namespace
{
constexpr int kPollMs = 2000;     // refresh interval
constexpr int kHistory = 90;      // samples on the graphs (3 minutes)

// The router reports device speeds as plain numbers. Tenda's own web UI shows
// KB/s, so numbers are treated as KB/s. If your speeds look 1024x too small or
// too large, change this to 1.0 (router reports bytes/s).
constexpr double kRouterUnitBytes = 1024.0;

QString formatSpeed(double bytesPerSecond)
{
    if (bytesPerSecond < 1024.0)
        return QString::number(bytesPerSecond, 'f', 0) + " B/s";

    if (bytesPerSecond < 1024.0 * 1024.0)
        return QString::number(bytesPerSecond / 1024.0, 'f', 1) + " KB/s";

    return QString::number(bytesPerSecond / (1024.0 * 1024.0), 'f', 2) + " MB/s";
}

QString formatBytes(double bytes)
{
    if (bytes < 1024.0)
        return QString::number(bytes, 'f', 0) + " B";

    if (bytes < 1024.0 * 1024.0)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";

    if (bytes < 1024.0 * 1024.0 * 1024.0)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";

    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

double niceMax(double value)
{
    const double minimum = 10.0 * 1024.0;

    if (value <= minimum)
        return minimum;

    const double power = std::pow(10.0, std::floor(std::log10(value)));
    const double n = value / power;

    const double step = n <= 1.0 ? 1.0 : n <= 2.0 ? 2.0 : n <= 5.0 ? 5.0 : 10.0;

    return step * power;
}

// ------------------------------------------------------------
// This computer's total traffic (all real network adapters)(Not 100% accurrate)
// ------------------------------------------------------------

bool readLocalCounters(quint64 &in, quint64 &out)
{
#ifdef Q_OS_WIN
    PMIB_IF_TABLE2 interfaces = nullptr;

    if (GetIfTable2(&interfaces) != NO_ERROR || interfaces == nullptr)
        return false;

    in = 0;
    out = 0;

    for (ULONG i = 0; i < interfaces->NumEntries; ++i)
    {
        const MIB_IF_ROW2 &row = interfaces->Table[i];

        if (row.OperStatus != IfOperStatusUp)
            continue;

        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        // Physical adapters only, so VPN/virtual adapters are not double counted.
        if (!row.InterfaceAndOperStatusFlags.HardwareInterface)
            continue;

        in += row.InOctets;
        out += row.OutOctets;
    }

    FreeMibTable(interfaces);
    return true;
#else
    Q_UNUSED(in);
    Q_UNUSED(out);
    return false;
#endif
}

// ------------------------------------------------------------
// Router reply parsing (tolerant: field names are matched loosely)
// ------------------------------------------------------------

QString normalise(const QString &key)
{
    QString result;

    for (const QChar c : key)
    {
        if (c.isLetterOrNumber())
            result.append(c.toLower());
    }

    return result;
}

double parseSpeed(const QJsonValue &value)
{
    if (value.isDouble())
        return value.toDouble() * kRouterUnitBytes;

    if (value.isString())
    {
        static const QRegularExpression expression(
            R"(^\s*([0-9]*\.?[0-9]+)\s*([KMGkmg]?)[Bb]?)");

        const QRegularExpressionMatch match = expression.match(value.toString());

        if (!match.hasMatch())
            return 0.0;

        const double number = match.captured(1).toDouble();
        const QString unit = match.captured(2).toUpper();

        if (unit == "K")
            return number * 1024.0;

        if (unit == "M")
            return number * 1024.0 * 1024.0;

        if (unit == "G")
            return number * 1024.0 * 1024.0 * 1024.0;

        return number * kRouterUnitBytes;
    }

    return 0.0;
}

// Finds the list of connected devices anywhere in the router's JSON.
QJsonArray findDeviceArray(const QJsonValue &value, int depth)
{
    if (depth > 4)
        return QJsonArray();

    if (value.isArray())
    {
        const QJsonArray array = value.toArray();

        if (!array.isEmpty() && array.first().isObject())
            return array;

        for (const QJsonValue &item : array)
        {
            const QJsonArray found = findDeviceArray(item, depth + 1);

            if (!found.isEmpty())
                return found;
        }

        return QJsonArray();
    }

    if (value.isObject())
    {
        const QJsonObject object = value.toObject();

        // Prefer a key called "onlineList".
        for (auto it = object.begin(); it != object.end(); ++it)
        {
            if (it.key().compare("onlineList", Qt::CaseInsensitive) == 0)
            {
                const QJsonArray found = findDeviceArray(it.value(), depth + 1);

                if (!found.isEmpty())
                    return found;
            }
        }

        for (auto it = object.begin(); it != object.end(); ++it)
        {
            const QJsonArray found = findDeviceArray(it.value(), depth + 1);

            if (!found.isEmpty())
                return found;
        }
    }

    return QJsonArray();
}

} // namespace

// ------------------------------------------------------------
// Line graph (global scope so it matches the forward declaration
// in livemonitorwidget.h)
// ------------------------------------------------------------

class SpeedGraph : public QWidget
{
public:
    explicit SpeedGraph(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void addSample(double down, double up)
    {
        downs.append(down);
        ups.append(up);

        while (downs.size() > kHistory)
        {
            downs.removeFirst();
            ups.removeFirst();
        }

        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        painter.setPen(QColor("#1D394D"));
        painter.setBrush(QColor("#091724"));
        painter.drawRoundedRect(frame, 6, 6);

        const QRectF plot = frame.adjusted(70, 12, -14, -24);

        double peak = 0.0;

        for (double v : downs)
            peak = (std::max)(peak, v);

        for (double v : ups)
            peak = (std::max)(peak, v);

        const double maxValue = niceMax(peak);

        QFont labelFont = font();
        labelFont.setPointSize(8);
        painter.setFont(labelFont);

        for (int i = 0; i <= 4; ++i)
        {
            const double y = plot.top() + plot.height() * i / 4.0;

            painter.setPen(QPen(QColor("#24465A"), 1));
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

            painter.setPen(QColor("#7891A5"));
            painter.drawText(
                QRectF(frame.left() + 4, y - 8, plot.left() - frame.left() - 10, 16),
                Qt::AlignRight | Qt::AlignVCenter,
                formatSpeed(maxValue * (4 - i) / 4.0));
        }

        painter.setPen(QColor("#7891A5"));

        painter.drawText(
            QRectF(plot.left(), plot.bottom() + 5, 120, 14),
            Qt::AlignLeft | Qt::AlignVCenter,
            QString("%1 s ago").arg(kHistory * kPollMs / 1000));

        painter.drawText(
            QRectF(plot.right() - 60, plot.bottom() + 5, 60, 14),
            Qt::AlignRight | Qt::AlignVCenter,
            "now");

        drawSeries(painter, plot, ups, maxValue, QColor("#F5B95C"));
        drawSeries(painter, plot, downs, maxValue, QColor("#38DDF0"));
    }

private:
    QVector<double> downs;
    QVector<double> ups;

    static void drawSeries(QPainter &painter,
                           const QRectF &plot,
                           const QVector<double> &values,
                           double maxValue,
                           const QColor &color)
    {
        if (values.size() < 2)
            return;

        const double step = plot.width() / (kHistory - 1);
        const int offset = kHistory - static_cast<int>(values.size());

        QPainterPath line;
        QPainterPath area;

        for (int i = 0; i < values.size(); ++i)
        {
            const double x = plot.left() + (offset + i) * step;

            const double y = plot.bottom() -
                             (std::min)(values[i], maxValue) / maxValue * plot.height();

            if (i == 0)
            {
                line.moveTo(x, y);
                area.moveTo(x, plot.bottom());
                area.lineTo(x, y);
            }
            else
            {
                line.lineTo(x, y);
                area.lineTo(x, y);
            }
        }

        area.lineTo(plot.left() + (offset + values.size() - 1) * step,
                    plot.bottom());
        area.closeSubpath();

        QColor fill = color;
        fill.setAlpha(45);

        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(area);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 2));
        painter.drawPath(line);
    }
};

// ------------------------------------------------------------
// LiveMonitorWidget
// ------------------------------------------------------------

LiveMonitorWidget::LiveMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    timer = new QTimer(this);
    timer->setInterval(kPollMs);

    connect(timer, &QTimer::timeout, this, [this]() { poll(); });

    // Scrollable content so the page works on small windows.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget;
    scroll->setWidget(content);

    // QScrollArea turns auto-fill on; turn it off again for the dark theme.
    content->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);

    outer->addWidget(scroll);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 10, 0);
    layout->setSpacing(12);

    // ---- Summary cards ----
    auto *cards = new QHBoxLayout;
    cards->setSpacing(12);

    cards->addWidget(makeCard("ROUTER DOWNLOAD", downValue, downNote));
    cards->addWidget(makeCard("ROUTER UPLOAD", upValue, upNote));
    cards->addWidget(makeCard("DEVICES ONLINE", devicesValue, devicesNote));
    cards->addWidget(makeCard("THIS COMPUTER", pcValue, pcNote));

    layout->addLayout(cards);

    // ---- Status line + raw data toggle ----
    auto *statusRow = new QHBoxLayout;

    statusLabel = new QLabel;
    statusLabel->setObjectName("mutedText");
    statusLabel->setWordWrap(true);

    rawButton = new QPushButton("Show raw router data");
    rawButton->setCursor(Qt::PointingHandCursor);

    statusRow->addWidget(statusLabel, 1);
    statusRow->addWidget(rawButton);

    layout->addLayout(statusRow);

    // ---- Graphs ----
    routerGraph = new SpeedGraph;
    localGraph = new SpeedGraph;

    const QString legend =
        "<span style='color:#38DDF0'>&#9632; Download</span>"
        "&nbsp;&nbsp;&nbsp;"
        "<span style='color:#F5B95C'>&#9632; Upload</span>";

    auto *graphs = new QHBoxLayout;
    graphs->setSpacing(12);

    graphs->addWidget(
        makeGraphCard("Router traffic (all devices) &nbsp; " + legend, routerGraph),
        1);

    graphs->addWidget(
        makeGraphCard("This computer &nbsp; " + legend, localGraph),
        1);

    layout->addLayout(graphs);

    // ---- Device table ----
    auto *tableCard = new QFrame;
    tableCard->setObjectName("card");

    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(18, 16, 18, 16);
    tableLayout->setSpacing(10);

    auto *tableTitle = new QLabel("Devices using bandwidth");
    tableTitle->setObjectName("sectionTitle");

    table = new QTableWidget(0, 7);

    table->setHorizontalHeaderLabels({
        "DEVICE", "IP ADDRESS", "MAC ADDRESS",
        "DOWNLOAD", "UPLOAD", "USED (SESSION)", "SHARE"
    });

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(38);
    table->setMinimumHeight(260);

    tableLayout->addWidget(tableTitle);
    tableLayout->addWidget(table);

    layout->addWidget(tableCard);

    // ---- Raw data (hidden until requested) ----
    rawView = new QPlainTextEdit;
    rawView->setReadOnly(true);
    rawView->setMinimumHeight(160);
    rawView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    rawView->setPlaceholderText("The router's last reply will appear here.");
    rawView->hide();

    layout->addWidget(rawView);
    layout->addStretch();

    connect(rawButton, &QPushButton::clicked, this, [this]()
            {
                const bool show = !rawView->isVisible();
                rawView->setVisible(show);
                rawButton->setText(show ? "Hide raw router data"
                                        : "Show raw router data");
            });

    downNote->setText("Peak: —");
    upNote->setText("Peak: —");
    devicesNote->setText("Sign in to see devices");
    pcNote->setText("Upload: —");

    setStatus("Sign in to your router (Dashboard, Discover Network) to see "
              "per-device traffic. This computer's traffic is live now.",
              false);
}

QFrame *LiveMonitorWidget::makeCard(const QString &title,
                                    QLabel *&value,
                                    QLabel *&note)
{
    auto *card = new QFrame;
    card->setObjectName("card");

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("cardTitle");

    value = new QLabel("—");
    value->setObjectName("cardValue");

    note = new QLabel(" ");
    note->setObjectName("cardDescription");
    note->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(value);
    layout->addWidget(note);

    return card;
}

QFrame *LiveMonitorWidget::makeGraphCard(const QString &titleHtml,
                                         SpeedGraph *graph)
{
    auto *card = new QFrame;
    card->setObjectName("card");

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    auto *title = new QLabel(titleHtml);
    title->setObjectName("sectionTitle");
    title->setTextFormat(Qt::RichText);

    layout->addWidget(title);
    layout->addWidget(graph, 1);

    return card;
}

void LiveMonitorWidget::setSession(QNetworkAccessManager *sessionManager,
                                   const QUrl &routerBaseUrl)
{
    manager = sessionManager;
    baseUrl = routerBaseUrl;

    routerFailures = 0;
    routerClockStarted = false;

    setStatus("Connected to router. Waiting for the first reading...", false);

    poll();
}

void LiveMonitorWidget::start()
{
    if (!timer->isActive())
        timer->start();

    poll();
}

void LiveMonitorWidget::stop()
{
    timer->stop();
}

void LiveMonitorWidget::setStatus(const QString &text, bool warning)
{
    statusLabel->setStyleSheet(
        warning ? "color: #F5B95C;" : "color: #7891A5;");

    statusLabel->setText(text);
}

void LiveMonitorWidget::setCell(int row, int column, const QString &text)
{
    QTableWidgetItem *item = table->item(row, column);

    if (!item)
    {
        item = new QTableWidgetItem(text);

        if (column >= 3 && column <= 5)
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        table->setItem(row, column, item);
    }
    else
    {
        item->setText(text);
    }
}

void LiveMonitorWidget::poll()
{
    pollLocal();
    pollRouter();
}

// ------------------------------------------------------------
// This computer
// ------------------------------------------------------------

void LiveMonitorWidget::pollLocal()
{
    quint64 in = 0;
    quint64 out = 0;

    if (!readLocalCounters(in, out))
    {
        pcValue->setText("N/A");
        pcNote->setText("Adapter counters unavailable");
        return;
    }

    if (!haveLocal)
    {
        localClock.start();
        haveLocal = true;
    }
    else
    {
        const double seconds = localClock.restart() / 1000.0;

        if (seconds > 0.2)
        {
            const double down = (in >= lastIn ? in - lastIn : 0) / seconds;
            const double up = (out >= lastOut ? out - lastOut : 0) / seconds;

            localGraph->addSample(down, up);

            pcValue->setText(formatSpeed(down));
            pcNote->setText("Upload: " + formatSpeed(up));
        }
    }

    lastIn = in;
    lastOut = out;
}

// ------------------------------------------------------------
// Router
// ------------------------------------------------------------

void LiveMonitorWidget::pollRouter()
{
    if (!manager || !baseUrl.isValid() || routerRequestActive)
        return;

    QUrl url = baseUrl;
    url.setPath("/goform/getQos");

    QUrlQuery query;
    query.addQueryItem("random",
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem("modules", "onlineList");

    url.setQuery(query);

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::UserAgentHeader, "OmniNet/1.0");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(5000);

    routerRequestActive = true;

    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                handleRouterReply(reply);
            });
}

void LiveMonitorWidget::handleRouterReply(QNetworkReply *reply)
{
    routerRequestActive = false;

    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    reply->deleteLater();

    if (error != QNetworkReply::NoError)
    {
        ++routerFailures;

        setStatus(QString("Router is not responding (HTTP %1). Retrying...")
                      .arg(status),
                  true);
        return;
    }

    routerFailures = 0;

    rawView->setPlainText(QString::fromUtf8(body.left(20000)));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        if (body.toLower().contains("login"))
        {
            setStatus("The router session has expired. Sign in again "
                      "(Dashboard, Discover Network).",
                      true);
        }
        else
        {
            setStatus("The router replied, but not with data this page "
                      "understands. Use \"Show raw router data\" to inspect it.",
                      true);
        }

        return;
    }

    const QJsonValue root = document.isObject()
                                ? QJsonValue(document.object())
                                : QJsonValue(document.array());

    const QJsonArray items = findDeviceArray(root, 0);

    QVector<DeviceSpeed> devices;

    for (const QJsonValue &item : items)
    {
        if (!item.isObject())
            continue;

        const QJsonObject object = item.toObject();

        DeviceSpeed device;

        for (auto it = object.begin(); it != object.end(); ++it)
        {
            const QString key = normalise(it.key());
            const QJsonValue value = it.value();

            const QString text =
                value.isString() ? value.toString().trimmed()
                : value.isDouble() ? QString::number(value.toDouble())
                                   : QString();

            if (key.contains("mac"))
            {
                if (device.mac.isEmpty())
                    device.mac = text.toUpper();
            }
            else if (key.endsWith("ip") || key.contains("ipaddr"))
            {
                if (device.ip.isEmpty())
                    device.ip = text;
            }
            else if (key.contains("down") &&
                     (key.contains("speed") || key.contains("rate")))
            {
                device.down = parseSpeed(value);
            }
            else if (key.contains("up") &&
                     (key.contains("speed") || key.contains("rate")))
            {
                device.up = parseSpeed(value);
            }
            else if (key.contains("host") || key.endsWith("name"))
            {
                if (device.name.isEmpty())
                    device.name = text;
            }
        }

        if (device.ip.isEmpty() && device.mac.isEmpty())
            continue;

        if (device.name.isEmpty())
            device.name = device.ip.isEmpty() ? "Unknown device" : device.ip;

        devices.append(device);
    }

    if (devices.isEmpty())
    {
        setStatus("Connected, but no devices were found in the router's reply. "
                  "Its format may differ: use \"Show raw router data\".",
                  true);
        return;
    }

    setStatus(QString("Live. Updating every %1 seconds.").arg(kPollMs / 1000),
              false);

    applyDevices(devices);
}

void LiveMonitorWidget::applyDevices(QVector<DeviceSpeed> devices)
{
    double seconds = 0.0;

    if (routerClockStarted)
        seconds = routerClock.restart() / 1000.0;
    else
    {
        routerClock.start();
        routerClockStarted = true;
    }

    seconds = (std::min)(seconds, 10.0);

    double totalDown = 0.0;
    double totalUp = 0.0;

    for (const DeviceSpeed &device : devices)
    {
        totalDown += device.down;
        totalUp += device.up;

        const QString key = device.mac.isEmpty() ? device.ip : device.mac;

        sessionUsed[key] += (device.down + device.up) * seconds;
    }

    peakDown = (std::max)(peakDown, totalDown);
    peakUp = (std::max)(peakUp, totalUp);

    downValue->setText(formatSpeed(totalDown));
    downNote->setText("Peak: " + formatSpeed(peakDown));

    upValue->setText(formatSpeed(totalUp));
    upNote->setText("Peak: " + formatSpeed(peakUp));

    devicesValue->setText(QString::number(devices.size()));
    devicesNote->setText("Connected to the router");

    routerGraph->addSample(totalDown, totalUp);

    std::sort(devices.begin(), devices.end(),
              [](const DeviceSpeed &a, const DeviceSpeed &b)
              {
                  return a.down + a.up > b.down + b.up;
              });

    const double total = totalDown + totalUp;

    table->setRowCount(static_cast<int>(devices.size()));

    for (int row = 0; row < devices.size(); ++row)
    {
        const DeviceSpeed &device = devices[row];

        const QString key = device.mac.isEmpty() ? device.ip : device.mac;

        setCell(row, 0, device.name);
        setCell(row, 1, device.ip);
        setCell(row, 2, device.mac);
        setCell(row, 3, formatSpeed(device.down));
        setCell(row, 4, formatSpeed(device.up));
        setCell(row, 5, formatBytes(sessionUsed.value(key)));

        QWidget *holder = table->cellWidget(row, 6);
        QProgressBar *bar = holder ? holder->findChild<QProgressBar *>() : nullptr;

        if (!bar)
        {
            holder = new QWidget;

            auto *holderLayout = new QHBoxLayout(holder);
            holderLayout->setContentsMargins(8, 0, 8, 0);

            bar = new QProgressBar;
            bar->setRange(0, 1000);
            bar->setTextVisible(false);
            bar->setFixedHeight(8);

            holderLayout->addWidget(bar, 1, Qt::AlignVCenter);

            table->setCellWidget(row, 6, holder);
        }

        bar->setValue(total > 0.0
                          ? static_cast<int>((device.down + device.up) / total * 1000.0)
                          : 0);
    }
}
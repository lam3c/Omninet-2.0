#include "wirelessintelligencewidget.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QTimer>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>

#include <cmath>
#include <cstring>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wlanapi.h>

// Compatibility with MinGW/older Windows SDK WLAN headers.
// These are the documented Windows WLAN API numeric values.
#ifndef DOT11_AUTH_ALGO_WPA3
#define DOT11_AUTH_ALGO_WPA3 8
#endif

#ifndef DOT11_AUTH_ALGO_WPA3_SAE
#define DOT11_AUTH_ALGO_WPA3_SAE 9
#endif

#ifndef WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES
#define WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES 0x00000001
#endif
#endif

namespace
{
QString normaliseKey(QString key)
{
    key = key.toLower();

    key.remove(QRegularExpression("[^a-z0-9]"));

    return key;
}

QString cardStyle()
{
    return QStringLiteral(
        "QFrame#wirelessCard {"
        "background:#0D1D2C;"
        "border:1px solid #1D394D;"
        "border-radius:11px;"
        "}"
        "QLabel#wirelessTitle {"
        "color:#83A2B5;"
        "font-size:9px;"
        "font-weight:800;"
        "letter-spacing:1px;"
        "}"
        "QLabel#wirelessValue {"
        "color:#43DDED;"
        "font-size:19px;"
        "font-weight:800;"
        "}"
        "QLabel#wirelessDescription {"
        "color:#91A7B8;"
        "font-size:10px;"
        "}"
        );
}

#ifdef Q_OS_WIN

QString ssidToQString(const DOT11_SSID &ssid)
{
    return QString::fromUtf8(
        reinterpret_cast<const char *>(ssid.ucSSID),
        static_cast<int>(ssid.uSSIDLength)
        );
}

QString bssidToQString(const BYTE *bssid)
{
    return QString("%1:%2:%3:%4:%5:%6")
    .arg(bssid[0], 2, 16, QChar('0'))
        .arg(bssid[1], 2, 16, QChar('0'))
        .arg(bssid[2], 2, 16, QChar('0'))
        .arg(bssid[3], 2, 16, QChar('0'))
        .arg(bssid[4], 2, 16, QChar('0'))
        .arg(bssid[5], 2, 16, QChar('0'))
        .toUpper();
}

QString securityName(const WLAN_AVAILABLE_NETWORK &network)
{
    if (!network.bSecurityEnabled)
        return "Open";

    switch (network.dot11DefaultAuthAlgorithm)
    {
    case DOT11_AUTH_ALGO_WPA:
        return "WPA";

    case DOT11_AUTH_ALGO_WPA_PSK:
        return "WPA-PSK";

    case DOT11_AUTH_ALGO_RSNA:
        return "WPA2 / 802.1X";

    case DOT11_AUTH_ALGO_RSNA_PSK:
        return "WPA2-PSK";

    case DOT11_AUTH_ALGO_WPA3:
        return "WPA3-Enterprise (192-bit)";

    case DOT11_AUTH_ALGO_WPA3_SAE:
        return "WPA3-Personal (SAE)";

    default:
        return "Secured";
    }
}

#endif
}

WirelessIntelligenceWidget::WirelessIntelligenceWidget(QWidget *parent)
    : QWidget(parent)
{
    buildInterface();

    refreshTimer = new QTimer(this);

    connect(refreshTimer, &QTimer::timeout,
            this, &WirelessIntelligenceWidget::refreshConnection);

    refreshTimer->start(5000);

    scanNetworks();
}

void WirelessIntelligenceWidget::buildInterface()
{
    setStyleSheet(cardStyle());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName("wirelessScroll");

    auto *content = new QWidget;
    content->setObjectName("wirelessContent");

    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(28, 24, 28, 26);
    root->setSpacing(12);

    auto *heading = new QLabel("Wireless Intelligence");
    heading->setStyleSheet(
        "font-size:27px;font-weight:900;color:#FFFFFF;"
        );

    auto *subtitle = new QLabel(
        "Inspect Repeater, WISP and AP operation, wireless networks, "
        "upstream link information and signal-based distance estimates."
        );

    subtitle->setStyleSheet("color:#8493AA;font-size:12px;");
    subtitle->setWordWrap(true);

    root->addWidget(heading);
    root->addWidget(subtitle);

    // Router mode

    auto *modeCard = new QFrame;
    modeCard->setObjectName("wirelessCard");

    auto *modeLayout = new QVBoxLayout(modeCard);
    modeLayout->setContentsMargins(18, 16, 18, 16);
    modeLayout->setSpacing(8);

    auto *modeTitle = new QLabel("ROUTER OPERATING MODE");
    modeTitle->setObjectName("wirelessTitle");

    modeValue = new QLabel("Waiting for router information");
    modeValue->setObjectName("wirelessValue");

    routerApiStatus = new QLabel(
        "Authenticate with a supported router profile to read its "
        "operating mode and upstream connection details."
        );
    routerApiStatus->setObjectName("wirelessDescription");
    routerApiStatus->setWordWrap(true);

    modeLayout->addWidget(modeTitle);
    modeLayout->addWidget(modeValue);
    modeLayout->addWidget(routerApiStatus);

    root->addWidget(modeCard);

    // Mode descriptions

    auto *modeGrid = new QGridLayout;
    modeGrid->setSpacing(10);

    const QStringList names = {
        "REPEATER",
        "WISP",
        "ACCESS POINT"
    };

    const QStringList descriptions = {
        "Receives an existing wireless network and rebroadcasts it.",
        "Uses Wi-Fi as the WAN connection and normally creates a "
        "separate local network.",
        "Uses a wired upstream connection to provide wireless access."
    };

    for (int i = 0; i < names.size(); ++i)
    {
        auto *card = new QFrame;
        card->setObjectName("wirelessCard");

        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(13, 13, 13, 13);

        auto *title = new QLabel(names[i]);
        title->setObjectName("wirelessTitle");

        auto *description = new QLabel(descriptions[i]);
        description->setObjectName("wirelessDescription");
        description->setWordWrap(true);

        layout->addWidget(title);
        layout->addWidget(description);

        modeGrid->addWidget(card, 0, i);
    }

    root->addLayout(modeGrid);

    // Upstream router information

    auto *masterCard = new QFrame;
    masterCard->setObjectName("wirelessCard");

    auto *masterLayout = new QVBoxLayout(masterCard);
    masterLayout->setContentsMargins(18, 16, 18, 16);
    masterLayout->setSpacing(10);

    auto *masterTitle = new QLabel("MASTER ROUTER / UPSTREAM LINK");
    masterTitle->setObjectName("wirelessTitle");

    auto *masterGrid = new QGridLayout;
    masterGrid->setSpacing(12);

    auto addMasterValue = [&](int row, int column,
                              const QString &title,
                              QLabel **value)
    {
        auto *box = new QVBoxLayout;

        auto *caption = new QLabel(title);
        caption->setObjectName("wirelessTitle");

        *value = new QLabel("Unavailable");
        (*value)->setObjectName("wirelessValue");

        box->addWidget(caption);
        box->addWidget(*value);

        masterGrid->addLayout(box, row, column);
    };

    addMasterValue(0, 0, "MASTER SSID", &masterSsidValue);
    addMasterValue(0, 1, "MASTER SIGNAL", &masterSignalValue);
    addMasterValue(1, 0, "UPSTREAM LINK RATE", &masterRateValue);

    masterLayout->addWidget(masterTitle);
    masterLayout->addLayout(masterGrid);

    auto *masterNote = new QLabel(
        "These values are populated only when the router's management "
        "interface exposes upstream details. A PC connected to a repeater "
        "normally measures its link to the repeater, not the repeater's "
        "wireless link to the master router."
        );

    masterNote->setObjectName("wirelessDescription");
    masterNote->setWordWrap(true);

    masterLayout->addWidget(masterNote);

    root->addWidget(masterCard);

    // Current Windows wireless connection

    auto *connectionCard = new QFrame;
    connectionCard->setObjectName("wirelessCard");

    auto *connectionLayout = new QVBoxLayout(connectionCard);
    connectionLayout->setContentsMargins(18, 16, 18, 16);
    connectionLayout->setSpacing(10);

    auto *connectionTitle = new QLabel(
        "THIS COMPUTER'S WIRELESS CONNECTION"
        );
    connectionTitle->setObjectName("wirelessTitle");

    auto *connectionGrid = new QGridLayout;
    connectionGrid->setSpacing(12);

    auto addConnectionValue = [&](int row, int column,
                                  const QString &title,
                                  QLabel **value)
    {
        auto *box = new QVBoxLayout;

        auto *caption = new QLabel(title);
        caption->setObjectName("wirelessTitle");

        *value = new QLabel("Unavailable");
        (*value)->setObjectName("wirelessValue");

        box->addWidget(caption);
        box->addWidget(*value);

        connectionGrid->addLayout(box, row, column);
    };

    addConnectionValue(0, 0, "CONNECTED SSID", &connectedSsidValue);
    addConnectionValue(0, 1, "CONNECTED BSSID", &bssidValue);
    addConnectionValue(1, 0, "SIGNAL", &signalValue);
    addConnectionValue(1, 1, "RECEIVE LINK RATE", &rxRateValue);
    addConnectionValue(2, 0, "TRANSMIT LINK RATE", &txRateValue);
    addConnectionValue(2, 1, "ESTIMATED DISTANCE TO CONNECTED AP",
                       &distanceValue);
    addConnectionValue(3, 0, "CHANNEL FREQUENCY", &frequencyValue);

    connectionLayout->addWidget(connectionTitle);
    connectionLayout->addLayout(connectionGrid);

    auto *connectionNote = new QLabel(
        "RX/TX are negotiated Wi-Fi link rates, not measured internet "
        "download/upload speeds. Distance is an approximate estimate "
        "to the access point your PC is currently connected to."
        );
    connectionNote->setObjectName("wirelessDescription");
    connectionNote->setWordWrap(true);

    connectionLayout->addWidget(connectionNote);

    root->addWidget(connectionCard);

    // Available networks

    auto *networksCard = new QFrame;
    networksCard->setObjectName("wirelessCard");

    auto *networksLayout = new QVBoxLayout(networksCard);
    networksLayout->setContentsMargins(18, 16, 18, 16);
    networksLayout->setSpacing(10);

    auto *networkHeader = new QHBoxLayout;

    auto *networkTitle = new QLabel("AVAILABLE WIRELESS NETWORKS");
    networkTitle->setObjectName("wirelessTitle");

    scanButton = new QPushButton("Scan Networks");
    scanButton->setObjectName("primaryButton");

    networkHeader->addWidget(networkTitle);
    networkHeader->addStretch();
    networkHeader->addWidget(scanButton);

    scanStatus = new QLabel("Waiting for scan...");
    scanStatus->setObjectName("wirelessDescription");

    networksTable = new QTableWidget(0, 4);

    networksTable->setHorizontalHeaderLabels({
        "SSID",
        "SIGNAL QUALITY",
        "SECURITY",
        "NETWORK TYPE"
    });

    networksTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch
        );

    networksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    networksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    networksTable->setAlternatingRowColors(true);
    networksTable->setShowGrid(false);
    networksTable->verticalHeader()->setVisible(false);
    networksTable->setMinimumHeight(210);

    networksLayout->addLayout(networkHeader);
    networksLayout->addWidget(scanStatus);
    networksLayout->addWidget(networksTable);

    root->addWidget(networksCard, 1);

    connect(scanButton, &QPushButton::clicked,
            this, &WirelessIntelligenceWidget::scanNetworks);

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void WirelessIntelligenceWidget::setRouterAuthenticated(bool authenticated)
{
    routerAuthenticated = authenticated;

    if (!authenticated)
    {
        modeValue->setText("Router not authenticated");

        routerApiStatus->setText(
            "Sign in to the router to retrieve its actual operating mode "
            "and upstream configuration."
            );

        masterSsidValue->setText("Unavailable");
        masterSignalValue->setText("Unavailable");
        masterRateValue->setText("Unavailable");
    }
}

void WirelessIntelligenceWidget::setRouterSettings(
    const QJsonObject &settings)
{
    inspectRouterSettings(settings);
}

void WirelessIntelligenceWidget::inspectRouterSettings(
    const QJsonObject &settings)
{
    QString detectedMode;
    QString masterSsid;
    QString masterSignal;
    QString masterRate;

    std::function<void(const QJsonObject &)> inspectObject;

    inspectObject = [&](const QJsonObject &object)
    {
        for (auto it = object.begin(); it != object.end(); ++it)
        {
            const QString key = normaliseKey(it.key());
            const QJsonValue value = it.value();

            if (value.isObject())
            {
                inspectObject(value.toObject());
                continue;
            }

            if (value.isArray())
            {
                const QJsonArray array = value.toArray();

                for (const QJsonValue &entry : array)
                {
                    if (entry.isObject())
                        inspectObject(entry.toObject());
                }

                continue;
            }

            if (!value.isString() && !value.isDouble() &&
                !value.isBool())
                continue;

            const QString text = value.toVariant().toString().trimmed();

            if (text.isEmpty())
                continue;

            if (key.contains("mode") || key == "opmode" ||
                key == "operation")
            {
                const QString lower = text.toLower();

                if (lower.contains("repeater") ||
                    lower.contains("range extender"))
                    detectedMode = "Repeater Mode";

                else if (lower.contains("wisp") ||
                         lower.contains("wireless wan"))
                    detectedMode = "WISP Mode";

                else if (lower == "ap" ||
                         lower.contains("access point"))
                    detectedMode = "Access Point Mode";

                else if (lower.contains("router"))
                    detectedMode = "Router Mode";
            }

            if ((key.contains("upstream") ||
                 key.contains("uplink") ||
                 key.contains("master") ||
                 key.contains("parent") ||
                 key.contains("repeater")) &&
                key.contains("ssid"))
            {
                masterSsid = text;
            }

            if ((key.contains("upstream") ||
                 key.contains("uplink") ||
                 key.contains("master") ||
                 key.contains("parent") ||
                 key.contains("repeater")) &&
                (key.contains("rssi") ||
                 key.contains("signal")))
            {
                masterSignal = text;
            }

            if ((key.contains("upstream") ||
                 key.contains("uplink") ||
                 key.contains("master") ||
                 key.contains("parent") ||
                 key.contains("repeater")) &&
                (key.contains("rate") ||
                 key.contains("speed") ||
                 key.contains("link")))
            {
                masterRate = text;
            }
        }
    };

    inspectObject(settings);

    if (!detectedMode.isEmpty())
        modeValue->setText(detectedMode);
    else
        modeValue->setText("Mode not exposed by router API");

    masterSsidValue->setText(
        masterSsid.isEmpty() ? "Not exposed" : masterSsid
        );

    masterSignalValue->setText(
        masterSignal.isEmpty() ? "Not exposed" : masterSignal
        );

    masterRateValue->setText(
        masterRate.isEmpty() ? "Not exposed" : masterRate
        );

    routerApiStatus->setText(
        "Router settings received. Fields are displayed only when "
        "the router returns matching upstream or operating-mode values. "
        "An unavailable field does not mean the feature is disabled."
        );
}

void WirelessIntelligenceWidget::scanNetworks()
{
#ifdef Q_OS_WIN

    scanButton->setEnabled(false);
    scanStatus->setText("Scanning nearby wireless networks...");

    QTimer::singleShot(2200, this, [this]()
                       {
                           loadAvailableNetworks();
                           refreshConnection();

                           scanButton->setEnabled(true);
                       });

#else

    scanStatus->setText(
        "Wireless scanning is currently supported on Windows only."
        );

#endif
}

void WirelessIntelligenceWidget::loadAvailableNetworks()
{
#ifdef Q_OS_WIN

    HANDLE client = nullptr;
    DWORD negotiatedVersion = 0;

    DWORD result = WlanOpenHandle(
        2, nullptr, &negotiatedVersion, &client
        );

    if (result != ERROR_SUCCESS)
    {
        scanStatus->setText(
            QString("Could not open Windows Wi-Fi service. Error %1")
                .arg(result)
            );
        return;
    }

    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;

    result = WlanEnumInterfaces(client, nullptr, &interfaces);

    if (result != ERROR_SUCCESS || !interfaces)
    {
        scanStatus->setText("No wireless adapter was found.");

        if (interfaces)
            WlanFreeMemory(interfaces);

        WlanCloseHandle(client, nullptr);
        return;
    }

    networksTable->setRowCount(0);

    int totalNetworks = 0;

    for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i)
    {
        const GUID interfaceGuid =
            interfaces->InterfaceInfo[i].InterfaceGuid;

        WlanScan(
            client, &interfaceGuid, nullptr, nullptr, nullptr
            );

        PWLAN_AVAILABLE_NETWORK_LIST available = nullptr;

        result = WlanGetAvailableNetworkList(
            client,
            &interfaceGuid,
            WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES,
            nullptr,
            &available
            );

        if (result != ERROR_SUCCESS || !available)
        {
            if (available)
                WlanFreeMemory(available);
            continue;
        }

        for (DWORD n = 0; n < available->dwNumberOfItems; ++n)
        {
            const WLAN_AVAILABLE_NETWORK &network =
                available->Network[n];

            QString ssid = ssidToQString(network.dot11Ssid);

            if (ssid.trimmed().isEmpty())
                ssid = "(Hidden network)";

            const QString type =
                network.dot11BssType == dot11_BSS_type_infrastructure
                    ? "Infrastructure"
                    : "Ad hoc";

            const int row = networksTable->rowCount();
            networksTable->insertRow(row);

            networksTable->setItem(
                row, 0, new QTableWidgetItem(ssid)
                );

            networksTable->setItem(
                row, 1,
                new QTableWidgetItem(
                    QString("%1%").arg(network.wlanSignalQuality)
                    )
                );

            networksTable->setItem(
                row, 2,
                new QTableWidgetItem(securityName(network))
                );

            networksTable->setItem(
                row, 3, new QTableWidgetItem(type)
                );

            ++totalNetworks;
        }

        WlanFreeMemory(available);
    }

    WlanFreeMemory(interfaces);
    WlanCloseHandle(client, nullptr);

    scanStatus->setText(
        QString("%1 network entries found. Scan time: %2")
            .arg(totalNetworks)
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        );

#endif
}

void WirelessIntelligenceWidget::refreshConnection()
{
#ifdef Q_OS_WIN

    HANDLE client = nullptr;
    DWORD negotiatedVersion = 0;

    if (WlanOpenHandle(2, nullptr, &negotiatedVersion,
                       &client) != ERROR_SUCCESS)
        return;

    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;

    if (WlanEnumInterfaces(client, nullptr,
                           &interfaces) != ERROR_SUCCESS ||
        !interfaces)
    {
        WlanCloseHandle(client, nullptr);
        return;
    }

    bool connected = false;

    for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i)
    {
        const WLAN_INTERFACE_INFO &interfaceInfo =
            interfaces->InterfaceInfo[i];

        if (interfaceInfo.isState != wlan_interface_state_connected)
            continue;

        WLAN_CONNECTION_ATTRIBUTES *attributes = nullptr;
        DWORD dataSize = 0;
        WLAN_OPCODE_VALUE_TYPE opcodeType;

        DWORD result = WlanQueryInterface(
            client,
            &interfaceInfo.InterfaceGuid,
            wlan_intf_opcode_current_connection,
            nullptr,
            &dataSize,
            reinterpret_cast<void **>(&attributes),
            &opcodeType
            );

        if (result != ERROR_SUCCESS || !attributes)
            continue;

        const WLAN_ASSOCIATION_ATTRIBUTES &association =
            attributes->wlanAssociationAttributes;

        currentSsid = ssidToQString(association.dot11Ssid);

        currentRssi = -100.0;
        currentFrequencyMHz = 0.0;

        PWLAN_BSS_LIST bssList = nullptr;

        // Some MinGW wlanapi.h versions declare this parameter as
        // PDOT11_SSID (non-const), so pass a local mutable copy.
        DOT11_SSID ssidCopy = association.dot11Ssid;

        if (WlanGetNetworkBssList(
                client,
                &interfaceInfo.InterfaceGuid,
                &ssidCopy,
                dot11_BSS_type_infrastructure,
                FALSE,
                nullptr,
                &bssList) == ERROR_SUCCESS && bssList)
        {
            for (DWORD b = 0; b < bssList->dwNumberOfItems; ++b)
            {
                const WLAN_BSS_ENTRY &entry =
                    bssList->wlanBssEntries[b];

                if (std::memcmp(
                        entry.dot11Bssid,
                        association.dot11Bssid,
                        6) == 0)
                {
                    currentRssi = entry.lRssi;
                    currentFrequencyMHz =
                        entry.ulChCenterFrequency / 1000.0;
                    break;
                }
            }

            WlanFreeMemory(bssList);
        }

        connectedSsidValue->setText(
            currentSsid.isEmpty() ? "Unknown" : currentSsid
            );

        bssidValue->setText(
            bssidToQString(association.dot11Bssid)
            );

        signalValue->setText(
            currentRssi > -100.0
                ? QString("%1 dBm").arg(currentRssi, 0, 'f', 0)
                : QString("%1% quality")
                      .arg(association.wlanSignalQuality)
            );

        rxRateValue->setText(
            QString("%1 Mbps")
                .arg(association.ulRxRate / 1000.0, 0, 'f', 1)
            );

        txRateValue->setText(
            QString("%1 Mbps")
                .arg(association.ulTxRate / 1000.0, 0, 'f', 1)
            );

        frequencyValue->setText(
            currentFrequencyMHz > 0.0
                ? QString("%1 MHz")
                      .arg(currentFrequencyMHz, 0, 'f', 0)
                : "Unavailable"
            );

        distanceValue->setText(
            currentFrequencyMHz > 0.0 && currentRssi > -100.0
                ? estimateDistance(currentRssi, currentFrequencyMHz)
                : "Unavailable"
            );

        connected = true;

        WlanFreeMemory(attributes);
        break;
    }

    if (!connected)
    {
        connectedSsidValue->setText("Not connected");
        bssidValue->setText("—");
        signalValue->setText("—");
        rxRateValue->setText("—");
        txRateValue->setText("—");
        distanceValue->setText("—");
        frequencyValue->setText("—");
    }

    WlanFreeMemory(interfaces);
    WlanCloseHandle(client, nullptr);

#endif
}

QString WirelessIntelligenceWidget::estimateDistance(
    double rssiDbm,
    double frequencyMHz) const
{
    if (frequencyMHz <= 0.0 || rssiDbm >= 0.0 ||
        rssiDbm <= -120.0)
        return "Unavailable";

    // Approximate indoor log-distance model.
    // It is not a precise ranging or positioning system.

    const double referenceRssi =
        frequencyMHz >= 4900.0 ? -46.0 : -40.0;

    const double pathLossExponent = 2.7;

    const double distance = std::pow(
        10.0,
        (referenceRssi - rssiDbm) /
            (10.0 * pathLossExponent)
        );

    if (!std::isfinite(distance))
        return "Unavailable";

    return QString("~%1 metres")
        .arg(distance, 0, 'f', 1);
}
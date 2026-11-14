
#include "routerdiscovery.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QHostAddress>
#include <QAbstractSocket>

#include <limits>
#include <memory>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <iptypes.h>
#endif

RouterDiscovery::RouterDiscovery(QObject *parent)
    : QObject(parent)
{
}

// --------------------------------------------------
// NETWORK DISCOVERY
// --------------------------------------------------

bool RouterDiscovery::discoverNetwork(
    NetworkDetails &details,
    QString &error)
{
#ifdef Q_OS_WIN

    // Allocate memory for Windows adapter information.
    ULONG bufferSize = 15000;

    std::unique_ptr<char[]> buffer(new char[bufferSize]);

    auto *addresses =
        reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.get());

    ULONG result = GetAdaptersAddresses(
        AF_INET,
        GAA_FLAG_INCLUDE_GATEWAYS,
        nullptr,
        addresses,
        &bufferSize);

    // Windows may request a larger buffer.
    if (result == ERROR_BUFFER_OVERFLOW)
    {
        buffer.reset(new char[bufferSize]);

        addresses =
            reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.get());

        result = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_INCLUDE_GATEWAYS,
            nullptr,
            addresses,
            &bufferSize);
    }

    if (result != NO_ERROR)
    {
        error =
            "Windows could not retrieve network adapter information.";

        return false;
    }

    // Select the active adapter with the lowest IPv4 route metric.
    IP_ADAPTER_ADDRESSES *bestAdapter = nullptr;

    ULONG bestMetric =
        (std::numeric_limits<ULONG>::max)();

    for (auto *adapter = addresses;
         adapter != nullptr;
         adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        if (adapter->FirstGatewayAddress == nullptr ||
            adapter->FirstUnicastAddress == nullptr)
            continue;

        if (adapter->Ipv4Metric < bestMetric)
        {
            bestMetric = adapter->Ipv4Metric;
            bestAdapter = adapter;
        }
    }

    if (bestAdapter == nullptr)
    {
        error =
            "No active network adapter with an IPv4 gateway was found.";

        return false;
    }

    // Retrieve the selected adapter's gateway and local IPv4 address.
    auto *gatewayAddress =
        reinterpret_cast<sockaddr_in *>(
            bestAdapter->FirstGatewayAddress->Address.lpSockaddr);

    auto *localAddress =
        reinterpret_cast<sockaddr_in *>(
            bestAdapter->FirstUnicastAddress->Address.lpSockaddr);

    char gatewayBuffer[INET_ADDRSTRLEN] = {};
    char localBuffer[INET_ADDRSTRLEN] = {};

    if (InetNtopA(
            AF_INET,
            &gatewayAddress->sin_addr,
            gatewayBuffer,
            sizeof(gatewayBuffer)) == nullptr ||
        InetNtopA(
            AF_INET,
            &localAddress->sin_addr,
            localBuffer,
            sizeof(localBuffer)) == nullptr)
    {
        error = "Could not convert the network addresses.";
        return false;
    }

    // Store adapter information.
    details.adapterName =
        QString::fromWCharArray(bestAdapter->FriendlyName);

    details.localIp =
        QString::fromLatin1(localBuffer);

    details.gatewayIp =
        QString::fromLatin1(gatewayBuffer);

    details.connectionType =
        bestAdapter->IfType == IF_TYPE_ETHERNET_CSMACD
            ? "Ethernet"
            : bestAdapter->IfType == IF_TYPE_IEEE80211
                  ? "Wi-Fi"
                  : "Other";

    // Convert Windows' subnet prefix length into a subnet mask.
    const ULONG prefixLength =
        bestAdapter->FirstUnicastAddress->OnLinkPrefixLength;

    if (prefixLength > 32)
    {
        error =
            "Windows returned an invalid subnet prefix length.";

        return false;
    }

    const quint32 mask =
        prefixLength == 0
            ? 0u
            : 0xFFFFFFFFu << (32 - prefixLength);

    details.subnetMask = QString("%1.%2.%3.%4")
                             .arg((mask >> 24) & 255)
                             .arg((mask >> 16) & 255)
                             .arg((mask >> 8) & 255)
                             .arg(mask & 255);

    // Confirm that the required IPv4 information is available.
    if (details.gatewayIp.isEmpty() ||
        details.localIp.isEmpty() ||
        details.subnetMask.isEmpty())
    {
        error =
            "The network adapter did not provide usable IPv4 details.";

        return false;
    }

    emit networkDiscovered(details);

    return true;

#else

    Q_UNUSED(details);

    error =
        "This network discovery implementation currently targets Windows.";

    return false;

#endif
}

// --------------------------------------------------
// GATEWAY WEBPAGE INSPECTION
// --------------------------------------------------

void RouterDiscovery::inspectGateway(const QString &gatewayIp)
{
    QHostAddress address;

    // Only inspect valid IPv4 gateway addresses.
    if (!address.setAddress(gatewayIp) ||
        address.protocol() != QAbstractSocket::IPv4Protocol)
    {
        emit inspectionFinished({});
        return;
    }

    pageResults.clear();
    pendingRequests = 2;

    emit inspectionProgress(
        "Inspecting the router's management webpage...");

    // Check both HTTP and HTTPS.
    inspectUrl("http://" + gatewayIp);
    inspectUrl("https://" + gatewayIp);
}

void RouterDiscovery::inspectUrl(const QString &url)
{
    QNetworkRequest request{QUrl(url)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "OmniNet/1.0 Router Discovery");

    request.setTransferTimeout(6000);

    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = networkManager.get(request);

    connect(reply, &QNetworkReply::finished,
            this, [this, reply, url]()
            {
                RouterPageResult result;

                result.url = url;

                result.statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute).toInt();

                result.reachable =
                    reply->error() == QNetworkReply::NoError ||
                    result.statusCode > 0;

                result.server =
                    reply->header(
                             QNetworkRequest::ServerHeader).toString();

                if (result.reachable)
                {
                    const QByteArray html = reply->readAll();

                    const QString page =
                        QString::fromUtf8(html);

                    // Extract webpage title.
                    const QRegularExpression titleExpression(
                        "<title[^>]*>(.*?)</title>",
                        QRegularExpression::CaseInsensitiveOption |
                            QRegularExpression::DotMatchesEverythingOption);

                    const auto titleMatch =
                        titleExpression.match(page);

                    if (titleMatch.hasMatch())
                    {
                        result.title =
                            titleMatch.captured(1).trimmed();
                    }

                    // Inspect the login form and determine its requirements.
                    result.authentication =
                        AuthenticationDetector::inspect(page);

                    result.loginFormDetected =
                        result.authentication.formFound;

                    if (result.loginFormDetected)
                    {
                        switch (result.authentication.type)
                        {
                        case AuthenticationType::PasswordOnly:

                            emit inspectionProgress(
                                "Router login detected: password required.");

                            break;

                        case AuthenticationType::UsernameAndPassword:

                            emit inspectionProgress(
                                "Router login detected: username and "
                                "password required.");

                            break;

                        case AuthenticationType::Unsupported:

                            emit inspectionProgress(
                                "Login detected, but its authentication "
                                "format needs further inspection.");

                            break;
                        }
                    }
                }
                else
                {
                    result.error = reply->errorString();
                }

                pageResults.append(result);

                reply->deleteLater();

                pendingRequests--;

                // Emit results after both HTTP and HTTPS inspections finish.
                if (pendingRequests == 0)
                {
                    emit inspectionFinished(pageResults);
                }
            });
}

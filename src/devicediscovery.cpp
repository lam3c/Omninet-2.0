#include "devicediscovery.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QMetaObject>
#include <QRunnable>
#include <QStringList>

// winsock2.h must come before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

// --------------------------------------------------
// Scan one IP address and attempt MAC discovery.
// --------------------------------------------------

static NetworkDevice scanAddress(quint32 address)
{
    NetworkDevice device;

    device.ipAddress = QHostAddress(address).toString();
    device.macAddress = "Unavailable";
    device.hostName = "Unknown";
    device.status = "Unreachable";

    // Windows expects the IPv4 address in network byte order.
    IPAddr destination = htonl(address);

    // ----------------------------------------------
    // 1. Attempt MAC address discovery using ARP.
    // ----------------------------------------------

    ULONG macBuffer[2] = {};
    ULONG macLength = 6;

    DWORD arpResult = SendARP(
        destination,
        0,
        macBuffer,
        &macLength
        );

    bool macFound = false;

    if (arpResult == NO_ERROR && macLength > 0)
    {
        auto *macBytes =
            reinterpret_cast<unsigned char *>(macBuffer);

        QStringList macParts;

        for (ULONG i = 0; i < macLength; ++i)
        {
            macParts.append(
                QString("%1")
                    .arg(macBytes[i], 2, 16, QChar('0'))
                    .toUpper()
                );
        }

        device.macAddress = macParts.join(":");
        macFound = true;
    }

    // ----------------------------------------------
    // 2. Independently attempt an ICMP ping.
    // ----------------------------------------------

    HANDLE icmpHandle = IcmpCreateFile();

    bool pingSucceeded = false;

    if (icmpHandle != INVALID_HANDLE_VALUE)
    {
        char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32] = {};

        DWORD replySize = IcmpSendEcho(
            icmpHandle,
            destination,
            nullptr,
            0,
            nullptr,
            replyBuffer,
            sizeof(replyBuffer),
            500
            );

        if (replySize > 0)
        {
            auto *reply =
                reinterpret_cast<ICMP_ECHO_REPLY *>(replyBuffer);

            if (reply->Status == IP_SUCCESS)
                pingSucceeded = true;
        }

        IcmpCloseHandle(icmpHandle);
    }

    // ----------------------------------------------
    // 3. Keep the device if either method succeeded.
    // ----------------------------------------------

    if (macFound || pingSucceeded)
    {
        if (pingSucceeded)
            device.status = "Reachable";
        else
            device.status = "Detected via ARP";

        // ------------------------------------------
        // 4. Attempt hostname resolution.
        // ------------------------------------------

        sockaddr_in addressInfo = {};

        addressInfo.sin_family = AF_INET;
        addressInfo.sin_addr.s_addr = destination;

        char hostName[NI_MAXHOST] = {};

        if (getnameinfo(
                reinterpret_cast<sockaddr *>(&addressInfo),
                sizeof(addressInfo),
                hostName,
                sizeof(hostName),
                nullptr,
                0,
                NI_NAMEREQD) == 0)
        {
            device.hostName =
                QString::fromLocal8Bit(hostName);
        }
    }

    return device;
}

// --------------------------------------------------
// Constructor
// --------------------------------------------------

DeviceDiscovery::DeviceDiscovery(QObject *parent)
    : QObject(parent)
{
    // Allow up to 32 addresses to be scanned concurrently.
    threadPool.setMaxThreadCount(32);

    threadPool.setExpiryTimeout(1000);
}

// --------------------------------------------------
// Destructor
// --------------------------------------------------

DeviceDiscovery::~DeviceDiscovery()
{
    threadPool.waitForDone();
}

// --------------------------------------------------
// Start network scan
// --------------------------------------------------

void DeviceDiscovery::scanNetwork(
    const QString &localIp,
    const QString &subnetMask)
{
    cancelScan();

    QHostAddress localAddress(localIp);
    QHostAddress maskAddress(subnetMask);

    if (localAddress.protocol() != QAbstractSocket::IPv4Protocol ||
        maskAddress.protocol() != QAbstractSocket::IPv4Protocol)
    {
        emit scanProgress("Invalid IPv4 address or subnet mask.");
        emit scanFinished({});
        return;
    }

    quint32 local = localAddress.toIPv4Address();
    quint32 mask = maskAddress.toIPv4Address();

    quint32 inverseMask = ~mask;

    // Validate that the subnet mask has continuous network bits.
    if ((inverseMask & (inverseMask + 1)) != 0)
    {
        emit scanProgress("Invalid subnet mask.");
        emit scanFinished({});
        return;
    }

    quint32 network = local & mask;
    quint32 broadcast = network | inverseMask;

    if (broadcast <= network + 1)
    {
        emit scanProgress("No usable host addresses in this network.");
        emit scanFinished({});
        return;
    }

    quint32 firstAddress = network + 1;
    quint32 lastAddress = broadcast - 1;

    // Limit the scan to 1024 addresses.
    if (lastAddress - firstAddress > 1022)
    {
        emit scanProgress("Network is too large for this scan.");
        emit scanFinished({});
        return;
    }

    discoveredDevices.clear();

    scanActive = true;
    pendingTasks = static_cast<int>(
        lastAddress - firstAddress + 1
        );

    const quint64 scanId = currentScanId;

    emit scanStarted();
    emit scanProgress("Discovering devices and MAC addresses...");

    // Create one background task for each IP address.
    for (quint32 address = firstAddress;
         address <= lastAddress;
         ++address)
    {
        threadPool.start(QRunnable::create(
            [this, address, scanId]()
            {
                NetworkDevice device = scanAddress(address);

                // Send results back to the main Qt thread.
                QMetaObject::invokeMethod(
                    this,
                    [this, device, scanId]()
                    {
                        // Ignore results from cancelled scans.
                        if (!scanActive || scanId != currentScanId)
                            return;

                        if (device.status != "Unreachable")
                        {
                            discoveredDevices.append(device);
                            emit deviceFound(device);
                        }

                        --pendingTasks;

                        emit scanProgress(
                            QString("Scanning... %1 addresses remaining.")
                                .arg(pendingTasks)
                            );

                        finishScanIfReady();
                    },
                    Qt::QueuedConnection
                    );
            }
            ));
    }
}

// --------------------------------------------------
// Finish scan
// --------------------------------------------------

void DeviceDiscovery::finishScanIfReady()
{
    if (!scanActive || pendingTasks > 0)
        return;

    scanActive = false;

    emit scanProgress("Network scan completed.");

    emit scanFinished(discoveredDevices);
}

// --------------------------------------------------
// Cancel scan
// --------------------------------------------------

void DeviceDiscovery::cancelScan()
{
    ++currentScanId;

    scanActive = false;
    pendingTasks = 0;

    // Remove tasks that haven't started.
    threadPool.clear();

    discoveredDevices.clear();
}
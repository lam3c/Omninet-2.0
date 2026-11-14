#include "networkdetector.h"

#include <QByteArray>

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <memory>

bool NetworkDetector::detect(NetworkInformation &info, QString &error)
{
    // Ask Windows how much memory is needed for adapter information.
    ULONG bufferSize = 0;

    ULONG result = GetAdaptersInfo(nullptr, &bufferSize);

    if (result != ERROR_BUFFER_OVERFLOW || bufferSize == 0)
    {
        error = "Unable to retrieve network adapter information.";
        return false;
    }

    // Allocate memory for the adapter information.
    auto buffer = std::make_unique<BYTE[]>(bufferSize);

    auto *adapters =
        reinterpret_cast<IP_ADAPTER_INFO *>(buffer.get());

    result = GetAdaptersInfo(adapters, &bufferSize);

    if (result != ERROR_SUCCESS)
    {
        error = "Failed to read network adapter information.";
        return false;
    }

    // Find an adapter with a valid IPv4 address and gateway.
    for (IP_ADAPTER_INFO *adapter = adapters;
         adapter != nullptr;
         adapter = adapter->Next)
    {
        QString ip = QString::fromLatin1(
            adapter->IpAddressList.IpAddress.String);

        QString mask = QString::fromLatin1(
            adapter->IpAddressList.IpMask.String);

        QString gateway = QString::fromLatin1(
            adapter->GatewayList.IpAddress.String);

        // Skip adapters without a usable IPv4 configuration.
        if (ip.isEmpty() ||
            ip == "0.0.0.0" ||
            mask.isEmpty() ||
            gateway.isEmpty() ||
            gateway == "0.0.0.0")
        {
            continue;
        }

        info.adapterName = QString::fromLatin1(adapter->Description);

        // Identify the connection type.
        if (adapter->Type == MIB_IF_TYPE_ETHERNET)
        {
            info.connectionType = "Ethernet";
        }
        else if (adapter->Type == IF_TYPE_IEEE80211)
        {
            info.connectionType = "Wi-Fi";
        }
        else
        {
            info.connectionType = "Other";
        }

        info.localIp = ip;
        info.subnetMask = mask;
        info.gatewayIp = gateway;

        error.clear();
        return true;
    }

    error = "No active network adapter with a default gateway was found.";
    return false;
}
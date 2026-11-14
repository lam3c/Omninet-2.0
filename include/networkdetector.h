#pragma once

#include <QString>

struct NetworkInformation
{
    QString adapterName;
    QString connectionType;
    QString localIp;
    QString subnetMask;
    QString gatewayIp;
};

class NetworkDetector
{
public:
    static bool detect(NetworkInformation &info, QString &error);
};
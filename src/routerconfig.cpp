
#include "routerconfig.h"

#include <QDebug>

// Constructor
RouterConfig::RouterConfig()
    : routerName("Unknown Router"),
    ipAddress("0.0.0.0"),
    username("admin"),
    password(""),
    firmwareVersion("Unknown"),
    configured(false)
{
}

// Setters
void RouterConfig::setRouterName(const QString &name)
{
    routerName = name;
}

void RouterConfig::setIPAddress(const QString &ip)
{
    ipAddress = ip;
}

void RouterConfig::setUsername(const QString &user)
{
    username = user;
}

void RouterConfig::setPassword(const QString &pass)
{
    password = pass;
}

void RouterConfig::setFirmwareVersion(const QString &version)
{
    firmwareVersion = version;
}

// Getters
QString RouterConfig::getRouterName() const
{
    return routerName;
}

QString RouterConfig::getIPAddress() const
{
    return ipAddress;
}

QString RouterConfig::getUsername() const
{
    return username;
}

QString RouterConfig::getFirmwareVersion() const
{
    return firmwareVersion;
}

// Configuration status
bool RouterConfig::isConfigured() const
{
    return configured;
}

void RouterConfig::setConfigured(bool status)
{
    configured = status;
}

// Display configuration
void RouterConfig::displayConfiguration()
{
    qDebug() << "_____ ROUTER CONFIGURATION _____";

    qDebug() << "Router:" << routerName;
    qDebug() << "IP Address:" << ipAddress;
    qDebug() << "Username:" << username;
    qDebug() << "Firmware:" << firmwareVersion;

    if (configured)
    {
        qDebug() << "Status: Configured";
    }
    else
    {
        qDebug() << "Status: Not Configured";
    }

    qDebug() << "________________________________";
}

// Reset configuration
void RouterConfig::resetConfiguration()
{
    routerName = "Unknown Router";
    ipAddress = "0.0.0.0";
    username = "admin";
    password = "";
    firmwareVersion = "Unknown";
    configured = false;

    qDebug() << "Router configuration has been reset";
}

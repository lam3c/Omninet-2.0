
#pragma once

#include <QString>

class RouterConfig
{
public:
    // Constructor
    RouterConfig();

    // Setters
    void setRouterName(const QString &name);
    void setIPAddress(const QString &ip);
    void setUsername(const QString &user);
    void setPassword(const QString &pass);
    void setFirmwareVersion(const QString &version);

    // Getters
    QString getRouterName() const;
    QString getIPAddress() const;
    QString getUsername() const;
    QString getFirmwareVersion() const;

    // Configuration status
    bool isConfigured() const;
    void setConfigured(bool status);

    // Operations
    void displayConfiguration();
    void resetConfiguration();

private:
    QString routerName;
    QString ipAddress;
    QString username;
    QString password;
    QString firmwareVersion;

    bool configured;
};

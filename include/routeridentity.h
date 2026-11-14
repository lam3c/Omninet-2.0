#pragma once

#include <QString>
#include <QList>

enum class IdentityEvidenceType
{
    GatewayAddress,
    PageTitle,
    ServerHeader,
    LoginPage,
    Manufacturer,
    Model,
    HardwareRevision,
    FirmwareVersion
};

struct IdentityEvidence
{
    IdentityEvidenceType type;
    QString value;
    QString source;
};

struct RouterIdentity
{
    QString gatewayIp;
    QString managementUrl;

    QString manufacturer;
    QString model;
    QString hardwareRevision;
    QString firmwareVersion;

    QString pageTitle;
    QString server;

    QList<IdentityEvidence> evidence;

    bool isEmpty() const
    {
        return manufacturer.isEmpty() &&
               model.isEmpty() &&
               hardwareRevision.isEmpty() &&
               firmwareVersion.isEmpty() &&
               pageTitle.isEmpty() &&
               server.isEmpty();
    }
};
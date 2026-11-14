#pragma once

#include <QString>
#include <QStringList>

#include "routeridentity.h"

struct ProfileMatchResult
{
    bool matched = false;
    int confidence = 0;                  // 0-100
    QString explanation;                 // why the profile matched
    bool compatibilityVerified = false;  // identity match != protocol support
};

class RouterProfile
{
public:
    virtual ~RouterProfile() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString manufacturer() const = 0;

    virtual ProfileMatchResult evaluate(
        const RouterIdentity &identity) const = 0;

    virtual QStringList capabilities() const = 0;
};
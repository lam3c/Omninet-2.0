#pragma once

#include "routerprofile.h"

class TendaF3Profile : public RouterProfile
{
public:
    QString id() const override;
    QString name() const override;
    QString manufacturer() const override;

    ProfileMatchResult evaluate(
        const RouterIdentity &identity) const override;

    QStringList capabilities() const override;
};
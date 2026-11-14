#pragma once

#include "routeridentity.h"
#include "gatewayinspector.h"
#include "routerdiscovery.h"

class RouterIdentification
{
public:
    static RouterIdentity fromGatewayInspection(
        const GatewayInformation &information);

    static RouterIdentity fromRouterPage(
        const RouterPageResult &result,
        const QString &gatewayIp);
};
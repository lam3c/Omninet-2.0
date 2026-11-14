
#include "routeridentification.h"

namespace
{
void addEvidence(
    RouterIdentity &identity,
    IdentityEvidenceType type,
    const QString &value,
    const QString &source)
{
    if (value.trimmed().isEmpty())
        return;

    identity.evidence.append({
        type,
        value.trimmed(),
        source
    });
}
}

RouterIdentity RouterIdentification::fromGatewayInspection(
    const GatewayInformation &information)
{
    RouterIdentity identity;

    identity.gatewayIp = information.gatewayIp;
    identity.managementUrl = information.url;
    identity.pageTitle = information.pageTitle;
    identity.server = information.server;

    addEvidence(
        identity,
        IdentityEvidenceType::GatewayAddress,
        information.gatewayIp,
        "Gateway inspection");

    addEvidence(
        identity,
        IdentityEvidenceType::PageTitle,
        information.pageTitle,
        "HTTP page title");

    addEvidence(
        identity,
        IdentityEvidenceType::ServerHeader,
        information.server,
        "HTTP Server header");

    return identity;
}

RouterIdentity RouterIdentification::fromRouterPage(
    const RouterPageResult &result,
    const QString &gatewayIp)
{
    RouterIdentity identity;

    identity.gatewayIp = gatewayIp;
    identity.managementUrl = result.url;
    identity.pageTitle = result.title;
    identity.server = result.server;

    addEvidence(
        identity,
        IdentityEvidenceType::GatewayAddress,
        gatewayIp,
        "Router discovery");

    addEvidence(
        identity,
        IdentityEvidenceType::PageTitle,
        result.title,
        "Router management page");

    addEvidence(
        identity,
        IdentityEvidenceType::ServerHeader,
        result.server,
        "HTTP response");

    if (result.loginFormDetected)
    {
        addEvidence(
            identity,
            IdentityEvidenceType::LoginPage,
            "Login form detected",
            "Authentication detector");
    }

    return identity;
}

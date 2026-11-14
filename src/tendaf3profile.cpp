
#include "tendaf3profile.h"

QString TendaF3Profile::id() const
{
    return "tenda.f3";
}

QString TendaF3Profile::name() const
{
    return "Tenda F3";
}

QString TendaF3Profile::manufacturer() const
{
    return "Tenda";
}

ProfileMatchResult TendaF3Profile::evaluate(
    const RouterIdentity &identity) const
{
    ProfileMatchResult result;

    const QString manufacturer =
        identity.manufacturer.trimmed().toLower();

    const QString model =
        identity.model.trimmed().toLower();

    const QString title =
        identity.pageTitle.trimmed().toLower();

    const bool manufacturerMatches =
        manufacturer.contains("tenda");

    const bool modelMatches =
        model == "f3" || model.startsWith("f3 ");

    const bool titleMatches =
        title.contains("tenda") &&
        title.contains("f3");

    if (manufacturerMatches && modelMatches)
    {
        result.matched = true;
        result.confidence = 100;
        result.explanation =
            "Manufacturer and model information identify a Tenda F3.";
    }
    else if (titleMatches)
    {
        result.matched = true;
        result.confidence = 65;
        result.explanation =
            "Management page title suggests a Tenda F3. "
            "Hardware identity has not been independently verified.";
    }

    // Matching identity does not prove protocol compatibility.
    result.compatibilityVerified = false;

    return result;
}

QStringList TendaF3Profile::capabilities() const
{
    return {
        "Identity detection"
    };
}

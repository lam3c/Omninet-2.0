
#include "routerprofilemanager.h"

void RouterProfileManager::registerProfile(
    std::unique_ptr<RouterProfile> profile)
{
    if (!profile)
        return;

    const QString profileId = profile->id().trimmed();

    if (profileId.isEmpty())
        return;

    if (findProfile(profileId))
        return;

    profiles.push_back(std::move(profile));
}

QList<ProfileCandidate>
RouterProfileManager::findMatchingProfiles(
    const RouterIdentity &identity) const
{
    QList<ProfileCandidate> candidates;

    for (const auto &profile : profiles)
    {
        ProfileMatchResult result = profile->evaluate(identity);

        if (result.matched)
        {
            candidates.append({
                profile.get(),
                result
            });
        }
    }

    return candidates;
}

RouterProfile *RouterProfileManager::findProfile(
    const QString &profileId) const
{
    for (const auto &profile : profiles)
    {
        if (profile->id().compare(
                profileId.trimmed(),
                Qt::CaseInsensitive) == 0)
        {
            return profile.get();
        }
    }

    return nullptr;
}

QList<RouterProfile *> RouterProfileManager::registeredProfiles() const
{
    QList<RouterProfile *> result;

    for (const auto &profile : profiles)
    {
        result.append(profile.get());
    }

    return result;
}

int RouterProfileManager::profileCount() const
{
    return static_cast<int>(profiles.size());
}

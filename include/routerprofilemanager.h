#pragma once

#include <QList>
#include <QString>
#include <memory>
#include <vector>

#include "routerprofile.h"

struct ProfileCandidate
{
    RouterProfile *profile = nullptr;
    ProfileMatchResult match;
};

class RouterProfileManager
{
public:
    RouterProfileManager() = default;

    void registerProfile(std::unique_ptr<RouterProfile> profile);

    QList<ProfileCandidate> findMatchingProfiles(
        const RouterIdentity &identity) const;

    RouterProfile *findProfile(const QString &profileId) const;

    QList<RouterProfile *> registeredProfiles() const;

    int profileCount() const;

private:
    std::vector<std::unique_ptr<RouterProfile>> profiles;
};
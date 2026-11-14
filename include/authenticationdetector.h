#pragma once

#include <QString>
#include <QStringList>

enum class AuthenticationType
{
    PasswordOnly,
    UsernameAndPassword,
    Unsupported
};

struct AuthenticationInfo
{
    AuthenticationType type = AuthenticationType::Unsupported;

    QString formAction;
    QString formMethod;

    QString passwordField;
    QString usernameField;

    QStringList hiddenFields;

    bool formFound = false;
    QString explanation;
};

class AuthenticationDetector
{
public:
    static AuthenticationInfo inspect(const QString &html);
};
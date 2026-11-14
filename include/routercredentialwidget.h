
#pragma once

#include <QWidget>
#include "authenticationdetector.h"

class QLabel;
class QLineEdit;
class QPushButton;

class RouterCredentialWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RouterCredentialWidget(QWidget *parent = nullptr);

    void setAuthenticationInfo(const AuthenticationInfo &info);

    QString username() const;
    QString password() const;

signals:
    void connectRequested(const QString &username,
                          const QString &password);

private:
    QLabel *authenticationStatus;
    QLabel *usernameLabel;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QPushButton *connectButton;

    AuthenticationType currentType =
        AuthenticationType::Unsupported;
};

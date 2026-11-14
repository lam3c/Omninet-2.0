#ifndef ROUTERCREDENTIALSWIDGET_H
#define ROUTERCREDENTIALSWIDGET_H

#include <QDialog>
#include <QString>
#include <QUrl>

class QLabel;
class QLineEdit;
class QPushButton;
class RouterAuthenticator;

class RouterCredentialWidget : public QDialog
{
    Q_OBJECT

public:
    explicit RouterCredentialWidget(const QUrl &routerUrl,
                                    RouterAuthenticator *authenticator,
                                    QWidget *parent = nullptr);

signals:
    void authenticationCompleted(bool success, const QString &message);

public slots:
    void reject() override;

private slots:
    void onLoginClicked();
    void handleProgress(const QString &message);
    void handleFinished(bool success, const QString &message);

private:
    void setBusy(bool busy);
    void showStatus(const QString &text, bool isError);

    QUrl m_routerUrl;
    RouterAuthenticator *m_authenticator = nullptr;
    bool m_completed = false;
    bool m_busy = false;

    QLineEdit   *m_passwordEdit = nullptr;
    QPushButton *m_loginButton  = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QLabel      *m_statusLabel  = nullptr;
};

#endif // ROUTERCREDENTIALSWIDGET_H
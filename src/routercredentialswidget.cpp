#include "routercredentialswidget.h"
#include "routerauthenticator.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

RouterCredentialWidget::RouterCredentialWidget(const QUrl &routerUrl,
                                               RouterAuthenticator *authenticator,
                                               QWidget *parent)
    : QDialog(parent)
    , m_routerUrl(routerUrl)
    , m_authenticator(authenticator)
{
    setWindowTitle(tr("Router Login"));
    setModal(true);
    setMinimumWidth(420);

    auto *heading = new QLabel(tr("Sign in to your router"), this);
    heading->setObjectName("sectionTitle");

    auto *target = new QLabel(m_routerUrl.toString(), this);
    target->setObjectName("mutedText");

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Router admin password"));

    auto *form = new QFormLayout;
    form->addRow(tr("Password:"), m_passwordEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    m_loginButton = new QPushButton(tr("Sign In"), this);
    m_loginButton->setObjectName("primaryButton");
    m_loginButton->setDefault(true);

    m_cancelButton = new QPushButton(tr("Cancel"), this);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_cancelButton);
    buttons->addWidget(m_loginButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 22);
    layout->setSpacing(12);
    layout->addWidget(heading);
    layout->addWidget(target);
    layout->addLayout(form);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttons);

    connect(m_loginButton, &QPushButton::clicked,
            this, &RouterCredentialWidget::onLoginClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &RouterCredentialWidget::reject);
    connect(m_passwordEdit, &QLineEdit::returnPressed,
            this, &RouterCredentialWidget::onLoginClicked);

    if (m_authenticator) {
        connect(m_authenticator, &RouterAuthenticator::progressChanged,
                this, &RouterCredentialWidget::handleProgress);
        connect(m_authenticator, &RouterAuthenticator::authenticationFinished,
                this, &RouterCredentialWidget::handleFinished);
    }

    m_passwordEdit->setFocus();
}

void RouterCredentialWidget::onLoginClicked()
{
    if (m_busy)
        return;

    if (!m_authenticator) {
        showStatus(tr("No authenticator is available."), true);
        return;
    }

    if (m_passwordEdit->text().isEmpty()) {
        showStatus(tr("Please enter the router password."), true);
        m_passwordEdit->setFocus();
        return;
    }

    showStatus(tr("Signing in..."), false);
    setBusy(true);

    m_authenticator->authenticate(m_routerUrl, m_passwordEdit->text());
}

void RouterCredentialWidget::handleProgress(const QString &message)
{
    if (m_busy)
        showStatus(message, false);
}

void RouterCredentialWidget::handleFinished(bool success, const QString &message)
{
    if (m_completed)
        return;

    if (!success) {
        // Stay open so the user can correct the password and try again.
        setBusy(false);
        showStatus(message.isEmpty() ? tr("Authentication failed.") : message, true);
        m_passwordEdit->selectAll();
        m_passwordEdit->setFocus();
        return;
    }

    m_completed = true;
    emit authenticationCompleted(true, message);
    accept();
}

void RouterCredentialWidget::reject()
{
    if (!m_completed) {
        m_completed = true;

        if (m_busy && m_authenticator)
            m_authenticator->cancel();

        emit authenticationCompleted(false, tr("Authentication was cancelled."));
    }

    QDialog::reject();
}

void RouterCredentialWidget::setBusy(bool busy)
{
    m_busy = busy;
    m_loginButton->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_loginButton->setText(busy ? tr("Signing in...") : tr("Sign In"));
}

void RouterCredentialWidget::showStatus(const QString &text, bool isError)
{
    m_statusLabel->setStyleSheet(isError ? "color: #FB7185;" : "color: #8BA4B7;");
    m_statusLabel->setText(text);
    m_statusLabel->setVisible(!text.isEmpty());
}
#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

class QFormLayout;
class QFrame;
class QLabel;
class QPushButton;
class QTextEdit;

class RouterSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RouterSettingsWidget(QWidget *parent = nullptr);

public slots:
    void displayWifiSettings(const QJsonObject &settings);

    // Parameter is "isLoading" so it does not shadow the "loading" member.
    void setLoading(bool isLoading);
    void showError(const QString &message);

private:
    // Status
    QLabel *statusLabel = nullptr;

    // Summary card
    QLabel *heroSsid = nullptr;
    QLabel *stateBadge = nullptr;
    QLabel *securityBadge = nullptr;
    QLabel *channelBadge = nullptr;
    QLabel *modeBadge = nullptr;

    // Wireless network card
    QLabel *ssidValue = nullptr;
    QLabel *passwordValue = nullptr;
    QLabel *securityValue = nullptr;
    QLabel *channelValue = nullptr;
    QLabel *stateValue = nullptr;
    QPushButton *copySsidButton = nullptr;
    QPushButton *togglePasswordButton = nullptr;
    QPushButton *copyPasswordButton = nullptr;

    // Radio & power card
    QLabel *powerValue = nullptr;
    QLabel *powerGearValue = nullptr;

    // Mode & schedule card
    QLabel *modeValue = nullptr;
    QLabel *offTimeValue = nullptr;

    // Additional settings
    QFrame *additionalCard = nullptr;
    QFormLayout *additionalForm = nullptr;

    // Diagnostics
    QPushButton *rawToggleButton = nullptr;
    QTextEdit *rawSettingsView = nullptr;

    // State
    QJsonObject originalSettings;
    bool settingsLoaded = false;
    bool loading = false;
    bool passwordVisible = false;
    QString wifiSsid;
    QString wifiPassword;

    // UI helpers
    QFrame *createCard(const QString &title, QFormLayout **formOut);
    QLabel *addValueRow(QFormLayout *form, const QString &caption);
    void resetDisplay();
    void refreshPasswordLabel();
    void rebuildAdditionalSettings(const QJsonObject &settings);
    void setStatus(const QString &message, const QString &color);

    // JSON helpers
    QJsonValue findValue(
        const QJsonObject &object,
        const QStringList &possibleKeys,
        QString *matchedKey = nullptr) const;

    QString normaliseKey(const QString &key) const;
    QString valueToString(const QJsonValue &value) const;
};
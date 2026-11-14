#include "routersettingswidget.h"

#include <QClipboard>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// --------------------------------------------------
// KEY LISTS (shared by lookup and "additional settings")
// --------------------------------------------------

const QStringList kSsidKeys      {"wifiSSID", "ssid", "ssidName", "wifiName"};
const QStringList kPasswordKeys  {"wifiPwd", "wifiPassword", "password", "psk", "passphrase"};
const QStringList kSecurityKeys  {"wifiSecurityMode", "securityMode", "security",
                                "encryptType", "encryption", "authMode"};
const QStringList kChannelKeys   {"wifiChannel", "channel", "wirelessChannel"};
const QStringList kEnabledKeys   {"wifiEn", "wifiEnable", "wirelessEnable", "enableWifi"};
const QStringList kPowerKeys     {"wifiPower", "txPower", "wifiTxPower"};
const QStringList kPowerGearKeys {"wifiPowerGear", "powerGear"};
const QStringList kModeKeys      {"wifiRelayType", "relayType"};
const QStringList kOffTimeKeys   {"wifiTimeClose", "wifiTimeOff"};

// --------------------------------------------------
// SMALL HELPERS
// --------------------------------------------------

QString dash()
{
    return QString(QChar(0x2014));
}

QString norm(const QString &key)
{
    QString result;
    for (const QChar character : key) {
        if (character.isLetterOrNumber())
            result.append(character.toLower());
    }
    return result;
}

bool isTruthy(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == "1" || v == "true" || v == "on" || v == "enabled" || v == "yes";
}

QString prettifyKey(const QString &key)
{
    QString spaced;
    for (int i = 0; i < key.size(); ++i) {
        const QChar c = key.at(i);

        if (c == QLatin1Char('_') || c == QLatin1Char('-')) {
            spaced += QLatin1Char(' ');
            continue;
        }

        if (i > 0 && c.isUpper()
            && (key.at(i - 1).isLower() || key.at(i - 1).isDigit())) {
            spaced += QLatin1Char(' ');
        }

        spaced += c;
    }

    static const QHash<QString, QString> special{
        {"wifi", "Wi-Fi"}, {"ssid", "SSID"}, {"ip", "IP"}, {"dns", "DNS"},
        {"mac", "MAC"},    {"wps", "WPS"},   {"wpa", "WPA"}, {"dhcp", "DHCP"},
        {"pwd", "Password"}, {"en", "Enabled"}, {"tx", "TX"}
    };

    QStringList words = spaced.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words) {
        const QString lower = word.toLower();
        if (special.contains(lower))
            word = special.value(lower);
        else
            word = lower.left(1).toUpper() + lower.mid(1);
    }

    return words.join(QLatin1Char(' '));
}

QString prettifyValue(const QString &raw)
{
    QString text = raw.trimmed();
    text.replace(QLatin1Char('_'), QLatin1Char(' '));

    if (!text.isEmpty() && text.at(0).isLetter())
        text[0] = text.at(0).toUpper();

    return text;
}

// --------------------------------------------------
// VALUE TRANSLATION
// --------------------------------------------------

QString securityLabel(const QString &raw, QString *kind)
{
    const QString key = norm(raw);
    *kind = "ok";

    if (key.isEmpty() || key == "none" || key == "open"
        || key == "disable" || key == "disabled") {
        *kind = "warn";
        return "Open (no password)";
    }

    if (key.contains("wpa3") && key.contains("wpa2"))
        return "WPA2/WPA3 Mixed";

    if (key.contains("wpa3"))
        return "WPA3";

    if (key.count("wpa") >= 2)
        return "WPA/WPA2 Mixed";

    if (key.contains("wpa2"))
        return "WPA2-PSK";

    if (key.contains("wpa")) {
        *kind = "warn";
        return "WPA-PSK (legacy)";
    }

    if (key.contains("wep")) {
        *kind = "warn";
        return "WEP (insecure)";
    }

    *kind = "neutral";
    return prettifyValue(raw);
}

QString channelLabel(const QString &raw)
{
    const QString value = raw.trimmed();

    if (value.compare("auto", Qt::CaseInsensitive) == 0 || value == "0")
        return "Auto";

    bool ok = false;
    const int number = value.toInt(&ok);

    if (ok)
        return QString("Channel %1").arg(number);

    return prettifyValue(value);
}

QString powerLabel(const QString &raw)
{
    const QString v = raw.trimmed().toLower();

    if (v == "high")
        return "High";
    if (v == "mid" || v == "middle" || v == "medium")
        return "Medium";
    if (v == "low")
        return "Low";

    return prettifyValue(raw);
}

QString modeLabel(const QString &raw)
{
    const QString key = norm(raw);

    if (key == "ap")
        return "Access Point";
    if (key == "wisp")
        return "WISP (wireless ISP client)";
    if (key == "apclient" || key == "clientap")
        return "Access Point + Client";
    if (key == "wds")
        return "WDS bridge";
    if (key == "client")
        return "Wireless client";

    return prettifyValue(raw);
}

QString scheduleLabel(const QString &raw)
{
    const QStringList parts = raw.split(QLatin1Char('-'));

    if (parts.size() == 2
        && parts[0].contains(QLatin1Char(':'))
        && parts[1].contains(QLatin1Char(':'))) {

        return parts[0].trimmed()
        + QStringLiteral(" ") + QChar(0x2013) + QStringLiteral(" ")
            + parts[1].trimmed();
    }

    return raw;
}

// --------------------------------------------------
// STYLING HELPERS
// --------------------------------------------------

void setValueText(QLabel *label, const QString &text)
{
    if (text.isEmpty()) {
        label->setText("Not reported");
        label->setStyleSheet("color: #54627A; font-style: italic;");
    } else {
        label->setText(text);
        label->setStyleSheet(QString());
    }
}

void setBadge(QLabel *label, const QString &text, const QString &kind)
{
    QString fg = "#9AA8BF";
    QString bg = "rgba(154, 168, 191, 26)";
    QString border = "#2A3950";

    if (kind == "ok") {
        fg = "#34D399";
        bg = "rgba(52, 211, 153, 26)";
        border = "#175E45";
    } else if (kind == "warn") {
        fg = "#FBBF24";
        bg = "rgba(251, 191, 36, 26)";
        border = "#6B5311";
    } else if (kind == "bad") {
        fg = "#F87171";
        bg = "rgba(248, 113, 113, 26)";
        border = "#7A2E2E";
    }

    label->setText(text);
    label->setStyleSheet(
        QString("QLabel { color: %1; background: %2; border: 1px solid %3;"
                " border-radius: 10px; padding: 3px 10px;"
                " font-size: 11px; font-weight: 700; }")
            .arg(fg, bg, border)
        );
}

// --------------------------------------------------
// COLLECT "OTHER" SETTINGS
// --------------------------------------------------

void collectLeaves(const QJsonObject &object,
                   const QSet<QString> &consumed,
                   QList<QPair<QString, QString>> &out)
{
    for (auto it = object.begin(); it != object.end(); ++it) {

        const QJsonValue &value = it.value();

        if (value.isObject()) {
            collectLeaves(value.toObject(), consumed, out);
            continue;
        }

        const QString key = norm(it.key());

        if (consumed.contains(key))
            continue;

        QString text;

        if (value.isArray()) {
            const QJsonArray array = value.toArray();
            QStringList items;
            bool simple = true;

            for (const QJsonValue &item : array) {
                if (item.isObject() || item.isArray()) {
                    simple = false;
                    break;
                }
                items << (item.isString() ? item.toString()
                                          : QString::number(item.toDouble()));
            }

            text = simple ? items.join(", ")
                          : QString("%1 entries").arg(array.size());
        }
        else if (value.isBool()) {
            text = value.toBool() ? "Yes" : "No";
        }
        else if (value.isDouble()) {
            text = QString::number(value.toDouble());
        }
        else if (value.isString()) {
            text = value.toString().trimmed();

            if (text.compare("true", Qt::CaseInsensitive) == 0)
                text = "Yes";
            else if (text.compare("false", Qt::CaseInsensitive) == 0)
                text = "No";
        }
        else {
            continue;
        }

        const bool secret = key.contains("pwd") || key.contains("password")
                            || key.contains("psk") || key.contains("passphrase");

        if (secret && !text.isEmpty())
            text = QString(10, QChar(0x2022));

        out.append(qMakePair(prettifyKey(it.key()), text));
    }
}

} // namespace

// --------------------------------------------------
// CONSTRUCTOR
// --------------------------------------------------

RouterSettingsWidget::RouterSettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("routerSettingsWidget");

    setStyleSheet(R"QSS(
        QLabel#settingsValue {
            color: #D9E8F1;
            font-family: "Consolas";
            font-size: 13px;
        }
        QLabel#settingsHero {
            color: #F1F7FB;
            font-size: 24px;
            font-weight: 800;
        }
        QPushButton#miniButton {
            padding: 5px 12px;
            font-size: 12px;
        }
        QTextEdit#routerRawSettings {
            font-family: "Consolas";
            font-size: 12px;
            color: #9BB5C7;
        }
    )QSS");

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scrollArea->viewport()->setAutoFillBackground(false);

    QWidget *content = new QWidget;
    content->setObjectName("routerSettingsContent");
    content->setAutoFillBackground(false);
    content->setStyleSheet("QWidget#routerSettingsContent { background: transparent; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(0, 4, 10, 22);
    mainLayout->setSpacing(12);

    // ---------------- STATUS ----------------

    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);

    QLabel *readOnlyNote = new QLabel(
        "Showing the configuration reported by your router. "
        "Editing is not available yet."
        );
    readOnlyNote->setObjectName("mutedText");
    readOnlyNote->setWordWrap(true);

    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(readOnlyNote);

    // ---------------- SUMMARY CARD ----------------

    QFrame *hero = new QFrame;
    hero->setObjectName("card");

    QVBoxLayout *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(22, 18, 22, 18);
    heroLayout->setSpacing(8);

    QLabel *heroCaption = new QLabel("WIRELESS NETWORK");
    heroCaption->setObjectName("cardTitle");

    heroSsid = new QLabel;
    heroSsid->setObjectName("settingsHero");
    heroSsid->setTextInteractionFlags(Qt::TextSelectableByMouse);

    stateBadge = new QLabel;
    securityBadge = new QLabel;
    channelBadge = new QLabel;
    modeBadge = new QLabel;

    QHBoxLayout *badgeLayout = new QHBoxLayout;
    badgeLayout->setSpacing(8);
    badgeLayout->addWidget(stateBadge);
    badgeLayout->addWidget(securityBadge);
    badgeLayout->addWidget(channelBadge);
    badgeLayout->addWidget(modeBadge);
    badgeLayout->addStretch();

    heroLayout->addWidget(heroCaption);
    heroLayout->addWidget(heroSsid);
    heroLayout->addLayout(badgeLayout);

    mainLayout->addWidget(hero);

    // ---------------- WIRELESS NETWORK CARD ----------------

    QFormLayout *networkForm = nullptr;
    QFrame *networkCard = createCard("Wireless network", &networkForm);

    // SSID row
    QLabel *ssidCaption = new QLabel("Network name (SSID)");
    ssidCaption->setObjectName("mutedText");
    ssidCaption->setMinimumWidth(130);

    ssidValue = new QLabel;
    ssidValue->setObjectName("settingsValue");
    ssidValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

    copySsidButton = new QPushButton("Copy");
    copySsidButton->setObjectName("miniButton");
    copySsidButton->setCursor(Qt::PointingHandCursor);

    QWidget *ssidRow = new QWidget;
    QHBoxLayout *ssidRowLayout = new QHBoxLayout(ssidRow);
    ssidRowLayout->setContentsMargins(0, 0, 0, 0);
    ssidRowLayout->setSpacing(8);
    ssidRowLayout->addWidget(ssidValue, 1);
    ssidRowLayout->addWidget(copySsidButton);

    networkForm->addRow(ssidCaption, ssidRow);

    // Password row
    QLabel *passwordCaption = new QLabel("Wi-Fi password");
    passwordCaption->setObjectName("mutedText");
    passwordCaption->setMinimumWidth(130);

    passwordValue = new QLabel;
    passwordValue->setObjectName("settingsValue");
    passwordValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

    togglePasswordButton = new QPushButton("Show");
    togglePasswordButton->setObjectName("miniButton");
    togglePasswordButton->setCursor(Qt::PointingHandCursor);

    copyPasswordButton = new QPushButton("Copy");
    copyPasswordButton->setObjectName("miniButton");
    copyPasswordButton->setCursor(Qt::PointingHandCursor);

    QWidget *passwordRow = new QWidget;
    QHBoxLayout *passwordRowLayout = new QHBoxLayout(passwordRow);
    passwordRowLayout->setContentsMargins(0, 0, 0, 0);
    passwordRowLayout->setSpacing(8);
    passwordRowLayout->addWidget(passwordValue, 1);
    passwordRowLayout->addWidget(togglePasswordButton);
    passwordRowLayout->addWidget(copyPasswordButton);

    networkForm->addRow(passwordCaption, passwordRow);

    securityValue = addValueRow(networkForm, "Security");
    channelValue  = addValueRow(networkForm, "Channel");
    stateValue    = addValueRow(networkForm, "Wi-Fi status");

    mainLayout->addWidget(networkCard);

    // ---------------- RADIO + MODE CARDS ----------------

    QFormLayout *radioForm = nullptr;
    QFrame *radioCard = createCard("Radio & power", &radioForm);
    powerValue     = addValueRow(radioForm, "Transmit power");
    powerGearValue = addValueRow(radioForm, "Power gear");

    QFormLayout *modeForm = nullptr;
    QFrame *modeCard = createCard("Mode & schedule", &modeForm);
    modeValue    = addValueRow(modeForm, "Operating mode");
    offTimeValue = addValueRow(modeForm, "Scheduled off-time");

    QHBoxLayout *columns = new QHBoxLayout;
    columns->setSpacing(14);
    columns->addWidget(radioCard, 1, Qt::AlignTop);
    columns->addWidget(modeCard, 1, Qt::AlignTop);

    mainLayout->addLayout(columns);

    // ---------------- ADDITIONAL SETTINGS ----------------

    additionalCard = createCard("Additional settings", &additionalForm);
    additionalCard->hide();

    mainLayout->addWidget(additionalCard);

    // ---------------- DIAGNOSTICS ----------------

    rawToggleButton = new QPushButton("Show raw router response");
    rawToggleButton->setObjectName("miniButton");
    rawToggleButton->setCheckable(true);
    rawToggleButton->setCursor(Qt::PointingHandCursor);

    rawSettingsView = new QTextEdit;
    rawSettingsView->setObjectName("routerRawSettings");
    rawSettingsView->setReadOnly(true);
    rawSettingsView->setLineWrapMode(QTextEdit::NoWrap);
    rawSettingsView->setMinimumHeight(220);
    rawSettingsView->setPlaceholderText(
        "The router response will appear here after retrieval."
        );
    rawSettingsView->hide();

    QHBoxLayout *rawButtonRow = new QHBoxLayout;
    rawButtonRow->addWidget(rawToggleButton);
    rawButtonRow->addStretch();

    mainLayout->addLayout(rawButtonRow);
    mainLayout->addWidget(rawSettingsView);
    mainLayout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    // ---------------- CONNECTIONS ----------------

    auto flash = [](QPushButton *button, const QString &original) {
        button->setText("Copied");
        QTimer::singleShot(1500, button, [button, original]() {
            button->setText(original);
        });
    };

    connect(copySsidButton, &QPushButton::clicked, this, [this, flash]() {
        if (wifiSsid.isEmpty())
            return;
        QGuiApplication::clipboard()->setText(wifiSsid);
        flash(copySsidButton, "Copy");
    });

    connect(copyPasswordButton, &QPushButton::clicked, this, [this, flash]() {
        if (wifiPassword.isEmpty())
            return;
        QGuiApplication::clipboard()->setText(wifiPassword);
        flash(copyPasswordButton, "Copy");
    });

    connect(togglePasswordButton, &QPushButton::clicked, this, [this]() {
        passwordVisible = !passwordVisible;
        refreshPasswordLabel();
    });

    connect(rawToggleButton, &QPushButton::toggled, this, [this](bool on) {
        rawSettingsView->setVisible(on);
        rawToggleButton->setText(on ? "Hide raw router response"
                                    : "Show raw router response");
    });

    resetDisplay();
    setStatus("Waiting for router settings...", "#7C8BA3");
}

// --------------------------------------------------
// UI HELPERS
// --------------------------------------------------

QFrame *RouterSettingsWidget::createCard(const QString &title, QFormLayout **formOut)
{
    QFrame *card = new QFrame;
    card->setObjectName("card");

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 18);
    layout->setSpacing(12);

    QLabel *heading = new QLabel(title);
    heading->setObjectName("sectionTitle");

    QFrame *divider = new QFrame;
    divider->setObjectName("contentDivider");
    divider->setFixedHeight(1);

    QFormLayout *form = new QFormLayout;
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    layout->addWidget(heading);
    layout->addWidget(divider);
    layout->addLayout(form);

    *formOut = form;
    return card;
}

QLabel *RouterSettingsWidget::addValueRow(QFormLayout *form, const QString &caption)
{
    QLabel *captionLabel = new QLabel(caption);
    captionLabel->setObjectName("mutedText");
    captionLabel->setMinimumWidth(130);

    QLabel *value = new QLabel(dash());
    value->setObjectName("settingsValue");
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);

    form->addRow(captionLabel, value);
    return value;
}

void RouterSettingsWidget::resetDisplay()
{
    auto placeholder = [](QLabel *label) {
        label->setText(dash());
        label->setStyleSheet("color: #54627A;");
    };

    heroSsid->setText(dash());
    setBadge(stateBadge, "Not loaded", "neutral");
    securityBadge->hide();
    channelBadge->hide();
    modeBadge->hide();

    placeholder(ssidValue);
    placeholder(passwordValue);
    placeholder(securityValue);
    placeholder(channelValue);
    placeholder(stateValue);
    placeholder(powerValue);
    placeholder(powerGearValue);
    placeholder(modeValue);
    placeholder(offTimeValue);

    copySsidButton->setEnabled(false);
    copyPasswordButton->setEnabled(false);
    togglePasswordButton->setEnabled(false);
}

void RouterSettingsWidget::refreshPasswordLabel()
{
    const bool hasPassword = !wifiPassword.isEmpty();

    copyPasswordButton->setEnabled(hasPassword);
    togglePasswordButton->setEnabled(hasPassword);

    if (!hasPassword) {
        setValueText(passwordValue, QString());
        togglePasswordButton->setText("Show");
        return;
    }

    passwordValue->setStyleSheet(QString());
    passwordValue->setText(passwordVisible ? wifiPassword
                                           : QString(10, QChar(0x2022)));
    togglePasswordButton->setText(passwordVisible ? "Hide" : "Show");
}

void RouterSettingsWidget::rebuildAdditionalSettings(const QJsonObject &settings)
{
    while (additionalForm->rowCount() > 0)
        additionalForm->removeRow(0);

    QSet<QString> consumed;
    const QList<QStringList> lists{
        kSsidKeys, kPasswordKeys, kSecurityKeys, kChannelKeys, kEnabledKeys,
        kPowerKeys, kPowerGearKeys, kModeKeys, kOffTimeKeys
    };

    for (const QStringList &list : lists) {
        for (const QString &key : list)
            consumed.insert(norm(key));
    }

    QList<QPair<QString, QString>> entries;
    collectLeaves(settings, consumed, entries);

    for (const auto &entry : entries) {
        QLabel *value = addValueRow(additionalForm, entry.first);
        setValueText(value, entry.second);
    }

    additionalCard->setVisible(!entries.isEmpty());
}

// --------------------------------------------------
// JSON HELPERS
// --------------------------------------------------

QString RouterSettingsWidget::normaliseKey(const QString &key) const
{
    return norm(key);
}

QString RouterSettingsWidget::valueToString(const QJsonValue &value) const
{
    if (value.isString())
        return value.toString();

    if (value.isBool())
        return value.toBool() ? "true" : "false";

    if (value.isDouble())
        return QString::number(value.toDouble());

    if (value.isObject()) {
        return QString::fromUtf8(
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)
            );
    }

    if (value.isArray()) {
        return QString::fromUtf8(
            QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
            );
    }

    return {};
}

QJsonValue RouterSettingsWidget::findValue(
    const QJsonObject &object,
    const QStringList &possibleKeys,
    QString *matchedKey
    ) const
{
    for (const QString &candidate : possibleKeys) {

        const QString target = normaliseKey(candidate);

        for (auto it = object.begin(); it != object.end(); ++it) {

            if (normaliseKey(it.key()) == target) {

                if (matchedKey)
                    *matchedKey = it.key();

                // Firmware often nests a value inside a same-named module.
                if (it.value().isObject()) {

                    const QJsonValue nested = findValue(
                        it.value().toObject(), possibleKeys, matchedKey
                        );

                    if (!nested.isUndefined())
                        return nested;
                }

                return it.value();
            }
        }
    }

    for (auto it = object.begin(); it != object.end(); ++it) {

        if (it.value().isObject()) {

            const QJsonValue result = findValue(
                it.value().toObject(), possibleKeys, matchedKey
                );

            if (!result.isUndefined())
                return result;
        }
    }

    return QJsonValue(QJsonValue::Undefined);
}

// --------------------------------------------------
// DISPLAY RETRIEVED SETTINGS
// --------------------------------------------------

void RouterSettingsWidget::displayWifiSettings(const QJsonObject &settings)
{
    originalSettings = settings;
    settingsLoaded = true;
    passwordVisible = false;

    auto get = [this, &settings](const QStringList &keys) -> QString {
        const QJsonValue value = findValue(settings, keys);

        if (value.isUndefined() || value.isNull()
            || value.isObject() || value.isArray())
            return QString();

        return valueToString(value).trimmed();
    };

    // ---- Network name ----
    wifiSsid = get(kSsidKeys);

    heroSsid->setText(wifiSsid.isEmpty() ? QString("SSID not reported") : wifiSsid);
    setValueText(ssidValue, wifiSsid);
    copySsidButton->setEnabled(!wifiSsid.isEmpty());

    // ---- Password ----
    wifiPassword = get(kPasswordKeys);
    refreshPasswordLabel();

    // ---- Wi-Fi on/off ----
    const QString enabledRaw = get(kEnabledKeys);

    if (enabledRaw.isEmpty()) {
        setValueText(stateValue, QString());
        setBadge(stateBadge, "Status unknown", "neutral");
    } else {
        const bool on = isTruthy(enabledRaw);

        setValueText(stateValue, on ? "Enabled" : "Disabled");
        stateValue->setStyleSheet(on ? "color: #34D399;" : "color: #F87171;");

        setBadge(stateBadge, on ? "Wi-Fi on" : "Wi-Fi off", on ? "ok" : "bad");
    }

    // ---- Security ----
    const QString securityRaw = get(kSecurityKeys);

    if (securityRaw.isEmpty()) {
        setValueText(securityValue, QString());
        securityBadge->hide();
    } else {
        QString kind;
        const QString text = securityLabel(securityRaw, &kind);

        setValueText(securityValue, text);

        if (kind == "warn")
            securityValue->setStyleSheet("color: #FBBF24;");

        setBadge(securityBadge, text, kind);
        securityBadge->show();
    }

    // ---- Channel ----
    const QString channelRaw = get(kChannelKeys);

    if (channelRaw.isEmpty()) {
        setValueText(channelValue, QString());
        channelBadge->hide();
    } else {
        const QString text = channelLabel(channelRaw);

        setValueText(channelValue, text);

        setBadge(channelBadge,
                 text.startsWith("Channel") ? text : "Channel: " + text,
                 "neutral");
        channelBadge->show();
    }

    // ---- Power ----
    const QString powerRaw = get(kPowerKeys);
    setValueText(powerValue, powerRaw.isEmpty() ? QString() : powerLabel(powerRaw));

    const QString gearRaw = get(kPowerGearKeys);
    setValueText(powerGearValue, gearRaw.isEmpty() ? QString() : prettifyValue(gearRaw));

    // ---- Mode ----
    const QString modeRaw = get(kModeKeys);

    if (modeRaw.isEmpty()) {
        setValueText(modeValue, QString());
        modeBadge->hide();
    } else {
        const QString text = modeLabel(modeRaw);

        setValueText(modeValue, text);
        setBadge(modeBadge, text, "neutral");
        modeBadge->show();
    }

    // ---- Schedule ----
    const QString offRaw = get(kOffTimeKeys);
    setValueText(offTimeValue, offRaw.isEmpty() ? QString() : scheduleLabel(offRaw));

    // ---- Everything else the router reported ----
    rebuildAdditionalSettings(settings);

    // ---- Diagnostics (hidden unless the user opens it) ----
    rawSettingsView->setPlainText(
        QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Indented))
        );

    setStatus("Wi-Fi configuration retrieved successfully.", "#34D399");
}

// --------------------------------------------------
// STATUS HANDLING
// --------------------------------------------------

void RouterSettingsWidget::setStatus(const QString &message, const QString &color)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(
        QString("color: %1; font-weight: 600;").arg(color)
        );
}

void RouterSettingsWidget::setLoading(bool isLoading)
{
    loading = isLoading;

    if (loading) {
        setStatus("Retrieving router configuration...", "#7C8BA3");
        return;
    }

    if (!settingsLoaded)
        setStatus("Router configuration retrieval finished.", "#7C8BA3");
}

void RouterSettingsWidget::showError(const QString &message)
{
    setStatus(message, "#F87171");
}
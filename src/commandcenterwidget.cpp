#include "commandcenterwidget.h"

#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QScrollArea>

CommandCenterWidget::CommandCenterWidget(QWidget *parent)
    : QWidget(parent)
{
    buildInterface();
}

QFrame *CommandCenterWidget::createMetricCard(
    const QString &title,
    const QString &value,
    const QString &description,
    QLabel **valueLabel)
{
    auto *card = new QFrame;
    card->setObjectName("commandMetricCard");

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(7);

    auto *heading = new QLabel(title);
    heading->setObjectName("commandMetricTitle");

    *valueLabel = new QLabel(value);
    (*valueLabel)->setObjectName("commandMetricValue");

    auto *note = new QLabel(description);
    note->setObjectName("commandMetricNote");
    note->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(*valueLabel);
    layout->addWidget(note);
    layout->addStretch();

    return card;
}

void CommandCenterWidget::buildInterface()
{
    setObjectName("commandCenter");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);

    auto *headingRow = new QHBoxLayout;

    auto *headingBox = new QVBoxLayout;

    auto *heading = new QLabel("Command Center");
    heading->setObjectName("commandHeading");

    auto *subtitle = new QLabel(
        "Your network operations overview and control panel."
        );
    subtitle->setObjectName("commandSubtitle");

    headingBox->addWidget(heading);
    headingBox->addWidget(subtitle);

    discoverButton = new QPushButton("Discover Network");
    discoverButton->setObjectName("primaryButton");
    discoverButton->setMinimumHeight(42);

    headingRow->addLayout(headingBox);
    headingRow->addStretch();
    headingRow->addWidget(discoverButton);

    root->addLayout(headingRow);

    connect(discoverButton, &QPushButton::clicked,
            this, &CommandCenterWidget::discoverNetworkRequested);

    auto *metrics = new QGridLayout;
    metrics->setSpacing(12);

    metrics->addWidget(
        createMetricCard(
            "NETWORK STATUS",
            "Offline",
            "Waiting for network detection",
            &networkValue
            ), 0, 0
        );

    metrics->addWidget(
        createMetricCard(
            "CONNECTED DEVICES",
            "0",
            "Devices discovered on your LAN",
            &devicesValue
            ), 0, 1
        );

    metrics->addWidget(
        createMetricCard(
            "ROUTER STATUS",
            "Unknown",
            "Router inspection status",
            &routerValue
            ), 0, 2
        );

    metrics->addWidget(
        createMetricCard(
            "ROUTER PROFILES",
            "0",
            "Registered router profiles",
            &profilesValue
            ), 0, 3
        );

    root->addLayout(metrics);

    auto *detailsGrid = new QGridLayout;
    detailsGrid->setSpacing(12);

    auto makePanel = [](const QString &title,
                        QLabel **label)
    {
        auto *panel = new QFrame;
        panel->setObjectName("commandPanel");

        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(10);

        auto *heading = new QLabel(title);
        heading->setObjectName("commandPanelTitle");

        *label = new QLabel("No information available.");
        (*label)->setObjectName("commandPanelText");
        (*label)->setWordWrap(true);

        layout->addWidget(heading);
        layout->addWidget(*label);
        layout->addStretch();

        return panel;
    };

    detailsGrid->addWidget(
        makePanel("NETWORK INFORMATION", &networkDetails),
        0, 0
        );

    detailsGrid->addWidget(
        makePanel("ROUTER INFORMATION", &routerDetails),
        0, 1
        );

    detailsGrid->addWidget(
        makePanel("ROUTER CAPTURE", &captureDetails),
        1, 0
        );

    detailsGrid->addWidget(
        makePanel("RECENT ACTIVITY", &activityDetails),
        1, 1
        );

    root->addLayout(detailsGrid, 1);

    auto *actions = new QGridLayout;
    actions->setSpacing(10);

    auto addAction = [&](const QString &label,
                         int row,
                         int column,
                         auto signal)
    {
        auto *button = new QPushButton(label);
        button->setMinimumHeight(42);

        actions->addWidget(button, row, column);

        connect(button, &QPushButton::clicked, this, signal);
    };

    addAction("Scan Devices", 0, 0,
              &CommandCenterWidget::scanDevicesRequested);

    addAction("Router Settings", 0, 1,
              &CommandCenterWidget::openRouterSettingsRequested);

    addAction("Connected Devices", 0, 2,
              &CommandCenterWidget::openDevicesRequested);

    addAction("Router Profiles", 1, 0,
              &CommandCenterWidget::openProfilesRequested);

    addAction("Router Capture", 1, 1,
              &CommandCenterWidget::openCaptureRequested);

    addAction("Live Monitor", 1, 2,
              &CommandCenterWidget::openLiveMonitorRequested);

    auto *wirelessButton = new QPushButton("Wireless Intelligence");
    wirelessButton->setObjectName("primaryButton");
    wirelessButton->setMinimumHeight(42);

    actions->addWidget(wirelessButton, 0, 3, 2, 1);

    connect(wirelessButton, &QPushButton::clicked,
            this,
            &CommandCenterWidget::openWirelessIntelligenceRequested);

    root->addLayout(actions);
}

void CommandCenterWidget::setNetworkState(
    bool online,
    const QString &adapter,
    const QString &ip,
    const QString &gateway)
{
    networkValue->setText(online ? "Online" : "Offline");

    networkDetails->setText(
        "Adapter: " + adapter +
        "\nLocal IP: " + ip +
        "\nGateway: " + gateway
        );
}

void CommandCenterWidget::setDeviceCount(int count)
{
    devicesValue->setText(QString::number(count));
}

void CommandCenterWidget::setProfileCount(int count)
{
    profilesValue->setText(QString::number(count));
}

void CommandCenterWidget::setRouterState(
    bool inspected,
    bool authenticated,
    const QString &manufacturer,
    const QString &model,
    const QString &message)
{
    if (authenticated)
        routerValue->setText("Authenticated");
    else if (inspected)
        routerValue->setText("Detected");
    else
        routerValue->setText("Unknown");

    routerDetails->setText(
        "Manufacturer: " + manufacturer +
        "\nModel: " + model +
        "\nStatus: " + message
        );
}

void CommandCenterWidget::setCaptureState(const QString &message)
{
    captureDetails->setText(message);
}

void CommandCenterWidget::addActivity(const QString &message)
{
    const QString timestamp =
        QDateTime::currentDateTime().toString("HH:mm:ss");

    const QString oldText = activityDetails->text();

    activityDetails->setText(
        timestamp + "  " + message +
        (oldText.isEmpty() ? "" : "\n" + oldText)
        );
}
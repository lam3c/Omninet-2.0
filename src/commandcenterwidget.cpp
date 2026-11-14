#include "commandcenterwidget.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

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
    card->setMinimumHeight(132);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(17, 15, 17, 15);
    layout->setSpacing(6);

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

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName("commandScroll");

    auto *content = new QWidget;
    content->setObjectName("commandContent");

    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(28, 24, 28, 25);
    root->setSpacing(15);

    auto *headingRow = new QHBoxLayout;
    auto *headingBox = new QVBoxLayout;
    headingBox->setSpacing(4);

    auto *eyebrow = new QLabel("OPERATIONS OVERVIEW");
    eyebrow->setObjectName("commandEyebrow");

    auto *heading = new QLabel("Command Center");
    heading->setObjectName("commandHeading");

    auto *subtitle = new QLabel(
        "A live overview of your local network, router and discovery activity."
    );
    subtitle->setObjectName("commandSubtitle");
    subtitle->setWordWrap(true);

    headingBox->addWidget(eyebrow);
    headingBox->addWidget(heading);
    headingBox->addWidget(subtitle);

    discoverButton = new QPushButton("Discover Network");
    discoverButton->setObjectName("primaryButton");
    discoverButton->setMinimumHeight(42);
    discoverButton->setCursor(Qt::PointingHandCursor);

    headingRow->addLayout(headingBox, 1);
    headingRow->addWidget(discoverButton, 0, Qt::AlignVCenter);
    root->addLayout(headingRow);

    connect(discoverButton, &QPushButton::clicked,
            this, &CommandCenterWidget::discoverNetworkRequested);

    auto *metrics = new QGridLayout;
    metrics->setHorizontalSpacing(11);
    metrics->setVerticalSpacing(11);

    metrics->addWidget(createMetricCard(
        "NETWORK STATUS", "Offline", "Waiting for network detection",
        &networkValue), 0, 0);
    metrics->addWidget(createMetricCard(
        "CONNECTED DEVICES", "0", "Responsive devices discovered on your LAN",
        &devicesValue), 0, 1);
    metrics->addWidget(createMetricCard(
        "ROUTER STATUS", "Unknown", "Inspection and authentication state",
        &routerValue), 0, 2);
    metrics->addWidget(createMetricCard(
        "ROUTER PROFILES", "0", "Registered compatibility profiles",
        &profilesValue), 0, 3);

    for (int column = 0; column < 4; ++column)
        metrics->setColumnStretch(column, 1);

    root->addLayout(metrics);

    auto makePanel = [](const QString &title, QLabel **label) {
        auto *panel = new QFrame;
        panel->setObjectName("commandPanel");
        panel->setMinimumHeight(128);

        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(17, 15, 17, 15);
        layout->setSpacing(9);

        auto *heading = new QLabel(title);
        heading->setObjectName("commandPanelTitle");

        *label = new QLabel("No information available.");
        (*label)->setObjectName("commandPanelText");
        (*label)->setWordWrap(true);
        (*label)->setTextInteractionFlags(Qt::TextSelectableByMouse);

        layout->addWidget(heading);
        layout->addWidget(*label);
        layout->addStretch();
        return panel;
    };

    auto *detailsGrid = new QGridLayout;
    detailsGrid->setSpacing(11);
    detailsGrid->addWidget(makePanel("NETWORK INFORMATION", &networkDetails), 0, 0);
    detailsGrid->addWidget(makePanel("ROUTER INFORMATION", &routerDetails), 0, 1);
    detailsGrid->addWidget(makePanel("CAPTURE STATUS", &captureDetails), 1, 0);
    detailsGrid->addWidget(makePanel("RECENT ACTIVITY", &activityDetails), 1, 1);
    detailsGrid->setColumnStretch(0, 1);
    detailsGrid->setColumnStretch(1, 1);
    root->addLayout(detailsGrid);

    auto *actionsPanel = new QFrame;
    actionsPanel->setObjectName("commandPanel");

    auto *actionsLayout = new QVBoxLayout(actionsPanel);
    actionsLayout->setContentsMargins(17, 15, 17, 16);
    actionsLayout->setSpacing(11);

    auto *actionsTitle = new QLabel("QUICK ACCESS");
    actionsTitle->setObjectName("commandPanelTitle");
    actionsLayout->addWidget(actionsTitle);

    auto *actions = new QGridLayout;
    actions->setSpacing(9);

    auto addAction = [&](const QString &label, int row, int column, auto signal) {
        auto *button = new QPushButton(label);
        button->setMinimumHeight(39);
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button, row, column);
        connect(button, &QPushButton::clicked, this, signal);
    };

    addAction("Scan Devices", 0, 0, &CommandCenterWidget::scanDevicesRequested);
    addAction("Router Settings", 0, 1, &CommandCenterWidget::openRouterSettingsRequested);
    addAction("Connected Devices", 0, 2, &CommandCenterWidget::openDevicesRequested);
    addAction("Router Profiles", 1, 0, &CommandCenterWidget::openProfilesRequested);
    addAction("Router Capture", 1, 1, &CommandCenterWidget::openCaptureRequested);
    addAction("Live Monitor", 1, 2, &CommandCenterWidget::openLiveMonitorRequested);

    auto *wirelessButton = new QPushButton("Wireless Intelligence");
    wirelessButton->setObjectName("primaryButton");
    wirelessButton->setMinimumHeight(87);
    wirelessButton->setCursor(Qt::PointingHandCursor);
    actions->addWidget(wirelessButton, 0, 3, 2, 1);
    connect(wirelessButton, &QPushButton::clicked,
            this, &CommandCenterWidget::openWirelessIntelligenceRequested);

    for (int column = 0; column < 4; ++column)
        actions->setColumnStretch(column, 1);

    actionsLayout->addLayout(actions);
    root->addWidget(actionsPanel);

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void CommandCenterWidget::setNetworkState(
    bool online, const QString &adapter, const QString &ip, const QString &gateway)
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
    bool inspected, bool authenticated, const QString &manufacturer,
    const QString &model, const QString &message)
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
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString oldText = activityDetails->text();

    activityDetails->setText(
        timestamp + "   " + message +
        (oldText.isEmpty() ? "" : "\n" + oldText)
    );
}

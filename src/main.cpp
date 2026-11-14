#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QStatusBar>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QProgressBar>
#include <QAbstractItemView>
#include <QList>
#include <QString>
#include <QUrl>
#include <QSizePolicy>
#include <QFont>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QGuiApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QIcon>
#include <QClipboard>
#include <QLineEdit>
#include <QFormLayout>
#include <QAbstractSocket>

#include <memory>

// OmniNet modules
#include "networkdetector.h"
#include "gatewayinspector.h"
#include "authenticationdetector.h"
#include "devicediscovery.h"
#include "routerauthenticator.h"
#include "routercredentialswidget.h"
#include "routersettingswidget.h"
#include "routercapture.h"
#include "livemonitorwidget.h"

// Router profile architecture
#include "routeridentity.h"
#include "routeridentification.h"
#include "routerprofile.h"
#include "routerprofilemanager.h"
#include "tendaf3profile.h"

// New dashboard modules
#include "commandcenterwidget.h"
#include "wirelessintelligencewidget.h"

#include "ipvalidator.h"

#include "routerconfig.h"


#include <QHostAddress>


class OmniNetWindow : public QMainWindow
{
public:
    explicit OmniNetWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("OmniNet Network Command Center");
        setWindowIcon(QIcon(":/icons/omninet.png"));

        setMinimumSize(1100, 720);
        resize(1450, 900);

        profileManager.registerProfile(
            std::make_unique<TendaF3Profile>()
            );

        buildInterface();
        connectSignals();
        applyStyle();
        refreshProfilesPage();

        commandCenter->setProfileCount(profileManager.profileCount());
        commandCenter->addActivity("OmniNet started");
    }

private:

    // --------------------------------------------------
    // NETWORK AND ROUTER COMPONENTS
    // --------------------------------------------------

    DeviceDiscovery deviceDiscovery;
    GatewayInspector gatewayInspector;
    RouterAuthenticator routerAuthenticator;
    RouterCapture routerCapture;

    NetworkInformation currentNetwork;
    bool networkAvailable = false;

    QString connectedRouterIp;
    bool routerAuthenticated = false;

    // --------------------------------------------------
    // ROUTER PROFILES
    // --------------------------------------------------

    RouterProfileManager profileManager;
    RouterIdentity currentRouterIdentity;

    // --------------------------------------------------
    // MAIN INTERFACE - FIRST PAGE
    // --------------------------------------------------

    QWidget *sidebar = nullptr;
    QStackedWidget *pages = nullptr;

    QList<QPushButton *> navigationButtons;

    CommandCenterWidget *commandCenter = nullptr;
    WirelessIntelligenceWidget *wirelessIntelligence = nullptr;

    RouterSettingsWidget *routerSettingsWidget = nullptr;
    LiveMonitorWidget *liveMonitor = nullptr;

    // --------------------------------------------------
    // DEVICES PAGE
    // --------------------------------------------------

    QPushButton *scanDevicesButton = nullptr;
    QLabel *deviceScanStatus = nullptr;
    QLabel *deviceCountLabel = nullptr;

    QProgressBar *deviceProgress = nullptr;
    QTableWidget *devicesTable = nullptr;

    // --------------------------------------------------
    // PROFILES PAGE
    // --------------------------------------------------

    QTableWidget *profilesTable = nullptr;
    QLabel *profileStatusLabel = nullptr;

    // --------------------------------------------------
    // ROUTER CAPTURE PAGE
    // --------------------------------------------------

    QPushButton *captureStartButton = nullptr;
    QPushButton *captureCancelButton = nullptr;
    QPushButton *captureCopyButton = nullptr;
    QPushButton *captureSaveButton = nullptr;

    QCheckBox *captureMaskMacs = nullptr;

    QProgressBar *captureProgress = nullptr;
    QLabel *captureStatus = nullptr;
    QPlainTextEdit *captureReportView = nullptr;

    // --------------------------------------------------
    // ROUTER CONFIGURATION LAB
    // --------------------------------------------------

    RouterConfig routerConfig;

    QLineEdit *routerNameEdit = nullptr;
    QLineEdit *routerIpEdit = nullptr;
    QLineEdit *routerUsernameEdit = nullptr;
    QLineEdit *routerPasswordEdit = nullptr;
    QLineEdit *routerFirmwareEdit = nullptr;

    QLabel *routerConfigStatus = nullptr;


    // ==================================================
    // PAGE HELPERS
    // ==================================================

    QWidget *createPage(
        const QString &title,
        const QString &description)
    {
        auto *page = new QWidget;

        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 25, 28, 25);
        layout->setSpacing(15);

        auto *heading = new QLabel(title);
        heading->setObjectName("pageTitle");

        auto *subtitle = new QLabel(description);
        subtitle->setObjectName("pageDescription");
        subtitle->setWordWrap(true);

        layout->addWidget(heading);
        layout->addWidget(subtitle);

        return page;
    }

    QFrame *createCard(const QString &title)
    {
        auto *card = new QFrame;
        card->setObjectName("card");

        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *heading = new QLabel(title);
        heading->setObjectName("sectionTitle");

        layout->addWidget(heading);

        return card;
    }


    // ==================================================
    // NAVIGATION PAGE
    // ==================================================

    QPushButton *createNavigationButton(
        const QString &text,
        int index)
    {
        auto *button = new QPushButton(text);

        button->setObjectName("navigationButton");
        button->setMinimumHeight(43);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);

        navigationButtons.append(button);

        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, index]()
            {
                setCurrentPage(index);
            }
            );

        return button;
    }

    void setCurrentPage(int index)
    {
        if (!pages || index < 0 || index >= pages->count())
            return;

        pages->setCurrentIndex(index);

        for (int i = 0; i < navigationButtons.size(); ++i)
        {
            navigationButtons[i]->setChecked(i == index);
        }
    }


    // ==================================================
    // MAIN INTERFACE
    // ==================================================

    void buildInterface()
    {
        auto *central = new QWidget(this);
        central->setObjectName("centralArea");
        setCentralWidget(central);

        auto *mainLayout = new QHBoxLayout(central);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        sidebar = new QWidget;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(246);

        auto *sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setContentsMargins(19, 24, 19, 18);
        sidebarLayout->setSpacing(7);

        auto *brandRow = new QHBoxLayout;
        brandRow->setSpacing(10);

        auto *brandMark = new QLabel("◈");
        brandMark->setObjectName("brandMark");
        brandMark->setAlignment(Qt::AlignCenter);
        brandMark->setFixedSize(39, 39);

        auto *brandText = new QVBoxLayout;
        brandText->setSpacing(1);

        auto *brand = new QLabel("OmniNet");
        brand->setObjectName("brand");

        auto *brandSubtitle = new QLabel("NETWORK INTELLIGENCE");
        brandSubtitle->setObjectName("brandSubtitle");

        brandText->addWidget(brand);
        brandText->addWidget(brandSubtitle);
        brandRow->addWidget(brandMark);
        brandRow->addLayout(brandText);
        brandRow->addStretch();

        sidebarLayout->addLayout(brandRow);
        sidebarLayout->addSpacing(29);

        auto *section = new QLabel("WORKSPACE");
        section->setObjectName("sidebarSection");
        sidebarLayout->addWidget(section);
        sidebarLayout->addSpacing(5);

        sidebarLayout->addWidget(createNavigationButton("◈   Command Center", 0));
        sidebarLayout->addWidget(createNavigationButton("⌘   Router Settings", 1));
        sidebarLayout->addWidget(createNavigationButton("▦   Connected Devices", 2));
        sidebarLayout->addWidget(createNavigationButton("▣   Router Profiles", 3));
        sidebarLayout->addWidget(createNavigationButton("◉   Router Capture", 4));
        sidebarLayout->addWidget(createNavigationButton("⌁   Live Monitor", 5));
        sidebarLayout->addWidget(createNavigationButton("⌖   Wireless Intelligence", 6));
        sidebarLayout->addWidget(createNavigationButton("⚙   Configuration Lab", 7));

        sidebarLayout->addStretch();

        auto *divider = new QFrame;
        divider->setObjectName("sidebarDivider");
        divider->setFixedHeight(1);
        sidebarLayout->addWidget(divider);

        auto *footerTitle = new QLabel("LOCAL OPERATIONS");
        footerTitle->setObjectName("sidebarFooterTitle");

        auto *connectionLabel = new QLabel("Ready for network discovery");
        connectionLabel->setObjectName("connectionStatus");
        connectionLabel->setWordWrap(true);

        sidebarLayout->addSpacing(10);
        sidebarLayout->addWidget(footerTitle);
        sidebarLayout->addWidget(connectionLabel);

        auto *workspace = new QWidget;
        workspace->setObjectName("workspace");

        auto *workspaceLayout = new QVBoxLayout(workspace);
        workspaceLayout->setContentsMargins(0, 0, 0, 0);
        workspaceLayout->setSpacing(0);

        auto *topBar = new QFrame;
        topBar->setObjectName("topBar");
        topBar->setFixedHeight(68);

        auto *topBarLayout = new QHBoxLayout(topBar);
        topBarLayout->setContentsMargins(28, 10, 28, 10);

        auto *contextBox = new QVBoxLayout;
        contextBox->setSpacing(2);

        auto *contextTitle = new QLabel("NETWORK INTELLIGENCE");
        contextTitle->setObjectName("topBarTitle");

        auto *contextSubtitle = new QLabel("OmniNet  /  Operations workspace");
        contextSubtitle->setObjectName("topBarSubtitle");

        contextBox->addWidget(contextTitle);
        contextBox->addWidget(contextSubtitle);

        auto *versionLabel = new QLabel("DESKTOP CLIENT   •   v1.4.0");
        versionLabel->setObjectName("topBarMeta");

        topBarLayout->addLayout(contextBox);
        topBarLayout->addStretch();
        topBarLayout->addWidget(versionLabel);

        pages = new QStackedWidget;
        pages->setObjectName("pageStack");

        pages->addWidget(buildDashboardPage());
        pages->addWidget(buildSettingsPage());
        pages->addWidget(buildDevicesPage());
        pages->addWidget(buildProfilesPage());
        pages->addWidget(buildCapturePage());
        pages->addWidget(buildLiveMonitorPage());
        pages->addWidget(buildWirelessPage());
        pages->addWidget(buildRouterConfigPage());

        workspaceLayout->addWidget(topBar);
        workspaceLayout->addWidget(pages, 1);

        mainLayout->addWidget(sidebar);
        mainLayout->addWidget(workspace, 1);

        setCurrentPage(0);
        statusBar()->showMessage("OmniNet is ready");
    }


    // ==================================================
    // COMMAND CENTER
    // ==================================================

    QWidget *buildDashboardPage()
    {
        commandCenter = new CommandCenterWidget;

        return commandCenter;
    }


    // ==================================================
    // ROUTER SETTINGS
    // ==================================================

    QWidget *buildSettingsPage()
    {
        auto *page = createPage(
            "Router Settings",
            "View the configuration returned by your authenticated router."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        routerSettingsWidget = new RouterSettingsWidget(page);

        routerSettingsWidget->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Expanding
            );

        layout->addWidget(routerSettingsWidget, 1);

        return page;
    }


    // ==================================================
    // CONNECTED DEVICES
    // ==================================================

    QWidget *buildDevicesPage()
    {
        auto *page = createPage(
            "Connected Devices",
            "Discover responsive devices on your local network."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        auto *card = createCard("Local Network Discovery");

        auto *cardLayout =
            qobject_cast<QVBoxLayout *>(card->layout());

        auto *controls = new QHBoxLayout;

        scanDevicesButton = new QPushButton("Scan Network");
        scanDevicesButton->setObjectName("primaryButton");
        scanDevicesButton->setMinimumHeight(40);

        deviceCountLabel = new QLabel("0 devices discovered");
        deviceCountLabel->setObjectName("mutedText");

        controls->addWidget(scanDevicesButton);
        controls->addStretch();
        controls->addWidget(deviceCountLabel);

        cardLayout->addLayout(controls);

        deviceProgress = new QProgressBar;
        deviceProgress->setRange(0, 1);
        deviceProgress->setValue(0);
        deviceProgress->setTextVisible(false);
        deviceProgress->setFixedHeight(5);

        cardLayout->addWidget(deviceProgress);

        deviceScanStatus = new QLabel(
            "Ready to scan your local network."
            );
        deviceScanStatus->setObjectName("bodyText");
        deviceScanStatus->setWordWrap(true);

        cardLayout->addWidget(deviceScanStatus);

        devicesTable = new QTableWidget(0, 4);

        devicesTable->setHorizontalHeaderLabels({
            "IP ADDRESS",
            "MAC ADDRESS",
            "HOSTNAME",
            "STATUS"
        });

        devicesTable->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
            );

        devicesTable->setSelectionBehavior(
            QAbstractItemView::SelectRows
            );

        devicesTable->setEditTriggers(
            QAbstractItemView::NoEditTriggers
            );

        devicesTable->setAlternatingRowColors(true);
        devicesTable->setShowGrid(false);
        devicesTable->verticalHeader()->setVisible(false);
        devicesTable->setMinimumHeight(320);

        cardLayout->addWidget(devicesTable, 1);

        layout->addWidget(card, 1);

        return page;
    }


    // ==================================================
    // ROUTER PROFILES
    // ==================================================

    QWidget *buildProfilesPage()
    {
        auto *page = createPage(
            "Router Profiles",
            "Router identification and registered compatibility profiles."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        auto *card = createCard("Registered Router Profiles");

        auto *cardLayout =
            qobject_cast<QVBoxLayout *>(card->layout());

        profileStatusLabel = new QLabel(
            "Waiting for router identification."
            );

        profileStatusLabel->setObjectName("bodyText");
        profileStatusLabel->setWordWrap(true);

        profilesTable = new QTableWidget(0, 5);

        profilesTable->setHorizontalHeaderLabels({
            "PROFILE",
            "MANUFACTURER",
            "MATCH",
            "CONFIDENCE",
            "COMPATIBILITY"
        });

        profilesTable->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
            );

        profilesTable->setSelectionBehavior(
            QAbstractItemView::SelectRows
            );

        profilesTable->setEditTriggers(
            QAbstractItemView::NoEditTriggers
            );

        profilesTable->setAlternatingRowColors(true);
        profilesTable->setShowGrid(false);
        profilesTable->verticalHeader()->setVisible(false);
        profilesTable->setMinimumHeight(260);

        cardLayout->addWidget(profileStatusLabel);
        cardLayout->addWidget(profilesTable);

        layout->addWidget(card, 1);

        return page;
    }


    // ==================================================
    // ROUTER CAPTURE
    // ==================================================

    QWidget *buildCapturePage()
    {
        auto *page = createPage(
            "Router Capture",
            "Inspect the router's web interface and generate a safe report."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        auto *card = createCard("Read-only Router Capture");

        auto *cardLayout =
            qobject_cast<QVBoxLayout *>(card->layout());

        auto *safety = new QLabel(
            "Capture uses read-only GET requests. Sensitive values are "
            "redacted and potentially destructive pages are skipped. "
            "Authenticate with your router before starting."
            );

        safety->setObjectName("bodyText");
        safety->setWordWrap(true);

        captureStartButton = new QPushButton("Start Capture");
        captureStartButton->setObjectName("primaryButton");

        captureCancelButton = new QPushButton("Cancel");
        captureCancelButton->setEnabled(false);

        captureMaskMacs = new QCheckBox("Mask MAC addresses");
        captureMaskMacs->setChecked(true);

        captureCopyButton = new QPushButton("Copy Report");
        captureCopyButton->setEnabled(false);

        captureSaveButton = new QPushButton("Save as TXT");
        captureSaveButton->setEnabled(false);

        auto *controls = new QHBoxLayout;

        controls->addWidget(captureStartButton);
        controls->addWidget(captureCancelButton);
        controls->addWidget(captureMaskMacs);
        controls->addStretch();
        controls->addWidget(captureCopyButton);
        controls->addWidget(captureSaveButton);

        captureProgress = new QProgressBar;
        captureProgress->setRange(0, 1);
        captureProgress->setValue(0);
        captureProgress->setTextVisible(false);
        captureProgress->setFixedHeight(5);

        captureStatus = new QLabel(
            "Ready. Authenticate with your router before starting."
            );
        captureStatus->setObjectName("mutedText");
        captureStatus->setWordWrap(true);

        captureReportView = new QPlainTextEdit;
        captureReportView->setReadOnly(true);
        captureReportView->setLineWrapMode(QPlainTextEdit::NoWrap);
        captureReportView->setMinimumHeight(300);
        captureReportView->setPlaceholderText(
            "The router capture report will appear here."
            );

        cardLayout->addWidget(safety);
        cardLayout->addLayout(controls);
        cardLayout->addWidget(captureProgress);
        cardLayout->addWidget(captureStatus);
        cardLayout->addWidget(captureReportView, 1);

        layout->addWidget(card, 1);

        return page;
    }


    // ==================================================
    // LIVE MONITOR
    // ==================================================

    QWidget *buildLiveMonitorPage()
    {
        auto *page = createPage(
            "Live Monitor",
            "Monitor router traffic, local traffic and connected-device usage."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        liveMonitor = new LiveMonitorWidget(page);

        layout->addWidget(liveMonitor, 1);

        liveMonitor->start();

        return page;
    }


    // ==================================================
    // WIRELESS INTELLIGENCE
    // ==================================================

    QWidget *buildWirelessPage()
    {
        wirelessIntelligence =
            new WirelessIntelligenceWidget;

        return wirelessIntelligence;
    }


    // ==================================================
    // ROUTER CONFIGURATION LAB
    // ==================================================

    QWidget *buildRouterConfigPage()
    {
        auto *page = createPage(
            "Router Configuration Lab",
            "Create and manage a local router configuration profile."
            );

        auto *layout =
            qobject_cast<QVBoxLayout *>(page->layout());

        auto *card = createCard("Router Configuration");

        auto *cardLayout =
            qobject_cast<QVBoxLayout *>(card->layout());

        auto *description = new QLabel(
            "Enter the router details below. These values are stored "
            "locally in OmniNet while the application is running. "
            "They are not automatically sent to the physical router."
            );

        description->setObjectName("bodyText");
        description->setWordWrap(true);
        cardLayout->addWidget(description);

        auto *form = new QFormLayout;
        form->setSpacing(15);

        routerNameEdit = new QLineEdit;
        routerNameEdit->setPlaceholderText("e.g. Home Router");

        routerIpEdit = new QLineEdit;
        routerIpEdit->setPlaceholderText("e.g. 192.168.0.1");

        routerUsernameEdit = new QLineEdit;
        routerUsernameEdit->setPlaceholderText("Router username");

        routerPasswordEdit = new QLineEdit;
        routerPasswordEdit->setPlaceholderText("Optional router password");
        routerPasswordEdit->setEchoMode(QLineEdit::Password);

        routerFirmwareEdit = new QLineEdit;
        routerFirmwareEdit->setPlaceholderText("e.g. V1.0.0");

        form->addRow("Router Name:", routerNameEdit);
        form->addRow("IP Address:", routerIpEdit);
        form->addRow("Username:", routerUsernameEdit);
        form->addRow("Password:", routerPasswordEdit);
        form->addRow("Firmware Version:", routerFirmwareEdit);

        cardLayout->addLayout(form);

        routerConfigStatus = new QLabel("Status: Not Configured");
        routerConfigStatus->setObjectName("mutedText");
        cardLayout->addWidget(routerConfigStatus);

        auto *controls = new QHBoxLayout;

        auto *saveButton = new QPushButton("Save Configuration");
        saveButton->setObjectName("primaryButton");

        auto *resetButton = new QPushButton("Reset");

        controls->addWidget(saveButton);
        controls->addWidget(resetButton);
        controls->addStretch();

        cardLayout->addLayout(controls);

        layout->addWidget(card);
        layout->addStretch();

        connect(
            saveButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                const QString name = routerNameEdit->text().trimmed();
                const QString ip = routerIpEdit->text().trimmed();
                const QString username = routerUsernameEdit->text().trimmed();
                const QString password = routerPasswordEdit->text();
                const QString firmware = routerFirmwareEdit->text().trimmed();

                if (name.isEmpty())
                {
                    QMessageBox::warning(
                        this, "Invalid Configuration",
                        "Enter a router name."
                        );
                    return;
                }

                QHostAddress address;
                if (!address.setAddress(ip) ||
                    address.protocol() != QAbstractSocket::IPv4Protocol)
                {
                    QMessageBox::warning(
                        this, "Invalid IP Address",
                        "Enter a valid IPv4 address."
                        );
                    return;
                }

                if (username.isEmpty())
                {
                    QMessageBox::warning(
                        this, "Invalid Configuration",
                        "Enter a router username."
                        );
                    return;
                }

                routerConfig.setRouterName(name);
                routerConfig.setIPAddress(ip);
                routerConfig.setUsername(username);
                routerConfig.setPassword(password);
                routerConfig.setFirmwareVersion(
                    firmware.isEmpty() ? "Unknown" : firmware
                    );
                routerConfig.setConfigured(true);

                routerConfigStatus->setText(
                    "Status: Configuration Saved (local only)"
                    );

                routerConfig.displayConfiguration();
                commandCenter->addActivity(
                    "Local router configuration saved"
                    );
                statusBar()->showMessage(
                    "Local router configuration saved", 5000
                    );
            }
            );

        connect(
            resetButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                routerConfig.resetConfiguration();

                routerNameEdit->setText(routerConfig.getRouterName());
                routerIpEdit->setText(routerConfig.getIPAddress());
                routerUsernameEdit->setText(routerConfig.getUsername());
                routerPasswordEdit->clear();
                routerFirmwareEdit->setText(
                    routerConfig.getFirmwareVersion()
                    );

                routerConfigStatus->setText("Status: Not Configured");
                commandCenter->addActivity(
                    "Local router configuration reset"
                    );
                statusBar()->showMessage(
                    "Local router configuration reset", 5000
                    );
            }
            );

        routerNameEdit->setText(routerConfig.getRouterName());
        routerIpEdit->setText(routerConfig.getIPAddress());
        routerUsernameEdit->setText(routerConfig.getUsername());
        routerFirmwareEdit->setText(routerConfig.getFirmwareVersion());

        return page;
    }


    // ==================================================
    // PROFILE REFRESH
    // ==================================================

    void refreshProfilesPage()
    {
        if (!profilesTable || !profileStatusLabel)
            return;

        profilesTable->setRowCount(0);

        const int registeredCount =
            profileManager.profileCount();

        const auto candidates =
            profileManager.findMatchingProfiles(
                currentRouterIdentity
                );

        if (currentRouterIdentity.isEmpty())
        {
            profileStatusLabel->setText(
                QString("%1 router profile(s) registered. "
                        "Discover a network to evaluate matches.")
                    .arg(registeredCount)
                );
        }
        else if (candidates.isEmpty())
        {
            profileStatusLabel->setText(
                QString("%1 profile(s) registered. "
                        "No profile currently matches this router.")
                    .arg(registeredCount)
                );
        }
        else
        {
            profileStatusLabel->setText(
                QString("%1 profile candidate(s) identified. "
                        "Compatibility still requires verification.")
                    .arg(candidates.size())
                );
        }

        for (const auto &candidate : candidates)
        {
            if (!candidate.profile)
                continue;

            const int row = profilesTable->rowCount();
            profilesTable->insertRow(row);

            profilesTable->setItem(
                row, 0,
                new QTableWidgetItem(candidate.profile->name())
                );

            profilesTable->setItem(
                row, 1,
                new QTableWidgetItem(candidate.profile->manufacturer())
                );

            profilesTable->setItem(
                row, 2,
                new QTableWidgetItem("Candidate match")
                );

            profilesTable->setItem(
                row, 3,
                new QTableWidgetItem(
                    QString("%1%").arg(candidate.match.confidence)
                    )
                );

            profilesTable->setItem(
                row, 4,
                new QTableWidgetItem(
                    candidate.match.compatibilityVerified
                        ? "Verified"
                        : "Unverified"
                    )
                );
        }
    }


    // ==================================================
    // NETWORK DETECTION
    // ==================================================

    void detectNetwork()
    {
        commandCenter->addActivity("Network discovery started");

        NetworkInformation detected;
        QString error;

        if (!NetworkDetector::detect(detected, error))
        {
            networkAvailable = false;

            commandCenter->setNetworkState(
                false, "Unavailable", "—", "—"
                );

            commandCenter->addActivity("Network detection failed");

            QMessageBox::warning(
                this,
                "Network Detection Failed",
                error
                );

            statusBar()->showMessage(
                "Network detection failed",
                5000
                );

            return;
        }

        currentNetwork = detected;
        networkAvailable = true;

        currentRouterIdentity = RouterIdentity();

        refreshProfilesPage();

        commandCenter->setNetworkState(
            true,
            detected.adapterName,
            detected.localIp,
            detected.gatewayIp
            );

        commandCenter->addActivity(
            "Network detected: " + detected.localIp
            );

        statusBar()->showMessage(
            "Network detected. Inspecting gateway...",
            5000
            );

        if (detected.gatewayIp.trimmed().isEmpty())
        {
            commandCenter->setRouterState(
                false, false, "Unknown", "Unknown",
                "No default gateway found"
                );

            return;
        }

        gatewayInspector.inspect(detected.gatewayIp);
    }


    // ==================================================
    // GATEWAY INSPECTION
    // ==================================================

    void handleGatewayInspection(
        const GatewayInformation &information)
    {
        if (!information.reachable)
        {
            commandCenter->setRouterState(
                false, false, "Unknown", "Unknown",
                "Router management page unavailable"
                );

            commandCenter->addActivity(
                "Gateway inspection unsuccessful"
                );

            statusBar()->showMessage(
                "Gateway inspection unsuccessful",
                5000
                );

            return;
        }

        const AuthenticationInfo authentication =
            AuthenticationDetector::inspect(information.html);

        currentRouterIdentity =
            RouterIdentification::fromGatewayInspection(information);

        const auto candidates =
            profileManager.findMatchingProfiles(
                currentRouterIdentity
                );

        QString profileSummary;

        if (candidates.isEmpty())
        {
            profileSummary = "No matching profile";
        }
        else
        {
            profileSummary = candidates.first().profile
                                 ? candidates.first().profile->name()
                                 : "Profile candidate found";
        }

        const QString manufacturer =
            currentRouterIdentity.manufacturer.isEmpty()
                ? "Unknown"
                : currentRouterIdentity.manufacturer;

        const QString model =
            currentRouterIdentity.model.isEmpty()
                ? "Unknown"
                : currentRouterIdentity.model;

        commandCenter->setRouterState(
            true,
            routerAuthenticated,
            manufacturer,
            model,
            "Gateway inspected. " + profileSummary
            );

        refreshProfilesPage();

        QString authenticationText;

        switch (authentication.type)
        {
        case AuthenticationType::PasswordOnly:
            authenticationText = "Password-only login detected.";
            break;

        case AuthenticationType::UsernameAndPassword:
            authenticationText =
                "Username and password login detected.";
            break;

        default:
            authenticationText =
                "Login method could not be identified.";
            break;
        }

        commandCenter->addActivity(
            "Router inspected: " + information.gatewayIp
            );

        statusBar()->showMessage(
            "Gateway inspection completed",
            5000
            );

        // Open the router login dialog.
        Q_UNUSED(authenticationText);

        connectToRouter(information.gatewayIp);
    }


    // ==================================================
    // ROUTER AUTHENTICATION
    // ==================================================

    void connectToRouter(const QString &gatewayIp)
    {
        if (gatewayIp.trimmed().isEmpty())
            return;

        if (routerAuthenticated &&
            connectedRouterIp == gatewayIp)
        {
            return;
        }

        connectedRouterIp = gatewayIp;

        QUrl routerUrl;
        routerUrl.setScheme("http");
        routerUrl.setHost(gatewayIp);

        RouterCredentialWidget dialog(
            routerUrl,
            &routerAuthenticator,
            this
            );

        connect(
            &dialog,
            &RouterCredentialWidget::authenticationCompleted,
            this,
            [this](bool success, const QString &message)
            {
                if (!success)
                {
                    routerAuthenticated = false;

                    routerSettingsWidget->showError(message);

                    wirelessIntelligence->setRouterAuthenticated(false);

                    commandCenter->setRouterState(
                        true, false,
                        currentRouterIdentity.manufacturer,
                        currentRouterIdentity.model,
                        "Authentication failed"
                        );

                    commandCenter->addActivity(
                        "Router authentication failed"
                        );

                    statusBar()->showMessage(
                        "Router authentication failed",
                        5000
                        );

                    return;
                }

                routerAuthenticated = true;

                liveMonitor->setSession(
                    routerAuthenticator.sessionManager(),
                    QUrl("http://" + connectedRouterIp + "/")
                    );

                wirelessIntelligence->setRouterAuthenticated(true);

                commandCenter->setRouterState(
                    true, true,
                    currentRouterIdentity.manufacturer,
                    currentRouterIdentity.model,
                    "Authenticated"
                    );

                commandCenter->addActivity(
                    "Router authenticated"
                    );

                statusBar()->showMessage(
                    "Authenticated. Retrieving router settings..."
                    );

                routerSettingsWidget->setLoading(true);

                routerAuthenticator.getWifiSettings();
            }
            );

        dialog.exec();
    }


    // ==================================================
    // DEVICE SCANNING
    // ==================================================

    void startDeviceScan()
    {
        if (!networkAvailable)
        {
            QMessageBox::information(
                this,
                "Network Required",
                "Discover your network before scanning for devices."
                );

            return;
        }

        deviceDiscovery.scanNetwork(
            currentNetwork.localIp,
            currentNetwork.subnetMask
            );
    }


    // ==================================================
    // ROUTER CAPTURE
    // ==================================================

    void startCapture()
    {
        if (!routerAuthenticated ||
            connectedRouterIp.trimmed().isEmpty())
        {
            QMessageBox::information(
                this,
                "Router Login Required",
                "Authenticate with your router before starting capture."
                );

            return;
        }

        QUrl base;
        base.setScheme("http");
        base.setHost(connectedRouterIp);
        base.setPath("/");

        captureReportView->clear();

        captureStartButton->setEnabled(false);
        captureCancelButton->setEnabled(true);

        captureCopyButton->setEnabled(false);
        captureSaveButton->setEnabled(false);

        captureProgress->setRange(0, 0);
        captureStatus->setText("Starting capture...");

        commandCenter->setCaptureState("Capture running...");
        commandCenter->addActivity("Router capture started");

        routerCapture.start(
            routerAuthenticator.sessionManager(),
            base,
            captureMaskMacs->isChecked()
            );
    }


    // ==================================================
    // SIGNAL CONNECTIONS
    // ==================================================

    void connectSignals()
    {
        // Command Center actions

        connect(
            commandCenter,
            &CommandCenterWidget::discoverNetworkRequested,
            this,
            [this]() { detectNetwork(); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::scanDevicesRequested,
            this,
            [this]() { startDeviceScan(); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openRouterSettingsRequested,
            this,
            [this]() { setCurrentPage(1); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openDevicesRequested,
            this,
            [this]() { setCurrentPage(2); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openProfilesRequested,
            this,
            [this]() { setCurrentPage(3); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openCaptureRequested,
            this,
            [this]() { setCurrentPage(4); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openLiveMonitorRequested,
            this,
            [this]() { setCurrentPage(5); }
            );

        connect(
            commandCenter,
            &CommandCenterWidget::openWirelessIntelligenceRequested,
            this,
            [this]() { setCurrentPage(6); }
            );


        // Gateway inspection

        connect(
            &gatewayInspector,
            &GatewayInspector::inspectionProgress,
            this,
            [this](const QString &message)
            {
                commandCenter->addActivity(message);
            }
            );

        connect(
            &gatewayInspector,
            &GatewayInspector::inspectionFinished,
            this,
            [this](const GatewayInformation &information)
            {
                handleGatewayInspection(information);
            }
            );


        // Device scanning

        connect(
            scanDevicesButton,
            &QPushButton::clicked,
            this,
            [this]() { startDeviceScan(); }
            );

        connect(
            &deviceDiscovery,
            &DeviceDiscovery::scanStarted,
            this,
            [this]()
            {
                devicesTable->setRowCount(0);

                deviceCountLabel->setText("0 devices discovered");
                deviceScanStatus->setText("Scanning local network...");

                deviceProgress->setRange(0, 0);

                scanDevicesButton->setEnabled(false);
                scanDevicesButton->setText("Scanning...");

                commandCenter->setDeviceCount(0);
                commandCenter->addActivity("Device scan started");
            }
            );

        connect(
            &deviceDiscovery,
            &DeviceDiscovery::deviceFound,
            this,
            [this](const NetworkDevice &device)
            {
                const int row = devicesTable->rowCount();

                devicesTable->insertRow(row);

                devicesTable->setItem(
                    row, 0,
                    new QTableWidgetItem(device.ipAddress)
                    );

                devicesTable->setItem(
                    row, 1,
                    new QTableWidgetItem(device.macAddress)
                    );

                devicesTable->setItem(
                    row, 2,
                    new QTableWidgetItem(device.hostName)
                    );

                devicesTable->setItem(
                    row, 3,
                    new QTableWidgetItem(device.status)
                    );

                deviceCountLabel->setText(
                    QString("%1 devices discovered")
                        .arg(devicesTable->rowCount())
                    );

                commandCenter->setDeviceCount(
                    devicesTable->rowCount()
                    );
            }
            );

        connect(
            &deviceDiscovery,
            &DeviceDiscovery::scanProgress,
            this,
            [this](const QString &message)
            {
                deviceScanStatus->setText(message);
            }
            );

        connect(
            &deviceDiscovery,
            &DeviceDiscovery::scanFinished,
            this,
            [this](const QList<NetworkDevice> &devices)
            {
                deviceProgress->setRange(0, 1);
                deviceProgress->setValue(1);

                scanDevicesButton->setEnabled(true);
                scanDevicesButton->setText("Scan Again");

                deviceCountLabel->setText(
                    QString("%1 devices discovered")
                        .arg(devices.size())
                    );

                commandCenter->setDeviceCount(devices.size());

                deviceScanStatus->setText(
                    devices.isEmpty()
                        ? "Scan completed. No responsive devices found."
                        : "Scan completed successfully."
                    );

                commandCenter->addActivity(
                    QString("Device scan completed: %1 found")
                        .arg(devices.size())
                    );
            }
            );


        // Router Wi-Fi settings

        connect(
            &routerAuthenticator,
            &RouterAuthenticator::wifiSettingsReceived,
            this,
            [this](const QJsonObject &settings)
            {
                routerSettingsWidget->displayWifiSettings(settings);
                routerSettingsWidget->setLoading(false);

                wirelessIntelligence->setRouterSettings(settings);

                commandCenter->addActivity(
                    "Router Wi-Fi settings retrieved"
                    );

                setCurrentPage(1);

                statusBar()->showMessage(
                    "Router Wi-Fi settings retrieved",
                    5000
                    );
            }
            );

        connect(
            &routerAuthenticator,
            &RouterAuthenticator::wifiSettingsFailed,
            this,
            [this](const QString &message)
            {
                routerSettingsWidget->setLoading(false);
                routerSettingsWidget->showError(message);

                statusBar()->showMessage(
                    "Could not retrieve router settings",
                    5000
                    );
            }
            );


        // Router capture controls

        connect(
            captureStartButton,
            &QPushButton::clicked,
            this,
            [this]() { startCapture(); }
            );

        connect(
            captureCancelButton,
            &QPushButton::clicked,
            this,
            [this]() { routerCapture.cancel(); }
            );

        connect(
            captureCopyButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                QGuiApplication::clipboard()->setText(
                    captureReportView->toPlainText()
                    );

                statusBar()->showMessage(
                    "Report copied to clipboard",
                    4000
                    );
            }
            );

        connect(
            captureSaveButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                const QString suggested =
                    "OmniNet-capture-" +
                    QDateTime::currentDateTime()
                        .toString("yyyyMMdd-HHmm") +
                    ".txt";

                const QString fileName =
                    QFileDialog::getSaveFileName(
                        this,
                        "Save capture report",
                        suggested,
                        "Text files (*.txt)"
                        );

                if (fileName.isEmpty())
                    return;

                QFile file(fileName);

                if (!file.open(
                        QIODevice::WriteOnly |
                        QIODevice::Text))
                {
                    QMessageBox::warning(
                        this,
                        "Save Failed",
                        file.errorString()
                        );

                    return;
                }

                file.write(
                    captureReportView->toPlainText().toUtf8()
                    );

                statusBar()->showMessage(
                    "Report saved",
                    5000
                    );
            }
            );


        // Capture progress

        connect(
            &routerCapture,
            &RouterCapture::progress,
            this,
            [this](int done, int total, const QString &message)
            {
                captureProgress->setRange(0, qMax(total, 1));
                captureProgress->setValue(done);

                captureStatus->setText(
                    QString("Scanning (%1 of about %2): %3")
                        .arg(done)
                        .arg(total)
                        .arg(message)
                    );

                commandCenter->setCaptureState(
                    QString("Capture running: %1 of %2")
                        .arg(done)
                        .arg(total)
                    );
            }
            );

        connect(
            &routerCapture,
            &RouterCapture::finished,
            this,
            [this](const QString &report, bool cancelled)
            {
                captureReportView->setPlainText(report);

                captureProgress->setRange(0, 1);
                captureProgress->setValue(1);

                captureStartButton->setEnabled(true);
                captureCancelButton->setEnabled(false);

                captureCopyButton->setEnabled(true);
                captureSaveButton->setEnabled(true);

                captureStatus->setText(
                    cancelled
                        ? "Capture cancelled. Partial report available."
                        : "Capture complete. Review, copy or save the report."
                    );

                commandCenter->setCaptureState(
                    cancelled ? "Capture cancelled"
                              : "Capture completed"
                    );

                commandCenter->addActivity(
                    cancelled
                        ? "Router capture cancelled"
                        : "Router capture completed"
                    );

                statusBar()->showMessage(
                    "Router capture finished",
                    5000
                    );
            }
            );
    }


    // ==================================================
    // APPLICATION STYLE
    // ==================================================

    void applyStyle()
    {
        setStyleSheet(R"QSS(
            * { font-family: "Segoe UI"; font-size: 13px; outline: none; }
            QMainWindow, QWidget#centralArea, QWidget#workspace, QWidget#pageStack {
                background: #07111E;
            }
            QWidget { color: #D6E2F0; }
            QLabel { background: transparent; }

            QWidget#sidebar {
                background: #091522;
                border-right: 1px solid #1A3043;
            }
            QLabel#brandMark {
                color: #07111E; background: #38DDF0; border-radius: 11px;
                font-size: 24px; font-weight: 900;
            }
            QLabel#brand { color: #F2F8FC; font-size: 23px; font-weight: 800; }
            QLabel#brandSubtitle {
                color: #6D91A7; font-size: 8px; font-weight: 800; letter-spacing: 1.3px;
            }
            QLabel#sidebarSection, QLabel#sidebarFooterTitle {
                color: #58758A; font-size: 9px; font-weight: 800;
                letter-spacing: 1.5px; padding-left: 9px;
            }
            QFrame#sidebarDivider { background: #1A3043; border: none; }
            QLabel#connectionStatus {
                color: #8BA4B7; padding: 7px 9px 3px 9px; font-size: 10px;
            }

            QFrame#topBar { background: #0A1725; border-bottom: 1px solid #1A3043; }
            QLabel#topBarTitle {
                color: #CDEAF2; font-size: 10px; font-weight: 800; letter-spacing: 1.7px;
            }
            QLabel#topBarSubtitle { color: #6F879B; font-size: 10px; }
            QLabel#topBarMeta {
                color: #6D91A7; background: #102438; border: 1px solid #1C3A50;
                border-radius: 6px; padding: 7px 10px; font-size: 9px;
                font-weight: 800; letter-spacing: 0.8px;
            }

            QPushButton {
                background: #14283A; color: #D5E5F0; border: 1px solid #25445A;
                border-radius: 7px; padding: 9px 14px; font-weight: 600;
            }
            QPushButton:hover { background: #1A354A; border-color: #34718A; }
            QPushButton:pressed { background: #102438; }
            QPushButton:disabled {
                color: #52697B; background: #0D1B29; border-color: #1B3042;
            }
            QPushButton#primaryButton {
                background: #38DDF0; color: #06202A; border: 1px solid #38DDF0;
                font-weight: 800;
            }
            QPushButton#primaryButton:hover {
                background: #76EEFA; border-color: #76EEFA;
            }
            QPushButton#primaryButton:pressed { background: #22BFD5; }
            QPushButton#navigationButton {
                background: transparent; color: #8BA3B6; text-align: left;
                border: 1px solid transparent; border-left: 3px solid transparent;
                border-radius: 7px; padding: 11px 10px; font-size: 11px;
                font-weight: 600;
            }
            QPushButton#navigationButton:hover {
                background: #102438; color: #E4F4F8; border-color: #19384B;
            }
            QPushButton#navigationButton:checked {
                background: #103044; color: #55E3F2; border: 1px solid #1D5367;
                border-left: 3px solid #38DDF0; font-weight: 800;
            }

            QLabel#pageTitle, QLabel#commandHeading {
                color: #F1F7FB; font-size: 25px; font-weight: 800;
            }
            QLabel#pageDescription, QLabel#commandSubtitle {
                color: #8AA1B4; font-size: 11px;
            }
            QLabel#sectionTitle { color: #DCEAF3; font-size: 14px; font-weight: 750; }
            QLabel#bodyText { color: #A2B5C5; font-size: 11px; }
            QLabel#mutedText { color: #7891A5; font-size: 10px; }

            QFrame#card, QFrame#commandMetricCard, QFrame#commandPanel,
            QFrame#wirelessCard {
                background: #0D1D2C; border: 1px solid #1D394D; border-radius: 11px;
            }
            QFrame#card:hover, QFrame#commandMetricCard:hover,
            QFrame#commandPanel:hover, QFrame#wirelessCard:hover {
                border-color: #2A6072;
            }
            QLabel#commandMetricTitle, QLabel#commandPanelTitle {
                color: #83A2B5; font-size: 9px; font-weight: 800; letter-spacing: 1px;
            }
            QLabel#commandMetricValue {
                color: #43DDED; font-size: 26px; font-weight: 800;
            }
            QLabel#commandMetricNote, QLabel#commandPanelText {
                color: #91A7B8; font-size: 10px;
            }

            QTableWidget {
                background: #091724; alternate-background-color: #0D1D2C;
                color: #C9DCE8; border: 1px solid #1D394D; border-radius: 8px;
                gridline-color: transparent; selection-background-color: #16465A;
                selection-color: #70EAF5; font-size: 11px;
            }
            QTableWidget::item { padding: 9px; border: none; }
            QTableWidget::item:hover { background: #112B3C; }
            QHeaderView::section {
                background: #102438; color: #82A4B8; border: none;
                border-bottom: 1px solid #24465A; padding: 11px 9px;
                font-size: 9px; font-weight: 800; letter-spacing: 0.7px;
            }

            QLineEdit, QComboBox, QSpinBox, QTextEdit, QPlainTextEdit {
                background: #091724; color: #D9E8F1; border: 1px solid #25445A;
                border-radius: 7px; padding: 9px 10px;
                selection-background-color: #176176;
            }
            QLineEdit:focus, QComboBox:focus, QSpinBox:focus,
            QTextEdit:focus, QPlainTextEdit:focus {
                border: 1px solid #38DDF0; background: #0B1D2C;
            }
            QCheckBox { color: #B5C9D7; spacing: 8px; }
            QCheckBox::indicator {
                width: 15px; height: 15px; border: 1px solid #3A5B70;
                border-radius: 4px; background: #0A1A28;
            }
            QCheckBox::indicator:checked { background: #38DDF0; border-color: #38DDF0; }

            QProgressBar {
                background: #173044; border: none; border-radius: 4px;
                text-align: center; color: #DDF8FA; min-height: 7px;
            }
            QProgressBar::chunk { background: #38DDF0; border-radius: 4px; }
            QStatusBar {
                background: #091522; color: #8BA4B7; border-top: 1px solid #1A3043;
                font-size: 10px;
            }
            QScrollBar:vertical {
                background: #081522; width: 9px; margin: 0; border: none;
            }
            QScrollBar::handle:vertical {
                background: #28495E; border-radius: 4px; min-height: 28px;
            }
            QScrollBar::handle:vertical:hover { background: #36748A; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QToolTip {
                background: #102438; color: #E4F5FA; border: 1px solid #2C6175;
                padding: 5px 8px;
            }
        )QSS");
    }

    }
};


// ======================================================
// APPLICATION ENTRY POINT
// ======================================================

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("OmniNet");
    QApplication::setApplicationVersion("1.4.0");
    QApplication::setOrganizationName("OmniNet");

    app.setFont(QFont("Segoe UI", 10));

    OmniNetWindow window;
    window.show();

    return app.exec();
}

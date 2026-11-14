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

        // Sidebar

        sidebar = new QWidget;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(230);

        auto *sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setContentsMargins(17, 25, 17, 20);
        sidebarLayout->setSpacing(8);

        auto *brand = new QLabel("OmniNet");
        brand->setObjectName("brand");

        auto *brandSubtitle = new QLabel(
            "NETWORK COMMAND CENTER"
            );
        brandSubtitle->setObjectName("brandSubtitle");

        sidebarLayout->addWidget(brand);
        sidebarLayout->addWidget(brandSubtitle);
        sidebarLayout->addSpacing(25);

        auto *section = new QLabel("WORKSPACE");
        section->setObjectName("sidebarSection");

        sidebarLayout->addWidget(section);
        sidebarLayout->addSpacing(5);

        sidebarLayout->addWidget(
            createNavigationButton("Command Center", 0)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Router Settings", 1)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Connected Devices", 2)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Router Profiles", 3)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Router Capture", 4)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Live Monitor", 5)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Wireless Intelligence", 6)
            );

        sidebarLayout->addWidget(
            createNavigationButton("Router Configuration Lab", 7)
            );

        sidebarLayout->addStretch();

        auto *divider = new QFrame;
        divider->setObjectName("sidebarDivider");
        divider->setFixedHeight(1);

        sidebarLayout->addWidget(divider);

        auto *connectionLabel = new QLabel("OmniNet is ready");
        connectionLabel->setObjectName("connectionStatus");
        connectionLabel->setWordWrap(true);

        sidebarLayout->addWidget(connectionLabel);

        // Main page stack

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

        mainLayout->addWidget(sidebar);
        mainLayout->addWidget(pages, 1);

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

            * {
                font-family: "Segoe UI";
                font-size: 13px;
            }

            QMainWindow,
            QWidget#centralArea,
            QWidget#pageStack {
                background: #0A0E14;
            }

            QWidget {
                color: #C9D4E5;
            }

            QLabel {
                background: transparent;
            }

            QWidget#sidebar {
                background: #0F1520;
                border-right: 1px solid #1B2638;
            }

            QLabel#brand {
                color: #FFFFFF;
                font-size: 27px;
                font-weight: 900;
            }

            QLabel#brandSubtitle,
            QLabel#sidebarSection {
                color: #64748B;
                font-size: 10px;
                font-weight: 700;
            }

            QLabel#sidebarSection {
                padding-left: 9px;
            }

            QFrame#sidebarDivider {
                background: #1B2638;
                border: none;
            }

            QLabel#connectionStatus {
                color: #7C8BA3;
                padding: 10px 4px;
                font-size: 11px;
            }

            QPushButton {
                background: #172131;
                color: #D5DEEC;
                border: 1px solid #2A3950;
                border-radius: 6px;
                padding: 9px 14px;
                font-weight: 600;
            }

            QPushButton:hover {
                background: #1D2A3E;
                border-color: #3B5173;
            }

            QPushButton:disabled {
                color: #54627A;
                background: #0F1520;
                border-color: #1B2638;
            }

            QPushButton#primaryButton {
                background: #22D3EE;
                color: #04222A;
                border: 1px solid #22D3EE;
                font-weight: 800;
            }

            QPushButton#primaryButton:hover {
                background: #67E8F9;
            }

            QPushButton#navigationButton {
                background: transparent;
                color: #8392AA;
                text-align: left;
                border: none;
                border-left: 3px solid transparent;
                padding: 11px 13px;
            }

            QPushButton#navigationButton:hover {
                background: #151F30;
                color: #E6EDF8;
            }

            QPushButton#navigationButton:checked {
                background: rgba(34, 211, 238, 28);
                color: #22D3EE;
                border-left: 3px solid #22D3EE;
                font-weight: 800;
            }

            QLabel#pageTitle {
                color: #FFFFFF;
                font-size: 26px;
                font-weight: 900;
            }

            QLabel#pageDescription {
                color: #8291A8;
                font-size: 12px;
            }

            QLabel#sectionTitle {
                color: #E6EDF8;
                font-size: 15px;
                font-weight: 800;
            }

            QLabel#bodyText {
                color: #9AA8BF;
            }

            QLabel#mutedText {
                color: #6B7A93;
                font-size: 11px;
            }

            QFrame#card,
            QFrame#commandMetricCard,
            QFrame#commandPanel,
            QFrame#wirelessCard {
                background: #121A27;
                border: 1px solid #26364D;
                border-radius: 8px;
            }

            QLabel#commandHeading {
                color: #FFFFFF;
                font-size: 27px;
                font-weight: 900;
            }

            QLabel#commandSubtitle {
                color: #8291A8;
                font-size: 12px;
            }

            QLabel#commandMetricTitle,
            QLabel#commandPanelTitle {
                color: #8293AC;
                font-size: 10px;
                font-weight: 800;
            }

            QLabel#commandMetricValue {
                color: #22D3EE;
                font-size: 25px;
                font-weight: 900;
            }

            QLabel#commandMetricNote,
            QLabel#commandPanelText {
                color: #8493AA;
                font-size: 11px;
            }

            QTableWidget {
                background: #0F1622;
                alternate-background-color: #121A27;
                color: #C9D4E5;
                border: 1px solid #26364D;
                border-radius: 6px;
                gridline-color: transparent;
                selection-background-color: rgba(34, 211, 238, 35);
                selection-color: #22D3EE;
            }

            QTableWidget::item {
                padding: 8px;
                border: none;
            }

            QHeaderView::section {
                background: #0F1520;
                color: #8293AC;
                border: none;
                border-bottom: 1px solid #26364D;
                padding: 10px;
                font-size: 10px;
                font-weight: 800;
            }

            QLineEdit,
            QComboBox,
            QSpinBox,
            QTextEdit,
            QPlainTextEdit {
                background: #0A0E14;
                color: #D5DEEC;
                border: 1px solid #2A3950;
                border-radius: 6px;
                padding: 8px;
            }

            QCheckBox {
                color: #C9D4E5;
                spacing: 8px;
            }

            QProgressBar {
                background: #1B2638;
                border: none;
                border-radius: 3px;
                text-align: center;
            }

            QProgressBar::chunk {
                background: #22D3EE;
                border-radius: 3px;
            }

            QStatusBar {
                background: #0F1520;
                color: #8291A8;
                border-top: 1px solid #1B2638;
                font-size: 11px;
            }

            QScrollBar:vertical {
                background: transparent;
                width: 9px;
            }

            QScrollBar::handle:vertical {
                background: #2A3950;
                border-radius: 4px;
                min-height: 30px;
            }

        )QSS");
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

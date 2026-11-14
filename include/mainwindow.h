
#pragma once

#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QStackedWidget;
class QWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    // Main interface
    void buildInterface();
    void applyApplicationStyle();
    QWidget *buildSidebar();
    QWidget *buildDashboardPage();
    QWidget *buildRouterSettingsPage();
    QWidget *buildDevicesPage();
    QWidget *buildProfilesPage();

    // Navigation
    void navigateToDashboard();
    void navigateToRouterSettings();
    void navigateToDevices();
    void navigateToProfiles();
    void setActivePage(int index);

    // Shared interface helpers
    QWidget *createPageHeading(
        const QString &title,
        const QString &description
    );

    QWidget *createInfoCard(
        const QString &title,
        const QString &value,
        const QString &description
    );

    QWidget *createSectionCard(
        const QString &title,
        const QString &description
    );

    // Main window components
    QWidget *centralContainer = nullptr;
    QWidget *sidebar = nullptr;
    QStackedWidget *pageStack = nullptr;

    // Navigation actions in the availabe section...Section assumed
    QAction *dashboardAction = nullptr;
    QAction *routerSettingsAction = nullptr;
    QAction *devicesAction = nullptr;
    QAction *profilesAction = nullptr;

    // Dashboard status indicators
    QLabel *routerConnectionLabel = nullptr;
    QLabel *connectedDevicesLabel = nullptr;
    QLabel *activeProfileLabel = nullptr;
    QLabel *statusMessageLabel = nullptr;

    // Page indices
    static constexpr int DashboardPage = 0;
    static constexpr int RouterSettingsPage = 1;
    static constexpr int DevicesPage = 2;
    static constexpr int ProfilesPage = 3;
};

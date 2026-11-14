
cmake_minimum_required(VERSION 3.21)

project(OmniNet VERSION 1.0.0 LANGUAGES CXX)

# C++ configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt automatic code generation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Required Qt modules
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

# Application executable
qt_add_executable(OmniNet

    src/main.cpp

    # Router and gateway discovery
    src/routerdiscovery.cpp
    include/routerdiscovery.h

    # Connected device discovery
    src/devicediscovery.cpp
    include/devicediscovery.h

    # Login form detection
    src/authenticationdetector.cpp
    include/authenticationdetector.h

    # Network detection
    src/networkdetector.cpp
    include/networkdetector.h

    # Gateway inspection
    src/gatewayinspector.cpp
    include/gatewayinspector.h

    # Router authentication
    src/routerauthenticator.cpp
    include/routerauthenticator.h

    # Router credentials interface
    src/routercredentialswidget.cpp
    include/routercredentialswidget.h

    # Router settings interface
    src/routersettingswidget.cpp
    include/routersettingswidget.h

    # Application resources
    resources.qrc
    include/routeridentity.h
    include/routerprofile.h
    include/routeridentification.h
    src/routeridentification.cpp
    include/routerprofilemanager.h
    src/routerprofilemanager.cpp
    include/tendaf3profile.h
    src/tendaf3profile.cpp
    include/routercapture.h
    src/routercapture.cpp
    include/livemonitorwidget.h
    src/livemonitorwidget.cpp
    include/wirelessintelligencewidget.h
    src/wirelessintelligence.cpp
    include/commandcenterwidget.h
    src/commandcenterwidget.cpp
    include/networkinfo.h
    src/networkinfo.cpp
    include/ipvalidator.h
    src/ipvalidator.cpp
    include/routerconfig.h
    src/routerconfig.cpp
)
if(WIN32)
    target_link_libraries(OmniNet PRIVATE wlanapi)
endif()
# Header directory
target_include_directories(OmniNet PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Required libraries
target_link_libraries(OmniNet PRIVATE
    Qt6::Widgets
    Qt6::Network
    iphlpapi
    ws2_32
)

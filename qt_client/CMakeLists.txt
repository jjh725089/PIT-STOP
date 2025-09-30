cmake_minimum_required(VERSION 3.16)
project(qt_client VERSION 0.1 LANGUAGES CXX)

# Enable automatic processing of Qt-specific files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: specify UI search path for Qt's UIC
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# --- Platform-specific settings: Windows --------------------------------------
if (WIN32)
    # Path to Qt 6 installation (use plain ASCII double quotes)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")

    # Set pkg-config environment variables for GStreamer
    set(ENV{PKG_CONFIG_PATH}
        "C:/Program Files/gstreamer/1.0/msvc_x86_64/lib/pkgconfig"
    )
    set(ENV{PKG_CONFIG_LIBDIR}
        "C:/Program Files/gstreamer/1.0/msvc_x86_64/lib/pkgconfig"
    )

    # Path to OpenCV config
    set(OpenCV_DIR "C:/opencv/build")
endif()

# --- Platform-specific settings: macOS ----------------------------------------
if(APPLE)
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.8.3/macos;$ENV{HOME}/Qt/qtmqtt-install")
    set(Qt6Mqtt_DIR "$ENV{HOME}/Qt/qtmqtt-install/lib/cmake/Qt6Mqtt")
endif()

# --- Find required Qt6 modules ------------------------------------------------
find_package(Qt6 REQUIRED
    COMPONENTS
        Widgets
        Multimedia
        MultimediaWidgets
        WebEngineWidgets
        Sql
        Charts
)
find_package(Qt6Mqtt REQUIRED)

# --- Find OpenCV --------------------------------------------------------------
find_package(OpenCV REQUIRED)

# --- Find GStreamer and related dependencies via pkg-config -------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED
    gstreamer-1.0
    gstreamer-video-1.0
    gstreamer-app-1.0
    gstreamer-rtsp-1.0
    gio-2.0
)

# --- Define project sources ---------------------------------------------------
set(PROJECT_SOURCES
    main.cpp

    # RTSP modules
    rtspclient.cpp
    rtspclientcamera1.cpp
    rtspclienttlsinteraction.cpp
    rtspclient.h
    rtspclientcamera1.h
    rtspclienttlsinteraction.h

    # MQTT module
    mqtt.cpp
    mqtt.h

    # Authentication
    mainwindow.cpp
    mainwindow.h
    signupwindow.cpp
    signupwindow.h
    databasemanager.cpp
    databasemanager.h

    # Monitoring UI
    monitorwindow.cpp
    monitorwindow.h

    # Qt resource file
    cert.qrc

    # Microphone
    
	microphone/microphone.cpp
    microphone/microphone.h
	microphone/microphone_socket.cpp
	microphone/microphone_socket.h
    microphone/microphone_input.cpp
    microphone/microphone_input.h
    microphone/audio_settings.cpp
    microphone/audio_settings.h
)

# --- Create executable --------------------------------------------------------
link_directories(${DEPS_LIBRARY_DIRS})

if (Qt6_FOUND)
    qt_add_executable(qt_client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        image.qrc
        Tab.qrc
        Header.qrc
    )
else()
    add_executable(qt_client ${PROJECT_SOURCES})
endif()

# --- Include directories ------------------------------------------------------
target_include_directories(qt_client PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${DEPS_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# --- Library directories ------------------------------------------------------
target_link_directories(qt_client PRIVATE
    ${DEPS_LIBRARY_DIRS}
)

# --- Link libraries -----------------------------------------------------------
target_link_libraries(qt_client PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::WebEngineWidgets
    Qt6::Sql
    Qt6::Mqtt
    Qt6::Charts
    ${DEPS_LIBRARIES}
    ${OpenCV_LIBS}
)

# --- Set executable properties for platform-specific behavior -----------------
if (Qt6_FOUND AND NOT APPLE)
    set_target_properties(qt_client PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if (Qt6_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND Qt6_VERSION VERSION_LESS 6.1.0)
    set_target_properties(qt_client PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.qt_client
    )
endif()

# --- Finalize Qt executable (required for qt_add_executable) ------------------
if (Qt6_FOUND)
    qt_finalize_executable(qt_client)
endif()

# --- Install configuration ----------------------------------------------------
include(GNUInstallDirs)
install(TARGETS qt_client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

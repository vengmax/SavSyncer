cmake_minimum_required(VERSION 3.16)
project(YandexService LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Network
        WebEngineWidgets
)
qt_standard_project_setup()

include_directories("../../../SavSyncer")
include_directories("../../../Interfaces")
include_directories("YandexIcons")

set(PROJECT_SOURCES

    ../../../Interfaces/IServices.h

    YandexService.h
    YandexService.cpp

    YandexIcons.qrc
)

add_library(YandexService SHARED ${PROJECT_SOURCES})

target_link_libraries(YandexService
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::WebEngineWidgets
)

set_target_properties(YandexService PROPERTIES PLUGIN TRUE)

# ClientID
#set(ClientID "")
#add_definitions(-DCLIENT_ID="${ClientID}")

# Client secret
#set(ClientSecret "")
#add_definitions(-DCLIENT_SECRET="${ClientSecret}")

# Redirect Uri
#set(RedirectUri "")
#add_definitions(-DREDIRECT_URI="${RedirectUri}")

if(NOT ClientID)
    message(FATAL_ERROR "ClientID is not set or is empty!")
endif()

if(NOT ClientSecret)
    message(FATAL_ERROR "ClientSecret is not set or is empty!")
endif()

if(NOT RedirectUri)
    message(FATAL_ERROR "RedirectUri is not set or is empty!")
endif()
cmake_minimum_required(VERSION 3.16)
project(SavSyncer LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(-DQT_NO_DEBUG_OUTPUT)
add_compile_options("/utf-8")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
)
qt_standard_project_setup()

find_library(VERSION_LIB version PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64")

include_directories("../SavSyncer")
include_directories("ItemGameList")
include_directories("PageGameList")
include_directories("../Interfaces")
include_directories("../About")
include_directories("../Profile")
include_directories("../Settings")

set(PROJECT_SOURCES
    main.cpp

    SavSyncer.ui
    SavSyncer.h
    SavSyncer.cpp

    Game.h
    Game.cpp

    ItemGameList/ItemGameList.h
    ItemGameList/ItemGameList.cpp

    pageGameList/pageGameList.h
    pageGameList/pageGameList.cpp

    ../About/About.h
    ../About/About.cpp

    ../Interfaces/IServices.h

    ../Profile/Profile.h
    ../Profile/Profile.cpp

    ../Settings/Settings.h
    ../Settings/Settings.cpp

    ../Images/Images.qrc
)

set(ICON_FILE "logoExe.rc") # Установка иконки для exe файла
add_executable(SavSyncer ${PROJECT_SOURCES} ${ICON_FILE})

set_target_properties(SavSyncer
    PROPERTIES
        WIN32_EXECUTABLE FALSE
)

target_link_libraries(SavSyncer
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        ${VERSION_LIB}
)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)  # Уменьшение размера заголовочных файлов Windows
    include_directories(${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(SavSyncer PRIVATE dbghelp)
endif()

set(OUTPUT_DIR "${CMAKE_HOME_DIRECTORY}/_Build/${CMAKE_BUILD_TYPE}")

get_filename_component(EXE_NAME ${PROJECT_BINARY_DIR} NAME)
set(PATH_TO_EXE "${PROJECT_BINARY_DIR}/${EXE_NAME}.exe")
if (EXISTS ${PATH_TO_EXE})
    message("***** SavSyncer: ${EXE_NAME}.exe copy to ${OUTPUT_DIR}")
    file(COPY "${PATH_TO_EXE}" DESTINATION "${OUTPUT_DIR}")
endif ()

file(GLOB FOLDERS "${CMAKE_BINARY_DIR}/Profile/Services/*")
foreach(SERVICE_FOLDER ${FOLDERS})
    get_filename_component(FOLDER_NAME ${SERVICE_FOLDER} NAME)
    set(PATH_TO_SERVICE "${SERVICE_FOLDER}/${FOLDER_NAME}.dll")
    if (EXISTS ${PATH_TO_SERVICE})
        message("***** SavSyncer: ${FOLDER_NAME}.dll copy to ${OUTPUT_DIR}/services")
        file(COPY "${PATH_TO_SERVICE}" DESTINATION "${OUTPUT_DIR}/services")
        file(COPY "${PATH_TO_SERVICE}" DESTINATION "${PROJECT_BINARY_DIR}/services")
    endif ()
endforeach()

# SavSyncerTools
if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(PATH_TO_TOOLS_EXE "${PROJECT_SOURCE_DIR}/../SavSyncerTools/out/build/release/SavSyncerTools/SavSyncerTools.exe")
    if (EXISTS ${PATH_TO_TOOLS_EXE})
        message("***** SavSyncer: SavSyncerTools.exe copy to ${OUTPUT_DIR}")
        file(COPY "${PATH_TO_TOOLS_EXE}" DESTINATION "${OUTPUT_DIR}")
    endif ()
else()
    set(PATH_TO_TOOLS_EXE "${PROJECT_SOURCE_DIR}/../SavSyncerTools/out/build/debug/SavSyncerTools/SavSyncerTools.exe")
    if (EXISTS ${PATH_TO_TOOLS_EXE})
        message("***** SavSyncer: SavSyncerTools.exe copy to ${OUTPUT_DIR}")
        file(COPY "${PATH_TO_TOOLS_EXE}" DESTINATION "${OUTPUT_DIR}")
    endif ()
endif ()

# windeployqt
if(EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    set(FILE_EXE "${OUTPUT_DIR}/${EXE_NAME}.exe")
    if (EXISTS ${FILE_EXE})
        message("***** WinDeployQt: ${FILE_EXE} *****")
        add_custom_command(TARGET SavSyncer POST_BUILD COMMAND ${CMAKE_COMMAND} -E env
                ${WINDEPLOYQT_EXECUTABLE} --release --dir "${OUTPUT_DIR}" "${FILE_EXE}")
    endif()

    file(GLOB ALL_SERVICES "${OUTPUT_DIR}/services/*")
    foreach(FILE_SERVICE_DLL ${ALL_SERVICES})
        if (EXISTS ${FILE_SERVICE_DLL})
            message("***** WinDeployQt: ${FILE_SERVICE_DLL} *****")
            add_custom_command(TARGET SavSyncer POST_BUILD COMMAND ${CMAKE_COMMAND} -E env
                    ${WINDEPLOYQT_EXECUTABLE} --release --dir "${OUTPUT_DIR}" "${FILE_SERVICE_DLL}")
        endif()
    endforeach()
endif()
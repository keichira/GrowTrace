cmake_minimum_required(VERSION 3.20)
project(GrowTrace CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/Source")
set(SOURCE_IO "${SOURCE_ROOT}/IO")
set(SOURCE_MATH "${SOURCE_ROOT}/Math")
set(SOURCE_MEMORY "${SOURCE_ROOT}/Memory")
set(SOURCE_NETWORK "${SOURCE_ROOT}/Network")
set(SOURCE_OS "${SOURCE_ROOT}/OS")
set(SOURCE_UTILS "${SOURCE_ROOT}/Utils")
set(SOURCE_PROTON "${SOURCE_ROOT}/Proton")
set(SOURCE_PACKET "${SOURCE_ROOT}/Packet")
set(SOURCE_ITEM "${SOURCE_ROOT}/Item")
set(SOURCE_WORLD "${SOURCE_ROOT}/World")

set(GTINSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/GrowTrace")
set(GTINSP_SERVER "${GTINSP_ROOT}/Server")
set(GTINSP_CORE "${GTINSP_ROOT}/Core")
set(GTINSP_EVENT "${GTINSP_ROOT}/Event")
set(GTINSP_UI "${GTINSP_ROOT}/UI")

set(THIRDPARTY_ROOT "${CMAKE_CURRENT_LIST_DIR}/ThirdParty")
set(THIRDPARTY_CONCURRENTQUEUE "${THIRDPARTY_ROOT}/concurrentqueue")
set(THIRDPARTY_ENET "${THIRDPARTY_ROOT}/enet")
set(THIRDPARTY_ZLIB "${THIRDPARTY_ROOT}/zlib")
set(THIRDPARTY_GLFW "${THIRDPARTY_ROOT}/glfw")
set(THIRDPARTY_IMGUI "${THIRDPARTY_ROOT}/imgui")

find_package(OpenGL REQUIRED)

add_subdirectory(${THIRDPARTY_GLFW})

add_library(imgui STATIC
    ${THIRDPARTY_IMGUI}/imgui.cpp
    ${THIRDPARTY_IMGUI}/imgui_draw.cpp
    ${THIRDPARTY_IMGUI}/imgui_tables.cpp
    ${THIRDPARTY_IMGUI}/imgui_widgets.cpp
    ${THIRDPARTY_IMGUI}/imgui_demo.cpp
    ${THIRDPARTY_IMGUI}/imgui_impl_glfw.cpp
    ${THIRDPARTY_IMGUI}/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${THIRDPARTY_IMGUI})
target_link_libraries(imgui PUBLIC glfw)

set(GrowTrace_SOURCES
    ${SOURCE_IO}/File.cpp
    ${SOURCE_IO}/Log.cpp

    ${SOURCE_MATH}/Color.cpp
    ${SOURCE_MATH}/Random.cpp
    ${SOURCE_MATH}/Vector2.cpp
    ${SOURCE_MATH}/Vector3.cpp

    ${SOURCE_MEMORY}/MemoryBuffer.cpp
    ${SOURCE_MEMORY}/RingBuffer.cpp

    ${SOURCE_NETWORK}/NetClient.cpp
    ${SOURCE_NETWORK}/NetSocket.cpp
    ${SOURCE_NETWORK}/NetHTTP.cpp

    ${SOURCE_OS}/OSPrecompiled.cpp
    ${SOURCE_PACKET}/NetPacket.cpp
    ${SOURCE_PROTON}/ProtonUtils.cpp

    ${SOURCE_ITEM}/ItemInfo.cpp
    ${SOURCE_ITEM}/ItemInfoManager.cpp
    ${SOURCE_ITEM}/ItemUtils.cpp

    ${SOURCE_UTILS}/Base64.cpp
    ${SOURCE_UTILS}/StringUtils.cpp
    ${SOURCE_UTILS}/Variant.cpp
    ${SOURCE_UTILS}/Timer.cpp

    ${SOURCE_WORLD}/WorldInfo.cpp
    ${SOURCE_WORLD}/TileInfo.cpp
    ${SOURCE_WORLD}/TileExtra.cpp
    ${SOURCE_WORLD}/WorldTileManager.cpp
    ${SOURCE_WORLD}/WorldObjectManager.cpp

    ${GTINSP_SERVER}/TCPServer.cpp
    ${GTINSP_SERVER}/ENetServer.cpp

    ${GTINSP_CORE}/Application.cpp
    ${GTINSP_CORE}/ProxyData.cpp

    ${GTINSP_UI}/GTProxyPanel.cpp
    ${GTINSP_UI}/UIUtils.cpp

    ${GTINSP_EVENT}/EventManager.cpp

    ${GTINSP_ROOT}/Main.cpp

    ${THIRDPARTY_ENET}/callbacks.c
    ${THIRDPARTY_ENET}/compress.c
    ${THIRDPARTY_ENET}/host.c
    ${THIRDPARTY_ENET}/list.c
    ${THIRDPARTY_ENET}/packet.c
    ${THIRDPARTY_ENET}/protocol.c
    ${THIRDPARTY_ENET}/peer.c

    ${THIRDPARTY_ZLIB}/adler32.c
    ${THIRDPARTY_ZLIB}/compress.c
    ${THIRDPARTY_ZLIB}/crc32.c
    ${THIRDPARTY_ZLIB}/deflate.c
    ${THIRDPARTY_ZLIB}/gzio.c
    ${THIRDPARTY_ZLIB}/infback.c
    ${THIRDPARTY_ZLIB}/inffast.c
    ${THIRDPARTY_ZLIB}/inflate.c
    ${THIRDPARTY_ZLIB}/inftrees.c
    ${THIRDPARTY_ZLIB}/trees.c
    ${THIRDPARTY_ZLIB}/zutil.c
    ${SOURCE_UTILS}/ZLibUtils.cpp
)

if(WIN32)
    list(APPEND GrowTrace_SOURCES 
        ${SOURCE_OS}/Windows/WindowsPrecompiled.cpp 
        ${SOURCE_NETWORK}/NetHTTP_WinHTTP.cpp
        ${THIRDPARTY_ENET}/win32.c
    )
else()
    find_package(CURL REQUIRED)
    list(APPEND GrowTrace_SOURCES 
        ${SOURCE_OS}/Linux/LinuxPrecompiled.cpp 
        ${SOURCE_NETWORK}/NetHTTP_Curl.cpp
        ${THIRDPARTY_ENET}/unix.c
    )
endif()

add_executable(GrowTrace)
target_sources(GrowTrace PRIVATE ${GrowTrace_SOURCES})

target_include_directories(GrowTrace PRIVATE 
    ${SOURCE_ROOT}
    ${THIRDPARTY_CONCURRENTQUEUE}/include
    ${THIRDPARTY_ENET}/include
    ${THIRDPARTY_ZLIB}
)

set_target_properties(GrowTrace PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Runtime"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Runtime"
)

target_link_libraries(GrowTrace PRIVATE
    glfw
    imgui
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(GrowTrace PRIVATE
        winmm
        ws2_32
        winhttp
    )
    target_compile_definitions(GrowTrace PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_include_directories(GrowTrace PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(GrowTrace PRIVATE ${CURL_LIBRARIES})
endif()
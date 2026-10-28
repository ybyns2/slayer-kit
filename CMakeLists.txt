cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED SK_APP_VERSION)
    set(SK_APP_VERSION 1.0.0)
endif()
project(SlayerKit VERSION ${SK_APP_VERSION} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)
FetchContent_Declare(qrcodegen
    URL https://github.com/nayuki/QR-Code-generator/archive/refs/tags/v1.8.0.tar.gz
    URL_HASH SHA256=2ec0a4d33d6f521c942eeaf473d42d5fe139abcfa57d2beffe10c5cf7d34ae60
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(qrcodegen)
if(NOT qrcodegen_POPULATED)
    FetchContent_Populate(qrcodegen)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(SlayerKit WIN32 ${SOURCES} res/app.rc ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp)
set_source_files_properties(res/app.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/kit/obs/decompte-live.lua;${CMAKE_SOURCE_DIR}/kit/obs/fin-de-live.lua;${CMAKE_SOURCE_DIR}/kit/obs/slayerkit-events.lua;${CMAKE_SOURCE_DIR}/res/logo.png;${CMAKE_SOURCE_DIR}/res/app.ico;${CMAKE_SOURCE_DIR}/res/app.manifest;${CMAKE_SOURCE_DIR}/res/app-dev.manifest")
target_include_directories(SlayerKit PRIVATE src ${qrcodegen_SOURCE_DIR}/cpp)
target_link_libraries(SlayerKit PRIVATE
    nlohmann_json::nlohmann_json
    d2d1 dwrite dxgi dbghelp windowscodecs winhttp ws2_32 iphlpapi crypt32 bcrypt shell32 shlwapi ole32 comctl32 dwmapi version uxtheme)
target_compile_definitions(SlayerKit PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    SK_VERSION="${PROJECT_VERSION}"
    SK_VERSION_COMMA=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0)
option(SK_DEV "Build de developpement sans elevation admin" OFF)
if(SK_DEV)
    target_compile_definitions(SlayerKit PRIVATE SK_DEV)
endif()
target_compile_options(SlayerKit PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/W4;/permissive-;/EHsc>")
target_link_options(SlayerKit PRIVATE /MANIFEST:NO)

cmake_minimum_required(VERSION 3.20)
project(routeforge VERSION 1.0.0 LANGUAGES CXX)
# Allows current CMake releases to configure third-party projects that still
# declare compatibility with versions older than CMake 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(ROUTEFORGE_WITH_POSTGRES "Enable PostgreSQL persistence through libpqxx" OFF)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# spdlog
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

add_library(routeforge_core
    src/core/graph.cpp
    src/core/dijkstra.cpp
    src/core/yen.cpp
    src/core/failure.cpp
)

target_include_directories(routeforge_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(routeforge_core PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)

add_library(routeforge_service
    src/service/route_service.cpp
)
target_include_directories(routeforge_service PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(routeforge_service PUBLIC routeforge_core)

if(ROUTEFORGE_WITH_POSTGRES)
    find_package(PQXX REQUIRED)
    target_sources(routeforge_service PRIVATE src/storage/postgres_repository.cpp)
    target_link_libraries(routeforge_service PRIVATE pqxx)
    target_compile_definitions(routeforge_service PRIVATE ROUTEFORGE_WITH_POSTGRES=1)
endif()

add_library(routeforge_api
    src/api/server.cpp
)
target_include_directories(routeforge_api PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(routeforge_api PUBLIC routeforge_service httplib)

add_executable(routeforge_server
    src/main.cpp
)
target_link_libraries(routeforge_server PRIVATE routeforge_api)

# Testing
enable_testing()

add_executable(unit_tests
    tests/unit/test_graph.cpp
    tests/unit/test_dijkstra.cpp
)

target_link_libraries(unit_tests PRIVATE routeforge_core gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)

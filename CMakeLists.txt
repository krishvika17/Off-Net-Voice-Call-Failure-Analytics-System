cmake_minimum_required(VERSION 3.16)
project(VoiceCallFailureAnalytics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Dependencies
# =============================================================================

include(FetchContent)

# nlohmann/json — header-only JSON library
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# =============================================================================
# Sources
# =============================================================================
set(SOURCES
    src/main.cpp
    src/DatasetParser.cpp
    src/FilterEngine.cpp
    src/AnalyticsEngine.cpp
    src/RecommendationEngine.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(vcfas ${SOURCES})

target_include_directories(vcfas PRIVATE include)
target_link_libraries(vcfas PRIVATE
    nlohmann_json::nlohmann_json
)


# =============================================================================
# Compiler warnings
# =============================================================================
if(MSVC)
    target_compile_options(vcfas PRIVATE /W4)
else()
    target_compile_options(vcfas PRIVATE -Wall -Wextra -Wpedantic)
endif()
install(TARGETS vcfas DESTINATION bin)
project(GridBot
        LANGUAGES CXX C
        DESCRIPTION "GridBot Lib used by SpotGridBot"
        )

set(SOURCE_FILES
        ${GridBot_SOURCE_DIR}/src/GridStrategy.cpp
        ${GridBot_SOURCE_DIR}/src/GridConfig.cpp
)

add_library(GridBot SHARED ${SOURCE_FILES})

target_include_directories(GridBot PUBLIC ${GridBot_SOURCE_DIR}/include)

target_link_libraries(GridBot PUBLIC
        Poco::Foundation
        Utils
)

#add_subdirectory(tests)

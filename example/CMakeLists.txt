file(GLOB EXAMPLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(ComponEx ${EXAMPLE_SOURCES})

target_include_directories(ComponEx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ComponEx
  PRIVATE ComponEng
)

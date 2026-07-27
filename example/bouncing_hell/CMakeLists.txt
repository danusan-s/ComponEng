file(GLOB BOUNCING_HELL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(ComponEx ${BOUNCING_HELL_SOURCES})

target_include_directories(ComponEx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ComponEx
  PRIVATE ComponEng
)

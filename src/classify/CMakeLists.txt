set(DENIGMA_CLASSIFY_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/articulations.cpp
    ${CMAKE_CURRENT_LIST_DIR}/clefs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dynamics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/jumps.cpp
)

# The dependencies here are deliberately kept as light as possible.
# Be careful about adding dependencies to this or to its dependencies.

add_denigma_internal_library(denigma_classify ${DENIGMA_CLASSIFY_SOURCES})
target_link_libraries(denigma_classify
    PUBLIC
        musx
    PRIVATE
        smufl_mapping
        denigma_utf8
)

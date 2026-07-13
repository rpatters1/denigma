set(DENIGMA_CLASSIFY_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/articulations.cpp
    ${CMAKE_CURRENT_LIST_DIR}/barlines.cpp
    ${CMAKE_CURRENT_LIST_DIR}/chords.cpp
    ${CMAKE_CURRENT_LIST_DIR}/clefs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dynamics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/entries.cpp
    ${CMAKE_CURRENT_LIST_DIR}/expressions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/general_lines.cpp
    ${CMAKE_CURRENT_LIST_DIR}/jumps.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard_pedals.cpp
    ${CMAKE_CURRENT_LIST_DIR}/noteheads.cpp
    ${CMAKE_CURRENT_LIST_DIR}/octaves.cpp
    ${CMAKE_CURRENT_LIST_DIR}/smartshapes.cpp
)

# The dependencies here are deliberately kept as light as possible.
# Be careful about adding dependencies to this or to its dependencies.
# Classifiers intentionally depend on musxdom plus smufl_mapping. They should
# not depend on json, pugixml, text metrics, zip, or full denigma_utils.

add_denigma_internal_library(denigma_classify MUSX_PCH ${DENIGMA_CLASSIFY_SOURCES})
add_library(denigma::classify ALIAS denigma_classify)
target_link_libraries(denigma_classify
    PUBLIC
        musx
    PRIVATE
        smufl_mapping
        denigma_utf8
)

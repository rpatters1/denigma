# Copyright (C) 2026, Robert Patterson
#
# Build-time git commit capture. Rewrites the generated source only when the
# value changes, so normal incremental builds stay warm while provenance follows
# HEAD without requiring a CMake reconfigure.

set(DENIGMA_GIT_COMMIT "unknown")

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${DENIGMA_GIT_COMMIT_SRC_DIR}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE _denigma_git_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _denigma_git_commit_result
        ERROR_QUIET
    )
    if(_denigma_git_commit_result EQUAL 0 AND _denigma_git_commit)
        set(DENIGMA_GIT_COMMIT "${_denigma_git_commit}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${DENIGMA_GIT_COMMIT_SRC_DIR}" status --porcelain
            OUTPUT_VARIABLE _denigma_git_dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_denigma_git_dirty)
            set(DENIGMA_GIT_COMMIT "${DENIGMA_GIT_COMMIT}-dirty")
        endif()
    endif()
endif()

configure_file("${DENIGMA_GIT_COMMIT_TEMPLATE}" "${DENIGMA_GIT_COMMIT_OUT}.tmp" @ONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${DENIGMA_GIT_COMMIT_OUT}.tmp" "${DENIGMA_GIT_COMMIT_OUT}"
)
file(REMOVE "${DENIGMA_GIT_COMMIT_OUT}.tmp")

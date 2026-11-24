# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-License-Identifier: GPL-2.0-or-later

# generate git/build information
include(GetSCMRev)

function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

get_timestamp(BUILD_DATE)

# Lichii for The Duskbloods releases are numbered v4, v4.1, v4.1.1...: dist/dusk_version.txt holds
# the number the updater parses (N, N.M or N.M.P; N.0 is spelled N).
set(DUSK_VERSION "")
set(DUSK_VERSION_FILE "${CMAKE_SOURCE_DIR}/dist/dusk_version.txt")
if (EXISTS "${DUSK_VERSION_FILE}" AND NOT LICHII_BUILD_TAG MATCHES "^build-[0-9]+$")
    file(STRINGS "${DUSK_VERSION_FILE}" DUSK_VERSION LIMIT_COUNT 1 REGEX "^[0-9]+([.][0-9]+)?([.][0-9]+)?$")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DUSK_VERSION_FILE}")
    # The log names the commit: reconfigure when HEAD moves (commit, checkout, merge, revert).
    if (EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
    endif()
endif()

if (LICHII_BUILD_TAG MATCHES "^build-[0-9]+$")
    set(BUILD_VERSION "${LICHII_BUILD_TAG}")
    set(IS_DEV_BUILD false)
elseif (DUSK_VERSION)
    set(BUILD_VERSION "v${DUSK_VERSION}")
    set(IS_DEV_BUILD false)
elseif (DEFINED GIT_RELEASE)
    set(BUILD_VERSION "${GIT_TAG}")
    set(GIT_REFSPEC "${GIT_RELEASE}")
    set(IS_DEV_BUILD false)
else()
    string(SUBSTRING ${GIT_COMMIT} 0 10 BUILD_VERSION)
    set(BUILD_VERSION "${BUILD_VERSION}-${GIT_REFSPEC}")
    set(IS_DEV_BUILD true)
endif()

if (NIGHTLY_BUILD)
    set(IS_NIGHTLY_BUILD true)
else()
    set(IS_NIGHTLY_BUILD false)
endif()

if (DUSK_VERSION)
    # Logs keep the commit: "Lichii v4.1 | <commit>".
    string(SUBSTRING ${GIT_COMMIT} 0 10 GIT_DESC)
else()
    set(GIT_DESC ${BUILD_VERSION})
endif()

# Generate cpp with Git revision from template

set(BUILD_AUTO_UPDATE_STABLE_REPO "Mr-Tb/lichii-dusk")
set(BUILD_AUTO_UPDATE_STABLE_API "api.github.com")
set(BUILD_AUTO_UPDATE_API_PATH "/repos/Mr-Tb/lichii-dusk/releases?per_page=100")
set(BUILD_AUTO_UPDATE_WEBSITE "https://github.com")
set(BUILD_AUTO_UPDATE_REPO "Mr-Tb/lichii-dusk")
if (NIGHTLY_BUILD)
    set(REPO_NAME "Lichii Nightly")
else()
    set(REPO_NAME "Lichii")
endif()

# What players see (title bar, About, update messages): dist/dusk_version_label.txt when it exists
# (a test build: "v4.1 test 9"), else the version.
set(BUILD_LABEL "${BUILD_VERSION}")
set(DUSK_LABEL_FILE "${CMAKE_SOURCE_DIR}/dist/dusk_version_label.txt")
if (DUSK_VERSION AND EXISTS "${DUSK_LABEL_FILE}")
    file(STRINGS "${DUSK_LABEL_FILE}" DUSK_LABEL LIMIT_COUNT 1)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DUSK_LABEL_FILE}")
    if (DUSK_LABEL)
        set(BUILD_LABEL "${DUSK_LABEL}")
    endif()
endif()

set(BUILD_ID ${GIT_REFSPEC})
set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")
set(CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(scm_rev.cpp.in scm_rev.cpp @ONLY)

cmake_minimum_required(VERSION 3.22.1)

project(mailfilter_sqlite_core LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

option(MF_ENABLE_ANDROID_LOG "Use Android logcat logging backend" ON)
option(MF_ENABLE_EXPLANATIONS "Enable explanation JSON generation" ON)
option(MF_ENABLE_ADDITIVE_TABLES "Allow additive Android-only tables" ON)

set(MF_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MF_COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat)
set(MF_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)

set(MF_CORE_SOURCES
    ${MF_CORE_DIR}/api/mf_core_api.cc
    ${MF_CORE_DIR}/api/mf_result_json.cc
    ${MF_CORE_DIR}/import/mf_db_open.cc
    ${MF_CORE_DIR}/import/mf_seq_header_import.cc
    ${MF_COMPAT_DIR}/feedback.cc
    ${MF_COMPAT_DIR}/mf_log.cc
    ${MF_COMPAT_DIR}/mf_paths.cc
    ${MF_CORE_DIR}/db/dblog.cc
    ${MF_CORE_DIR}/header/header.cc
)

add_library(mailfilter_sqlite_core SHARED ${MF_CORE_SOURCES})

target_include_directories(mailfilter_sqlite_core
    PUBLIC
        ${MF_INCLUDE_DIR}
    PRIVATE
        ${MF_COMPAT_DIR}
        ${MF_CORE_DIR}
        ${MF_CORE_DIR}/api
        ${MF_CORE_DIR}/db
        ${MF_CORE_DIR}/header
        ${MF_CORE_DIR}/rules
        ${MF_CORE_DIR}/config
        ${MF_CORE_DIR}/parser
        ${MF_CORE_DIR}/import
)

target_compile_definitions(mailfilter_sqlite_core
    PRIVATE
        MF_ANDROID_CORE=1
        MF_DISABLE_TRANSPORT=1
        MF_DISABLE_CLI=1
        $<$<BOOL:${MF_ENABLE_ANDROID_LOG}>:MF_ENABLE_ANDROID_LOG=1>
        $<$<BOOL:${MF_ENABLE_EXPLANATIONS}>:MF_ENABLE_EXPLANATIONS=1>
        $<$<BOOL:${MF_ENABLE_ADDITIVE_TABLES}>:MF_ENABLE_ADDITIVE_TABLES=1>
)

find_library(z-lib z)
find_library(m-lib m)

if(ANDROID)
    find_library(log-lib log)
endif()

target_link_libraries(mailfilter_sqlite_core
    PRIVATE
        ${z-lib}
        ${m-lib}
        sqlite3
)

if(ANDROID AND log-lib)
    target_link_libraries(mailfilter_sqlite_core PRIVATE ${log-lib})
endif()

set_target_properties(mailfilter_sqlite_core PROPERTIES
    OUTPUT_NAME "mailfilter_sqlite_core"
)

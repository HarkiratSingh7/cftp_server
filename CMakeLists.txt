cmake_minimum_required(VERSION 3.10)
project(cftp_server VERSION 1.0)

set(CMAKE_C_STANDARD 99)
# Link required libraries
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent libevent_openssl)

# Misc
set(CERT_DIR /etc/ssl/certs)
set(KEY_DIR /etc/ssl/private)
set(CRT_FILE ${CERT_DIR}/cftp_server.crt)
set(KEY_FILE ${KEY_DIR}/cftp_server.key)
# Make sure the certs directory exists
file(MAKE_DIRECTORY ${CERT_DIR})
add_custom_command(
    OUTPUT ${KEY_FILE} ${CRT_FILE}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating TLS certs..."
    COMMAND openssl req -x509 -newkey rsa:2048 -nodes -keyout ${KEY_FILE} -out ${CRT_FILE}
            -days 365 -subj "/CN=localhost"
    COMMENT "Generating self-signed TLS cert and key and placing them in ${CERT_DIR}"
    VERBATIM
)

add_custom_target(generate_certs ALL
    DEPENDS ${KEY_FILE} ${CRT_FILE}
)


# Include header files
include_directories(src/actions)
include_directories(src/core)   
include_directories(src/engine)
include_directories(src/ipc)
include_directories(src/security)

set(CFTP_ACTIONS
    src/actions/list.c
    src/actions/retr.c
    src/actions/stor.c
    src/actions/dele.c
    src/actions/command_actions.c)

set(CFTP_IPC
    src/ipc/interprocess_handler.c)

set(CFTP_ENGINE
    src/engine/control_handler.c
    src/engine/data_handler.c
    src/engine/command_parser.c
    src/engine/main.c)

set(CFTP_SECURITY
    src/security/auth.c
    src/security/security.c)

set(CFTP_CORE
    src/core/connection.c
    src/core/error.c
    src/core/structures/hashmap.c
    src/config_manager/config_manager.c
    src/core/logger.c
    src/core/server_state.c)

# Source files
set(SOURCES
    ${CFTP_CORE}
    ${CFTP_SECURITY}
    ${CFTP_ENGINE}
    ${CFTP_IPC}
    ${CFTP_ACTIONS}
)

# Executable
add_executable(cftp_server ${SOURCES})
add_dependencies(cftp_server generate_certs)

# Optimization flags for release
target_compile_options(cftp_server PRIVATE -O3)
target_link_options(cftp_server PRIVATE -s)  # Strip symbols
target_compile_definitions(cftp_server PRIVATE NDEBUG)  # Disable asserts
target_link_libraries(cftp_server ${LIBEVENT_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto crypt)
target_include_directories(cftp_server PRIVATE ${LIBEVENT_INCLUDE_DIRS})

# ----------------------------------------
# Debug Executable (Full Debug + Valgrind-friendly)
# ----------------------------------------
add_executable(cftp_server_debug ${SOURCES})
add_dependencies(cftp_server_debug generate_certs)

# ----------------------------------------
# Shared Library for Unit Tests
# ----------------------------------------
add_library(cftp_unit_test_lib SHARED ${SOURCES})
add_dependencies(cftp_unit_test_lib generate_certs)
target_compile_definitions(cftp_unit_test_lib PRIVATE DEBUG_TRY_BIND)  # Optional debug macros
target_link_libraries(cftp_unit_test_lib ${LIBEVENT_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto crypt)
target_include_directories(cftp_unit_test_lib PRIVATE ${LIBEVENT_INCLUDE_DIRS})

# Debug flags: DWARF-4, No LTO, lots of diagnostics
target_compile_options(cftp_server_debug PRIVATE ${STRICT__WARNINGS}
    -O0
    -g3
    -gdwarf-4
    -Wall
    -Wextra
    -Wpedantic
    -fno-omit-frame-pointer
)

set(STRICT_WARNINGS
    -Wall                   # all common warnings
    -Wextra                 # extra warnings
    -Wpedantic              # strict ISO compliance
    -Werror                 # turn warnings into errors
    -Wunused                 # warns about unused variables, functions, labels
    -Wunused-function        # specifically unused functions
    -Wunused-variable        # specifically unused variables
    -Wunused-parameter       # unused function params
    -Wshadow                # shadowing warnings
    -Wuninitialized          # uninitialized vars
    -Wmissing-prototypes     # functions must be declared before use
    -Wstrict-prototypes      # strict C prototypes
    -Wformat=2              # format string warnings
)

target_compile_options(cftp_server PRIVATE ${STRICT_WARNINGS})
target_compile_options(cftp_server_debug PRIVATE ${STRICT_WARNINGS})

target_link_options(cftp_server_debug PRIVATE -fsanitize=address,undefined)
target_compile_definitions(cftp_server_debug PRIVATE DEBUG)  # Optional debug macros
target_link_libraries(cftp_server_debug ${LIBEVENT_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto crypt)
target_include_directories(cftp_server_debug PRIVATE ${LIBEVENT_INCLUDE_DIRS})

# Add strict warnings and treat them as errors for GCC/Clang
if (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(cftp_server PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror                         # Turn all warnings into errors
        -Wshadow                        # Warn on variable shadowing
        -Wunused                        # Warn on unused variables/functions/etc.
        -Wuninitialized                 # Warn on uninitialized variables
        -Wmissing-prototypes            # Warn if function not declared before use
        -Wstrict-prototypes             # Enforce proper C prototypes
        -Wformat=2                      # Stricter format string checks
        -Wno-unused-parameter           # Ignore unused param warnings (common in callbacks)
    )
endif()

find_package(Git QUIET)

set(GIT_DESCRIBE "unknown")
set(GIT_BRANCH "unknown")
set(GIT_HASH "nogit")

if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --match v[0-9]* --dirty --long --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

# Extract SemVer if present
set(BASE "0.0.0")
set(COMMITS "0")
set(DIRTY "")
if(GIT_DESCRIBE MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
  set(BASE "${CMAKE_MATCH_1}")
  set(COMMITS "${CMAKE_MATCH_2}")
  if(CMAKE_MATCH_4)
    set(DIRTY "-dirty")
  endif()
elseif(GIT_DESCRIBE MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)(-dirty)?$")
  set(BASE "${CMAKE_MATCH_1}")
  if(CMAKE_MATCH_2)
    set(DIRTY "-dirty")
  endif()
else()
  # Untagged repo fallback: 0.0.0-<branch>[-dirty]
  if(GIT_DESCRIBE MATCHES "-dirty$")
    set(DIRTY "-dirty")
  endif()
endif()

# Sanitize branch for embedding in version (replace anything not A-Za-z0-9._- with '-')
set(BRANCH_SAFE "${GIT_BRANCH}")
string(REGEX REPLACE "[^A-Za-z0-9._-]" "-" BRANCH_SAFE "${BRANCH_SAFE}")

# Handle detached HEAD by substituting a hash-based identifier
if(BRANCH_SAFE STREQUAL "HEAD")
  set(BRANCH_SAFE "detached-${GIT_HASH}")
endif()

# Compose SERVER_VERSION
if(NOT BASE STREQUAL "0.0.0")
  # Tagged history: keep prior behavior (example)
  if(COMMITS STREQUAL "0")
    set(SERVER_VERSION "${BASE}${DIRTY}")
  else()
    set(SERVER_VERSION "${BASE}-dev.${COMMITS}${DIRTY}+g${GIT_HASH}")
  endif()
else()
  # Untagged repo: requested format 0.0.0-<branch-name>-dirty
  set(SERVER_VERSION "0.0.0-${BRANCH_SAFE}${DIRTY}")
endif()

add_compile_definitions(SERVER_VERSION="${SERVER_VERSION}")

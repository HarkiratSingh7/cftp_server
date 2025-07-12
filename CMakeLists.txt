cmake_minimum_required(VERSION 3.10)
project(cftp_server C)

# Misc
set(CERT_DIR ${PROJECT_SOURCE_DIR}/certs)
set(KEY_FILE ${CERT_DIR}/server.key)
set(CRT_FILE ${CERT_DIR}/server.crt)
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

set(CMAKE_C_STANDARD 99)

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
    src/core/hashmap.c
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

# Link required libraries
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent libevent_openssl)

target_include_directories(cftp_server PRIVATE ${LIBEVENT_INCLUDE_DIRS})
target_link_libraries(cftp_server ${LIBEVENT_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto crypt)

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
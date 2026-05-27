cmake_minimum_required(VERSION 3.16)
project(petrush LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Avisos estritos (C)
set(PETRUSH_C_FLAGS "-Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wnull-dereference -Wunused-parameter -Wconversion")

# Build type Sanitize (ASan + UBSan)
if(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
    set(SANITIZE_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS} ${PETRUSH_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZE_FLAGS}")
    message(STATUS "Sanitize build: ASan + UBSan enabled")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PETRUSH_C_FLAGS}")
endif()

# Build type Coverage
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    message(STATUS "Coverage instrumentation enabled (--coverage)")
endif()

# Includes
include_directories(include src)

# Executável principal
add_executable(petrush
    src/main.c
    src/mid/parser.c
    src/mid/dispatcher.c
    vendor/linenoise/linenoise.c
)

target_include_directories(petrush PRIVATE vendor/linenoise)

# linenoise é código terceiro antigo — relaxamos avisos só nele
# (mantemos todo o nosso código com -Werror total)
set_source_files_properties(vendor/linenoise/linenoise.c PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-function"
)

# Hardening (adaptado de driver_brother)
# FORTIFY_SOURCE exige otimização; aplicamos só em builds não-Debug
target_compile_options(petrush PRIVATE
    -fstack-protector-strong
    -fPIE
)
target_link_options(petrush PRIVATE
    -pie
    -Wl,-z,relro,-z,now,-z,noexecstack
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(petrush PRIVATE -D_FORTIFY_SOURCE=2)
endif()

# ============================================================
# Testes com acutest (escolha do usuário - melhor ROI para S0)
# ============================================================
enable_testing()

# Função auxiliar para adicionar testes com acutest
function(add_acutest TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/include
    )
    # Relaxamos apenas sign-conversion porque o acutest.h (header de terceiros) gera avisos
    target_compile_options(${TEST_NAME} PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
    )
    # Registramos o teste. Durante desenvolvimento use diretamente: ./build/test_parser
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Testes TDD ativos
add_executable(test_parser 
    tests/test_parser.c 
    src/mid/parser.c
)
target_include_directories(test_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_parser PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_parser COMMAND test_parser)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_parser
    COMMENT "Executando todos os testes unitários"
)

# ============================================================
# Targets de qualidade (lint)
# ============================================================
find_program(CPPCHECK_EXE NAMES cppcheck)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXE}
            --enable=warning,performance,portability
            --error-exitcode=1
            --inline-suppr
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --std=c23
            --quiet
            -I${CMAKE_SOURCE_DIR}/include
            -I${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running cppcheck (C23)"
        VERBATIM
    )
endif()

if(CLANG_TIDY_EXE)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p=${CMAKE_BINARY_DIR}
            --quiet
            ${CMAKE_SOURCE_DIR}/src/main.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
endif()

if(CPPCHECK_EXE AND CLANG_TIDY_EXE)
    add_custom_target(lint DEPENDS cppcheck clang-tidy)
endif()

# Mensagem final
message(STATUS "petrush configurado para C23 com hardening e análise estática")
message(STATUS "Use: cmake --build . --target lint   (se cppcheck/clang-tidy estiverem instalados)")

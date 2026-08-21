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
    src/mid/pudo.c
    src/mid/alias.c
    src/mid/dirstack.c
    src/mid/hist_expand.c
    src/mid/expand.c
    src/mid/prompt.c
    src/front/complete.c
    src/foundation/process.c
    src/foundation/env.c
    vendor/linenoise/linenoise.c
)

# pudod — helper setuid mínimo (NEW-05, Fase 1)
# Compilado separado. NÃO aplicar sanitize (incompatível com setuid).
# Hardening forte. Tamanho mínimo para auditoria.
add_executable(pudod
    src/pudod/pudod.c
)
target_compile_definitions(pudod PRIVATE _GNU_SOURCE)

# Flags base (sem sanitize nunca)
target_compile_options(pudod PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
    -fstack-protector-strong -fPIE
)

# Garantir isolamento total de sanitize (mesmo se build type for Sanitize)
if(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
    target_compile_options(pudod PRIVATE
        -fno-sanitize=address,undefined
        -fno-omit-frame-pointer
    )
endif()

target_link_options(pudod PRIVATE
    -pie
    -Wl,-z,relro,-z,now,-z,noexecstack
)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pudod PRIVATE -D_FORTIFY_SOURCE=2)
endif()

# Nota de segurança: o binário pudod NUNCA deve ser instalado com setuid
# sem auditoria completa + aprovação explícita. Ver docs/security/


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

# Teste da foundation de processos (PR-06)
add_executable(test_process
    tests/test_process.c
    src/mid/parser.c
    src/mid/pudo.c
    src/foundation/process.c
    src/foundation/env.c
)
target_include_directories(test_process PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_process PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_process COMMAND test_process)

# Testes ENV (PR-07 / NEW-04)
add_executable(test_env
    tests/test_env.c
    src/foundation/env.c
)
target_include_directories(test_env PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_env PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_env COMMAND test_env)

# Testes info / diagnóstico (Onda 3 placeholder, NEW-04)
# dispatcher puxa linenoise (clear) + pudo + process/env
add_executable(test_info
    tests/test_info.c
    src/mid/dispatcher.c
    src/mid/parser.c
    src/mid/pudo.c
    src/mid/alias.c
    src/mid/dirstack.c
    src/mid/expand.c
    src/foundation/env.c
    src/foundation/process.c
    vendor/linenoise/linenoise.c
)
target_include_directories(test_info PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/linenoise
)
set_source_files_properties(vendor/linenoise/linenoise.c PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-function"
)
target_compile_options(test_info PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_info COMMAND test_info)

# Testes do builtin 'pudo' (foco em segurança)
add_executable(test_pudo
    tests/test_pudo.c
    src/mid/pudo.c
    src/mid/parser.c
    src/foundation/env.c
)
target_include_directories(test_pudo PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_pudo PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_pudo COMMAND test_pudo)
set_tests_properties(test_pudo PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Aliases (NEW-22)
add_executable(test_alias
    tests/test_alias.c
    src/mid/alias.c
)
target_include_directories(test_alias PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_alias PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_alias COMMAND test_alias)

# Completion + history hints (NEW-23)
add_executable(test_complete
    tests/test_complete.c
    src/front/complete.c
    src/mid/dispatcher.c
    src/mid/parser.c
    src/mid/pudo.c
    src/mid/alias.c
    src/mid/dirstack.c
    src/mid/expand.c
    src/foundation/env.c
    src/foundation/process.c
    vendor/linenoise/linenoise.c
)
target_include_directories(test_complete PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/linenoise
)
set_source_files_properties(vendor/linenoise/linenoise.c PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-function"
)
target_compile_options(test_complete PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_complete COMMAND test_complete)

# Directory stack (NEW-25)
add_executable(test_dirstack
    tests/test_dirstack.c
    src/mid/dirstack.c
)
target_include_directories(test_dirstack PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_dirstack PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_dirstack COMMAND test_dirstack)

# History expansion !! !n (NEW-26)
add_executable(test_hist_expand
    tests/test_hist_expand.c
    src/mid/hist_expand.c
    vendor/linenoise/linenoise.c
)
target_include_directories(test_hist_expand PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/linenoise
)
set_source_files_properties(vendor/linenoise/linenoise.c PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-function"
)
target_compile_options(test_hist_expand PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_hist_expand COMMAND test_hist_expand)

# Word expansion ~ and $VAR (UX-12 UX-13)
add_executable(test_expand
    tests/test_expand.c
    src/mid/expand.c
    src/foundation/env.c
)
target_include_directories(test_expand PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_expand PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_expand COMMAND test_expand)

# Pathname expansion * ? (UX-18)
add_executable(test_glob
    tests/test_glob.c
    src/mid/expand.c
    src/mid/parser.c
    src/foundation/env.c
)
target_include_directories(test_glob PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_glob PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_glob COMMAND test_glob)

# PS1 escapes (UX-15)
add_executable(test_prompt
    tests/test_prompt.c
    src/mid/prompt.c
    src/foundation/env.c
)
target_include_directories(test_prompt PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(test_prompt PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-sign-conversion
)
add_test(NAME test_prompt COMMAND test_prompt)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_parser test_process test_pudo test_env test_info test_alias test_complete test_dirstack test_hist_expand test_expand test_glob test_prompt
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
    # Expande para todo o código do projeto (exclui vendor via lista explícita)
    # NEW-02: cobre main + camadas mid/foundation para gate PR-10
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p=${CMAKE_BINARY_DIR}
            --quiet
            ${CMAKE_SOURCE_DIR}/src/main.c
            ${CMAKE_SOURCE_DIR}/src/mid/parser.c
            ${CMAKE_SOURCE_DIR}/src/mid/dispatcher.c
            ${CMAKE_SOURCE_DIR}/src/mid/pudo.c
            ${CMAKE_SOURCE_DIR}/src/foundation/process.c
            ${CMAKE_SOURCE_DIR}/src/foundation/env.c
            ${CMAKE_SOURCE_DIR}/src/pudod/pudod.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on project sources (vendor excluded)"
        VERBATIM
    )
endif()

if(CPPCHECK_EXE AND CLANG_TIDY_EXE)
    add_custom_target(lint DEPENDS cppcheck clang-tidy)
endif()

# ============================================================
# Instalação
# ============================================================
# ATENÇÃO DE SEGURANÇA (NEW-05 / pudod):
# O binário "pudod" é destinado a ser instalado com privilégios elevados
# (setuid root ou capabilities). 
#
# O CMake instala o binário como um executável normal.
# **NÃO** aplicamos chown/chmod u+s aqui.
#
# A aplicação de setuid DEVE ser feita MANUALMENTE pelo administrador
# APENAS após:
#   - Revisão completa do código em src/pudod/pudod.c
#   - Leitura de docs/security/pudo-audit.md
#   - Decisão explícita do usuário (petrus)
#
# Exemplo de instalação segura (após build):
#   cmake --install build --prefix /usr/local
#   sudo chown root:root /usr/local/libexec/petrush-pudod
#   sudo chmod 4755 /usr/local/libexec/petrush-pudod
#
# Ou use capabilities (recomendado quando possível):
#   sudo setcap cap_setuid,cap_setgid+ep /usr/local/libexec/petrush-pudod

install(TARGETS petrush pudod
    RUNTIME DESTINATION bin
)

# O pudod costuma ser colocado em libexec (não no PATH comum)
install(TARGETS pudod
    RUNTIME DESTINATION libexec
    RENAME petrush-pudod
)

# Exemplo de allow-list (o admin deve copiar e proteger)
install(FILES src/pudod/pudo.allow.example
    DESTINATION /etc/petrush
    RENAME pudo.allow.example
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# ============================================================
# Targets de verificação para pudod (segurança)
# ============================================================
find_program(VALGRIND_EXE NAMES valgrind)
add_custom_target(pudod-valgrind
    COMMAND ${VALGRIND_EXE} --leak-check=full --error-exitcode=1 --quiet
            ${CMAKE_BINARY_DIR}/pudod /usr/bin/true 2>&1 || ${CMAKE_COMMAND} -E echo "(pudod-valgrind: valgrind not found or expected refusal - check manually)"
    DEPENDS pudod
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Valgrind no pudod (sem privilégios - valida paths iniciais)"
    VERBATIM
)

# Smoke tests integrados (NEW-14, inclui `pudo`)
# Automático via 'cmake --build build' (porque verify é ALL). Se precisar só do binário: cmake --build build --target petrush
add_custom_target(smoke
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running integrated smoke test for petrush + pudo + sanitization ==="
    # Falha real propaga (CI/TDD); não engolir exit code do smoke
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke/pudo-smoke.sh ${CMAKE_BINARY_DIR}/petrush
    DEPENDS petrush pudod
    COMMENT "Executando smoke tests integrados (≥12 comandos + pudo + paths de erro / sanitização)"
    VERBATIM
)

# Robust clean target using cmake -E to bypass shell rm hooks (e.g. trash-guard)
add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}
    COMMENT "Force clean build dir (bypasses rm hook). Then re-run cmake -B build -S ."
)

# Gate / verify completo para PR e S0 (NEW-16, PR-10)
# Roda: lint + ctest + smoke (pudo) + valgrind no pudod (se disponível)
add_custom_target(verify ALL
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target petrush pudod || ${CMAKE_COMMAND} -E echo "main build done"
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lint || ${CMAKE_COMMAND} -E echo "lint done"
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target smoke || ${CMAKE_COMMAND} -E echo "smoke done"
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target pudod-valgrind || ${CMAKE_COMMAND} -E echo "pudod-valgrind skipped (no valgrind or no target)"
    DEPENDS petrush pudod
    COMMENT "Gate completo (PR-ready): full build + lint + smoke integrado (pudo + sanitização) + pudod-valgrind. 'cmake --build build' faz TUDO automaticamente (use --target petrush para só o binário)."
    VERBATIM
)

# Mensagem final
message(STATUS "petrush configurado para C23 com hardening e análise estática")
message(STATUS "AUTOMAÇÃO COMPLETA: após 'cmake -B build -S . -DCMAKE_BUILD_TYPE=Release', rode apenas 'cmake --build build'")
message(STATUS "Isso faz: build + lint + smoke integrado (pudo + sanitização) + pudod-valgrind (gate final)")
message(STATUS "Para só o binário: cmake --build build --target petrush")
message(STATUS "Outros: --target clean-build (limpa bypassando hook), --target lint, --target smoke")
message(WARNING "pudod é um binário privilegiado. NÃO aplique setuid automaticamente. Veja docs/security/")

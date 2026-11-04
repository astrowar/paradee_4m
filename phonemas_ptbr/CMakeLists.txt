cmake_minimum_required(VERSION 3.16)
project(phonemas_ptbr VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Opção de Sanitizadores para verificação de vazamento e memória
option(ENABLE_SANITIZERS "Habilitar AddressSanitizer, LeakSanitizer e UndefinedBehaviorSanitizer" OFF)
if(ENABLE_SANITIZERS)
    message(STATUS "Sanitizadores ativados (-fsanitize=address,undefined)")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Diretório de includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Biblioteca estática (para inclusão direta em ESP-IDF, Arduino ou C++ embarcado)
add_library(phonemas_ptbr_static STATIC
    src/phonemas_ptbr.cpp
)

# Biblioteca compartilhada (.so)
add_library(phonemas_ptbr SHARED
    src/phonemas_ptbr.cpp
)

# Ferramenta de linha de comando CLI
add_executable(phonemas_ptbr_cli
    src/main.cpp
)
target_link_libraries(phonemas_ptbr_cli PRIVATE phonemas_ptbr_static)
set_target_properties(phonemas_ptbr_cli PROPERTIES OUTPUT_NAME "phonemas_ptbr")

# Executável de testes unitários
add_executable(test_phonemas_ptbr
    tests/test_main.cpp
)
target_link_libraries(test_phonemas_ptbr PRIVATE phonemas_ptbr_static)

# Executável de testes de estresse e sanitização
add_executable(stress_test_phonemas_ptbr
    tests/stress_test.cpp
)
target_link_libraries(stress_test_phonemas_ptbr PRIVATE phonemas_ptbr_static)

# Instalação
install(TARGETS phonemas_ptbr phonemas_ptbr_static phonemas_ptbr_cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES include/phonemas_ptbr.h include/embedded_vocab.h include/embedded_exceptions.h DESTINATION include)

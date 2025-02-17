# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# == DO NOT EDIT ... etc. ==

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# PICO_BOARD e includes do pico_sdk
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(checkin C CXX ASM)

# (1) Definir lwipopts.h aqui
set(PICO_LWIP_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/lwipopts.h")

# (2) Inicializar o SDK
pico_sdk_init()

# (3) Adicionar executável
add_executable(checkin 
    checkin.c
    src/ssd1306_i2c.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
    # ... se tiver mais fontes ...
)

# Criação explícita do diretório 'generated' para armazenar os arquivos gerados
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

# Gera o cabeçalho a partir do arquivo ws2812.pio e coloca em /generated
pico_generate_pio_header(checkin ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

pico_set_program_name(checkin "checkin")
pico_set_program_version(checkin "0.1")

pico_enable_stdio_uart(checkin 0)
pico_enable_stdio_usb(checkin 1)

# (4) Linkar bibliotecas necessárias
target_link_libraries(checkin 
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_i2c
    hardware_pio
)

# (5) Incluir diretórios
target_include_directories(checkin PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}    # para encontrar lwipopts.h na raiz
    ${CMAKE_CURRENT_LIST_DIR}/inc
    # se precisar: ${CMAKE_CURRENT_LIST_DIR}/dhcpserver ...
)

pico_add_extra_outputs(checkin)

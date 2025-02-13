# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(checkin C CXX ASM)

# Define o arquivo lwIP de configuração (use caminho relativo)
set(PICO_LWIP_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/lwipopts.h")

# Inicializa o SDK do Pico (chamada única)
pico_sdk_init()

# Adiciona o executável
add_executable(checkin 

    checkin.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
)

pico_set_program_name(checkin "checkin")
pico_set_program_version(checkin "0.1")

# Configura a saída (USB ou UART)
pico_enable_stdio_uart(checkin 0)
pico_enable_stdio_usb(checkin 1)

# Linka bibliotecas necessárias
target_link_libraries(checkin
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
)

# Adiciona o diretório atual ao path de includes
target_include_directories(checkin PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
    ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
    ${CMAKE_CURRENT_LIST_DIR}/dnsserver
)

pico_add_extra_outputs(checkin)


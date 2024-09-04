# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoPro C CXX ASM)

set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/Pico-PIO-USB")

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(lib/Pico-PIO-USB)

# Add executable. Default name is the project name, version 0.1

set(target_name PicoPro)
#add_executable(${target_name})
add_executable(PicoPro PicoPro.c usb_descriptors.c)

target_sources(${target_name} PRIVATE
 # can use 'tinyusb_pico_pio_usb' library later when pico-sdk is updated
 ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
 #${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
 )


pico_set_program_name(PicoPro "PicoPro")
pico_set_program_version(PicoPro "0.1")

target_compile_definitions(PicoPro PRIVATE
PICO_DEFAULT_UART_TX_PIN=16
PICO_DEFAULT_UART_RX_PIN=17
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PicoPro 1)
pico_enable_stdio_usb(PicoPro 0)

# Add the standard library to the build
target_link_libraries(PicoPro
        pico_stdlib
        tinyusb_device
        tinyusb_host
        )

# Add the standard include files to the build
target_include_directories(PicoPro PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(PicoPro 
        hardware_pio
        pico_multicore
        tinyusb_pico_pio_usb
        
        #tinyusb_board
        )

pico_add_extra_outputs(PicoPro)




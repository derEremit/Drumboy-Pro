# Drumboy Pro simulator: the firmware compiled for the host (presets "Simulator"
# on macOS, "SimulatorLinux" on Linux).
#
# The application (Core/Library, Core/Plugin, main.cpp), the CubeMX init files
# (Core/Src) and FatFs compile unchanged against the real ST headers. Replaced:
#   - CMSIS core header (Simulator/hal/core_cm7.h): intrinsics -> sim runtime
#   - HAL driver sources (src/sim_hal.cpp)
#   - SD card driver (src/sim_diskio.cpp): FatFs on a card image
#   - Lcd.cpp: compiled from a generated copy (cmake/lcd_source.cmake)
#
# Built for x86_64: the firmware keeps MCU addresses (SDRAM 0xC0000000, RAM_D3
# 0x38000000, registers) in uint32_t, so they are mapped at the same addresses,
# below 4 GB. On macOS that needs a small __PAGEZERO (arm64 reserves the whole
# low 4 GB, so Apple silicon runs the x86_64 build under Rosetta). On Linux it
# needs a non-PIE executable, so the program itself stays below 4 GB too.

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        message(FATAL_ERROR "The simulator must be built with CMAKE_OSX_ARCHITECTURES=x86_64 (use the Simulator preset)")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        message(FATAL_ERROR "The simulator needs an x86_64 Linux host (MCU addresses must fit below 4 GB)")
    endif()
else()
    message(FATAL_ERROR "The simulator builds on macOS and Linux only")
endif()

set(FW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

file(GLOB_RECURSE SIM_FIRMWARE_SOURCES CONFIGURE_DEPENDS
    ${FW_DIR}/Core/Library/*.cpp
    ${FW_DIR}/Core/Plugin/*.cpp
    ${FW_DIR}/Middlewares/Third_Party/FatFs/src/*.c
)
# Lcd.cpp is compiled from a generated copy (cmake/lcd_source.cmake)
set(LCD_SOURCE ${FW_DIR}/Core/Library/Lcd/Lcd.cpp)
set(LCD_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated/Lcd.cpp)
list(REMOVE_ITEM SIM_FIRMWARE_SOURCES ${LCD_SOURCE})
add_custom_command(
    OUTPUT ${LCD_GENERATED}
    COMMAND ${CMAKE_COMMAND} -DIN=${LCD_SOURCE} -DOUT=${LCD_GENERATED} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lcd_source.cmake
    DEPENDS ${LCD_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lcd_source.cmake
    COMMENT "Generating simulator copy of Lcd.cpp"
)

list(APPEND SIM_FIRMWARE_SOURCES
    ${LCD_GENERATED}
    ${FW_DIR}/Core/Src/main.cpp
    ${FW_DIR}/Core/Src/bdma.c
    ${FW_DIR}/Core/Src/dma.c
    ${FW_DIR}/Core/Src/fmc.c
    ${FW_DIR}/Core/Src/gpio.c
    ${FW_DIR}/Core/Src/i2c.c
    ${FW_DIR}/Core/Src/i2s.c
    ${FW_DIR}/Core/Src/sdmmc.c
    ${FW_DIR}/Core/Src/stm32h7xx_hal_msp.c
    ${FW_DIR}/Core/Src/tim.c
    ${FW_DIR}/Core/Src/usart.c
    ${FW_DIR}/FATFS/App/fatfs.c
)

# SDL3 for the panel window and sound. The distribution's SDL3 when it has one
# (Linux), else built from the release source as a static x86_64 library.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
    set(SIM_SDL_TARGET SDL3::SDL3)
    message(STATUS "Simulator: system SDL3 ${SDL3_VERSION}")
else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.10/SDL3-3.2.10.tar.gz
        URL_HASH SHA256=f87be7b4dec66db4098e9c167b2aa34e2ca10aeb5443bdde95ae03185ed513e0
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(SDL3)
    set(SIM_SDL_TARGET SDL3::SDL3-static)
endif()

# Panel drawing (a PDF): CoreGraphics on macOS, poppler + cairo on Linux
if(NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SIM_PDF REQUIRED IMPORTED_TARGET poppler-glib cairo)
endif()

# Official panel drawing (MIT), rendered at run time
get_filename_component(PANEL_ART "${FW_DIR}/../../01 Hardware/Drawing/Drumboy Pro Graphic.ai" ABSOLUTE)

set(SIM_SOURCES
    src/sim_audio.cpp
    src/sim_control.cpp
    src/sim_cpu.cpp
    src/sim_diskio.cpp
    src/sim_gpio.cpp
    src/sim_hal.cpp
    src/sim_lcd.cpp
    src/sim_libc.cpp
    src/sim_main.cpp
    src/sim_memory.cpp
    src/sim_panel.cpp
    src/sim_timers.cpp
    src/sim_window.cpp
)

add_executable(DrumboySim ${SIM_FIRMWARE_SOURCES} ${SIM_SOURCES})

target_include_directories(DrumboySim PRIVATE
    hal                                         # before the CMSIS headers: replaces core_cm7.h
    src
    ${FW_DIR}/Core/Inc
    ${FW_DIR}/FATFS/Target
    ${FW_DIR}/FATFS/App
    ${FW_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
    ${FW_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    ${FW_DIR}/Middlewares/Third_Party/FatFs/src
    ${FW_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ${FW_DIR}/Core/Library/Global
    ${FW_DIR}/Core/Library/Lcd
    ${FW_DIR}/Core/Library/Dac
    ${FW_DIR}/Core/Library/Controller
    ${FW_DIR}/Core/Plugin
)

target_compile_definitions(DrumboySim PRIVATE
    USE_PWR_LDO_SUPPLY
    USE_HAL_DRIVER
    STM32H723xx
    DRUMBOY_SIMULATOR
    $<$<BOOL:${DRUMBOY_PLUGINS}>:DRUMBOY_PLUGINS>
    $<$<BOOL:${DRUMBOY_BRANDING}>:DRUMBOY_BRANDING>
    DRUMBOY_BUILD_NUMBER=${FW_BUILD_NUMBER}
)

set(DRUMBOY_SIM_OPT "-O2" CACHE STRING "Optimisation level of the simulator build (-O0 for debugging)")
target_compile_options(DrumboySim PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/hal/sim_prelude.h
    ${DRUMBOY_SIM_OPT} -g -ffast-math
    -fno-strict-aliasing                        # register/buffer type punning as on the MCU
    -fshort-enums                               # as arm-none-eabi: enums are 1 byte (KEYSTATE arrays, file formats)
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable
)
# Firmware sources: no warnings, and no stack protector by default, like the
# device. DRUMBOY_SIM_STACK_CHECK=ON turns it on to catch stack buffer
# overflows (the simulator then stops with a backtrace).
option(DRUMBOY_SIM_STACK_CHECK "Build firmware sources with -fstack-protector-all" OFF)
if(DRUMBOY_SIM_STACK_CHECK)
    set(SIM_FIRMWARE_FLAGS "-w;-fstack-protector-all")
else()
    set(SIM_FIRMWARE_FLAGS "-w;-fno-stack-protector")
endif()
set_source_files_properties(${SIM_FIRMWARE_SOURCES} PROPERTIES COMPILE_OPTIONS "${SIM_FIRMWARE_FLAGS}")
set_source_files_properties(${FW_DIR}/Core/Src/main.cpp PROPERTIES COMPILE_DEFINITIONS "main=firmware_main")

if(APPLE)
    target_link_options(DrumboySim PRIVATE -Wl,-pagezero_size,0x1000)
    target_link_libraries(DrumboySim PRIVATE z ${SIM_SDL_TARGET} "-framework CoreGraphics" "-framework CoreFoundation")
else()
    target_link_options(DrumboySim PRIVATE -no-pie)
    target_link_libraries(DrumboySim PRIVATE z ${SIM_SDL_TARGET} PkgConfig::SIM_PDF)
endif()
# sim_window.cpp only uses host libraries: no firmware prelude
set_source_files_properties(src/sim_window.cpp PROPERTIES
    COMPILE_DEFINITIONS "SIM_NO_PRELUDE;DRUMBOY_PANEL_ART=\"${PANEL_ART}\""
    COMPILE_OPTIONS "-fno-short-enums"          # host ABI for SDL; shares no enum types with the firmware
)

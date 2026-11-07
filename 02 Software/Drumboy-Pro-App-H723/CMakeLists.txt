# Command-line build of the Drumboy Pro application. Mirrors the STM32CubeIDE
# "Debug" configuration in .cproject: -Ofast -g3 -ffast-math, DEBUG defined.
#
#   cmake -G Ninja -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
#   ninja -C build
#
# build/ is gitignored. Outputs: the raw application image for 0x08020000 and,
# through ../Firmware/make_sd_package.py, the SD-card package with the 36-byte
# header the bootloader checks (title, version, size, CRC).
cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake")
endif()
project(Drumboy-Pro-App-H723 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_FLAGS -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    Core/*.c Core/*.cpp Core/*.s
    FATFS/*.c Middlewares/*.c Drivers/*.c)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc FATFS/Target FATFS/App
    Drivers/STM32H7xx_HAL_Driver/Inc Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    Middlewares/Third_Party/FatFs/src
    Drivers/CMSIS/Device/ST/STM32H7xx/Include Drivers/CMSIS/Include
    Core/Library/Global Core/Library/Lcd Core/Library/Dac Core/Library/Controller)

target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG USE_PWR_LDO_SUPPLY USE_HAL_DRIVER STM32H723xx)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -Ofast -g3 -ffast-math
    -ffunction-sections -fdata-sections -Wall -fstack-usage --specs=nano.specs
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-use-cxa-atexit>
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>)

target_link_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H723ZGTX_FLASH.ld
    --specs=nosys.specs --specs=nano.specs
    -Wl,-Map=${PROJECT_NAME}.map -Wl,--gc-sections -Wl,--print-memory-usage -static)
target_link_libraries(${PROJECT_NAME} PRIVATE -Wl,--start-group c m stdc++ supc++ -Wl,--end-group)

# Raw image, size report, and the SD-card package (Drumboy_Pro_Fw_MM_mm.bin,
# version from kVersionMajor/kVersionMinor in Global.h) for the factory bootloader.
find_package(Python3 COMPONENTS Interpreter)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>)
if(Python3_Interpreter_FOUND)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/make_sd_package.py"
                pack ${PROJECT_NAME}.bin -o .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

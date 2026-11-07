# Arm GNU Toolchain (arm-none-eabi). Search order: $ARM_GNU_HOME/bin, a tools/
# directory up to three levels above this project (the workspace that holds this
# checkout keeps it in tools/, gitignored), /tmp/arm-gnu-14.3, then PATH.
# Public Arm GNU 14.3 builds this tree but does not reproduce the factory bytes.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
file(GLOB _ARM_GCC_DIRS
    "$ENV{ARM_GNU_HOME}/bin"
    "${CMAKE_CURRENT_LIST_DIR}/../tools/arm-gnu-toolchain-*-arm-none-eabi/bin"
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/arm-gnu-toolchain-*-arm-none-eabi/bin"
    "${CMAKE_CURRENT_LIST_DIR}/../../../tools/arm-gnu-toolchain-*-arm-none-eabi/bin"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../tools/arm-gnu-toolchain-*-arm-none-eabi/bin"
    "/tmp/arm-gnu-14.3/arm-gnu-toolchain-*-arm-none-eabi/bin")
find_program(CMAKE_C_COMPILER arm-none-eabi-gcc HINTS ${_ARM_GCC_DIRS} REQUIRED)
get_filename_component(_ARM_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
set(CMAKE_CXX_COMPILER "${_ARM_BIN}/arm-none-eabi-g++")
set(CMAKE_ASM_COMPILER "${_ARM_BIN}/arm-none-eabi-gcc")
set(CMAKE_OBJCOPY      "${_ARM_BIN}/arm-none-eabi-objcopy")
set(CMAKE_SIZE         "${_ARM_BIN}/arm-none-eabi-size")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Writes the simulator's copy of Lcd.cpp (run at build time, see ../CMakeLists.txt).
#
# Lcd.cpp caches GPIO register addresses in raw pointers for its pixel loops:
#     volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
# Writes through a raw pointer bypass SimReg32, so the LCD bus decoder would
# miss them. The copy declares these as `auto`, which on the device deduces
# the same volatile uint32_t* and in the simulator keeps SimReg32*. The #line
# directive keeps diagnostics and the debugger on the original file.

file(READ "${IN}" SOURCE)
string(REGEX MATCHALL "volatile uint32_t\\* *[A-Za-z_]+ *= *&\\(" FOUND "${SOURCE}")
list(LENGTH FOUND COUNT)
if(COUNT EQUAL 0)
    message(FATAL_ERROR "No cached register pointers found in ${IN}; update Simulator/cmake/lcd_source.cmake")
endif()
string(REGEX REPLACE "volatile uint32_t\\* *([A-Za-z_]+) *= *&\\(" "auto \\1 = &(" SOURCE "${SOURCE}")
file(WRITE "${OUT}" "#line 1 \"${IN}\"\n${SOURCE}")

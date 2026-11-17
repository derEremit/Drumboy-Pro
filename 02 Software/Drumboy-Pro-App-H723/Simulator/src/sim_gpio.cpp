// GPIO output writes from C++ firmware code (SimReg32 in Simulator/hal/stm32h7xx.h).
//
// ODR and BSRR keep their hardware meaning on the mapped registers. Output
// pins with a simulated device behind them are decoded here: the LCD's
// 16-bit 8080 bus (data on GPIOB, CS/RD/WR/RS on GPIOD) latches on each WR
// rising edge, the LCD reset and backlight sit on GPIOG.

#include "Global.h"
#include "sim.h"

namespace sim {

static const uint32_t kOdrOffset = 0x14;
static const uint32_t kBsrrOffset = 0x18;

static void outputChanged(uintptr_t port, uint32_t before, uint32_t after) {
    if (port == reinterpret_cast<uintptr_t>(LCD_WR_GPIO_Port)) {
        bool wrRose = !(before & LCD_WR_Pin) && (after & LCD_WR_Pin);
        if (wrRose && !(after & LCD_CS_Pin)) {
            bool command = !(after & LCD_RS_Pin);
            lcdBusWrite(command, static_cast<uint16_t>(LCD_DATA_GPIO_Port->ODR));
        }
    } else if (port == reinterpret_cast<uintptr_t>(LCD_BL_GPIO_Port)) {
        if ((before ^ after) & LCD_BL_Pin) lcdSetBacklight(after & LCD_BL_Pin);
    }
}

}  // namespace sim

extern "C" void sim_gpio_written(volatile uint32_t* reg, uint32_t value) {
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t port = address & ~static_cast<uintptr_t>(0x3FF);
    volatile uint32_t* odr = reinterpret_cast<volatile uint32_t*>(port + sim::kOdrOffset);
    uint32_t before = *odr;
    uint32_t after;
    if ((address - port) == sim::kBsrrOffset) {
        after = (before & ~(value >> 16)) | (value & 0xFFFF);      // Set wins over reset
    } else {
        after = value;
    }
    *odr = after;
    if (before != after) sim::outputChanged(port, before, after);
}

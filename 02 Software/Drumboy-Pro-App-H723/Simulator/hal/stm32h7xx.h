// Simulator wrapper around the ST device header (found first on the include path).
//
// In C++ sources GPIO_TypeDef becomes SimGpioPort: same layout as the
// hardware struct, but writes to ODR and BSRR go through SimReg32, which
// reports them to the simulator (LCD bus decoder, LEDs). Everything else stays
// plain memory at the real register address. C sources (CubeMX init, FatFs)
// keep the hardware struct; both views point at the same registers.

#ifndef __SIM_STM32H7XX_H
#define __SIM_STM32H7XX_H

#define GPIO_TypeDef GPIO_TypeDef_Hw
#include_next "stm32h7xx.h"
#undef GPIO_TypeDef

#ifdef __cplusplus

#include <cstdint>

extern "C" void sim_gpio_written(volatile uint32_t* reg, uint32_t value);

// A write-observed 32-bit register
struct SimReg32 {
    volatile uint32_t value;

    operator uint32_t() const { return value; }
    SimReg32& operator=(uint32_t v) {
        sim_gpio_written(&value, v);
        return *this;
    }
    SimReg32& operator|=(uint32_t v) { return *this = value | v; }
    SimReg32& operator&=(uint32_t v) { return *this = value & v; }
    SimReg32& operator^=(uint32_t v) { return *this = value ^ v; }
};

struct SimGpioPort {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    SimReg32 ODR;
    SimReg32 BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
};

static_assert(sizeof(SimGpioPort) == sizeof(GPIO_TypeDef_Hw), "SimGpioPort must match the GPIO register layout");

typedef SimGpioPort GPIO_TypeDef;

#else

typedef GPIO_TypeDef_Hw GPIO_TypeDef;

#endif

#endif

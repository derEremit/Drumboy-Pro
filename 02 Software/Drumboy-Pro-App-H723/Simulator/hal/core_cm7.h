// Simulator replacement for the CMSIS Cortex-M7 core header.
//
// stm32h723xx.h includes "core_cm7.h"; Simulator/hal comes first on the include
// path, so this file is used instead of Drivers/CMSIS/Include/core_cm7.h (whose
// cmsis_gcc.h is ARM inline assembly). It provides the register qualifiers, the
// core peripherals the firmware touches (SCB, at their real addresses, which the
// simulator maps as ordinary memory) and the intrinsics, routed to the sim
// runtime (Simulator/src/sim_cpu.cpp).

#ifndef __CORE_CM7_H_GENERIC
#define __CORE_CM7_H_GENERIC
#define __CORE_CM7_H_DEPENDANT

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compiler and register qualifiers ------------------------------ */

#define __CM7_CMSIS_VERSION_MAIN    (5U)
#define __CM7_CMSIS_VERSION_SUB     (17U)
#define __CM7_CMSIS_VERSION         ((__CM7_CMSIS_VERSION_MAIN << 16U) | __CM7_CMSIS_VERSION_SUB)
#define __CORTEX_M                  (7U)

#ifdef __cplusplus
#define __I                         volatile
#else
#define __I                         volatile const
#endif
#define __O                         volatile
#define __IO                        volatile
#define __IM                        volatile const
#define __OM                        volatile
#define __IOM                       volatile

#define __ASM                       __asm
#define __INLINE                    inline
#define __STATIC_INLINE             static inline
#define __STATIC_FORCEINLINE        __attribute__((always_inline)) static inline
#define __NO_RETURN                 __attribute__((__noreturn__))
#define __USED                      __attribute__((used))
#define __WEAK                      __attribute__((weak))
#define __PACKED                    __attribute__((packed, aligned(1)))
#define __PACKED_STRUCT             struct __attribute__((packed, aligned(1)))
#define __PACKED_UNION              union __attribute__((packed, aligned(1)))
#define __ALIGNED(x)                __attribute__((aligned(x)))
#define __RESTRICT                  __restrict
#define __COMPILER_BARRIER()        __asm volatile("" ::: "memory")

/* Core peripherals ---------------------------------------------- */

typedef struct {
    __IM uint32_t CPUID;
    __IOM uint32_t ICSR;
    __IOM uint32_t VTOR;
    __IOM uint32_t AIRCR;
    __IOM uint32_t SCR;
    __IOM uint32_t CCR;
    __IOM uint8_t SHPR[12U];
    __IOM uint32_t SHCSR;
    __IOM uint32_t CFSR;
    __IOM uint32_t HFSR;
    __IOM uint32_t DFSR;
    __IOM uint32_t MMFAR;
    __IOM uint32_t BFAR;
    __IOM uint32_t AFSR;
    __IM uint32_t ID_PFR[2U];
    __IM uint32_t ID_DFR;
    __IM uint32_t ID_AFR;
    __IM uint32_t ID_MFR[4U];
    __IM uint32_t ID_ISAR[5U];
    uint32_t RESERVED0[1U];
    __IM uint32_t CLIDR;
    __IM uint32_t CTR;
    __IM uint32_t CCSIDR;
    __IOM uint32_t CSSELR;
    __IOM uint32_t CPACR;
} SCB_Type;

#define SCS_BASE                    (0xE000E000UL)
#define SCB_BASE                    (SCS_BASE + 0x0D00UL)
#define SCB                         ((SCB_Type*)SCB_BASE)

/* Interrupt masking (sim runtime) -------------------------------- */

void __enable_irq(void);
void __disable_irq(void);
uint32_t __get_PRIMASK(void);
void __set_PRIMASK(uint32_t priMask);
void __WFI(void);
void __WFE(void);
__NO_RETURN void NVIC_SystemReset(void);                       // Restarts the simulator process

/* Other intrinsics ---------------------------------------------- */

extern uint32_t sim_fpscr;

__STATIC_INLINE uint32_t __get_FPSCR(void) { return sim_fpscr; }
__STATIC_INLINE void __set_FPSCR(uint32_t fpscr) { sim_fpscr = fpscr; }
__STATIC_INLINE void __NOP(void) {}
__STATIC_INLINE void __SEV(void) {}
__STATIC_INLINE void __ISB(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
__STATIC_INLINE void __DSB(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
__STATIC_INLINE void __DMB(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
__STATIC_INLINE uint32_t __REV(uint32_t value) { return __builtin_bswap32(value); }
__STATIC_INLINE uint32_t __REV16(uint32_t value) { return ((value & 0xFF00FF00UL) >> 8) | ((value & 0x00FF00FFUL) << 8); }
__STATIC_INLINE uint8_t __CLZ(uint32_t value) { return value ? (uint8_t)__builtin_clz(value) : 32U; }
__STATIC_INLINE uint32_t __RBIT(uint32_t value) {
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (value & 1U);
        value >>= 1;
    }
    return result;
}
__STATIC_INLINE uint32_t __LDREXW(volatile uint32_t* addr) { return *addr; }
__STATIC_INLINE uint32_t __STREXW(uint32_t value, volatile uint32_t* addr) {
    *addr = value;
    return 0U;
}
__STATIC_INLINE uint16_t __LDREXH(volatile uint16_t* addr) { return *addr; }
__STATIC_INLINE uint32_t __STREXH(uint16_t value, volatile uint16_t* addr) {
    *addr = value;
    return 0U;
}
__STATIC_INLINE void __CLREX(void) {}
__STATIC_INLINE int32_t __SSAT(int32_t value, uint32_t bits) {
    const int32_t max = (int32_t)((1U << (bits - 1U)) - 1U);
    const int32_t min = -1 - max;
    return value > max ? max : (value < min ? min : value);
}
__STATIC_INLINE uint32_t __USAT(int32_t value, uint32_t bits) {
    const int32_t max = (int32_t)((1U << bits) - 1U);
    return value > max ? (uint32_t)max : (value < 0 ? 0U : (uint32_t)value);
}

/* Caches and NVIC are no-ops on the host ------------------------- */

__STATIC_INLINE void SCB_EnableICache(void) {}
__STATIC_INLINE void SCB_DisableICache(void) {}
__STATIC_INLINE void SCB_InvalidateICache(void) {}
__STATIC_INLINE void SCB_EnableDCache(void) {}
__STATIC_INLINE void SCB_DisableDCache(void) {}
__STATIC_INLINE void SCB_InvalidateDCache(void) {}
__STATIC_INLINE void SCB_CleanDCache(void) {}
__STATIC_INLINE void SCB_CleanInvalidateDCache(void) {}
__STATIC_INLINE void SCB_InvalidateDCache_by_Addr(volatile void* addr, int32_t size) { (void)addr; (void)size; }
__STATIC_INLINE void SCB_CleanDCache_by_Addr(volatile void* addr, int32_t size) { (void)addr; (void)size; }
__STATIC_INLINE void SCB_CleanInvalidateDCache_by_Addr(volatile void* addr, int32_t size) { (void)addr; (void)size; }

#ifdef __cplusplus
}
#endif

#endif /* __CORE_CM7_H_GENERIC */

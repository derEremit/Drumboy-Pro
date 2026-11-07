// Fixed-address memory for the simulator.
//
// The firmware uses absolute MCU addresses: SDRAM buffers and images at
// 0xC0000000, icons in RAM_D3 at 0x38000000, the DWT cycle counter at
// 0xE0001004, and every peripheral register through the ST headers. Many of
// these addresses are kept in uint32_t variables, so they must stay below 4 GB.
// The simulator maps anonymous memory at the same addresses (an x86_64 build
// with a small __PAGEZERO on macOS, a non-PIE executable on Linux, see
// Simulator/CMakeLists.txt) and the firmware accesses it unchanged; registers
// become plain memory that the sim reads.

#include <sys/mman.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "main.h"
#include "sim.h"

namespace sim {

struct Region {
    const char* name;
    uintptr_t base;
    size_t size;
};

static const Region kRegions[] = {
    {"RAM_D2", 0x30000000, 0x00010000},                         // SDMMC DMA buffer
    {"RAM_D3", 0x38000000, 0x00010000},                         // I2C buffers, icons
    {"peripherals", 0x40000000, 0x20000000},                    // APB/AHB/D3 peripheral registers
    {"SDRAM", 0xC0000000, 0x01000000},                          // 16 MB external SDRAM
    {"core", 0xE0000000, 0x00100000},                           // DWT, SCB and other core registers
};

// AddressSanitizer builds (-fsanitize=address, for finding firmware buffer
// overflows): on x86_64 ASan keeps a protected gap from 0x8fff7000 up, where
// the SDRAM and core regions live. They are mapped over the gap, and their
// shadow bytes (addr / 8 + 0x7fff8000) are mapped as well, so instrumented
// accesses see them as addressable.
#if defined(__SANITIZE_ADDRESS__)
#define SIM_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SIM_ASAN 1
#endif
#endif

void mapMemory() {
    static bool mapped = false;
    if (mapped) return;
    mapped = true;
    for (const Region& region : kRegions) {
        void* want = reinterpret_cast<void*>(region.base);
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(SIM_ASAN) && defined(__linux__)
        const uintptr_t kAsanGapStart = 0x8fff7000ULL;         // Below it ASan already has shadow for the region
        if (region.base >= kAsanGapStart) {
            flags |= MAP_FIXED;                                 // Replace the protected gap mapping
            void* shadow = reinterpret_cast<void*>((region.base >> 3) + 0x7fff8000ULL);
            if (mmap(shadow, region.size >> 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != shadow) {
                perror("sim: cannot map ASan shadow");
                abort();
            }
        } else {
            flags |= MAP_FIXED_NOREPLACE;
        }
#elif defined(MAP_FIXED_NOREPLACE)
        flags |= MAP_FIXED_NOREPLACE;                           // Linux: fail instead of landing elsewhere
#endif
        void* got = mmap(want, region.size, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (got != want) {
            fprintf(stderr, "sim: cannot map %s at 0x%08lx (got %p). Is this the x86_64 build with -pagezero_size (macOS) / -no-pie (Linux)?\n",
                    region.name, static_cast<unsigned long>(region.base), got);
            abort();
        }
    }
    resetPeripherals();
}

// Runs before the firmware's global constructors (Controller etc.).
__attribute__((constructor(101))) static void mapMemoryEarly() { mapMemory(); }

void resetPeripherals() {
    // Inputs idle high (pull-ups): keypad interrupts, function button (low = test mode), SD detect
    GPIO_TypeDef* ports[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG};
    for (GPIO_TypeDef* port : ports) port->IDR = 0xFFFF;

    // SystemClock_Config waits for the voltage scaling to be ready
    PWR->D3CR |= PWR_D3CR_VOSRDY;
}

/* Time ---------------------------------------------------------- */

static const auto kStart = std::chrono::steady_clock::now();

uint64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - kStart).count();
}

// DWT_CYCCNT runs at the core clock. A thread adds the elapsed cycles every few
// microseconds, so firmware writes (reset to 0) are kept and delay_us() works.
void startCycleCounter() {
    static std::atomic<bool> started(false);
    if (started.exchange(true)) return;
    std::thread([] {
        volatile uint32_t* cyccnt = reinterpret_cast<volatile uint32_t*>(0xE0001004);
        const uint64_t cyclesPerUs = SystemCoreClock / 1000000;
        uint64_t last = nowUs();
        while (true) {
            std::this_thread::sleep_for(std::chrono::microseconds(20));
            uint64_t now = nowUs();
            *cyccnt += static_cast<uint32_t>((now - last) * cyclesPerUs);
            last = now;
        }
    }).detach();
}

}  // namespace sim

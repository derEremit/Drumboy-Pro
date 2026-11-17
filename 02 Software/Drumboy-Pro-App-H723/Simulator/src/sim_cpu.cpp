// Interrupt model and CPU intrinsics (declared in Simulator/hal/core_cm7.h).
//
// The firmware's main loop runs on its own thread; interrupt sources (timers,
// audio, EXTI from the panel) run their HAL callbacks on other threads through
// sim::runIsr(). One recursive "interrupt" mutex makes this behave like a
// single core: an ISR holds it while it runs, and __disable_irq() /
// __set_PRIMASK(1) on the main thread take it, so masked sections still keep
// interrupts out. Inside an ISR the mask calls only track the flag.

#include <mutex>
#include <thread>

#include "core_cm7.h"
#include "sim.h"

namespace sim {

static std::recursive_mutex irqMutex;
static thread_local int isrDepth = 0;                           // > 0 while this thread runs an ISR
static thread_local bool isrMasked = false;                     // PRIMASK as seen inside the ISR
static thread_local bool threadMasked = false;                  // This (non-ISR) thread holds the lock via PRIMASK

void runIsr(const std::function<void()>& handler) {
    std::lock_guard<std::recursive_mutex> lock(irqMutex);
    isrDepth++;
    bool savedMask = isrMasked;
    handler();
    isrMasked = savedMask;
    isrDepth--;
}

void lockInterrupts() { irqMutex.lock(); }
void unlockInterrupts() { irqMutex.unlock(); }

}  // namespace sim

using namespace sim;

extern "C" {

uint32_t sim_fpscr = 0;

void __disable_irq(void) {
    if (isrDepth) {
        isrMasked = true;
    } else if (!threadMasked) {
        irqMutex.lock();
        threadMasked = true;
    }
}

void __enable_irq(void) {
    if (isrDepth) {
        isrMasked = false;
    } else if (threadMasked) {
        threadMasked = false;
        irqMutex.unlock();
    }
}

uint32_t __get_PRIMASK(void) { return (isrDepth ? isrMasked : threadMasked) ? 1U : 0U; }

void __set_PRIMASK(uint32_t priMask) {
    if (priMask & 1U) {
        __disable_irq();
    } else {
        __enable_irq();
    }
}

void __WFI(void) { std::this_thread::yield(); }
void __WFE(void) { std::this_thread::yield(); }

void NVIC_SystemReset(void) { sim::restart(); }

}  // extern "C"

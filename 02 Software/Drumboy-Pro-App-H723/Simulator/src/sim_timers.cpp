// Timer interrupts.
//
// One thread runs all basic timers from their (memory-mapped) registers, so
// HAL_TIM_Base_Start/Stop_IT, __HAL_TIM_SET_AUTORELOAD and
// __HAL_TIM_SET_COUNTER work as on the device: while CEN and UIE are set, CNT
// advances at 275 MHz / (PSC + 1) and each wrap past ARR calls
// HAL_TIM_PeriodElapsedCallback as an interrupt.

#include <chrono>
#include <thread>

#include "sim.h"
#include "tim.h"

namespace sim {

static const double kTimerClockMhz = 275.0;                     // APB timer clock (2 x 137.5 MHz)
static const int kMaxCatchUp = 64;                              // Updates fired per timer per pass after a stall

static TIM_HandleTypeDef* const kTimers[] = {&htim4,  &htim5,  &htim6,  &htim7,  &htim8,  &htim12, &htim13,
                                             &htim14, &htim15, &htim16, &htim17, &htim23, &htim24};

void startTimers() {
    std::thread([] {
        const int count = sizeof(kTimers) / sizeof(kTimers[0]);
        double pending[count] = {};                             // Fractional timer ticks
        uint64_t last = nowUs();
        while (true) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            uint64_t now = nowUs();
            double elapsedUs = static_cast<double>(now - last);
            last = now;

            for (int i = 0; i < count; i++) {
                TIM_HandleTypeDef* htim = kTimers[i];
                TIM_TypeDef* tim = htim->Instance;
                if (!tim) continue;
                if (tim->EGR & TIM_EGR_UG) {
                    tim->EGR = 0;
                    tim->CNT = 0;
                }
                if (!(tim->CR1 & TIM_CR1_CEN) || !(tim->DIER & TIM_DIER_UIE)) {
                    pending[i] = 0;
                    continue;
                }

                pending[i] += elapsedUs * kTimerClockMhz / (tim->PSC + 1.0);
                uint64_t ticks = static_cast<uint64_t>(pending[i]);
                pending[i] -= ticks;

                uint64_t cnt = tim->CNT + ticks;
                int fired = 0;
                while (cnt > tim->ARR && (tim->CR1 & TIM_CR1_CEN)) {
                    cnt -= tim->ARR + 1ULL;
                    tim->CNT = static_cast<uint32_t>(cnt);
                    if (fired++ < kMaxCatchUp) runIsr([htim] { HAL_TIM_PeriodElapsedCallback(htim); });
                    cnt = tim->CNT;                             // The callback may have reset the counter
                }
                if (cnt > tim->ARR) cnt = 0;
                tim->CNT = static_cast<uint32_t>(cnt);
            }
        }
    }).detach();
}

}  // namespace sim

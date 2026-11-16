#include "main.h"

#include "Controller.h"
#include "bdma.h"
#include "dma.h"
#include "fatfs.h"
#include "fmc.h"
#include "gpio.h"
#include "i2c.h"
#include "i2s.h"
#include "sdmmc.h"
#include "tim.h"
#include "usart.h"

/* Extern Function Declarations ---------------------------------- */

extern "C" void SystemClock_Config();
extern "C" void PeriphCommonClock_Config();
extern "C" void Sdram_Initialize();
extern "C" void System_Initialize();

/* Memory Allocation In D1 Ram ----------------------------------- */

// 32-byte aligned, a multiple of 32 bytes: the data cache build maintains them around the DMA.
uint32_t i2s_rxBuffer[I2S_TOTAL_BUFFER_SIZE] __attribute__((aligned(32))) = {0x00};
uint32_t i2s_txBuffer[I2S_TOTAL_BUFFER_SIZE] __attribute__((aligned(32))) = {0x00};

/* Memory Allocation In D3 Ram ----------------------------------- */

__attribute__((section(".RAM_D3"))) uint8_t i2c3_rxData[2] = {0x00};
__attribute__((section(".RAM_D3"))) uint8_t i2c3_txData[2] = {0x00};
__attribute__((section(".RAM_D3"))) uint8_t i2c4_rxData[2] = {0x00};
__attribute__((section(".RAM_D3"))) uint8_t i2c4_txData[2] = {0x00};

/* Sync And Midi Buffers ----------------------------------------- */

MidiRxBox midiRxBox;

/* Global Controller Instance ------------------------------------ */

Controller controller;

#ifdef ENABLE_DCACHE
// Memory attributes for the data cache build (make DCACHE=1, backlog 53 headroom work).
//   SDRAM (0xC0000000, 16 MB): normal, write-back and write-allocate. Only the CPU uses it (the
//   screen is bit-banged, samples reach it through the CPU), so it needs no maintenance.
//   RAM_D3 (0x38000000, 16 KB): not cacheable: the I2C DMA buffers live there.
// The DMA buffers in the AXI RAM (I2S, MIDI, SD) are cleaned and invalidated by address instead.
static void mpu_configureForDataCache() {
    ARM_MPU_Disable();
    ARM_MPU_SetRegion(ARM_MPU_RBAR(0, 0xC0000000u), ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 1, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_16MB));
    ARM_MPU_SetRegion(ARM_MPU_RBAR(1, 0x38000000u), ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 1, 1, 0, 0, 0, ARM_MPU_REGION_SIZE_16KB));
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
    __DSB();
    __ISB();
}
#endif

int main() {
    // System Startup
    __disable_irq();
    SCB->VTOR = APPLICATION_ADDRESS;
    SCB_InvalidateICache();
    SCB_InvalidateDCache();
    SCB_EnableICache();
#ifdef ENABLE_DCACHE
    mpu_configureForDataCache();
    SCB_EnableDCache();
#endif
    HAL_Init();

    // The stereo work memory in the DTCM RAM is not initialised by the start-up code.
    {
        extern uint32_t __dtcm_start, __dtcm_end;
        for (uint32_t* w = &__dtcm_start; w < &__dtcm_end; w++) *w = 0;
    }

    // Enable FPU (Floating Point Unit)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));

    // Enable Hardware FTZ (Flush-to-Zero)
    __set_FPSCR(__get_FPSCR() | (1 << 24) | (1 << 25));
    __enable_irq();

    // Configure Clocks
    SystemClock_Config();
    PeriphCommonClock_Config();

    // Initialize Hardware
    System_Initialize();
    Sdram_Initialize();

    // Initialize Application
    controller.initialize();

    // Main Loop
    while (true) {
        controller.update();
    }
}

extern "C" void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Supply configuration update enable
     */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 68;
    RCC_OscInitStruct.PLL.PLLP = 1;
    RCC_OscInitStruct.PLL.PLLQ = 3;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 6144;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
        Error_Handler();
    }

    /** Enables the Clock Security System
     */
    HAL_RCC_EnableCSS();
}

extern "C" void PeriphCommonClock_Config(void) {
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Initializes the peripherals clock
     */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC | RCC_PERIPHCLK_SDMMC;
    PeriphClkInitStruct.PLL2.PLL2M = 1;
    PeriphClkInitStruct.PLL2.PLL2N = 30;
    PeriphClkInitStruct.PLL2.PLL2P = 1;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 1;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
    PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
    PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }
}

extern "C" void System_Initialize() {
    // Initialize HAL Modules
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_BDMA_Init();
    MX_FMC_Init();
    MX_FATFS_Init();
    MX_I2C3_Init();
    MX_I2C4_Init();
    MX_I2S3_Init();
    // MX_SDMMC2_SD_Init();
    MX_USART1_UART_Init();
    MX_USART6_UART_Init();
    MX_TIM4_Init();
    MX_TIM5_Init();
    MX_TIM6_Init();
    MX_TIM7_Init();
    MX_TIM8_Init();
    MX_TIM12_Init();
    MX_TIM13_Init();
    MX_TIM14_Init();
    MX_TIM15_Init();
    MX_TIM16_Init();
    MX_TIM17_Init();
    MX_TIM23_Init();
    MX_TIM24_Init();

    // MIDI IN (backlog 30/34): the UART's receive FIFO holds bytes while higher-priority interrupts
    // (audio, SD) run, and the receive interrupts sit above the sequencer timer so each clock
    // byte is timestamped and handled promptly.
    HAL_UARTEx_EnableFifoMode(&huart1);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 1, 0);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);

    // Sequencer (TIM15) and clock (TIM4) timers: a new period takes effect at the next update, so
    // shortening it never lets the counter run past the new end (a 65 ms wrap of the 16-bit timer).
    htim15.Instance->CR1 |= TIM_CR1_ARPE;
    htim4.Instance->CR1 |= TIM_CR1_ARPE;
}

extern "C" void Sdram_Initialize() {
    // Configure Sdram
    SDRAM_HandleTypeDef* hsdram = &hsdram1;
    FMC_SDRAM_CommandTypeDef Command;
    __IO uint32_t tmpmrd = 0;

    // 1. Clock Enable
    Command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT);
    HAL_Delay(1);

    // 2. Precharge All
    Command.CommandMode = FMC_SDRAM_CMD_PALL;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT);

    // 3. Auto Refresh
    Command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 4;
    Command.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT);

    // 4. Load Mode Register
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_2 | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | SDRAM_MODEREG_CAS_LATENCY_2 | SDRAM_MODEREG_OPERATING_MODE_STANDARD | SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
    Command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = tmpmrd;
    HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT);

    // 5. Set Refresh Rate
    HAL_SDRAM_ProgramRefreshRate(hsdram, 1854);
}

/* Gpio Interrupts ----------------------------------------------- */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (controller.getMenu() != INIT_MENU) {
        switch (GPIO_Pin) {
            case MX_X_INT_Pin:
                controller.interruptLayerKeypadRead();
                break;
            case MX_A_INT_Pin:
                controller.interruptLeftKeypadRead();
                break;
            case MX_B_INT_Pin:
                controller.interruptRightKeypadRead();
                break;
            case ENCO_0A_Pin:
                controller.interruptEncoderRead(0);
                break;
            case ENCO_1A_Pin:
                controller.interruptEncoderRead(1);
                break;
            case ENCO_2A_Pin:
                controller.interruptEncoderRead(2);
                break;
            case ENCO_3A_Pin:
                controller.interruptEncoderRead(3);
                break;
            case ENCO_4A_Pin:
                controller.interruptEncoderRead(4);
                break;
            case ENCO_5A_Pin:
                controller.interruptEncoderRead(5);
                break;
            case ENCO_6A_Pin:
                controller.interruptEncoderRead(6);
                break;
            case ENCO_7A_Pin:
                controller.interruptEncoderRead(7);
                break;
            case BUTTON_Pin:
                controller.interruptFuncButtonRead();
                break;
            case SYNC_IN_PULSE_Pin:
                controller.interruptSyncInPulse();
                break;
        }
    }
}

/* Timer Interrupts ---------------------------------------------- */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM4) controller.interruptMidiTxClock();
    if (htim->Instance == TIM5) controller.interruptTransition();
    if (htim->Instance == TIM6) controller.interruptSystemPreset();
    if (htim->Instance == TIM7) controller.interruptEncoderPreset();
    if (htim->Instance == TIM8) controller.interruptLedEq();
    if (htim->Instance == TIM12) controller.interruptUpKeyRead();
    if (htim->Instance == TIM13) controller.interruptDownKeyRead();
    if (htim->Instance == TIM14) controller.interruptLongKeyRead();
    if (htim->Instance == TIM15) controller.interruptPlay();
    if (htim->Instance == TIM16) controller.interruptText();
    if (htim->Instance == TIM17) controller.interruptSd();
    if (htim->Instance == TIM23) controller.interruptBeatSync();
    if (htim->Instance == TIM24) controller.interruptLimitAlert();
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef* htim) {}

/* I2s Interrupts ------------------------------------------------ */

// 1. Half Transfer (Process First 32 Samples)
void HAL_I2SEx_TxRxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s->Instance == SPI3) {
#ifdef ENABLE_DCACHE
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(&i2s_rxBuffer[0]), I2S_BLOCK_SIZE * 2 * sizeof(uint32_t));
#endif
        controller.processAudioBlock(&i2s_rxBuffer[0], &i2s_txBuffer[0]);
#ifdef ENABLE_DCACHE
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(&i2s_txBuffer[0]), I2S_BLOCK_SIZE * 2 * sizeof(uint32_t));
#endif
    }
}

// 2. Complete Transfer (Process Second 32 Samples)
void HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s->Instance == SPI3) {
#ifdef ENABLE_DCACHE
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(&i2s_rxBuffer[I2S_BLOCK_SIZE * 2]), I2S_BLOCK_SIZE * 2 * sizeof(uint32_t));
#endif
        controller.processAudioBlock(&i2s_rxBuffer[I2S_BLOCK_SIZE * 2], &i2s_txBuffer[I2S_BLOCK_SIZE * 2]);
#ifdef ENABLE_DCACHE
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(&i2s_txBuffer[I2S_BLOCK_SIZE * 2]), I2S_BLOCK_SIZE * 2 * sizeof(uint32_t));
#endif
    }
}

void HAL_I2SEx_TxRxErrorCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s->Instance == SPI3) {
    }
}

/* I2c Interrupts ------------------------------------------------ */

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef* hi2c) {
    if (hi2c->Instance == I2C3) {
    }
    if (hi2c->Instance == I2C4) {
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    if (hi2c->Instance == I2C3) {
    }
    if (hi2c->Instance == I2C4) {
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c) {}

/* Uart Interrupts ----------------------------------------------- */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart6) {
        controller.serviceMidiTx();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart1) {
#ifdef ENABLE_DCACHE
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(&midiRxBox), sizeof midiRxBox);
#endif
        controller.receiveMidiCommand();
        HAL_UART_Receive_DMA(&huart1, &midiRxData, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    if (huart == &huart1) {
        controller.midi_countRxError();  // overrun/framing: diagnostics
        HAL_UART_Receive_DMA(&huart1, &midiRxData, 1);
    }
}

void Error_Handler(void) {
    __disable_irq();
    // Record who called it and restart (CrashRecord.h); a repeat during start-up stays here.
    uint32_t frame[8] = {0};
    frame[6] = (uint32_t)reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    Crash_Capture(frame, CRASH_ERROR_HANDLER);
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {}
#endif

// Host implementation of the STM32 HAL functions the firmware calls.
//
// The CubeMX init files (Core/Src/gpio.c, tim.c, i2c.c, ...) and the
// application compile unchanged against the real ST headers; only the HAL
// driver sources are replaced by this file. Init functions copy the
// configuration into the (memory-mapped) registers like the real HAL, so the
// simulated peripherals read timer periods etc. from the same place.

#include <chrono>
#include <cstring>
#include <thread>

#include "main.h"
#include "sim.h"

extern "C" {

uint32_t SystemCoreClock = 550000000;
uint32_t SystemD2Clock = 275000000;
const uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

/* Core ---------------------------------------------------------- */

HAL_StatusTypeDef HAL_Init(void) {
    HAL_MspInit();
    return HAL_OK;
}

uint32_t HAL_GetTick(void) { return static_cast<uint32_t>(sim::nowUs() / 1000); }

void HAL_Delay(uint32_t Delay) {
    // Same minimum wait as the HAL (one extra tick), scaled by --delay-scale
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<uint64_t>((Delay + 1) * 1000.0 * sim::options.delayScale)));
}

void HAL_SYSCFG_AnalogSwitchConfig(uint32_t SYSCFG_AnalogSwitch, uint32_t SYSCFG_SwitchState) {}

void HAL_NVIC_SetPriorityGrouping(uint32_t PriorityGroup) {}
void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority) {}
void HAL_NVIC_EnableIRQ(IRQn_Type IRQn) {}
void HAL_NVIC_DisableIRQ(IRQn_Type IRQn) {}

/* Clocks and power ---------------------------------------------- */

HAL_StatusTypeDef HAL_PWREx_ConfigSupply(uint32_t SupplySource) { return HAL_OK; }
HAL_StatusTypeDef HAL_RCC_OscConfig(const RCC_OscInitTypeDef* RCC_OscInitStruct) { return HAL_OK; }
HAL_StatusTypeDef HAL_RCC_ClockConfig(const RCC_ClkInitTypeDef* RCC_ClkInitStruct, uint32_t FLatency) { return HAL_OK; }
void HAL_RCC_EnableCSS(void) {}
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef* PeriphClkInit) { return HAL_OK; }

/* GPIO ---------------------------------------------------------- */

void HAL_GPIO_Init(GPIO_TypeDef* GPIOx, const GPIO_InitTypeDef* GPIO_Init) {}
void HAL_GPIO_DeInit(GPIO_TypeDef* GPIOx, uint32_t GPIO_Pin) {}

GPIO_PinState HAL_GPIO_ReadPin(const GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    return (GPIOx->IDR & GPIO_Pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
    if (PinState != GPIO_PIN_RESET) {
        GPIOx->ODR |= GPIO_Pin;
    } else {
        GPIOx->ODR &= ~static_cast<uint32_t>(GPIO_Pin);
    }
}

void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) { GPIOx->ODR ^= GPIO_Pin; }

/* DMA ----------------------------------------------------------- */

HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef* hdma) { return HAL_OK; }
HAL_StatusTypeDef HAL_DMA_DeInit(DMA_HandleTypeDef* hdma) { return HAL_OK; }

/* Timers -------------------------------------------------------- */

HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef* htim) {
    HAL_TIM_Base_MspInit(htim);
    htim->Instance->PSC = htim->Init.Prescaler;
    htim->Instance->ARR = htim->Init.Period;
    htim->Instance->CNT = 0;
    htim->State = HAL_TIM_STATE_READY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_ConfigClockSource(TIM_HandleTypeDef* htim, const TIM_ClockConfigTypeDef* sClockSourceConfig) { return HAL_OK; }
HAL_StatusTypeDef HAL_TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef* htim, const TIM_MasterConfigTypeDef* sMasterConfig) { return HAL_OK; }

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef* htim) {
    htim->Instance->DIER |= TIM_DIER_UIE;
    htim->Instance->CR1 |= TIM_CR1_CEN;
    htim->State = HAL_TIM_STATE_BUSY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef* htim) {
    htim->Instance->DIER &= ~TIM_DIER_UIE;
    htim->Instance->CR1 &= ~TIM_CR1_CEN;
    htim->State = HAL_TIM_STATE_READY;
    return HAL_OK;
}

/* I2C ----------------------------------------------------------- */

// Devices on both buses live in the front panel model (sim_panel.cpp)
static bool isBus4(const I2C_HandleTypeDef* hi2c) { return hi2c->Instance == I2C4; }

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef* hi2c) {
    HAL_I2C_MspInit(hi2c);
    hi2c->State = HAL_I2C_STATE_READY;
    hi2c->Mode = HAL_I2C_MODE_NONE;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef* hi2c, uint32_t AnalogFilter) { return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef* hi2c, uint32_t DigitalFilter) { return HAL_OK; }
HAL_I2C_StateTypeDef HAL_I2C_GetState(const I2C_HandleTypeDef* hi2c) { return HAL_I2C_STATE_READY; }
// Used by the I2C3 recovery and keypad scanner timeouts (daupuck v2.09 port); the bus never fails here
uint32_t HAL_I2C_GetError(const I2C_HandleTypeDef* hi2c) { return HAL_I2C_ERROR_NONE; }
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef* hi2c) {
    hi2c->State = HAL_I2C_STATE_RESET;
    return HAL_OK;
}
HAL_I2C_ModeTypeDef HAL_I2C_GetMode(const I2C_HandleTypeDef* hi2c) { return HAL_I2C_MODE_NONE; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout) { return HAL_OK; }

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData,
                                    uint16_t Size, uint32_t Timeout) {
    sim::panelI2cWrite(isBus4(hi2c), static_cast<uint8_t>(DevAddress), static_cast<uint8_t>(MemAddress), pData, Size);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData,
                                   uint16_t Size, uint32_t Timeout) {
    sim::panelI2cRead(isBus4(hi2c), static_cast<uint8_t>(DevAddress), static_cast<uint8_t>(MemAddress), pData, Size);
    return HAL_OK;
}

// DMA transfers complete immediately
HAL_StatusTypeDef HAL_I2C_Mem_Write_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData,
                                        uint16_t Size) {
    HAL_I2C_Mem_Write(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, 0);
    HAL_I2C_MemTxCpltCallback(hi2c);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData,
                                       uint16_t Size) {
    HAL_I2C_Mem_Read(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, 0);
    HAL_I2C_MemRxCpltCallback(hi2c);
    return HAL_OK;
}

// Codec (SGTL5000) register access: accepted, reads return 0
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout) {
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout) {
    memset(pData, 0, Size);
    return HAL_OK;
}

/* I2S ----------------------------------------------------------- */

HAL_StatusTypeDef HAL_I2S_Init(I2S_HandleTypeDef* hi2s) {
    HAL_I2S_MspInit(hi2s);
    hi2s->State = HAL_I2S_STATE_READY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2SEx_TransmitReceive_DMA(I2S_HandleTypeDef* hi2s, const uint16_t* pTxData, uint16_t* pRxData, uint16_t Size) {
    hi2s->pTxBuffPtr = pTxData;
    hi2s->pRxBuffPtr = pRxData;
    hi2s->TxXferSize = Size;
    hi2s->RxXferSize = Size;
    hi2s->State = HAL_I2S_STATE_BUSY_TX_RX;
    sim::audioStart();
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2S_DMAStop(I2S_HandleTypeDef* hi2s) {
    hi2s->State = HAL_I2S_STATE_READY;
    return HAL_OK;
}

/* UART (MIDI) --------------------------------------------------- */

HAL_StatusTypeDef HAL_HalfDuplex_Init(UART_HandleTypeDef* huart) {
    HAL_UART_MspInit(huart);
    huart->gState = HAL_UART_STATE_READY;
    huart->RxState = HAL_UART_STATE_READY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_HalfDuplex_EnableTransmitter(UART_HandleTypeDef* huart) { return HAL_OK; }
HAL_StatusTypeDef HAL_HalfDuplex_EnableReceiver(UART_HandleTypeDef* huart) { return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef* huart) { return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_EnableFifoMode(UART_HandleTypeDef* huart) { return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_SetTxFifoThreshold(UART_HandleTypeDef* huart, uint32_t Threshold) { return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_SetRxFifoThreshold(UART_HandleTypeDef* huart, uint32_t Threshold) { return HAL_OK; }

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef* huart, uint8_t* pData, uint16_t Size) {
    huart->pRxBuffPtr = pData;
    huart->RxXferSize = Size;
    huart->RxState = HAL_UART_STATE_BUSY_RX;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef* huart, uint8_t* pData, uint16_t Size) { return HAL_UART_Receive_IT(huart, pData, Size); }

// Nothing is connected yet: the transfer stays pending
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef* huart, const uint8_t* pData, uint16_t Size) {
    huart->pTxBuffPtr = pData;
    huart->TxXferSize = Size;
    huart->gState = HAL_UART_STATE_BUSY_TX;
    return HAL_OK;
}

/* SD card (data goes through sim_diskio) ------------------------ */

HAL_StatusTypeDef HAL_SD_Init(SD_HandleTypeDef* hsd) {
    HAL_SD_MspInit(hsd);
    hsd->State = HAL_SD_STATE_READY;
    return HAL_OK;
}

HAL_SD_StateTypeDef HAL_SD_GetState(const SD_HandleTypeDef* hsd) { return HAL_SD_STATE_READY; }
HAL_StatusTypeDef HAL_SD_Abort(SD_HandleTypeDef* hsd) { return HAL_OK; }

/* SDRAM (mapped as plain memory) -------------------------------- */

HAL_StatusTypeDef HAL_SDRAM_Init(SDRAM_HandleTypeDef* hsdram, FMC_SDRAM_TimingTypeDef* Timing) {
    HAL_SDRAM_MspInit(hsdram);
    hsdram->State = HAL_SDRAM_STATE_READY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SDRAM_SendCommand(SDRAM_HandleTypeDef* hsdram, FMC_SDRAM_CommandTypeDef* Command, uint32_t Timeout) { return HAL_OK; }
HAL_StatusTypeDef HAL_SDRAM_ProgramRefreshRate(SDRAM_HandleTypeDef* hsdram, uint32_t RefreshRate) { return HAL_OK; }

}  // extern "C"

/* MiguelMoraleja V1.14+ ----------------------------------------- */

// Fault and Error_Handler record (stm32h7xx_it.c on the device, not compiled here): stop with a message.
extern "C" void Crash_Capture(uint32_t* frame, uint32_t kind) {
    fprintf(stderr, "sim: Crash_Capture kind %u, caller 0x%08x\n", kind, frame ? frame[6] : 0);
    abort();
}

#include "Dac.h"
#include "string.h"

// Line-in ADC analog gain, in 1.5 dB steps: 0x0 = 0 dB, 0x3 = +4.5 dB, 0xF = +22.5 dB.
#define LINE_IN_ADC_GAIN 0x00

// ADC_VOL_M6DB (bit 8) shifts the whole gain range down 6 dB, so 0x0 becomes -6 dB.
// Needed because consumer sources pin this input at full scale even at unity gain.
#define LINE_IN_ADC_M6DB 0

Dac::Dac() {}
Dac::~Dac() {}

////////////////////////////////////////////////////////////////////////////////
/* Private Functions ---------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////

// v2.09 freeze fix: every codec transfer used HAL_MAX_DELAY ("wait forever"), so a stuck or
// switched-off I2C3 bus hung the main loop for good on the next volume / mute / line-in change.
// Transfers now give up after kI2cTimeoutMs and a timed-out bus is recovered; a failed read is
// reported, so read-modify-write never writes a random value into a codec register and the
// volume ramps no longer start from garbage.
static const uint32_t kI2cTimeoutMs = 10;     // one 2-4 byte transfer takes well under 1 ms
static const uint32_t kI2cReadyWaitMs = 20;   // a keypad DMA read may be finishing

bool Dac::i2cTransmit(uint8_t* data, uint16_t len) {
    /// @brief Sends bytes to the codec with a bounded wait; recovers a stuck bus on timeout.

    if (i2cWaitReady(kI2cReadyWaitMs) != HAL_OK) {
        I2C3_BusRecover();
        return false;
    }
    const HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c3, SGTL5000_I2C_ADDRESS, data, len, kI2cTimeoutMs);
    if ((status == HAL_TIMEOUT) || (status == HAL_BUSY)) I2C3_BusRecover();
    return status == HAL_OK;
}

bool Dac::i2cReceive(uint8_t* data, uint16_t len) {
    /// @brief Reads bytes from the codec with a bounded wait; recovers a stuck bus on timeout.

    if (i2cWaitReady(kI2cReadyWaitMs) != HAL_OK) {
        I2C3_BusRecover();
        return false;
    }
    const HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c3, SGTL5000_I2C_ADDRESS, data, len, kI2cTimeoutMs);
    if ((status == HAL_TIMEOUT) || (status == HAL_BUSY)) I2C3_BusRecover();
    return status == HAL_OK;
}

bool Dac::readRegister(uint16_t reg, uint16_t& out) {
    /// @brief Reads a full 16-bit SGTL5000 register. Returns false (out untouched) on failure.

    uint8_t txData[2] = {(uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF)};
    uint8_t rxData[2] = {0, 0};
    if (!i2cTransmit(txData, 2)) return false;
    if (!i2cReceive(rxData, 2)) return false;
    out = (uint16_t)((rxData[0] << 8) | rxData[1]);
    return true;
}

uint16_t Dac::readRegisterFull(uint16_t reg) {
    /// @brief Reads a full 16-bit SGTL5000 register over I2C (0 when the read fails).

    uint16_t data = 0;
    readRegister(reg, data);
    return data;
}

bool Dac::writeRegisterFull(uint16_t reg, uint16_t data) {
    /// @brief Writes a full 16-bit value to an SGTL5000 register.

    uint8_t txData[4];
    txData[0] = (reg >> 8) & 0xFF;
    txData[1] = reg & 0xFF;
    txData[2] = (data >> 8) & 0xFF;
    txData[3] = data & 0xFF;
    return i2cTransmit(txData, 4);
}

uint16_t Dac::readRegisterMask(uint16_t reg, uint16_t mask, uint16_t shift) {
    /// @brief Reads a masked bitfield from a 16-bit SGTL5000 register (0 when the read fails).

    uint16_t data = 0;
    readRegister(reg, data);
    return (data & mask) >> shift;
}

void Dac::writeRegisterMask(uint16_t reg, uint16_t mask, uint16_t shift, uint16_t input) {
    /// @brief Updates a masked bitfield in a register using read-modify-write.
    /// Nothing is written when the read fails: patching an unknown value would put random bits
    /// into the other fields of the register.

    uint16_t data = 0;
    if (!readRegister(reg, data)) return;
    data = (data & (~mask)) + (input << shift);
    writeRegisterFull(reg, data);
}

HAL_StatusTypeDef Dac::i2cWaitReady(uint32_t timeout) {
    /// @brief Waits until the I2C peripheral is ready or timeout expires.

    // 1. BUS READY WAIT LOOP
    // Poll bus state/mode until idle or timeout.
    uint32_t t0 = HAL_GetTick();
    while ((HAL_I2C_GetState(&hi2c3) != HAL_I2C_STATE_READY) || (HAL_I2C_GetMode(&hi2c3) != HAL_I2C_MODE_NONE)) {
        if ((HAL_GetTick() - t0) > timeout) return HAL_TIMEOUT;
        __WFE();
    }
    return HAL_OK;
}

////////////////////////////////////////////////////////////////////////////////
/* Public Functions ----------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////

void Dac::initialize() {
    /// @brief Initializes SGTL5000 codec, routing, and startup gain staging.

    // 1. DEVICE PRESENCE CHECK
    // Verify codec responds on I2C before configuration.
    if (HAL_I2C_IsDeviceReady(&hi2c3, SGTL5000_I2C_ADDRESS, 1, 10) != HAL_OK) {
        return;
    }

    // 2. PRE-POWER POP REDUCTION
    // Force headphone level to minimum before analog power-up.
    writeRegisterMask(ANA_HP_CTRL_REG, ANA_HP_CTRL_HP_VOL_LEFT_MASK, ANA_HP_CTRL_HP_VOL_LEFT_SHIFT, 0x7F);
    writeRegisterMask(ANA_HP_CTRL_REG, ANA_HP_CTRL_HP_VOL_RIGHT_MASK, ANA_HP_CTRL_HP_VOL_RIGHT_SHIFT, 0x7F);

    // 3. REFERENCE STABILIZATION POWER-UP
    // Bring up internal reference and protection blocks.
    writeRegisterFull(ANA_POWER_REG, 0x4060);      // Power up VAG and Internal Regulators
    writeRegisterFull(LINREG_CTRL_REG, 0x006C);    // Set internal regulator to ~1.2V
    writeRegisterFull(REF_CTRL_REG, 0x01F2);       // Set VAG to 0.8V and Bias currents
    writeRegisterFull(LINE_OUT_CTRL_REG, 0x0F22);  // Set Line-Out reference current
    writeRegisterFull(SHORT_CTRL_REG, 0x0000);     // Short circuit protection thresholds

    // 4. ANALOG CONTROL CONFIGURATION
    // Register 0x0024 (CHIP_ANA_CTRL): Value 0x0035
    // Bit 8 (MUTE_LO): 0 (Unmute Line-Out)
    // Bit 6 (SELECT_HP): 0 (Route DAC output to Headphones)
    // Bit 5 (EN_ZCD_HP): 1 (Enable Zero-Cross Detector for pop-free volume adjustment)
    // Bit 4 (MUTE_HP): 1 (Keep HP output muted during setup)
    // Bit 2 (SELECT_ADC): 1 (Select Line-In for ADC input to isolate Microphone path)
    // Bit 0 (MUTE_ADC): 1 (Mute ADC unit to eliminate idle input noise)
    writeRegisterFull(ANA_CTRL_REG, 0x0035);

    // 5. DIGITAL ROUTING CONFIGURATION
    // Register 0x000A (CHIP_SSS_CTRL): Value 0x0010
    // Bits 5:4 (DAC_SELECT): 0x01 (Route I2S_IN directly to DAC, bypassing Digital Audio Processor)
    // Bits 1:0 (I2S_SELECT): 0x00 (ADC input to I2S_OUT - recording path is kept clean)
    writeRegisterFull(SSS_CTRL_REG, 0x0010);

    // Disable DAP core to keep startup signal path clean.
    writeRegisterMask(DAP_CONTROL_REG, DAP_CONTROL_DAP_EN_MASK, DAP_CONTROL_DAP_EN_SHIFT, 0x00);
    writeRegisterMask(REF_CTRL_REG, REF_CTRL_SMALL_POP_MASK, REF_CTRL_SMALL_POP_SHIFT, 0x01);

    // 6. I2S HARDWARE START
    // Start full-duplex I2S clocks before final analog stage enable.
    __HAL_UNLOCK(&hi2s3);
    HAL_I2SEx_TransmitReceive_DMA(&hi2s3, (uint16_t*)i2s_txBuffer, (uint16_t*)i2s_rxBuffer, I2S_TOTAL_BUFFER_SIZE);

    // 7. ANALOG POWER OPTIMIZATION
    // Register 0x0030 (CHIP_ANA_POWER): Value 0x40BD
    // Bit 14 (DAC_MONO): 1 (Stereo Mode)
    // Bit 7 (VAG_POWERUP): 1 (Power up VAG reference buffer)
    // Bit 5 (REFTOP_POWERUP): 1 (Power up reference bias currents)
    // Bit 4 (HP_POWERUP): 1 (Power up Headphone Amplifier)
    // Bit 3 (DAC_POWERUP): 1 (Power up DAC unit)
    // Bit 2 (CAPLESS_HP_POWERUP): 1 (Enable Capless mode for direct-coupled output)
    // Bit 1 (ADC_POWERUP): 0 (ADC OFF - Eliminates high-frequency analog hiss)
    // Bit 0 (LINEOUT_POWERUP): 1 (Line-Out Amplifier ON)
    writeRegisterFull(ANA_POWER_REG, 0x40BD);

    // 8. DIGITAL POWER CONFIGURATION
    // Register 0x0002 (CHIP_DIG_POWER): Value 0x0021
    // Bit 5 (DAC_EN): 1 (Enable Digital DAC logic)
    // Bit 4 (DAP_EN): 0 (DAP unit OFF for maximum digital silence)
    // Bit 1 (I2S_OUT_EN): 0 (Recording path digital logic OFF)
    // Bit 0 (I2S_IN_EN): 1 (Playback path digital logic ON)
    writeRegisterFull(DIG_POWER_REG, 0x0021);

    // 9. CLOCK AND DATA FORMAT CONFIGURATION
    // Apply I2S clock and data width settings for STM32H7 path.
    writeRegisterMask(CLK_CTRL_REG, CLK_CTRL_SYS_FS_MASK, CLK_CTRL_SYS_FS_SHIFT, 0x01);  // 44.1 kHz
    writeRegisterMask(I2S_CTRL_REG, I2S_CTRL_DLEN_MASK, I2S_CTRL_DLEN_SHIFT, 0x00);      // 32-bit Word Length (Matches STM32 DMA)
    writeRegisterMask(I2S_CTRL_REG, I2S_CTRL_SCLK_INV_MASK, I2S_CTRL_SCLK_INV_SHIFT, 0x00);

    // 10. FINAL UNMUTE AND GAIN STAGING
    // Set DAC and analog output gain to startup operating levels.
    writeRegisterMask(ADCDAC_CTRL_REG, ADCDAC_CTRL_DAC_MUTE_LEFT_MASK, ADCDAC_CTRL_DAC_MUTE_LEFT_SHIFT, 0x00);
    writeRegisterMask(ADCDAC_CTRL_REG, ADCDAC_CTRL_DAC_MUTE_RIGHT_MASK, ADCDAC_CTRL_DAC_MUTE_RIGHT_SHIFT, 0x00);
    writeRegisterMask(DAC_VOL_REG, DAC_VOL_LEFT_MASK, DAC_VOL_LEFT_SHIFT, 0x3C);
    writeRegisterMask(DAC_VOL_REG, DAC_VOL_RIGHT_MASK, DAC_VOL_RIGHT_SHIFT, 0x3C);

    // Apply base headphone level and unmute HP path.
    writeRegisterMask(ANA_HP_CTRL_REG, ANA_HP_CTRL_HP_VOL_LEFT_MASK, ANA_HP_CTRL_HP_VOL_LEFT_SHIFT, kHpBase);
    writeRegisterMask(ANA_HP_CTRL_REG, ANA_HP_CTRL_HP_VOL_RIGHT_MASK, ANA_HP_CTRL_HP_VOL_RIGHT_SHIFT, kHpBase);
    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_HP_MASK, ANA_CTRL_MUTE_HP_SHIFT, 0x00);

    // Apply line-out startup level.
    writeRegisterMask(LINE_OUT_VOL_REG, LINE_OUT_VOL_LO_VOL_LEFT_MASK, LINE_OUT_VOL_LO_VOL_LEFT_SHIFT, 0x19);
    writeRegisterMask(LINE_OUT_VOL_REG, LINE_OUT_VOL_LO_VOL_RIGHT_MASK, LINE_OUT_VOL_LO_VOL_RIGHT_SHIFT, 0x19);
}

void Dac::enableLineIn() {
    /*
    // 1. ANALOG POWER-UP (REFTOP and VAG ONLY)
    // First, only power up the reference voltages to let VAG stabilize.
    // Bit 5: REFTOP, Bit 0: VAG
    writeRegisterFull(ANA_POWER_REG, 0x40BD);
    HAL_Delay(400); // Wait for VAG to charge (Critical for 0.1uF)
    */
    // 2. ADC ANALOG POWER-UP
    // Now enable the actual ADC analog block.
    // Bit 1: ADC_POWERUP, Bit 6: ADC_MONO (1 = stereo, 0 = left channel only)
    // The very first power-up after reset also has to charge the line-in coupling caps
    // through the input bias network, which takes far longer than a later re-enable where
    // the references are already up. Give that case a much longer settle.
    static bool firstEnable = true;
    writeRegisterFull(ANA_POWER_REG, 0x40FF);
    HAL_Delay(firstEnable ? 400 : 100);
    firstEnable = false;

    // 3. DIGITAL POWER-UP (ADC & I2S OUT)
    // Enable digital logic after analog is rock solid.
    writeRegisterFull(DIG_POWER_REG, 0x0063); 
    HAL_Delay(50);

    // 4. MUTE EVERYTHING & ROUTE
    // Set volume to absolute zero and keep ADC muted during routing.
    writeRegisterFull(ANA_ADC_CTRL_REG, (LINE_IN_ADC_M6DB << 8));
    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_SELECT_ADC_MASK, ANA_CTRL_SELECT_ADC_SHIFT, 0x01);
    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_ADC_MASK, ANA_CTRL_MUTE_ADC_SHIFT, 0x01);

    // 5. THE "CLEAN START" UNMUTE
    // Finally, unmute the ADC and ramp volume.
    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_ADC_MASK, ANA_CTRL_MUTE_ADC_SHIFT, 0x00);
    
    // Gradual gain ramp. ANA_ADC_CTRL is [8] M6DB range shift, [7:4] right, [3:0] left,
    // 1.5 dB per step from 0 dB at 0x0 up to +22.5 dB at 0xF. Measured at 0x03 the line-in
    // pinned the ADC at full scale, so this sits at unity. If it still clips, set M6DB
    // (bit 8) to drop the whole range another 6 dB.
    for (uint8_t v = 0; v <= LINE_IN_ADC_GAIN; v++) {
        writeRegisterFull(ANA_ADC_CTRL_REG, (LINE_IN_ADC_M6DB << 8) | (v << 4) | v);
        HAL_Delay(2);
    }
}

void Dac::disableLineIn() {
    /// @brief Disables line-in capture path and powers down record chain.

    // 1. PRE-MUTE GAIN FLOOR
    // Ramp ADC gain down before muting to avoid clicks.
    writeRegisterMask(ANA_ADC_CTRL_REG, ANA_ADC_CTRL_ADC_VOL_LEFT_MASK, ANA_ADC_CTRL_ADC_VOL_LEFT_SHIFT, 0x00);
    writeRegisterMask(ANA_ADC_CTRL_REG, ANA_ADC_CTRL_ADC_VOL_RIGHT_MASK, ANA_ADC_CTRL_ADC_VOL_RIGHT_SHIFT, 0x00);

    // 2. ADC MUTE APPLY
    // Mute ADC path in analog control register.
    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_ADC_MASK, ANA_CTRL_MUTE_ADC_SHIFT, 0x01);

    // 3. ANALOG RECORD POWER-DOWN
    // Disable ADC analog power block.
    writeRegisterFull(ANA_POWER_REG, 0x40BD);

    // 4. DIGITAL RECORD POWER-DOWN
    // Disable ADC and I2S-out digital recording logic.
    writeRegisterFull(DIG_POWER_REG, 0x0021);
}

void Dac::loadPEQFilter() {
    /// @brief Loads predefined PEQ coefficients into SGTL5000 DAP filter slots.

    // 1. BAND TYPE CONFIGURATION
    // Set band modes: band 0 peaking, band 5-6 LPF.
    // 0x00: peaking/shelf, 0x01: lpf, 0x02: hpf
    writeRegisterFull(DAP_PEQ_PN_CFG_REG, 0x0030);  // 0011 0000 -> bands 5 & 6 as lpf

    // 2. BAND 5 COEFFICIENT LOAD
    // Load stage-1 LPF coefficients.
    writeRegisterFull(DAP_PEQ_PN_COEF_0_REG, 0x0FBA);  // b0
    writeRegisterFull(DAP_PEQ_PN_COEF_1_REG, 0x1F74);  // b1
    writeRegisterFull(DAP_PEQ_PN_COEF_2_REG, 0x0FBA);  // b2
    writeRegisterFull(DAP_PEQ_PN_COEF_3_REG, 0xBD7A);  // a1
    writeRegisterFull(DAP_PEQ_PN_COEF_4_REG, 0x397A);  // a2

    // 3. BAND 6 COEFFICIENT LOAD
    // Load stage-2 LPF coefficients.
    // note: registers for band 6 are usually indexed via the PN index register
    // assuming sequential access or specific register mapping:
    writeRegisterFull(DAP_PEQ_PN_IDX_REG, 0x06);       // select band 6
    writeRegisterFull(DAP_PEQ_PN_COEF_0_REG, 0x0B85);  // b0
    writeRegisterFull(DAP_PEQ_PN_COEF_1_REG, 0x170A);  // b1
    writeRegisterFull(DAP_PEQ_PN_COEF_2_REG, 0x0B85);  // b2
    writeRegisterFull(DAP_PEQ_PN_COEF_3_REG, 0xD26A);  // a1
    writeRegisterFull(DAP_PEQ_PN_COEF_4_REG, 0x290B);  // a2
}

void Dac::enableAudioPostProcessor() {
    /// @brief Enables SGTL5000 DAP post-processing block.
    writeRegisterMask(DAP_CONTROL_REG, DAP_CONTROL_DAP_EN_MASK, DAP_CONTROL_DAP_EN_SHIFT, 0x01);
}

void Dac::disableAudioPostProcessor() {
    /// @brief Disables SGTL5000 DAP post-processing block.
    writeRegisterMask(DAP_CONTROL_REG, DAP_CONTROL_DAP_EN_MASK, DAP_CONTROL_DAP_EN_SHIFT, 0x00);
}

void Dac::enableSurround() {
    /// @brief Enables surround processing mode in the DAP block.
    writeRegisterMask(DAP_SGTL_SURROUND_REG, DAP_SGTL_SURROUND_SELECT_MASK, DAP_SGTL_SURROUND_SELECT_SHIFT, 0x02);
}

void Dac::disableSurround() {
    /// @brief Disables surround processing mode in the DAP block.
    writeRegisterMask(DAP_SGTL_SURROUND_REG, DAP_SGTL_SURROUND_SELECT_MASK, DAP_SGTL_SURROUND_SELECT_SHIFT, 0x00);
}

void Dac::setSurround(uint8_t width) {
    /// @brief Ramps surround width parameter to the target value.

    // 1. RANGE CHECK
    // Accept only hardware-supported width range.
    if ((width >= 0x00) && (width <= 0x07)) {
        // 2. SMOOTH PARAMETER RAMP
        // Step toward target width to avoid abrupt artifacts.
        uint8_t currentWidth = readRegisterMask(DAP_SGTL_SURROUND_REG, DAP_SGTL_SURROUND_WIDTH_CONTROL_MASK, DAP_SGTL_SURROUND_WIDTH_CONTROL_SHIFT);
        uint8_t step = abs(width - currentWidth);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (width > currentWidth) ? currentWidth += 1 : currentWidth -= 1;
                writeRegisterMask(DAP_SGTL_SURROUND_REG, DAP_SGTL_SURROUND_WIDTH_CONTROL_MASK, DAP_SGTL_SURROUND_WIDTH_CONTROL_SHIFT, currentWidth);
            }
        }
    }
}

void Dac::enableBassEnhance() {
    /// @brief Enables bass enhancement in DAP.
    writeRegisterMask(DAP_BASS_ENHANCE_REG, DAP_BASS_ENHANCE_EN_MASK, DAP_BASS_ENHANCE_EN_SHIFT, 0x01);
}

void Dac::disableBassEnhance() {
    /// @brief Disables bass enhancement in DAP.
    writeRegisterMask(DAP_BASS_ENHANCE_REG, DAP_BASS_ENHANCE_EN_MASK, DAP_BASS_ENHANCE_EN_SHIFT, 0x00);
}

void Dac::setBassEnhance(uint8_t lrLevel, uint8_t bassLevel) {
    /// @brief Ramps LR and bass enhancement levels to requested targets.

    // 1. LR LEVEL UPDATE
    // Validate range and ramp LR enhancement level.
    if ((lrLevel >= 0x00) && (lrLevel <= 0x3F)) {
        uint8_t currentLevel = readRegisterMask(DAP_BASS_ENHANCE_CTRL_REG, DAP_BASS_ENHANCE_CTRL_LR_LEVEL_MASK, DAP_BASS_ENHANCE_CTRL_LR_LEVEL_SHIFT);
        uint8_t step = abs(lrLevel - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (lrLevel > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_BASS_ENHANCE_CTRL_REG, DAP_BASS_ENHANCE_CTRL_LR_LEVEL_MASK, DAP_BASS_ENHANCE_CTRL_LR_LEVEL_SHIFT, currentLevel);
            }
        }
    }

    // 2. BASS LEVEL UPDATE
    // Validate range and ramp bass enhancement level.
    if ((bassLevel >= 0x00) && (bassLevel <= 0x7F)) {
        uint8_t currentLevel = readRegisterMask(DAP_BASS_ENHANCE_CTRL_REG, DAP_BASS_ENHANCE_BASS_LEVEL_MASK, DAP_BASS_ENHANCE_BASS_LEVEL_SHIFT);
        uint8_t step = abs(bassLevel - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (bassLevel > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_BASS_ENHANCE_CTRL_REG, DAP_BASS_ENHANCE_BASS_LEVEL_MASK, DAP_BASS_ENHANCE_BASS_LEVEL_SHIFT, currentLevel);
            }
        }
    }
}

void Dac::enable5BandEq() {
    /// @brief Enables the 5-band equalizer processing mode.
    writeRegisterMask(DAP_AUDIO_EQ_REG, DAP_AUDIO_EQ_EN_MASK, DAP_AUDIO_EQ_EN_SHIFT, 0x03);
}

void Dac::disable5BandEq() {
    /// @brief Disables the 5-band equalizer processing mode.
    writeRegisterMask(DAP_AUDIO_EQ_REG, DAP_AUDIO_EQ_EN_MASK, DAP_AUDIO_EQ_EN_SHIFT, 0x00);
}

void Dac::set5BandEQ_Freq00(uint8_t level) {
    /// @brief Ramps EQ band 0 gain to the target level.

    if ((level >= 0x00) && (level <= 0x5F)) {
        uint8_t currentLevel = readRegisterMask(DAP_AUDIO_EQ_BAND0_REG, DAP_AUDIO_EQ_BAND0_VOLUME_MASK, DAP_AUDIO_EQ_BAND0_VOLUME_SHIFT);
        uint8_t step = abs(level - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (level > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_AUDIO_EQ_BAND0_REG, DAP_AUDIO_EQ_BAND0_VOLUME_MASK, DAP_AUDIO_EQ_BAND0_VOLUME_SHIFT, currentLevel);
                HAL_Delay(1);
            }
        }
    }
}

void Dac::set5BandEQ_Freq01(uint8_t level) {
    /// @brief Ramps EQ band 1 gain to the target level.

    if ((level >= 0x00) && (level <= 0x5F)) {
        uint8_t currentLevel = readRegisterMask(DAP_AUDIO_EQ_BAND1_REG, DAP_AUDIO_EQ_BAND1_VOLUME_MASK, DAP_AUDIO_EQ_BAND1_VOLUME_SHIFT);
        uint8_t step = abs(level - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (level > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_AUDIO_EQ_BAND1_REG, DAP_AUDIO_EQ_BAND1_VOLUME_MASK, DAP_AUDIO_EQ_BAND1_VOLUME_SHIFT, currentLevel);
                HAL_Delay(1);
            }
        }
    }
}

void Dac::set5BandEQ_Freq02(uint8_t level) {
    /// @brief Ramps EQ band 2 gain to the target level.

    if ((level >= 0x00) && (level <= 0x5F)) {
        uint8_t currentLevel = readRegisterMask(DAP_AUDIO_EQ_BAND2_REG, DAP_AUDIO_EQ_BAND2_VOLUME_MASK, DAP_AUDIO_EQ_BAND2_VOLUME_SHIFT);
        uint8_t step = abs(level - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (level > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_AUDIO_EQ_BAND2_REG, DAP_AUDIO_EQ_BAND2_VOLUME_MASK, DAP_AUDIO_EQ_BAND2_VOLUME_SHIFT, currentLevel);
            }
        }
    }
}

void Dac::set5BandEQ_Freq03(uint8_t level) {
    /// @brief Ramps EQ band 3 gain to the target level.

    if ((level >= 0x00) && (level <= 0x5F)) {
        uint8_t currentLevel = readRegisterMask(DAP_AUDIO_EQ_BAND3_REG, DAP_AUDIO_EQ_BAND3_VOLUME_MASK, DAP_AUDIO_EQ_BAND3_VOLUME_SHIFT);
        uint8_t step = abs(level - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (level > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_AUDIO_EQ_BAND3_REG, DAP_AUDIO_EQ_BAND3_VOLUME_MASK, DAP_AUDIO_EQ_BAND3_VOLUME_SHIFT, currentLevel);
            }
        }
    }
}

void Dac::set5BandEQ_Freq04(uint8_t level) {
    /// @brief Ramps EQ band 4 gain to the target level.

    if ((level >= 0x00) && (level <= 0x5F)) {
        uint8_t currentLevel = readRegisterMask(DAP_AUDIO_EQ_BAND4_REG, DAP_AUDIO_EQ_BAND4_VOLUME_MASK, DAP_AUDIO_EQ_BAND4_VOLUME_SHIFT);
        uint8_t step = abs(level - currentLevel);
        if (step != 0) {
            for (uint8_t i = 0; i < step; i++) {
                (level > currentLevel) ? currentLevel += 1 : currentLevel -= 1;
                writeRegisterMask(DAP_AUDIO_EQ_BAND4_REG, DAP_AUDIO_EQ_BAND4_VOLUME_MASK, DAP_AUDIO_EQ_BAND4_VOLUME_SHIFT, currentLevel);
            }
        }
    }
}

void Dac::muteHeadphone() {
    /// @brief Mutes headphone output path.

    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_HP_MASK, ANA_CTRL_MUTE_HP_SHIFT, 0x01);
}

void Dac::unmuteHeadphone() {
    /// @brief Unmutes headphone output path.

    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_HP_MASK, ANA_CTRL_MUTE_HP_SHIFT, 0x00);
}

void Dac::setHeadphoneVolume(uint8_t volume_) {
    /// @brief Ramps headphone volume smoothly to target mixer step.

    // 1. TARGET LOOKUP
    // Map UI volume index to codec register data.
    const uint8_t target = kMixerHpVolumeDataLibrary[volume_].data;

    // v2.09: when the current value cannot be read, jump straight to the target instead of
    // ramping from a random start (that took up to 255 writes on a busy bus).
    uint16_t reg = 0;
    if (!readRegister(ANA_HP_CTRL_REG, reg)) {
        writeRegisterFull(ANA_HP_CTRL_REG, (target << 8) | target);
        return;
    }
    uint8_t current = (uint8_t)(reg >> 8);

    // 2. SMOOTH RAMP
    // Increment/decrement one step at a time to target; a failed write ends the ramp with
    // one direct write of the target.
    while (current != target) {
        current = (current < target) ? (uint8_t)(current + 1) : (uint8_t)(current - 1);
        if (!writeRegisterFull(ANA_HP_CTRL_REG, (current << 8) | current)) {
            writeRegisterFull(ANA_HP_CTRL_REG, (target << 8) | target);
            return;
        }
    }
}

void Dac::muteLineout() {
    /// @brief Mutes line-out output path.

    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_LO_MASK, ANA_CTRL_MUTE_LO_SHIFT, 0x01);
}

void Dac::unmuteLineout() {
    /// @brief Unmutes line-out output path.

    writeRegisterMask(ANA_CTRL_REG, ANA_CTRL_MUTE_LO_MASK, ANA_CTRL_MUTE_LO_SHIFT, 0x00);
}

void Dac::setLineoutVolume(uint8_t volume_) {
    /// @brief Ramps line-out volume smoothly to target mixer step.

    // 1. TARGET LOOKUP
    // Map UI volume index to codec register data.
    const uint8_t target = kMixerLoVolumeDataLibrary[volume_].data;

    // v2.09: when the current value cannot be read, jump straight to the target instead of
    // ramping from a random start (that took up to 255 writes on a busy bus).
    uint16_t reg = 0;
    if (!readRegister(LINE_OUT_VOL_REG, reg)) {
        writeRegisterFull(LINE_OUT_VOL_REG, (target << 8) | target);
        return;
    }
    uint8_t current = (uint8_t)(reg >> 8);

    // 2. SMOOTH RAMP
    // Increment/decrement one step at a time to target; a failed write ends the ramp with
    // one direct write of the target.
    while (current != target) {
        current = (current < target) ? (uint8_t)(current + 1) : (uint8_t)(current - 1);
        if (!writeRegisterFull(LINE_OUT_VOL_REG, (current << 8) | current)) {
            writeRegisterFull(LINE_OUT_VOL_REG, (target << 8) | target);
            return;
        }
    }
}

void Dac::audioOn() {
    /// @brief Placeholder for enabling runtime audio output state.
}

void Dac::audioOff() {
    /// @brief Mutes both output paths and halts the I2S DMA stream, zeroing the TX buffer
    /// so no stale audio replays if the stream is restarted without being refilled.

    muteHeadphone();
    muteLineout();
    HAL_I2S_DMAStop(&hi2s3);
    memset(i2s_txBuffer, 0, sizeof(i2s_txBuffer));
}

void Dac::enableAVC() {
    /// @brief Enables automatic volume control.

    writeRegisterMask(DAP_AVC_CTRL_REG, DAP_AVC_CTRL_EN_MASK, DAP_AVC_CTRL_EN_SHIFT, 0x01);
}

void Dac::disableAVC() {
    /// @brief Disables automatic volume control.

    writeRegisterMask(DAP_AVC_CTRL_REG, DAP_AVC_CTRL_EN_MASK, DAP_AVC_CTRL_EN_SHIFT, 0x00);
}

void Dac::testOn() {
    /// @brief Loads diagnostic filter coefficients into test filter slots.

    // 1. TEST COEFFICIENT TABLE
    // Define hardcoded biquad coefficients for test path.
    int filterCoef[7][5] = {
        {266072, -514164, 248121, 514165, -252048},
        {262120, -522148, 260093, 522149, -260069},
        {256275, -512012, 256275, 512013, -250406},
        {206760, -391574, 206760, 391575, -151375},
        {268252, -367346, 155924, 367347, -162031},
        {263811, -234013, 204217, 234014, -205882},
        {321343, -9222, 249942, 112935, -205427}};

    // 2. COEFFICIENT WRITE LOOP
    // Push each filter section into codec coefficient RAM.
    for (int i = 0; i < 7; i++) {
        writeRegisterFull(DAP_COEF_WR_B0_MSB_REG, (filterCoef[i][0] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B0_LSB_REG, filterCoef[i][0] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_B1_MSB_REG, (filterCoef[i][1] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B1_LSB_REG, filterCoef[i][1] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_B2_MSB_REG, (filterCoef[i][2] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B2_LSB_REG, filterCoef[i][2] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_A1_MSB_REG, (filterCoef[i][3] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_A1_LSB_REG, filterCoef[i][3] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_A2_MSB_REG, (filterCoef[i][4] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_A2_LSB_REG, filterCoef[i][4] & 0x0F);
        writeRegisterFull(DAP_FILTER_COEF_ACCESS_REG, 0x0101 + i);
    }
}

void Dac::testOff() {
    /// @brief Clears diagnostic filter coefficients from test filter slots.

    // 1. ZERO COEFFICIENT TABLE
    // Prepare zeroed coefficients to disable test filters.
    int filterCoef[7][5] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}};

    // 2. COEFFICIENT CLEAR LOOP
    // Write zeroed sections into codec coefficient RAM.
    for (int i = 0; i < 7; i++) {
        writeRegisterFull(DAP_COEF_WR_B0_MSB_REG, (filterCoef[i][0] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B0_LSB_REG, filterCoef[i][0] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_B1_MSB_REG, (filterCoef[i][1] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B1_LSB_REG, filterCoef[i][1] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_B2_MSB_REG, (filterCoef[i][2] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_B2_LSB_REG, filterCoef[i][2] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_A1_MSB_REG, (filterCoef[i][3] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_A1_LSB_REG, filterCoef[i][3] & 0x0F);
        writeRegisterFull(DAP_COEF_WR_A2_MSB_REG, (filterCoef[i][4] >> 4) & 0xFFFF);
        writeRegisterFull(DAP_COEF_WR_A2_LSB_REG, filterCoef[i][4] & 0x0F);
        writeRegisterFull(DAP_FILTER_COEF_ACCESS_REG, 0x0101 + i);
    }
}

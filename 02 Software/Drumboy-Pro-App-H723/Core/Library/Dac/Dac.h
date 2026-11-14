#ifndef __DAC_H
#define __DAC_H

#include "Global.h"
#include "SGTL5000.h"
#include "i2c.h"
#include "i2s.h"
#include "main.h"

// clang-format off

/* Memory Allocation In D1 Ram ----------------------------------- */

extern uint32_t i2s_rxBuffer[I2S_TOTAL_BUFFER_SIZE];
extern uint32_t i2s_txBuffer[I2S_TOTAL_BUFFER_SIZE];

class Dac {
   private:
    /* Codec Register Access Helpers ----------------------------- */

    bool i2cTransmit(uint8_t* data_, uint16_t len_);
    bool i2cReceive(uint8_t* data_, uint16_t len_);
    bool readRegister(uint16_t reg_, uint16_t& out_);
    uint16_t readRegisterFull(uint16_t reg_);
    bool writeRegisterFull(uint16_t reg_, uint16_t data_);
    uint16_t readRegisterMask(uint16_t reg_, uint16_t mask_, uint16_t shift_);
    void writeRegisterMask(uint16_t reg_, uint16_t mask, uint16_t shift_, uint16_t input_);
    HAL_StatusTypeDef i2cWaitReady(uint32_t timeout_);

    /* Line-In Non-Blocking Enable State Machine ----------------- */

    enum class LineInEnableStage : uint8_t { IDLE, WAIT_VAG, WAIT_ADC, WAIT_DIG, RAMP };
    LineInEnableStage lineInStage  = LineInEnableStage::IDLE;
    uint32_t          lineInTick   = 0;
    uint8_t           lineInRampVol = 0;

   public:
    /* Lifecycle ------------------------------------------------- */

    Dac();
    ~Dac();
    void initialize();

    /* Input Routing --------------------------------------------- */

    void enableLineIn();
    void disableLineIn();

    /* Post Processor / Effects ---------------------------------- */

    void loadPEQFilter();
    void enableAudioPostProcessor();
    void disableAudioPostProcessor();

    void enableSurround();
    void disableSurround();
    void setSurround(uint8_t width_);

    void enableBassEnhance();
    void disableBassEnhance();
    void setBassEnhance(uint8_t lrLevel_, uint8_t bassLevel_);

    /* Equalizer ------------------------------------------------- */

    void enable5BandEq();
    void disable5BandEq();
    void set5BandEQ_Freq00(uint8_t level_);
    void set5BandEQ_Freq01(uint8_t level_);
    void set5BandEQ_Freq02(uint8_t level_);
    void set5BandEQ_Freq03(uint8_t level_);
    void set5BandEQ_Freq04(uint8_t level_);

    /* Output Gain / Mute ---------------------------------------- */

    void muteHeadphone();
    void unmuteHeadphone();
    void setHeadphoneVolume(uint8_t volume_);

    void muteLineout();
    void unmuteLineout();
    void setLineoutVolume(uint8_t volume_);

    /* Transport Audio State ------------------------------------- */

    void audioOn();
    void audioOff();

    /* Dynamic Range Control ------------------------------------- */

    void enableAVC();
    void disableAVC();

    /* Test Functions -------------------------------------------- */

    void testOn();
    void testOff();
};

// clang-format on

#endif

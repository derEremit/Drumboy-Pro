// Simulator runtime shared between the sim sources.

#ifndef __SIM_H
#define __SIM_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sim {

struct Options {
    std::string cardImage;                                      // Raw SD card image (FAT32), empty = no card
    bool cardCopy = false;                                      // Run on a throwaway clone of the image
    std::string outDir;                                         // Base for relative screenshot/record paths
    std::string script;                                         // Command file to run, "-" = stdin
    std::string controlSocket;                                  // Unix socket for simctl
    float delayScale = 1.0f;                                    // HAL_Delay multiplier (< 1 boots faster)
    int window = -1;                                            // 1 = panel window, 0 = headless, -1 = window unless scripted
    uint32_t runMs = 0;                                         // Stop after this time, 0 = run forever
};

extern Options options;

// Memory map (sim_memory.cpp)
void mapMemory();                                               // Fixed-address MCU regions, runs before firmware globals
void resetPeripherals();                                        // Register reset values the firmware relies on
void startCycleCounter();                                       // Thread that advances DWT_CYCCNT at the core clock

uint64_t nowUs();                                               // Monotonic time since simulator start

// Process (sim_main.cpp)
[[noreturn]] void restart();                                    // NVIC_SystemReset: re-exec with the same arguments
[[noreturn]] void finish(int code);                             // Exit, removing the card clone

// Interrupts (sim_cpu.cpp)
void runIsr(const std::function<void()>& handler);              // Runs a HAL callback as an interrupt
void lockInterrupts();                                          // Simulator access to firmware state, like an ISR
void unlockInterrupts();

// Timers (sim_timers.cpp)
void startTimers();

// SD card (sim_diskio.cpp)
bool cardOpen(const std::string& path);
bool cardInserted();

// LCD (sim_lcd.cpp)
void lcdBusWrite(bool command, uint16_t value);                 // One WR strobe with CS low
void lcdSetBacklight(bool on);
struct LcdInfo {
    uint16_t width, height;                                     // Current orientation
    bool displayOn, backlight;
    uint64_t busWrites, pixels;                                 // Since start
};
LcdInfo lcdInfo();
bool lcdScreenshot(const std::string& path);                    // PNG, as the screen is seen
uint32_t lcdChecksum();                                         // Changes when the image changes
bool lcdCopyFrame(uint16_t* rgb565, uint16_t& width, uint16_t& height);  // Visible image (960x540 max); false = dark
bool writePng(const std::string& path, uint32_t width, uint32_t height, const uint8_t* rgb);

// Window (sim_window.cpp)
int windowRun();                                                // Main thread; returns when the window closes
bool windowScreenshot(const std::string& path);                 // PNG of the whole window (waits for a frame)
bool windowActive();

// Front panel (sim_panel.cpp)
bool panelI2cRead(bool bus4, uint8_t address, uint8_t reg, uint8_t* data, uint16_t size);
void panelI2cWrite(bool bus4, uint8_t address, uint8_t reg, const uint8_t* data, uint16_t size);
bool panelKey(const std::string& name, bool down);              // false = unknown key
void panelTurn(int encoder, int detents);                       // Blocks while the edges play out
void panelTurnAsync(int encoder, int detents);                  // Queued, for the window
int panelKnobPosition(int encoder);                             // Detents turned since start
uint16_t panelKeypadInputs(int keypad);                         // 0 left, 1 right, 2 layer pads; bit low = pressed
uint16_t panelLedOutputs(uint8_t address7);                     // LED expander outputs (0x22 pads, 0x23/0x24 EQ)
void panelFuncButton(bool down);
std::vector<std::string> panelKeyNames();
std::string panelLedsJson();

// Audio (sim_audio.cpp)
void audioStart();                                              // Called when the firmware starts I2S DMA
struct AudioLevel {
    float peakDb[2], rmsDb[2];                                  // dBFS over the requested window
};
AudioLevel audioLevel(uint32_t ms);                             // Most recent ms of output
bool audioRecord(const std::string& path, uint32_t ms);         // Blocks while recording, writes a WAV
uint64_t audioBlocks();
void audioSetOutput(void (*output)(const int32_t* frames, uint32_t count));  // Live playback (window)

// Firmware problems found at run time (sim_libc.cpp)
void warn(const std::string& site, const std::string& message);  // Reported once per site
std::vector<std::string> warnings();

// Control (sim_control.cpp)
std::string runCommand(const std::string& line);                // One command, returns the reply
int runScript(const std::string& path);                         // Exit code: 0 = all expectations passed
void startControlServer(const std::string& path);

}  // namespace sim

#endif

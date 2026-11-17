// Front panel: keys, encoders, function button and LEDs.
//
// Keys sit on three MCP23017 expanders on I2C3 (left 0x20, right 0x21, layer
// pads 0x25), LEDs on three on I2C4 (layer 0x22, EQ left 0x23, EQ right 0x24).
// The expanders are emulated as register files, so the stock key scan
// (interrupt flag -> I2C read of GPIOA/B, active low) and LED writes run
// unchanged. Pressing a key changes the expander's input pins and raises its
// INT line (EXTI). Encoders drive their A/B GPIO pins with quadrature edges,
// A raising EXTI on both edges, as on the board.

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>

#include "MCP23017.h"
#include "main.h"
#include "sim.h"

namespace sim {

/* Expanders ----------------------------------------------------- */

struct Expander {
    std::array<uint8_t, 0x16> regs{};                           // IOCON.BANK = 0 layout
    uint16_t inputs = 0xFFFF;                                   // Pin levels driven from outside (pull-ups idle high)
};

static std::mutex expanderMutex;
static std::map<uint16_t, Expander> expanders;                  // Key: bus (3/4) << 8 | 8-bit address
static std::map<uint16_t, std::array<uint8_t, 256>> otherDevices;

static uint16_t deviceKey(bool bus4, uint8_t address) { return static_cast<uint16_t>((bus4 ? 4 : 3) << 8 | address); }

static bool isExpander(uint8_t address) { return (address >> 1) >= 0x20 && (address >> 1) <= 0x27; }

static uint8_t expanderRead(Expander& e, uint8_t reg) {
    if (reg == MCP23017_GPIOA || reg == MCP23017_GPIOB || reg == MCP23017_INTCAPA || reg == MCP23017_INTCAPB) {
        int port = reg & 1;
        uint8_t dir = e.regs[MCP23017_IODIRA + port];           // 1 = input
        uint8_t in = static_cast<uint8_t>(e.inputs >> (8 * port));
        uint8_t latch = e.regs[MCP23017_OLATA + port];
        uint8_t level = (in & dir) | (latch & ~dir);
        return level ^ (e.regs[MCP23017_IPOLA + port] & dir);
    }
    return reg < e.regs.size() ? e.regs[reg] : 0;
}

static void expanderWrite(Expander& e, uint8_t reg, uint8_t value) {
    if (reg == MCP23017_GPIOA || reg == MCP23017_GPIOB) reg = MCP23017_OLATA + (reg & 1);
    if (reg < e.regs.size()) e.regs[reg] = value;
}

bool panelI2cRead(bool bus4, uint8_t address, uint8_t reg, uint8_t* data, uint16_t size) {
    std::lock_guard<std::mutex> lock(expanderMutex);
    if (isExpander(address)) {
        Expander& e = expanders[deviceKey(bus4, address)];
        for (uint16_t i = 0; i < size; i++) data[i] = expanderRead(e, static_cast<uint8_t>(reg + i));  // SEQOP: auto-increment
    } else {
        auto& regs = otherDevices[deviceKey(bus4, address)];
        for (uint16_t i = 0; i < size; i++) data[i] = regs[(reg + i) & 0xFF];
    }
    return true;
}

void panelI2cWrite(bool bus4, uint8_t address, uint8_t reg, const uint8_t* data, uint16_t size) {
    std::lock_guard<std::mutex> lock(expanderMutex);
    if (isExpander(address)) {
        Expander& e = expanders[deviceKey(bus4, address)];
        for (uint16_t i = 0; i < size; i++) expanderWrite(e, static_cast<uint8_t>(reg + i), data[i]);
    } else {
        auto& regs = otherDevices[deviceKey(bus4, address)];
        for (uint16_t i = 0; i < size; i++) regs[(reg + i) & 0xFF] = data[i];
    }
}

/* Keys ---------------------------------------------------------- */

struct Keypad {
    uint8_t address;                                            // 8-bit (shifted) I2C address on I2C3
    uint16_t intPin;                                            // EXTI line of the expander's INT output
    const char* names[16];                                      // Bit i = key index i in Global.h
};

static const Keypad kKeypads[] = {
    {(MCP23017_I2C_ADDRESS | 0x20) << 1, MX_A_INT_Pin,
     {"RESET", "PLAYSTOP", "REC", "ADD", "UP", "CLEAR", "LEFT", "CENTER", "RIGHT", "COPY", "DOWN", "PASTE", "MUTE", "FILL",
      "STYLE", "SONG"}},
    {(MCP23017_I2C_ADDRESS | 0x21) << 1, MX_B_INT_Pin,
     {"FILE", "DRUMKIT", "MIXER", "RHYTHM", "METRONOME", "IO", "FILTER_0", "FILTER_1", "EQ", "EFFECT_0", "EFFECT_1", "REVERB",
      "LFO", "LAYER_INST", "LAYER_SONG", "KNOB"}},
    {(MCP23017_I2C_ADDRESS | 0x25) << 1, MX_X_INT_Pin,
     {"LAYER_0", "LAYER_1", "LAYER_2", "LAYER_3", "LAYER_4", "LAYER_5", "LAYER_6", "LAYER_7", "LAYER_8", "LAYER_9", nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr}},
};

static std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

static void raiseExti(uint16_t pin) {
    runIsr([pin] { HAL_GPIO_EXTI_Callback(pin); });
}

bool panelKey(const std::string& name, bool down) {
    std::string key = upper(name);
    if (key == "PLAY" || key == "STOP") key = "PLAYSTOP";
    if (key.size() == 1 && isdigit(static_cast<unsigned char>(key[0]))) key = "LAYER_" + key;  // "3" = layer pad 3
    for (const Keypad& pad : kKeypads) {
        for (int bit = 0; bit < 16; bit++) {
            if (!pad.names[bit] || key != pad.names[bit]) continue;
            {
                std::lock_guard<std::mutex> lock(expanderMutex);
                Expander& e = expanders[deviceKey(false, pad.address)];
                if (down) {
                    e.inputs &= ~(1u << bit);
                } else {
                    e.inputs |= (1u << bit);
                }
            }
            raiseExti(pad.intPin);
            return true;
        }
    }
    return false;
}

uint16_t panelKeypadInputs(int keypad) {
    if (keypad < 0 || keypad > 2) return 0xFFFF;
    std::lock_guard<std::mutex> lock(expanderMutex);
    return expanders[deviceKey(false, kKeypads[keypad].address)].inputs;
}

std::vector<std::string> panelKeyNames() {
    std::vector<std::string> names;
    for (const Keypad& pad : kKeypads) {
        for (const char* name : pad.names) {
            if (name) names.push_back(name);
        }
    }
    return names;
}

void panelFuncButton(bool down) {
    if (down) {
        BUTTON_GPIO_Port->IDR &= ~static_cast<uint32_t>(BUTTON_Pin);
        raiseExti(BUTTON_Pin);
    } else {
        BUTTON_GPIO_Port->IDR |= BUTTON_Pin;
    }
}

/* Encoders ------------------------------------------------------ */

static std::atomic<int> knobPositions[8];                       // Detents turned so far (for the knob drawing)

struct EncoderPins {
    GPIO_TypeDef* aPort;
    uint16_t aPin;
    GPIO_TypeDef* bPort;
    uint16_t bPin;
};

static EncoderPins encoderPins(int n) {
    switch (n) {
        case 0: return {ENCO_0A_GPIO_Port, ENCO_0A_Pin, ENCO_0B_GPIO_Port, ENCO_0B_Pin};
        case 1: return {ENCO_1A_GPIO_Port, ENCO_1A_Pin, ENCO_1B_GPIO_Port, ENCO_1B_Pin};
        case 2: return {ENCO_2A_GPIO_Port, ENCO_2A_Pin, ENCO_2B_GPIO_Port, ENCO_2B_Pin};
        case 3: return {ENCO_3A_GPIO_Port, ENCO_3A_Pin, ENCO_3B_GPIO_Port, ENCO_3B_Pin};
        case 4: return {ENCO_4A_GPIO_Port, ENCO_4A_Pin, ENCO_4B_GPIO_Port, ENCO_4B_Pin};
        case 5: return {ENCO_5A_GPIO_Port, ENCO_5A_Pin, ENCO_5B_GPIO_Port, ENCO_5B_Pin};
        case 6: return {ENCO_6A_GPIO_Port, ENCO_6A_Pin, ENCO_6B_GPIO_Port, ENCO_6B_Pin};
        default: return {ENCO_7A_GPIO_Port, ENCO_7A_Pin, ENCO_7B_GPIO_Port, ENCO_7B_Pin};
    }
}

static void setPin(GPIO_TypeDef* port, uint16_t pin, bool high) {
    if (high) {
        port->IDR |= pin;
    } else {
        port->IDR &= ~static_cast<uint32_t>(pin);
    }
}

// One detent is a full quadrature cycle from the idle state (A = B = 1).
// Clockwise A leads: 11 -> 01 -> 00 -> 10 -> 11, so the firmware samples AB =
// 1 then 2 on the two A edges. Edges are spaced beyond its 700 us debounce.
void panelTurn(int encoder, int detents) {
    if (encoder < 0 || encoder > 7) return;
    EncoderPins p = encoderPins(encoder);
    const auto kEdgeGap = std::chrono::microseconds(1500);
    bool clockwise = detents > 0;
    int count = clockwise ? detents : -detents;
    knobPositions[encoder] += detents;
    for (int i = 0; i < count; i++) {
        // Phase order for one cycle: (A, B) levels after each change
        static const bool kCw[4][2] = {{false, true}, {false, false}, {true, false}, {true, true}};
        static const bool kCcw[4][2] = {{true, false}, {false, false}, {false, true}, {true, true}};
        const bool(*phases)[2] = clockwise ? kCw : kCcw;
        bool lastA = true;
        for (int step = 0; step < 4; step++) {
            setPin(p.aPort, p.aPin, phases[step][0]);
            setPin(p.bPort, p.bPin, phases[step][1]);
            if (phases[step][0] != lastA) raiseExti(p.aPin);
            lastA = phases[step][0];
            std::this_thread::sleep_for(kEdgeGap);
        }
    }
}

// Turns requested from the window are queued and played out by one worker
// thread, so the UI never waits for the quadrature edges.
static std::atomic<int> pendingTurns[8];

void panelTurnAsync(int encoder, int detents) {
    if (encoder < 0 || encoder > 7 || detents == 0) return;
    static std::atomic<bool> started(false);
    if (!started.exchange(true)) {
        std::thread([] {
            while (true) {
                bool idle = true;
                for (int i = 0; i < 8; i++) {
                    int n = pendingTurns[i].exchange(0);
                    if (n) {
                        panelTurn(i, n);
                        idle = false;
                    }
                }
                if (idle) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }).detach();
    }
    pendingTurns[encoder] += detents;
}

int panelKnobPosition(int encoder) { return (encoder >= 0 && encoder < 8) ? knobPositions[encoder].load() : 0; }

/* LEDs ---------------------------------------------------------- */

uint16_t panelLedOutputs(uint8_t address7) {
    std::lock_guard<std::mutex> lock(expanderMutex);
    Expander& e = expanders[deviceKey(true, static_cast<uint8_t>(address7 << 1))];
    return static_cast<uint16_t>(e.regs[MCP23017_OLATB] << 8 | e.regs[MCP23017_OLATA]);
}

std::string panelLedsJson() {
    std::lock_guard<std::mutex> lock(expanderMutex);
    auto outputs = [](uint8_t address7) {
        Expander& e = expanders[deviceKey(true, static_cast<uint8_t>(address7 << 1))];
        return static_cast<unsigned>(e.regs[MCP23017_OLATB] << 8 | e.regs[MCP23017_OLATA]);
    };
    char json[256];
    snprintf(json, sizeof(json), "{\"layer\": \"0x%04x\", \"eq_left\": \"0x%04x\", \"eq_right\": \"0x%04x\", \"status\": [%d, %d, %d]}",
             outputs(0x22), outputs(0x23), outputs(0x24), (LED0_GPIO_Port->ODR & LED0_Pin) ? 1 : 0, (LED1_GPIO_Port->ODR & LED1_Pin) ? 1 : 0,
             (LED2_GPIO_Port->ODR & LED2_Pin) ? 1 : 0);
    return json;
}

}  // namespace sim

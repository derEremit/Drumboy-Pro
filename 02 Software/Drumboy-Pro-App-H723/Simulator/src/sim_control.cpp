// Command interface for scripted and AI-driven tests.
//
// The same commands come from a script file (--script), stdin (--script -) or
// a Unix socket (--control, used by Simulator/tools/simctl). Each command gets
// a one-line reply starting with "ok" or "error"; `state` replies with JSON.
// `help` lists the commands.

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Read access to the controller's state for `state` and `expect`
#define private public
#include "Controller.h"
#undef private

#ifdef DRUMBOY_PLUGINS
#include "Generator.h"
#include "Manager.h"
#include "Plugin.h"
#endif
#include "sim.h"

extern Controller controller;

namespace sim {

static const char* const kMenuNames[] = {
    "INIT",     "GLOBAL",   "FILE",     "DRUMKIT",  "MIXER",        "RHYTHM",       "METRO",        "IO",          "FILTER_0", "FILTER_1",
    "EQ",       "EFFECT_0", "EFFECT_1", "REVERB",   "LFO",          "LAYER_INST_0", "LAYER_INST_1", "LAYER_INST_2", "LAYER_SONG", "TEST",
    "ROUTING",  "EUCLID",   "SONG",     "NOTE",     "DUB"};  // 0x14.. are MiguelMoraleja additions

static std::string menuName(int menu) {
    int count = sizeof(kMenuNames) / sizeof(kMenuNames[0]);
    return (menu >= 0 && menu < count) ? kMenuNames[menu] : std::to_string(menu);
}

// Relative output paths go under --out
static std::string outputPath(const std::string& path) {
    if (path.empty() || path[0] == '/' || options.outDir.empty()) return path;
    return options.outDir + "/" + path;
}

static void sleepMs(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static std::string fmt(const char* format, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), format, value);
    return buf;
}

/* Observable values --------------------------------------------- */

// Values usable in `get` and `expect`, read like an ISR would
static std::map<std::string, std::string> snapshot() {
    std::map<std::string, std::string> v;
    lockInterrupts();
    v["menu"] = menuName(controller.menu);
    v["play"] = controller.playActive ? "1" : "0";
    v["record"] = controller.recordActive ? "1" : "0";
    v["alert"] = controller.alertFlag ? "1" : "0";
    v["tempo"] = std::to_string(controller.rhythm.tempo);
    v["layer"] = std::to_string(controller.selectedLayerNum);
    v["bank"] = std::to_string(controller.activeSongBank);
    v["led.padsPlay"] = controller.ledLayerPlayMode ? "1" : "0";  // Pad LEDs follow playback (else show a selection)
    v["led.padsData"] = std::to_string(controller.ledLayerPlayData);  // Pads the firmware wants lit
    v["keys.active"] = controller.keyboardActive ? "1" : "0";
    v["keys.left"] = std::to_string(controller.keyboard.keyLeft.previousState);  // Last scanned state, bit low = pressed
    v["keys.right"] = std::to_string(controller.keyboard.keyRight.previousState);
    v["keys.pending"] = std::to_string(controller.keyboard.keyLayer.read | controller.keyboard.keyLeft.read << 1 | controller.keyboard.keyRight.read << 2);
    // Modifier keys the firmware thinks are held (stuck flags show up here)
    const auto& kb = controller.keyboard;
    std::string held;
    const std::pair<const char*, bool> mods[] = {{"MUTE", kb.muteKeyPress},       {"FILL", kb.fillKeyPress},     {"STYLE", kb.styleKeyPress},
                                                 {"LAYER_INST", kb.layerInstKeyPress}, {"LAYER_SONG", kb.layerSongKeyPress}, {"LFO", kb.lfoKeyPress},
                                                 {"BANK", kb.bankKeyPress},       {"KNOB", kb.encKeyPress},      {"SELECT", kb.shiftKeyPress}};
    for (const auto& m : mods) {
        if (m.second) held += std::string(held.empty() ? "" : ",") + m.first;
    }
    v["keys.held"] = held.empty() ? "none" : held;

    // Current song bank: active beats per track, F (fill) and M (mute) per track
    std::string beats, fills, mutes, hashes;
    for (int i = 0; i < kLayerLibrarySize; i++) {
        const Layer& layer = controller.layerLibrary[i];
        int n = 0;
        uint32_t hash = 2166136261u;                            // Beat contents, to see any change (EVOLVE, re-roll, lock)
        auto mix = [&hash](uint32_t v) { hash = (hash ^ v) * 16777619u; };
        for (const Beat& b : layer.bankLibrary[controller.activeSongBank].beatLibrary) {
            n += b.getActive() ? 1 : 0;
            if (b.getActive()) {
                mix(b.data);
                mix(b.startInterval);
                mix(b.endInterval);
                mix(b.fillTarget);
            }
        }
        char hex[12];
        snprintf(hex, sizeof(hex), "%04x", (hash ^ (hash >> 16)) & 0xFFFF);
        hashes += (i ? "," : "") + std::string(hex);
        beats += (i ? "," : "") + std::to_string(n);
        fills += layer.fill ? "1" : "0";
        mutes += layer.mute ? "1" : "0";
    }
    // MiguelMoraleja additions: EUCLID page fields and the Layer Song selection
    v["euclid"] = std::to_string(controller.euclidSteps) + "," + std::to_string(controller.euclidHits) + "," + std::to_string(controller.euclidRotate) + "," +
                  std::to_string(controller.euclidGrid) + "," + (controller.euclidMerge ? "1" : "0") + "," + std::to_string(controller.euclidAccentCount);
    v["beat.selected"] = std::to_string(controller.selectedBeatNum);
    v["quantize"] = std::to_string(controller.rhythm.quantize);
    v["song.beats"] = beats;
    v["song.hash"] = hashes;
    v["song.fills"] = fills;
    v["song.mutes"] = mutes;
#ifdef DRUMBOY_PLUGINS
    v["generator"] = generator_isOpen() ? "1" : "0";
    v["manager"] = manager_isOpen() ? "1" : "0";
    v["plugins.dropped"] = std::to_string(pluginHost_getDroppedEvents());
    for (uint8_t i = 0; i < 8; i++) {
        Plugin* plugin = pluginHost_getPlugin(i);
        if (!plugin) break;
        std::string key = std::string("plugin.") + plugin->id();
        v[key] = std::string(pluginHost_isRunning(i) ? "running" : "off") + (pluginHost_isConfigured(i) ? ",on" : ",off");
    }
#endif
    unlockInterrupts();

    LcdInfo lcd = lcdInfo();
    v["lcd.on"] = (lcd.displayOn && lcd.backlight) ? "1" : "0";
    v["lcd.width"] = std::to_string(lcd.width);
    v["lcd.height"] = std::to_string(lcd.height);
    v["lcd.checksum"] = std::to_string(lcdChecksum());
    v["lcd.pixels"] = std::to_string(lcd.pixels);

    AudioLevel level = audioLevel(200);
    v["audio.blocks"] = std::to_string(audioBlocks());
    v["audio.peak"] = fmt("%.1f", std::max(level.peakDb[0], level.peakDb[1]));
    v["audio.rms"] = fmt("%.1f", std::max(level.rmsDb[0], level.rmsDb[1]));
    v["uptime_ms"] = std::to_string(nowUs() / 1000);
    return v;
}

static std::string stateJson() {
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& kv : snapshot()) {
        char* end = nullptr;
        strtod(kv.second.c_str(), &end);
        const std::string& t = kv.second;
        size_t d = (t[0] == '-') ? 1 : 0;
        bool leadingZero = t.size() > d + 1 && t[d] == '0' && isdigit(static_cast<unsigned char>(t[d + 1]));
        bool number = !t.empty() && *end == '\0' && !leadingZero;  // JSON numbers have no leading zeros
        out << (first ? "" : ", ") << "\"" << kv.first << "\": " << (number ? kv.second : "\"" + kv.second + "\"");
        first = false;
    }
    out << ", \"leds\": " << panelLedsJson() << "}";
    return out.str();
}

static bool compare(const std::string& actual, const std::string& op, const std::string& expected) {
    char* endA;
    char* endE;
    double a = strtod(actual.c_str(), &endA);
    double e = strtod(expected.c_str(), &endE);
    bool numeric = !actual.empty() && !expected.empty() && *endA == 0 && *endE == 0;
    if (op == "==") return numeric ? a == e : actual == expected;
    if (op == "!=") return numeric ? a != e : actual != expected;
    if (!numeric) return false;
    if (op == "<") return a < e;
    if (op == "<=") return a <= e;
    if (op == ">") return a > e;
    if (op == ">=") return a >= e;
    return false;
}

/* Commands ------------------------------------------------------ */

static const char* kHelp =
    "ok commands: "
    "press KEY[+KEY...] [hold_ms] | down KEY | up KEY | turn ENC(0-7) DETENTS | button | "
    "wait MS | wait-ready [timeout_ms] | wait-menu NAME [timeout_ms] | wait-stable [quiet_ms] [timeout_ms] | "
    "screenshot PATH | window-screenshot PATH | record PATH MS | level [ms] | state | get FIELD | dump-track N | expect FIELD OP VALUE | keys | warnings | restart | quit [code]";

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, sep)) {
        if (!part.empty()) parts.push_back(part);
    }
    return parts;
}

static std::string waitFor(const std::function<bool()>& done, uint32_t timeoutMs, const std::string& what) {
    uint64_t end = nowUs() + timeoutMs * 1000ULL;
    while (!done()) {
        if (nowUs() > end) return "error timeout waiting for " + what;
        sleepMs(10);
    }
    return "ok";
}

std::string runCommand(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::vector<std::string> args;
    for (std::string a; in >> a;) args.push_back(a);
    auto arg = [&](size_t i, const std::string& fallback) { return i < args.size() ? args[i] : fallback; };
    auto num = [&](size_t i, uint32_t fallback) { return i < args.size() ? static_cast<uint32_t>(strtoul(args[i].c_str(), nullptr, 10)) : fallback; };

    if (cmd.empty() || cmd[0] == '#') return "";
    if (cmd == "help") return kHelp;

    if (cmd == "press") {
        if (args.empty()) return "error usage: press KEY[+KEY...] [hold_ms]";
        std::vector<std::string> keys = split(args[0], '+');
        for (const std::string& k : keys) {
            if (!panelKey(k, true)) return "error unknown key " + k + " (see `keys`)";
            sleepMs(30);                                        // One scan per change: the firmware reads each keypad separately
        }
        sleepMs(num(1, 80));
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            panelKey(*it, false);
            sleepMs(30);
        }
        sleepMs(50);
        return "ok";
    }
    if (cmd == "down" || cmd == "up") {
        if (args.empty() || !panelKey(args[0], cmd == "down")) return "error unknown key " + arg(0, "") + " (see `keys`)";
        sleepMs(30);
        return "ok";
    }
    if (cmd == "keys") {
        std::string out = "ok";
        for (const std::string& k : panelKeyNames()) out += " " + k;
        return out;
    }
    if (cmd == "turn") {
        if (args.size() < 2) return "error usage: turn ENC(0-7) DETENTS";
        int enc = atoi(args[0].c_str());
        if (enc < 0 || enc > 7) return "error encoder must be 0-7";
        panelTurn(enc, atoi(args[1].c_str()));
        sleepMs(30);
        return "ok";
    }
    if (cmd == "button") {
        panelFuncButton(true);
        sleepMs(80);
        panelFuncButton(false);
        return "ok";
    }
    if (cmd == "wait") {
        sleepMs(num(0, 100));
        return "ok";
    }
    if (cmd == "wait-ready") {
        return waitFor([] { return controller.getMenu() != INIT_MENU; }, num(0, 30000), "the main loop");
    }
    if (cmd == "wait-menu") {
        if (args.empty()) return "error usage: wait-menu NAME [timeout_ms]";
        std::string want = args[0];
        return waitFor([want] { return menuName(controller.getMenu()) == want; }, num(1, 5000), "menu " + want);
    }
    if (cmd == "wait-stable") {
        uint32_t quietMs = num(0, 300);
        uint32_t last = lcdChecksum();
        uint64_t since = nowUs();
        return waitFor(
            [&] {
                uint32_t now = lcdChecksum();
                if (now != last) {
                    last = now;
                    since = nowUs();
                }
                return nowUs() - since >= quietMs * 1000ULL;
            },
            num(1, 10000), "a stable screen");
    }
    if (cmd == "screenshot") {
        if (args.empty()) return "error usage: screenshot PATH";
        std::string path = outputPath(args[0]);
        return lcdScreenshot(path) ? "ok " + path : "error cannot write " + path;
    }
    if (cmd == "window-screenshot") {
        if (args.empty()) return "error usage: window-screenshot PATH";
        if (!windowActive()) return "error no window (start without --headless, or with --window)";
        std::string path = outputPath(args[0]);
        return windowScreenshot(path) ? "ok " + path : "error cannot write " + path;
    }
    if (cmd == "record") {
        if (args.size() < 2) return "error usage: record PATH MS";
        std::string path = outputPath(args[0]);
        return audioRecord(path, num(1, 1000)) ? "ok " + path : "error cannot write " + path;
    }
    if (cmd == "level") {
        AudioLevel l = audioLevel(num(0, 200));
        char buf[160];
        snprintf(buf, sizeof(buf), "ok peak %.1f %.1f rms %.1f %.1f dBFS", l.peakDb[0], l.peakDb[1], l.rmsDb[0], l.rmsDb[1]);
        return buf;
    }
    if (cmd == "state") return "ok " + stateJson();
    if (cmd == "dump-track") {
        // Raw beats of one track in the current bank: start-end/type.pattern.time.level/fill target
        int t = atoi(arg(0, "1").c_str()) - 1;
        if (t < 0 || t >= kLayerLibrarySize) return "error track 1-10";
        lockInterrupts();
        const Bank& bank = controller.layerLibrary[t].bankLibrary[controller.activeSongBank];
        std::string out = "ok";
        for (const Beat& b : bank.beatLibrary) {
            if (!b.getActive()) continue;
            char item[48];
            snprintf(item, sizeof(item), " %u-%u/%u.%u.%u.%u", b.startInterval, b.endInterval, b.getFillType(), b.getFillPattern(), b.getFillTime(),
                     b.getFillLevel());
            out += item;
            if (b.getFillTarget() != NULL_FILL_TARGET) out += "/f" + std::to_string(b.getFillTarget());
        }
        int fills = 0;
        for (const BeatFill& f : bank.beatFillLibrary) fills += f.getActive() ? 1 : 0;
        unlockInterrupts();
        return out + " | fills " + std::to_string(fills);
    }
    if (cmd == "warnings") {
        std::vector<std::string> list = warnings();
        std::string out = "ok " + std::to_string(list.size());
        for (const std::string& w : list) out += " | " + w;
        return out;
    }
    if (cmd == "get") {
        auto values = snapshot();
        auto it = values.find(arg(0, ""));
        return it == values.end() ? "error unknown field (see `state`)" : "ok " + it->second;
    }
    if (cmd == "expect") {
        if (args.size() < 3) return "error usage: expect FIELD OP VALUE";
        auto values = snapshot();
        auto it = values.find(args[0]);
        if (it == values.end()) return "error unknown field " + args[0] + " (see `state`)";
        if (compare(it->second, args[1], args[2])) return "ok " + args[0] + " = " + it->second;
        return "error expected " + args[0] + " " + args[1] + " " + args[2] + ", got " + it->second;
    }
    if (cmd == "restart") restart();                             // Power cycle: re-exec on the same card
    if (cmd == "quit") finish(static_cast<int>(num(0, 0)));
    return "error unknown command " + cmd + " (try `help`)";
}

/* Script and socket --------------------------------------------- */

int runScript(const std::string& path) {
    std::ifstream file;
    std::istream* in = &std::cin;
    if (path != "-") {
        file.open(path);
        if (!file) {
            printf("sim: cannot open script %s\n", path.c_str());
            return 2;
        }
        in = &file;
    }
    int lineNumber = 0;
    for (std::string line; std::getline(*in, line);) {
        lineNumber++;
        std::string reply = runCommand(line);
        if (reply.empty()) continue;
        printf("> %s\n%s\n", line.c_str(), reply.c_str());
        if (reply.compare(0, 5, "error") == 0) {
            printf("sim: script failed at line %d\n", lineNumber);
            return 1;
        }
    }
    return 0;
}

static void serveClient(int fd) {
    std::string buffer;
    char chunk[512];
    ssize_t n;
    while ((n = read(fd, chunk, sizeof(chunk))) > 0) {
        buffer.append(chunk, static_cast<size_t>(n));
        size_t eol;
        while ((eol = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, eol);
            buffer.erase(0, eol + 1);
            std::string reply = runCommand(line);
            if (reply.empty()) reply = "ok";
            reply += "\n";
            if (write(fd, reply.data(), reply.size()) < 0) break;
        }
    }
    close(fd);
}

void startControlServer(const std::string& path) {
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(server, F_SETFD, FD_CLOEXEC);                         // A restart (exec) must not inherit the sockets
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        fprintf(stderr, "sim: control socket path longer than %zu bytes, use a shorter (or relative) path\n", sizeof(addr.sun_path) - 1);
        finish(2);
    }
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    unlink(path.c_str());
    if (server < 0 || bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(server, 4) != 0) {
        perror("sim: control socket");
        finish(2);
    }
    printf("sim: control socket %s\n", path.c_str());
    std::thread([server] {
        while (true) {
            int client = accept(server, nullptr, nullptr);
            if (client < 0) continue;
            fcntl(client, F_SETFD, FD_CLOEXEC);
            std::thread(serveClient, client).detach();
        }
    }).detach();
}

}  // namespace sim


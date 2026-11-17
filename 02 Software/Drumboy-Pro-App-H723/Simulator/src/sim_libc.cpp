// C library hooks used by the firmware build (see Simulator/hal/sim_prelude.h).

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "sim.h"

namespace sim {

static std::mutex warningsMutex;
static std::vector<std::string> warningList;
static std::set<std::string> warningSites;

void warn(const std::string& site, const std::string& message) {
    std::lock_guard<std::mutex> lock(warningsMutex);
    if (!warningSites.insert(site).second) return;
    warningList.push_back(message);
    fprintf(stderr, "sim: warning: %s\n", message.c_str());
}

std::vector<std::string> warnings() {
    std::lock_guard<std::mutex> lock(warningsMutex);
    return warningList;
}

}  // namespace sim

extern "C" int sim_sprintf(char* buffer, size_t size, const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (size == static_cast<size_t>(-1)) {
        int n = vsprintf(buffer, format, args);
        va_end(args);
        return n;
    }

    char text[1024];
    int n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    size_t needed = static_cast<size_t>(n) + 1;                 // Including the terminating NUL
    if (needed <= size) {
        memcpy(buffer, text, needed);
        return n;
    }

    // Keep what fits and terminate inside the buffer: a byte shorter than the
    // device shows, but the string stays a string for the strcat/strlen that
    // follow. The device writes the NUL past the end instead.
    if (size > 0) {
        memcpy(buffer, text, size - 1);
        buffer[size - 1] = '\0';
    }
    const char* name = strrchr(file, '/');
    std::string site = std::string(name ? name + 1 : file) + ":" + std::to_string(line);
    sim::warn(site, "firmware sprintf overflow at " + site + ": writes " + std::to_string(needed) + " bytes into a " +
                        std::to_string(size) + "-byte buffer");
    return n;
}

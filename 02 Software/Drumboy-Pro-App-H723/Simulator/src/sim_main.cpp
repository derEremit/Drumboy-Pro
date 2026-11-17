// Drumboy Pro simulator entry point.
//
// The firmware's main() is compiled as firmware_main() (see
// Simulator/CMakeLists.txt) and runs on its own thread, like the MCU core.
// The host main thread runs the test script, or waits while a client drives
// the simulator over the control socket.

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/clonefile.h>
#else
#include <fcntl.h>
#include <link.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <linux/fs.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "Controller.h"
#include "sim.h"

int firmware_main();
extern Controller controller;

static char** savedArgv;
static char cardClonePath[1024];                                // Removed on exit when --card-copy is used

namespace sim {

Options options;

void finish(int code) {
    fflush(stdout);
    if (cardClonePath[0]) unlink(cardClonePath);
    _Exit(code);
}

void restart() {
    printf("sim: system reset\n");
    fflush(stdout);
    if (!options.controlSocket.empty()) unlink(options.controlSocket.c_str());
    char path[4096];
#ifdef __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) execv(path, savedArgv);
#else
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0) {
        path[n] = '\0';
        execv(path, savedArgv);
    }
#endif
    perror("sim: restart failed");
    _Exit(1);
}

// Copy of the card image for --card-copy: a clone on file systems that can
// (APFS, btrfs, XFS), else a plain copy. Returns false with errno set.
static bool cloneFile(const char* source, const char* target) {
#ifdef __APPLE__
    return clonefile(source, target, 0) == 0;
#else
    int in = open(source, O_RDONLY | O_CLOEXEC);
    if (in < 0) return false;
    int out = open(target, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (out < 0) {
        close(in);
        return false;
    }
    bool ok = ioctl(out, FICLONE, in) == 0;                     // Reflink: instant, no extra space (btrfs, XFS)
    if (!ok) {
        // Plain copy that keeps the holes of a sparse image: walk the data extents
        struct stat st {};
        ok = fstat(in, &st) == 0 && ftruncate(out, st.st_size) == 0;
        off_t pos = 0;
        while (ok && pos < st.st_size) {
            off_t data = lseek(in, pos, SEEK_DATA);
            if (data < 0) break;                                // No more data: the rest is a hole
            off_t hole = lseek(in, data, SEEK_HOLE);
            if (hole < 0) hole = st.st_size;
            while (ok && data < hole) {
                size_t chunk = static_cast<size_t>(std::min<off_t>(hole - data, 1 << 20));
                ssize_t n = copy_file_range(in, &data, out, &data, chunk, 0);
                if (n < 0 && (errno == EXDEV || errno == ENOSYS || errno == EINVAL)) {
                    // Different file systems (e.g. image on disk, TMPDIR on tmpfs): plain read/write
                    static thread_local std::vector<char> buffer(1 << 20);
                    if (pread(in, buffer.data(), chunk, data) != static_cast<ssize_t>(chunk) ||
                        pwrite(out, buffer.data(), chunk, data) != static_cast<ssize_t>(chunk)) {
                        ok = false;
                        break;
                    }
                    data += static_cast<off_t>(chunk);
                } else if (n <= 0) {
                    ok = false;
                }
            }
            pos = hole;
        }
    }
    int savedErrno = errno;
    close(in);
    close(out);
    if (!ok) unlink(target);
    errno = savedErrno;
    return ok;
#endif
}

}  // namespace sim

#ifndef __APPLE__
static int firstLoadAddress(struct dl_phdr_info* info, size_t, void* out) {
    *static_cast<uintptr_t*>(out) = info->dlpi_addr;             // The main program comes first; 0 for a non-PIE build
    return 1;
}
#endif

// Firmware crashes print the faulting address and a backtrace, so a failed
// test run shows where (symbolize with: atos -o DrumboySim -l LOAD_ADDRESS PC
// on macOS, addr2line -e DrumboySim PC on Linux)
static void crashHandler(int sig, siginfo_t* info, void* context) {
    auto* uc = static_cast<ucontext_t*>(context);
#ifdef __APPLE__
    unsigned long long pc = uc->uc_mcontext->__ss.__rip;
    const void* load = _dyld_get_image_header(0);
#else
    unsigned long long pc = static_cast<unsigned long long>(uc->uc_mcontext.gregs[REG_RIP]);
    uintptr_t loadAddress = 0;
    dl_iterate_phdr(firstLoadAddress, &loadAddress);
    const void* load = reinterpret_cast<const void*>(loadAddress);
#endif
    fprintf(stderr, "sim: crashed with signal %d at pc 0x%llx (address %p), load address %p\n", sig, pc, info->si_addr, load);
    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, 2);
    if (cardClonePath[0]) unlink(cardClonePath);
    _Exit(128 + sig);
}

static void usage(const char* argv0) {
    printf("Usage: %s [options]\n", argv0);
    printf("  --card IMAGE       raw FAT32 SD card image (Simulator/tools/make_card_image.sh),\n");
    printf("                     default $DRUMBOY_SIM_CARD, else no card\n");
    printf("  --card-copy        run on a temporary clone of the image, leaving it unchanged\n");
    printf("  --out DIR          directory for relative screenshot/record paths\n");
    printf("  --script FILE      run test commands from FILE (- = stdin), exit 0 if all pass\n");
    printf("  --control SOCKET   accept commands on a Unix socket (Simulator/tools/simctl)\n");
    printf("  --run-ms N         stop after N ms; exit 0 if the firmware reached its main loop\n");
    printf("  --delay-scale F    multiply HAL_Delay by F (e.g. 0.1 skips most of the boot animation)\n");
    printf("  --window           show the panel window (default unless --script or --run-ms is given)\n");
    printf("  --headless         no window and no sound\n");
}

static bool parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        bool hasValue = i + 1 < argc;
        if (!strcmp(argv[i], "--card") && hasValue) {
            sim::options.cardImage = argv[++i];
        } else if (!strcmp(argv[i], "--card-copy")) {
            sim::options.cardCopy = true;
        } else if (!strcmp(argv[i], "--out") && hasValue) {
            sim::options.outDir = argv[++i];
        } else if (!strcmp(argv[i], "--script") && hasValue) {
            sim::options.script = argv[++i];
        } else if (!strcmp(argv[i], "--control") && hasValue) {
            sim::options.controlSocket = argv[++i];
        } else if (!strcmp(argv[i], "--run-ms") && hasValue) {
            sim::options.runMs = static_cast<uint32_t>(strtoul(argv[++i], nullptr, 10));
        } else if (!strcmp(argv[i], "--window")) {
            sim::options.window = 1;
        } else if (!strcmp(argv[i], "--headless")) {
            sim::options.window = 0;
        } else if (!strcmp(argv[i], "--delay-scale") && hasValue) {
            sim::options.delayScale = strtof(argv[++i], nullptr);
        } else {
            usage(argv[0]);
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    savedArgv = argv;
    struct sigaction action {};
    action.sa_sigaction = crashHandler;
    action.sa_flags = SA_RESETHAND | SA_SIGINFO;
    for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE}) sigaction(sig, &action, nullptr);
    if (!parseArgs(argc, argv)) return 2;
    const char* envCard = getenv("DRUMBOY_SIM_CARD");
    if (sim::options.cardImage.empty() && envCard && *envCard) sim::options.cardImage = envCard;
    if (!sim::options.cardImage.empty() && access(sim::options.cardImage.c_str(), F_OK) != 0) {
        fprintf(stderr, "sim: card image not found: %s\n"
                        "     make one with Simulator/tools/make_card_image.sh, or pass --card / set DRUMBOY_SIM_CARD\n",
                sim::options.cardImage.c_str());
        return 1;
    }

    sim::mapMemory();
    // A restart (NVIC_SystemReset) re-execs with the same arguments and keeps using the same clone
    if (!sim::options.cardImage.empty() && sim::options.cardCopy) {
        const char* reuse = getenv("DRUMBOY_SIM_CARD_CLONE");
        if (reuse) {
            snprintf(cardClonePath, sizeof(cardClonePath), "%s", reuse);
        } else {
#ifdef __APPLE__
            const char* tmp = getenv("TMPDIR");
            std::string cloneDir = tmp ? tmp : "/tmp";
#else
            // Next to the image: same file system, so FICLONE can reflink it (TMPDIR overrides)
            const char* tmp = getenv("TMPDIR");
            std::string cloneDir = tmp ? tmp : sim::options.cardImage.substr(0, sim::options.cardImage.find_last_of('/') == std::string::npos ? 0 : sim::options.cardImage.find_last_of('/'));
            if (cloneDir.empty()) cloneDir = ".";
#endif
            snprintf(cardClonePath, sizeof(cardClonePath), "%s/drumboy-card-%d.img", cloneDir.c_str(), getpid());
            unlink(cardClonePath);
            if (!sim::cloneFile(sim::options.cardImage.c_str(), cardClonePath)) {
                perror("sim: cannot clone the card image");
                return 1;
            }
            setenv("DRUMBOY_SIM_CARD_CLONE", cardClonePath, 1);
        }
        sim::options.cardImage = cardClonePath;
    }
    if (!sim::options.cardImage.empty() && !sim::cardOpen(sim::options.cardImage)) {
        fprintf(stderr, "sim: cannot open card image %s\n", sim::options.cardImage.c_str());
        return 1;
    }
    printf("sim: card %s\n", sim::cardInserted() ? sim::options.cardImage.c_str() : "not inserted");

    sim::startCycleCounter();
    sim::startTimers();
    if (!sim::options.controlSocket.empty()) sim::startControlServer(sim::options.controlSocket);
    std::thread(firmware_main).detach();

    bool window = sim::options.window == 1 || (sim::options.window == -1 && sim::options.script.empty() && !sim::options.runMs);
    if (window) {
        // The window needs the main thread (macOS); a script runs beside it
        if (!sim::options.script.empty()) std::thread([] { sim::finish(sim::runScript(sim::options.script)); }).detach();
        if (sim::options.runMs) {
            std::thread([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(sim::options.runMs));
                sim::finish(0);
            }).detach();
        }
        sim::finish(sim::windowRun());
    }

    if (!sim::options.script.empty()) {
        sim::finish(sim::runScript(sim::options.script));
    }

    // Without a script: report boot progress, stop after --run-ms
    Menu lastMenu = INIT_MENU;
    uint64_t mainLoopUs = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        Menu menu = controller.getMenu();
        if (menu != lastMenu) {
            printf("sim: %6.3f s  menu %d\n", sim::nowUs() / 1e6, static_cast<int>(menu));
            if (!mainLoopUs && menu != INIT_MENU && menu != TEST_MENU) mainLoopUs = sim::nowUs();
            lastMenu = menu;
        }
        if (sim::options.runMs && sim::nowUs() >= sim::options.runMs * 1000ULL) break;
    }

    if (mainLoopUs) {
        printf("sim: firmware reached the main loop after %.3f s\n", mainLoopUs / 1e6);
        sim::finish(0);
    }
    printf("sim: firmware did not reach the main loop\n");
    sim::finish(1);
}

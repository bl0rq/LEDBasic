#include "Utf8Path.h"
#include "HostSession.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static void printUsage() {
    std::printf("Usage:\n");
    std::printf("  ledbasic run <file.bas> [--leds N] [--frames N] [--dt MS] [--dump-ascii]\n");
    std::printf("  ledbasic --help\n");
}

static char lumChar(const ledbasic::LedPixel& p) {
    int y = (30 * p.r + 59 * p.g + 11 * p.b) / 100;
    const char* scale = " .:-=+*#%@";
    int i = y * 9 / 255;
    if (i < 0) i = 0;
    if (i > 9) i = 9;
    return scale[i];
}

static void printHexFrame(int index, unsigned long t, const std::vector<ledbasic::LedPixel>& frame) {
    std::printf("frame %d t=%lu\n", index, t);
    for (size_t i = 0; i < frame.size(); i++) {
        std::printf("%02x%02x%02x", frame[i].r, frame[i].g, frame[i].b);
        if (i + 1 < frame.size()) std::printf(" ");
    }
    std::printf("\n");
}

static void printAsciiFrame(int index, unsigned long t, const std::vector<ledbasic::LedPixel>& frame) {
    std::printf("frame %d t=%lu  ", index, t);
    for (size_t i = 0; i < frame.size(); i++) {
        std::putchar(lumChar(frame[i]));
    }
    std::printf("\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::vector<std::string> args = ledbasic::utf8Args(argc, argv);
    if (args.size() < 2) {
        printUsage();
        return 1;
    }
    if (args[1] == "--help" || args[1] == "-h") {
        printUsage();
        return 0;
    }
    if (args[1] != "run" || args.size() < 3) {
        printUsage();
        return 1;
    }

    const std::string& pathArg = args[2]; // UTF-8
    const char* path = pathArg.c_str();
    int leds = ledbasic::HostSession::kDefaultLeds;
    int frames = 5;
    unsigned dt = ledbasic::HostSession::kDefaultFrameDtMs;
    bool ascii = false;

    for (size_t i = 3; i < args.size(); i++) {
        if (args[i] == "--leds" && i + 1 < args.size()) {
            leds = std::atoi(args[++i].c_str());
        } else if (args[i] == "--frames" && i + 1 < args.size()) {
            frames = std::atoi(args[++i].c_str());
        } else if (args[i] == "--dt" && i + 1 < args.size()) {
            dt = (unsigned)std::atoi(args[++i].c_str());
        } else if (args[i] == "--dump-ascii") {
            ascii = true;
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", args[i].c_str());
            printUsage();
            return 1;
        }
    }

    ledbasic::HostSession session(leds);
    session.setFrameDtMs(dt);
    if (!session.loadFile(path)) {
        std::fprintf(stderr, "Failed to load %s\n", path);
        auto diags = session.diagnostics();
        for (size_t i = 0; i < diags.size(); i++) {
            const char* kind = diags[i].kind == Diagnostic::Lex ? "lex" :
                               diags[i].kind == Diagnostic::Parse ? "parse" : "runtime";
            std::fprintf(stderr, "  %s:%d:%d: %s\n", kind, diags[i].line, diags[i].column,
                         diags[i].message.c_str());
        }
        return 1;
    }

    std::vector<std::vector<ledbasic::LedPixel>> dumps;
    if (!session.runFrames(frames, &dumps)) {
        std::fprintf(stderr, "Runtime error while running %s\n", path);
        auto diags = session.diagnostics();
        for (size_t i = 0; i < diags.size(); i++) {
            std::fprintf(stderr, "  runtime:%d:%d: %s\n", diags[i].line, diags[i].column,
                         diags[i].message.c_str());
        }
        return 1;
    }

    bool anyLit = false;
    for (size_t f = 0; f < dumps.size(); f++) {
        unsigned long t = (unsigned long)f * dt;
        if (ascii) printAsciiFrame((int)f, t, dumps[f]);
        else printHexFrame((int)f, t, dumps[f]);
        for (size_t p = 0; p < dumps[f].size(); p++) {
            if (dumps[f][p].r || dumps[f][p].g || dumps[f][p].b) anyLit = true;
        }
    }
    std::printf("any_lit=%d frames=%d leds=%d\n", anyLit ? 1 : 0, frames, leds);
    return anyLit ? 0 : 2;
}

// Memory statistics of the layer: bytes of CPU copies and accesses to them after upload.
#include "common.h"

#include <cstdarg>
#include <chrono>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace d3d9gl {

std::atomic<unsigned> g_presentCount{0};
static std::atomic<int64_t> g_bytes[CPU_KINDS];
static std::atomic<int64_t> g_peak[CPU_KINDS];
static std::atomic<unsigned> g_touches[TOUCH_KINDS];
static std::atomic<int> g_glObjects[GLO_KINDS];

void GLObjects(int kind, int delta) { g_glObjects[kind] += delta; }

void CpuBytes(int kind, int64_t delta) {
    int64_t now = g_bytes[kind] += delta;
    int64_t peak = g_peak[kind];
    while (now > peak && !g_peak[kind].compare_exchange_weak(peak, now)) {}
}

void Touched(int kind, const char *fmt, ...) {
    unsigned n = ++g_touches[kind];
    if (n > (kind >= TOUCH_LATE_GL ? 40u : 8u)) return;
    if (kind == TOUCH_BUF_UNREADABLE) {
        char msg[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        Log("error: %s", msg);  // always, not only with PORTICO_D3D9_STATS
        return;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Log("after upload (%u): %s", n, buf);
}

void LogStats(unsigned presents) {
    static const char *names[CPU_KINDS] = {"tex managed", "tex default", "tex system", "surfaces",
                                           "vb static", "vb dynamic", "ib static", "ib dynamic"};
    static const char *touches[TOUCH_KINDS] = {"tex lock", "tex copy", "static buf lock", "dynamic buf lock", "readonly buf lock", "late on GL thread", "late on other thread",
                                             "unreadable off-thread buffer lock"};
    std::string line;
    char buf[96];
    int64_t total = 0;
    for (int i = 0; i < CPU_KINDS; i++) {
        total += g_bytes[i];
        snprintf(buf, sizeof(buf), " %s %.1f (peak %.1f)", names[i], g_bytes[i] / 1048576.0, g_peak[i] / 1048576.0);
        line += buf;
    }
    Log("stats at present %u: CPU copies %.1f MB:%s", presents, total / 1048576.0, line.c_str());
    line.clear();
    for (int i = 0; i < TOUCH_KINDS; i++) {
        snprintf(buf, sizeof(buf), " %s %u", touches[i], g_touches[i].load());
        line += buf;
    }
    Log("stats at present %u: accesses after upload:%s", presents, line.c_str());
    Log("stats at present %u: GL objects: textures %d, buffers %d, renderbuffers %d, framebuffers %d, samplers %d, "
        "programs %d", presents, g_glObjects[GLO_TEXTURE].load(), g_glObjects[GLO_BUFFER].load(),
        g_glObjects[GLO_RENDERBUFFER].load(), g_glObjects[GLO_FRAMEBUFFER].load(), g_glObjects[GLO_SAMPLER].load(),
        g_glObjects[GLO_PROGRAM].load());
}

bool ProfileOn() {
    static const bool s_on = [] {
        char v[8] = {0};
        return GetEnv("PORTICO_D3D9_PROFILE", v, sizeof(v)) && v[0] == '1';
    }();
    return s_on;
}

double NowMs() {
#ifdef __EMSCRIPTEN__
    return emscripten_get_now();
#else
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

static double g_profMs[PROF_KINDS];
static unsigned g_profCalls[PROF_KINDS];

void ProfileAdd(int kind, double ms) {
    g_profMs[kind] += ms;
    g_profCalls[kind]++;
}

void ProfileFrame() {
    if (!ProfileOn()) return;
    static double s_start = 0;
    static unsigned s_frames = 0;
    double now = NowMs();
    if (!s_start) s_start = now;
    s_frames++;
    if (now - s_start < 5000.0) return;
    double f = s_frames;
    Log("profile: %.1f fps; per frame: draw %.2f ms (%.0f calls), clear %.2f ms, upload %.2f ms (%.1f), present %.2f ms; "
        "layer %.2f of %.2f ms", f * 1000.0 / (now - s_start), g_profMs[PROF_DRAW] / f, g_profCalls[PROF_DRAW] / f,
        g_profMs[PROF_CLEAR] / f, g_profMs[PROF_UPLOAD] / f, g_profCalls[PROF_UPLOAD] / f, g_profMs[PROF_PRESENT] / f,
        (g_profMs[PROF_DRAW] + g_profMs[PROF_CLEAR] + g_profMs[PROF_PRESENT]) / f, (now - s_start) / f);
    for (int i = 0; i < PROF_KINDS; i++) g_profMs[i] = 0, g_profCalls[i] = 0;
    s_start = now;
    s_frames = 0;
}

}  // namespace d3d9gl

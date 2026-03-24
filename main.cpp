#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include "memscan.h"
#include "nop.h"
#include "proc.h"
#include <sys/mman.h>
static const char* SIG =
    "?? ?? 40 F9 ?? ?? ?? EB ?? ?? ?? 54 ?? ?? 40 F9 ?? 81 40 F9 E0 03 ?? AA"
    " 00 01 3F D6 ?? ?? 00 37 ?? ?? 40 F9 ?? ?? ?? A9 ?? ?? ?? CB ?? ?? ?? D3"
    " ?? ?? 00 51 ?? ?? ?? 8A";

static constexpr size_t PATCH_OFFSET = 8;
static constexpr size_t PATCH_SIZE   = 4;

static void*   g_target   = nullptr;
static uint8_t g_saved[PATCH_SIZE];
static bool    g_patched  = false;

static void* PatchThread(void*) {
    while (!g_patched) {
        sigscan_handle* h = sigscan_setup(SIG, "libminecraftpe.so", 0);
        if (h) {
            void* addr = get_sigscan_result(h);
            sigscan_cleanup(h);

            if (addr) {
                g_target = (void*)((uintptr_t)addr + PATCH_OFFSET);
                int prot = get_prot((uintptr_t)g_target);
                if (prot > 0 && (prot & PROT_READ)) {
                    memcpy(g_saved, g_target, PATCH_SIZE);
                    g_patched = patch_nop(g_target, PATCH_SIZE);
                }
            }
        }
        if (!g_patched) usleep(100 * 1000);
    }
    return nullptr;
}

__attribute__((constructor))
void StartUp() {
    pthread_t t;
    pthread_create(&t, nullptr, PatchThread, nullptr);
    pthread_detach(t);
}
__attribute__((destructor))
void Shutdown() {
    if (!g_patched || !g_target) return;
    memcpy(g_target, g_saved, PATCH_SIZE);
    __builtin___clear_cache((char*)g_target, (char*)g_target + PATCH_SIZE);
}
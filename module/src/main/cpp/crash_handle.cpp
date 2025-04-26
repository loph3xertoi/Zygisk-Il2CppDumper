//
// Created by daw on 4/26/25.
//

#include "crash_handle.h"
#include <unwind.h>
#include <dlfcn.h>
#include <csignal>
#include "log.h"
#include <unistd.h>

static _Unwind_Reason_Code trace_function(struct _Unwind_Context *context, void *arg) {
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        Dl_info info;
        if (dladdr((void *) pc, &info) && info.dli_sname) {
            LOGE("  #%p : %s", (void *) pc, info.dli_sname);
        } else {
            LOGE("  #%p : <unknown>", (void *) pc);
        }
    }
    return _URC_NO_REASON;
}

void signal_handler(int sig) {
    LOGE("Caught signal %d", sig);
    _Unwind_Backtrace(trace_function, nullptr);
    _exit(1);  // make sure the app exits
}

void install_crash_handler() {
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGFPE, signal_handler);
}

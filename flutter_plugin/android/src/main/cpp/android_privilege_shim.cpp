#ifdef __ANDROID__

#include <sys/types.h>

// Android app processes are already sandboxed and seccomp may block setuid/setgid.
// DCMTK's SCP startup calls privilege-drop helpers that invoke these APIs.
// Intercepting them avoids SIGSYS on recent Android versions.
extern "C" int __wrap_setuid(uid_t) {
    return 0;
}

extern "C" int __wrap_setgid(gid_t) {
    return 0;
}

#endif

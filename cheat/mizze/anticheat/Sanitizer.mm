// Sanitizer.mm - layer 4: leave nothing behind.
//
// A process that was injected into carries traces the game can look for:
// loader variables in the environment, files the toolkit wrote, descriptors
// that were inherited from the injector, and the ability to attach a
// debugger. All of that is cleaned up here.
#import <Foundation/Foundation.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

// The iOS SDK does not ship sys/ptrace.h, but the call is in libSystem.
extern "C" int ptrace(int request, int pid, void* address, int data);
#ifndef PT_DENY_ATTACH
#define PT_DENY_ATTACH 31
#endif

namespace mizze {
namespace sanitize {
namespace {

// Files the toolkit is known to write.
const char* const kTraceFiles[] = {
    "/var/mobile/Library/Caches/com.dts.bugchi.pid",
    "/var/mobile/Library/Caches/mizze.pid",
    "/var/mobile/Library/Caches/iOSViet.pid",
    "/var/mobile/Library/Caches/tiz.pid",
};

const char* const kLoaderVariables[] = {
    "DYLD_INSERT_LIBRARIES",
    "DYLD_LIBRARY_PATH",
    "DYLD_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH",
};

}  // namespace

// No debugger may attach to the game while it runs.
void DenyDebuggerAttach() {
  ptrace(PT_DENY_ATTACH, 0, 0, 0);
}

// The loader variables tell the game how the library got in. They are removed
// from the environment of the running process, so a getenv or a walk over
// environ finds nothing.
void CleanEnvironment() {
  for (const char* name : kLoaderVariables) {
    unsetenv(name);
  }
  unsetenv("TIZANTICHEAT_INSERT");
}

// Files the toolkit wrote are removed.
void RemoveTraces() {
  for (const char* path : kTraceFiles) {
    unlink(path);
  }
}

// Every descriptor that is open is marked close on exec, so none of it is
// handed over to a child process the game starts.
void SealInheritedDescriptors() {
  int highest = 1024;
  int sealed = 0;
  for (int fd = 3; fd < highest; ++fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) continue;
    if (flags & FD_CLOEXEC) continue;
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0) sealed++;
  }
  NSLog(@"[Mizze] sealed %d inherited descriptors", sealed);
}

}  // namespace sanitize
}  // namespace mizze

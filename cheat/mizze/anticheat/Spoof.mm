// Spoof.mm - layer 1: hide the injected images from every file system view.
//
// The game and its native security library look at /proc/self/maps to find
// out what is mapped into their own process, and they read the directory of
// the tweak loader. Both views are interposed here, so the injected library
// simply is not there.
//
// Everything else is passed through untouched: only the paths that belong to
// this toolkit are answered differently.
#import <Foundation/Foundation.h>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace mizze {
namespace spoof {
namespace {

// Name fragments that must not show up anywhere.
struct Marker {
  const char* needle;
  bool dylib_only;  // only matches inside a library path
};

const Marker kMarkers[] = {
    {"iOSViet", false},
    {"libsubsystem", false},
    {"TIZAntiCheat", false},
    {"com.dts.bugchi.pid", false},
    {"gg", true},
};

bool IsMarker(const char* text) {
  if (!text) return false;
  for (const Marker& marker : kMarkers) {
    if (strstr(text, marker.needle)) {
      if (!marker.dylib_only) return true;
      // Short names only count when the entry is a library.
      if (strstr(text, ".dylib") || strstr(text, "DynamicLibraries")) return true;
    }
  }
  return false;
}

bool IsMarkerPath(const char* path) {
  if (!path) return false;
  for (const Marker& marker : kMarkers) {
    if (strstr(path, marker.needle)) {
      if (!marker.dylib_only) return true;
      if (strstr(path, ".dylib") || strstr(path, "DynamicLibraries")) return true;
    }
  }
  return false;
}

// The real libc entry points, resolved once.
typedef int (*OpenFn)(const char*, int, ...);
typedef int (*OpenAtFn)(int, const char*, int, ...);
typedef FILE* (*FopenFn)(const char*, const char*);
typedef int (*StatFn)(const char*, struct stat*);
typedef DIR* (*OpendirFn)(const char*);
typedef struct dirent* (*ReaddirFn)(DIR*);
typedef char* (*GetenvFn)(const char*);

OpenFn real_open() {
  static OpenFn fn = reinterpret_cast<OpenFn>(dlsym(RTLD_NEXT, "open"));
  return fn;
}

OpenAtFn real_openat() {
  static OpenAtFn fn = reinterpret_cast<OpenAtFn>(dlsym(RTLD_NEXT, "openat"));
  return fn;
}

FopenFn real_fopen() {
  static FopenFn fn = reinterpret_cast<FopenFn>(dlsym(RTLD_NEXT, "fopen"));
  return fn;
}

StatFn real_stat() {
  static StatFn fn = reinterpret_cast<StatFn>(dlsym(RTLD_NEXT, "stat"));
  return fn;
}

StatFn real_lstat() {
  static StatFn fn = reinterpret_cast<StatFn>(dlsym(RTLD_NEXT, "lstat"));
  return fn;
}

OpendirFn real_opendir() {
  static OpendirFn fn = reinterpret_cast<OpendirFn>(dlsym(RTLD_NEXT, "opendir"));
  return fn;
}

ReaddirFn real_readdir() {
  static ReaddirFn fn = reinterpret_cast<ReaddirFn>(dlsym(RTLD_NEXT, "readdir"));
  return fn;
}

GetenvFn real_getenv() {
  static GetenvFn fn = reinterpret_cast<GetenvFn>(dlsym(RTLD_NEXT, "getenv"));
  return fn;
}

bool IsMapsPath(const char* path) {
  if (!path) return false;
  if (strstr(path, "/maps") == nullptr) return false;
  return strstr(path, "/proc/") != nullptr;
}

// Builds a filtered copy of /proc/self/maps and returns a descriptor for it.
// The copy lives in an unlinked temporary file, so it leaves no trace.
int OpenFilteredMaps(const char* path) {
  OpenFn open_fn = real_open();
  if (!open_fn) return -1;
  int fd = open_fn(path, O_RDONLY);
  if (fd < 0) return -1;
  std::string content;
  char buffer[4096];
  ssize_t got = 0;
  while ((got = read(fd, buffer, sizeof(buffer))) > 0) {
    content.append(buffer, static_cast<size_t>(got));
  }
  close(fd);

  std::string filtered;
  size_t start = 0;
  while (start < content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string::npos) end = content.size();
    std::string line = content.substr(start, end - start);
    if (!IsMarker(line.c_str())) {
      filtered.append(line);
      filtered.push_back('\n');
    }
    start = end + 1;
  }

  char tmp[] = "/tmp/tizmapsXXXXXX";
  int out = mkstemp(tmp);
  if (out < 0) return -1;
  unlink(tmp);
  size_t written = 0;
  while (written < filtered.size()) {
    ssize_t put = write(out, filtered.data() + written, filtered.size() - written);
    if (put <= 0) break;
    written += static_cast<size_t>(put);
  }
  lseek(out, 0, SEEK_SET);
  return out;
}

int TIZOpen(const char* path, int flags, ...) {
  if (IsMarkerPath(path)) {
    errno = ENOENT;
    return -1;
  }
  if (IsMapsPath(path)) {
    int filtered = OpenFilteredMaps(path);
    if (filtered >= 0) return filtered;
  }
  OpenFn fn = real_open();
  if (!fn) return -1;
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = static_cast<mode_t>(va_arg(args, int));
    va_end(args);
  }
  return fn(path, flags, mode);
}

int TIZOpenat(int dirfd, const char* path, int flags, ...) {
  if (IsMarkerPath(path)) {
    errno = ENOENT;
    return -1;
  }
  if (IsMapsPath(path)) {
    int filtered = OpenFilteredMaps(path);
    if (filtered >= 0) return filtered;
  }
  OpenAtFn fn = real_openat();
  if (!fn) return -1;
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = static_cast<mode_t>(va_arg(args, int));
    va_end(args);
  }
  return fn(dirfd, path, flags, mode);
}

FILE* TIZFopen(const char* path, const char* mode) {
  if (IsMarkerPath(path)) return nullptr;
  FopenFn fn = real_fopen();
  if (!fn) return nullptr;
  return fn(path, mode);
}

int TIZStat(const char* path, struct stat* buf) {
  if (IsMarkerPath(path)) {
    errno = ENOENT;
    return -1;
  }
  StatFn fn = real_stat();
  if (!fn) return -1;
  return fn(path, buf);
}

int TIZLstat(const char* path, struct stat* buf) {
  if (IsMarkerPath(path)) {
    errno = ENOENT;
    return -1;
  }
  StatFn fn = real_lstat();
  if (!fn) return -1;
  return fn(path, buf);
}

DIR* TIZOpendir(const char* path) {
  OpendirFn fn = real_opendir();
  if (!fn) return nullptr;
  return fn(path);
}

struct dirent* TIZReaddir(DIR* dirp) {
  ReaddirFn fn = real_readdir();
  if (!fn) return nullptr;
  struct dirent* entry = nullptr;
  while ((entry = fn(dirp)) != nullptr) {
    if (IsMarkerPath(entry->d_name)) continue;
    break;
  }
  return entry;
}

char* TIZGetenv(const char* name) {
  // The loader variables tell the game how the library got in.
  if (name) {
    if (strstr(name, "DYLD_") != nullptr) return nullptr;
    if (strstr(name, "TIZANTICHEAT") != nullptr) return nullptr;
  }
  GetenvFn fn = real_getenv();
  if (!fn) return nullptr;
  return fn(name);
}

}  // namespace
}  // namespace spoof
}  // namespace mizze

// DYLD interpose: the replacements win for every image loaded after this one.
__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_open __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZOpen,
    (const void*)(uintptr_t)&open,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_openat __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZOpenat,
    (const void*)(uintptr_t)&openat,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_fopen __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZFopen,
    (const void*)(uintptr_t)&fopen,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_stat __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZStat,
    (const void*)(uintptr_t)&stat,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_lstat __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZLstat,
    (const void*)(uintptr_t)&lstat,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_readdir __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZReaddir,
    (const void*)(uintptr_t)&readdir,
};

__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_getenv __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&mizze::spoof::TIZGetenv,
    (const void*)(uintptr_t)&getenv,
};

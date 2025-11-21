#ifndef WIN32UTILS_H
#define WIN32UTILS_H

#include <memory>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Functor for RAII closing of handles
struct HandleDeleter {
    void operator()(HANDLE h) const {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

// The type alias
using ScopedHandle = std::unique_ptr<void, HandleDeleter>;

struct RegistryKeyDeleter {
    // Defines what "pointer" type unique_ptr holds.
    // HKEY is already a pointer-like type (struct HKEY__*).
    using pointer = HKEY;

    void operator()(HKEY h) const {
        if (h) {
            RegCloseKey(h);
        }
    }
};
using ScopedRegistryKey = std::unique_ptr<HKEY, RegistryKeyDeleter>;

#endif // WIN32UTILS_H

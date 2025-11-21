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

#endif // WIN32UTILS_H

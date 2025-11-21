#include "utils.h"

DWORD qtKeyToWinVK(int key) {
    // Strip modifiers
    key &= ~Qt::KeyboardModifierMask;

    // 1. Handle A-Z, 0-9 (WinAPI just uses ASCII for these)
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) {
        return key;
    }

    // 2. Handle Special Keys
    switch (key) {
    case Qt::Key_F1: return VK_F1;
    case Qt::Key_F2: return VK_F2;
    case Qt::Key_F3: return VK_F3;
    case Qt::Key_F4: return VK_F4;
    case Qt::Key_F5: return VK_F5;
    case Qt::Key_F6: return VK_F6;
    case Qt::Key_F7: return VK_F7;
    case Qt::Key_F8: return VK_F8;
    case Qt::Key_F9: return VK_F9;
    case Qt::Key_F10: return VK_F10;
    case Qt::Key_F11: return VK_F11;
    case Qt::Key_F12: return VK_F12;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Space:  return VK_SPACE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Tab: return VK_TAB;
    }

    return 0;
}

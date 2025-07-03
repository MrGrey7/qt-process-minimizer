#include "hotkeyeventfilter.h"
#include <QByteArray>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

bool HotkeyEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) {
    Q_UNUSED(result)
    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
        return false;

    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && onHotkeyPressed) {
        onHotkeyPressed(msg->wParam); // wParam holds the hotkey ID
        return true;
    }
    return false;
}

#ifndef HOTKEYEVENTFILTER_H
#define HOTKEYEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include <functional>

class HotkeyEventFilter : public QAbstractNativeEventFilter {
public:
    std::function<void(int)> onHotkeyPressed;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};

#endif // HOTKEYEVENTFILTER_H

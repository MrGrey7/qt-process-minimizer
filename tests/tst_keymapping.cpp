#include <QtTest>
#include "../utils.h"

class TestKeyMapping : public QObject
{
    Q_OBJECT

private slots:
    void testLetters() {
        // In WinAPI, 'A' is just 0x41
        QCOMPARE(qtKeyToWinVK(Qt::Key_A), (DWORD)'A');
        QCOMPARE(qtKeyToWinVK(Qt::Key_Z), (DWORD)'Z');
    }

    void testNumbers() {
        QCOMPARE(qtKeyToWinVK(Qt::Key_0), (DWORD)'0');
        QCOMPARE(qtKeyToWinVK(Qt::Key_9), (DWORD)'9');
    }

    void testFunctionKeys() {
        // VK_F1 is 0x70
        QCOMPARE(qtKeyToWinVK(Qt::Key_F1), (DWORD)VK_F1);
        QCOMPARE(qtKeyToWinVK(Qt::Key_F12), (DWORD)VK_F12);
    }

    void testModifiersIgnored() {
        // The function should strip Ctrl/Alt/Shift and return just the key
        QCOMPARE(qtKeyToWinVK(Qt::Key_A | Qt::ControlModifier), (DWORD)'A');
    }

    void testUnknownKey() {
        // Qt::Key_Exclam -> Should return 0 (not mapped in your switch)
        QCOMPARE(qtKeyToWinVK(Qt::Key_Exclam), (DWORD)0);
    }
};

QTEST_MAIN(TestKeyMapping)
#include "tst_keymapping.moc"

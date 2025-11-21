#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <Windows.h>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <TlHelp32.h>   // For process snapshot
#include <Psapi.h>      // For GetModuleBaseName
#include <QCloseEvent>
#include <QSettings>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "ProcessPickerDialog.h"
#include "win32utils.h"
// #pragma comment(lib, "Psapi.lib")

DWORD qtKeyToWinVK(int key) {
    // Strip modifiers first
    key &= ~Qt::KeyboardModifierMask;

    // 1. Handle A-Z, 0-9 (They map 1:1)
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) {
        return key;
    }

    // 2. Handle Special Keys (Mapping needed)
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

    return 0; // Unsupported key
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create and install native hotkey filter
    qApp->installNativeEventFilter(&hotkeyFilter);

    minimizeHotkeyId = GlobalAddAtom(L"MrGrey_Minimizer_Min_Key");
    maximizeHotkeyId = GlobalAddAtom(L"MrGrey_Minimizer_Max_Key");

    hotkeyFilter.onHotkeyPressed = [this](int id) {
        if (id == minimizeHotkeyId) {
            qDebug() << "Minimize hotkey triggered!";
            minimizeProcessWindows();
        } else if (id == maximizeHotkeyId) {
            qDebug() << "Maximize hotkey triggered!";
            maximizeProcessWindows();
        }
    };

    createTrayIcon();
    loadSettings();
}


void MainWindow::closeEvent(QCloseEvent *event) {
    // Default: no override or just call base
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            QTimer::singleShot(0, this, [this]() {
                this->hide();          // Hide the window
                trayIcon->show();      // Show tray icon
            });
        }
    }
    QMainWindow::changeEvent(event);
}

MainWindow::~MainWindow()
{
    qApp->removeNativeEventFilter(&hotkeyFilter);
    GlobalDeleteAtom(minimizeHotkeyId);
    GlobalDeleteAtom(maximizeHotkeyId);
    if (trayIcon) {
        trayIcon->hide(); // Forces Windows to remove the icon immediately
        delete trayIcon;
    }
    delete ui;
}

void MainWindow::registerHotkeys() {
    UnregisterHotKey(nullptr, minimizeHotkeyId);
    UnregisterHotKey(nullptr, maximizeHotkeyId);

    minimizeKey = ui->hotkeyMinimize->keySequence();
    maximizeKey = ui->hotkeyMaximize->keySequence();

    if (minimizeKey == maximizeKey) {
        QMessageBox::warning(this, "Hotkey Conflict", "Minimize and maximize hotkeys must be different.");
        return;
    }

    UINT minMod = 0, minVk = 0, maxMod = 0, maxVk = 0;

    auto extract = [](const QKeySequence &seq, UINT &mod, UINT &vk) {
        mod = 0;
        vk = 0;
        if (seq.isEmpty()) return;

        QKeyCombination combination = seq[0];

        Qt::KeyboardModifiers qMods = combination.keyboardModifiers();
        if (qMods & Qt::ControlModifier) mod |= MOD_CONTROL;
        if (qMods & Qt::AltModifier)     mod |= MOD_ALT;
        if (qMods & Qt::ShiftModifier)   mod |= MOD_SHIFT;
        if (qMods & Qt::MetaModifier)    mod |= MOD_WIN;

        Qt::Key qKey = combination.key();

        vk = qtKeyToWinVK(qKey);
    };

    extract(minimizeKey, minMod, minVk);
    extract(maximizeKey, maxMod, maxVk);

    bool minRegistered = RegisterHotKey(nullptr, minimizeHotkeyId, minMod, minVk);
    if (!minRegistered) {
        QMessageBox::warning(this, "Hotkey Registration Failed",
                             "Minimize hotkey is already in use by another app or invalid.");
        return;
    }


    bool maxRegistered = RegisterHotKey(nullptr, maximizeHotkeyId, maxMod, maxVk);
    if (!maxRegistered) {
        QMessageBox::warning(this, "Hotkey Registration Failed",
                             "Maximize hotkey is already in use or invalid.");
        UnregisterHotKey(nullptr, 1);
        return;
    }
    qDebug() << "Hotkeys registered.";
}

void MainWindow::on_apply_clicked()
{
    registerHotkeys();
    saveSettings();
}

void MainWindow::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icon/web/icon.png")); // Replace with your icon path
    trayIcon->setToolTip("Minimizer is running");

    trayMenu = new QMenu(this);
    trayMenu->addAction("Show", this, [this]() {
        this->showNormal();
        this->activateWindow();
    });
    trayMenu->addAction("Exit", qApp, &QCoreApplication::quit);

    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);

    trayIcon->show();
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        this->showNormal();
        this->activateWindow();
    }
}


bool isTargetProcessWindow(HWND hwnd, const std::vector<DWORD>& targetPIDs) {
    DWORD windowPID = 0;
    GetWindowThreadProcessId(hwnd, &windowPID);
    return std::find(targetPIDs.begin(), targetPIDs.end(), windowPID) != targetPIDs.end()
           && IsWindowVisible(hwnd)
           && (GetWindow(hwnd, GW_OWNER) == nullptr); // Top-level only
}

struct WindowSearchContext {
    std::vector<DWORD> pids;
    std::vector<HWND> hwnds;
};


std::vector<HWND> getProcessWindows(const QStringList& processNames) {
    // 1. Optimization: Use a Set for O(1) lookup
    std::unordered_set<DWORD> targetPids;

    // 2. Optimization: Convert target names to lowercase ONCE, not every loop
    QSet<QString> targetsLower;
    for (const auto& name : processNames) targetsLower.insert(name.toLower());

    // --- Phase 1: Get PIDs (The Snapshot) ---
    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.get() != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot.get(), &pe)) {
            do {
                // Fast comparison using QSet lookup
                QString exeName = QString::fromWCharArray(pe.szExeFile).toLower();
                if (targetsLower.contains(exeName)) {
                    targetPids.insert(pe.th32ProcessID);
                }
            } while (Process32Next(snapshot.get(), &pe));
        }
    }

    // Early exit if no processes found (saves walking windows)
    if (targetPids.empty()) return {};

    // --- Phase 2: Get Windows (The Lightweight Walk) ---
    struct Context {
        const std::unordered_set<DWORD>* pids;
        std::vector<HWND> hwnds;
    };
    Context ctx { &targetPids, {} };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<Context*>(lParam);

        // Filter 1: Visibility (Fastest check)
        if (!IsWindowVisible(hwnd)) return TRUE;

        // Filter 2: PID Check (O(1) Set Lookup)
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        if (c->pids->count(pid)) {
            // Filter 3: Ownership (Prevent minimizing tooltips/popups)
            if (GetWindow(hwnd, GW_OWNER) == nullptr) {
                c->hwnds.push_back(hwnd);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.hwnds;
}


void MainWindow::minimizeProcessWindows() {
    QStringList processNames;
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        processNames << ui->listWidgetProcesses->item(i)->text();
    }

    auto hwnds = getProcessWindows(processNames);

    for (HWND hwnd : hwnds) {
        ShowWindowAsync(hwnd, SW_MINIMIZE);
    }
}

void MainWindow::maximizeProcessWindows() {
    QStringList processNames;
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        processNames << ui->listWidgetProcesses->item(i)->text();
    }

    auto hwnds = getProcessWindows(processNames);

    for (HWND hwnd : hwnds) {
        ShowWindowAsync(hwnd, SW_RESTORE);
    }
}

void MainWindow::loadSettings() {
    QSettings settings("MrGrey", "Minimizer");

    QStringList processList = settings.value("processList").toStringList();
    ui->listWidgetProcesses->clear();
    ui->listWidgetProcesses->addItems(processList);

    ui->hotkeyMinimize->setKeySequence(QKeySequence(settings.value("minHotkey", "Ctrl+G").toString()));
    ui->hotkeyMaximize->setKeySequence(QKeySequence(settings.value("maxHotkey", "Ctrl+H").toString()));
    ui->checkBoxLaunchAtStartup->setChecked(settings.value("launchAtStartup", "false") == "true" ? true : false);

    // Apply immediately
    registerHotkeys();
}

void MainWindow::saveSettings() {
    QSettings settings("MrGrey", "Minimizer");

    QStringList processList;
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        processList << ui->listWidgetProcesses->item(i)->text();
    }
    settings.setValue("processList", processList);
    settings.setValue("minHotkey", ui->hotkeyMinimize->keySequence().toString());
    settings.setValue("maxHotkey", ui->hotkeyMaximize->keySequence().toString());
    settings.setValue("launchAtStartup", ui->checkBoxLaunchAtStartup->isChecked() ? "true" : "false");
}

bool addToStartup() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    std::wstring value = std::wstring(L"\"") + exePath + L"\" --minimized";

    result = RegSetValueExW(hKey, L"MinimizerApp", 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            (value.size() + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    qDebug("Added to startup.");
    return (result == ERROR_SUCCESS);
}

bool removeFromStartup() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteValueW(hKey, L"MinimizerApp");
    RegCloseKey(hKey);
    qDebug("Removed from startup");
    return (result == ERROR_SUCCESS);
}

void MainWindow::on_checkBoxLaunchAtStartup_stateChanged(int arg1)
{
    if (arg1) {
        addToStartup();
    } else
        removeFromStartup();
}


void MainWindow::on_btnRemoveProcess_clicked()
{
    auto items = ui->listWidgetProcesses->selectedItems();
    for (QListWidgetItem* item : items) {
        delete ui->listWidgetProcesses->takeItem(ui->listWidgetProcesses->row(item));
    }
}


void MainWindow::on_btnAddProcess_clicked()
{
    QString processName = ui->lineEditProcess->text().trimmed();
    if (processName.isEmpty()) return;

    // Avoid duplicates
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        if (ui->listWidgetProcesses->item(i)->text().compare(processName, Qt::CaseInsensitive) == 0) {
            return; // Already in list
        }
    }

    ui->listWidgetProcesses->addItem(processName);
    ui->lineEditProcess->clear();
}


void MainWindow::on_btnSelectProcess_clicked()
{
    ProcessPickerDialog dlg(this);
    connect(&dlg, &ProcessPickerDialog::processSelected, this, [&](const QString &procName) {
        ui->lineEditProcess->setText(procName);
    });
    dlg.exec();
    // ProcessPickerDialog dlg(this);
    // if (dlg.exec() == QDialog::Accepted) {
    //     // Add a getter to your dialog: QString selectedProcess() const;
    //     ui->lineEditProcess->setText(dlg.selectedProcess());
    // }
}


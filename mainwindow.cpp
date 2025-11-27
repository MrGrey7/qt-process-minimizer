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
#include "utils.h"
// #pragma comment(lib, "Psapi.lib")

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
    trayIcon->setIcon(QIcon(":/icon/web/icon.png"));
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
    std::unordered_set<DWORD> targetPids;

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
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    HKEY rawKey = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_WRITE, &rawKey);

    if (result != ERROR_SUCCESS) return false;

    // RAII: Key automatically closes when this scope ends (even if RegSetValueExW throws/fails)
    ScopedRegistryKey hKey(rawKey);

    std::wstring value = std::wstring(L"\"") + exePath + L"\" --minimized";

    result = RegSetValueExW(hKey.get(), L"MinimizerApp", 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            (value.size() + 1) * sizeof(wchar_t));

    qDebug("Added to startup.");
    return (result == ERROR_SUCCESS);
}

bool removeFromStartup() {
    HKEY rawKey = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_WRITE, &rawKey);

    if (result != ERROR_SUCCESS) return false;

    // RAII Wrapper
    ScopedRegistryKey hKey(rawKey);

    result = RegDeleteValueW(hKey.get(), L"MinimizerApp");

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
    if (dlg.exec() == QDialog::Accepted) {
        ui->lineEditProcess->setText(dlg.selectedProcess());
    }
}


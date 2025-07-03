#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <Windows.h>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include "hotkeyeventfilter.h"
#include <TlHelp32.h>   // For process snapshot
#include <Psapi.h>      // For GetModuleBaseName
#include <QCloseEvent>
#include <QSettings>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "ProcessPickerDialog.h"
// #pragma comment(lib, "Psapi.lib")


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create and install native hotkey filter
    auto *hotkeyFilter = new HotkeyEventFilter();
    qApp->installNativeEventFilter(hotkeyFilter);

    hotkeyFilter->onHotkeyPressed = [this](int id) {
        if (id == 1) {
            qDebug() << "Minimize hotkey triggered!";
            minimizeProcessWindows();
        } else if (id == 2) {
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
    delete ui;
}

void MainWindow::registerHotkeys() {
    UnregisterHotKey(nullptr, 1);
    UnregisterHotKey(nullptr, 2);

    minimizeKey = ui->hotkeyMinimize->keySequence();
    maximizeKey = ui->hotkeyMaximize->keySequence();

    if (minimizeKey == maximizeKey) {
        QMessageBox::warning(this, "Hotkey Conflict", "Minimize and maximize hotkeys must be different.");
        return;
    }

    UINT minMod, minVk, maxMod, maxVk;
    auto extract = [](const QKeySequence &seq, UINT &mod, UINT &vk) {
        int key = seq[0];
        mod = 0;

        if (key & Qt::CTRL) mod |= MOD_CONTROL;
        if (key & Qt::ALT)  mod |= MOD_ALT;
        if (key & Qt::SHIFT) mod |= MOD_SHIFT;
        if (key & Qt::META) mod |= MOD_WIN;

        vk = key & 0xFFFF; // Strip modifier bits
    };

    extract(minimizeKey, minMod, minVk);
    extract(maximizeKey, maxMod, maxVk);

    bool minRegistered = RegisterHotKey(nullptr, 1, minMod, minVk);
    if (!minRegistered) {
        QMessageBox::warning(this, "Hotkey Registration Failed",
                             "Minimize hotkey is already in use by another app or invalid.");
        return;
    }

    bool maxRegistered = RegisterHotKey(nullptr, 2, maxMod, maxVk);
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
    WindowSearchContext context;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return {};

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe)) {
        do {
            QString exe = QString::fromWCharArray(pe.szExeFile);
            for (const QString& targetName : processNames) {
                if (exe.compare(targetName, Qt::CaseInsensitive) == 0) {
                    context.pids.push_back(pe.th32ProcessID);
                    break;
                }
            }
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);

    // Now find windows that belong to those PIDs
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* ctx = reinterpret_cast<WindowSearchContext*>(lParam);
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        if (std::find(ctx->pids.begin(), ctx->pids.end(), pid) != ctx->pids.end()) {
            if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
                ctx->hwnds.push_back(hwnd);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    qDebug() << "Found windows:" << context.hwnds.size();

    return context.hwnds;
}


void MainWindow::minimizeProcessWindows() {
    QStringList processNames;
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        processNames << ui->listWidgetProcesses->item(i)->text();
    }

    auto hwnds = getProcessWindows(processNames);

    for (HWND hwnd : hwnds) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
}

void MainWindow::maximizeProcessWindows() {
    QStringList processNames;
    for (int i = 0; i < ui->listWidgetProcesses->count(); ++i) {
        processNames << ui->listWidgetProcesses->item(i)->text();
    }

    auto hwnds = getProcessWindows(processNames);

    for (HWND hwnd : hwnds) {
        ShowWindow(hwnd, SW_RESTORE);
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
}


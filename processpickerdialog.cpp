#include "processpickerdialog.h"
#include "ui_processpickerdialog.h"

#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <QIcon>
#include <QPixmap>
#include <QMessageBox>
#include <commctrl.h>

// #pragma comment(lib, "Shlwapi.lib")
// #pragma comment(lib, "Shell32.lib")
// #pragma comment(lib, "comctl32.lib")

struct HandleDeleter {
    void operator()(HANDLE h) const {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
};
using ScopedHandle = std::unique_ptr<void, HandleDeleter>;


ProcessPickerDialog::ProcessPickerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProcessPickerDialog),
    model(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->listView->setModel(model);
    populateProcessList();
}

ProcessPickerDialog::~ProcessPickerDialog() {
    delete ui;
}

void ProcessPickerDialog::populateProcessList() {
    model->clear();
    model->setHorizontalHeaderLabels({"Process Name"});

    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    // WinAPI Quirk: Snapshot failure is -1 (INVALID_HANDLE_VALUE), not 0 (NULL)
    if (snapshot.get() == INVALID_HANDLE_VALUE) {
        QMessageBox::warning(this, "Error", "Failed to get process snapshot");
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    struct ProcessItem {
        QString name;
        QIcon icon;
    };
    QSet<QString> seen;
    QVector<ProcessItem> processItems;

    if (Process32First(snapshot.get(), &pe)) {
        do {
            QString exeName = QString::fromWCharArray(pe.szExeFile, -1);
            if (seen.contains(exeName))
                continue;
            seen.insert(exeName);
            // Get icon from executable path (if possible)
            WCHAR exePath[MAX_PATH] = {0};
            ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
            if (hProcess) {
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageName(hProcess.get(), 0, exePath, &size)) {
                    SHFILEINFO shfi;
                    SHGetFileInfo(exePath, 0, &shfi, sizeof(shfi),
                                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SYSICONINDEX);
                    QIcon icon = QIcon(QPixmap::fromImage(QImage::fromHICON(shfi.hIcon)));
                    if (shfi.hIcon) DestroyIcon(shfi.hIcon);

                    ProcessItem item;
                    item.icon = icon;
                    item.name = exeName;

                    processItems.append(item);
                    // QStandardItem* item = new QStandardItem(icon, exeName);
                    // model->appendRow(item);
                }
            } else {
                // fallback without icon
                // QStandardItem* item = new QStandardItem(exeName);
                // model->appendRow(item);
                ProcessItem item;
                item.name = exeName;

                processItems.append(item);
            }
        } while (Process32Next(snapshot.get(), &pe));
    }

    std::sort(processItems.begin(), processItems.end(), [](const ProcessItem &a, const ProcessItem &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    for (const ProcessItem &proc : processItems) {
        QStandardItem *item = new QStandardItem(proc.icon, proc.name);
        model->appendRow(item);
    }
}

QString ProcessPickerDialog::getProcessNameAt(int row) const {
    auto item = model->item(row);
    return item ? item->text() : QString();
}

void ProcessPickerDialog::on_listView_doubleClicked(const QModelIndex &index) {
    QString procName = getProcessNameAt(index.row());
    if (!procName.isEmpty()) {
        emit processSelected(procName);
        accept();
    }
}

void ProcessPickerDialog::on_btnOk_clicked() {
    QModelIndex index = ui->listView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "No selection", "Please select a process");
        return;
    }
    QString procName = getProcessNameAt(index.row());
    if (!procName.isEmpty()) {
        emit processSelected(procName);
        accept();
    }
}

void ProcessPickerDialog::on_btnCancel_clicked()
{
    reject();
}


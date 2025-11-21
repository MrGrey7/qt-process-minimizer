#include "processpickerdialog.h"
#include "ui_processpickerdialog.h"
#include "win32utils.h"

#include <tlhelp32.h>
#include <shellapi.h>
#include <QIcon>
#include <QPixmap>
#include <QMessageBox>

ProcessPickerDialog::ProcessPickerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProcessPickerDialog),
    model(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->listView->setModel(model);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Good UX practice
    populateProcessList();
}

ProcessPickerDialog::~ProcessPickerDialog() {
    delete ui;
}

QString ProcessPickerDialog::selectedProcess() const
{
    return m_selectedProcess;
}

void ProcessPickerDialog::populateProcessList() {
    model->clear();

    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
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
            // Qt 6: QString::fromWCharArray takes size, -1 means null-terminated
            QString exeName = QString::fromWCharArray(pe.szExeFile, -1);

            if (seen.contains(exeName)) continue;
            seen.insert(exeName);

            WCHAR exePath[MAX_PATH] = {0};
            ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));

            ProcessItem item;
            item.name = exeName;

            if (hProcess) {
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageName(hProcess.get(), 0, exePath, &size)) {
                    SHFILEINFO shfi = {0};
                    SHGetFileInfo(exePath, 0, &shfi, sizeof(shfi),
                                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

                    if (shfi.hIcon) {
                        item.icon = QIcon(QPixmap::fromImage(QImage::fromHICON(shfi.hIcon)));
                        DestroyIcon(shfi.hIcon);
                    }
                }
            }
            processItems.append(item);

        } while (Process32Next(snapshot.get(), &pe));
    }

    // Sort alphabetically
    std::sort(processItems.begin(), processItems.end(), [](const ProcessItem &a, const ProcessItem &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    for (const ProcessItem &proc : processItems) {
        auto *item = new QStandardItem(proc.icon, proc.name);
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
        m_selectedProcess = procName;
        accept(); // Closes dialog with QDialog::Accepted
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
        m_selectedProcess = procName;
        accept();
    }
}

void ProcessPickerDialog::on_btnCancel_clicked()
{
    reject(); // Closes dialog with QDialog::Rejected
}

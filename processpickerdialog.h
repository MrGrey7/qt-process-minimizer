#ifndef PROCESSPICKERDIALOG_H
#define PROCESSPICKERDIALOG_H

#pragma once

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
class ProcessPickerDialog;
}

class ProcessPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProcessPickerDialog(QWidget *parent = nullptr);
    ~ProcessPickerDialog();

    // The "Pull" accessor
    QString selectedProcess() const;

private slots:
    void on_listView_doubleClicked(const QModelIndex &index);
    void on_btnOk_clicked();
    void on_btnCancel_clicked();

private:
    Ui::ProcessPickerDialog *ui;
    QStandardItemModel* model;

    // Internal state to store selection
    QString m_selectedProcess;

    void populateProcessList();
    QString getProcessNameAt(int row) const;
};

#endif // PROCESSPICKERDIALOG_H

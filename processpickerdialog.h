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

signals:
    void processSelected(const QString &processName);

private slots:
    void on_listView_doubleClicked(const QModelIndex &index);
    void on_btnOk_clicked();

    void on_btnCancel_clicked();

private:
    Ui::ProcessPickerDialog *ui;
    QStandardItemModel* model;

    void populateProcessList();
    QString getProcessNameAt(int row) const;
};


#endif // PROCESSPICKERDIALOG_H

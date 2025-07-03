#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
// #include <QHotkey>  // Qt doesn't support global hotkeys by default, we’ll fix that soon

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void on_apply_clicked();
    void on_checkBoxLaunchAtStartup_stateChanged(int arg1);
    void on_btnRemoveProcess_clicked();
    void on_btnAddProcess_clicked();
    void on_btnSelectProcess_clicked();

private:
    Ui::MainWindow *ui;
    QSystemTrayIcon *trayIcon;
    QMenu* trayMenu;
    QString targetProcess;
    QKeySequence minimizeKey;
    QKeySequence maximizeKey;

    void createTrayIcon();
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent* event) override;
    void registerHotkeys();
    void minimizeProcessWindows();
    void maximizeProcessWindows();
    void loadSettings();
    void saveSettings();
};

#endif // MAINWINDOW_H

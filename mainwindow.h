#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void on_pushButtonAddTopic_clicked();
    void on_pushButtonAddTask_clicked();
    void on_pushButtonDeleteTask_clicked();
    void on_pushButtonDeleteTopic_clicked();
    void on_pushButtonEditTask_clicked();
    void on_pushButtonEditTopic_clicked();
    void onTaskSelected();
    void onTaskItemChanged(QListWidgetItem *item);
    void on_actionExit_triggered();
private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    QSystemTrayIcon* trayIcon;
    QTimer* notificationTimer;
    void setupDatabase();
    void loadTopics();
    void loadTasks(const QString &topic);
    void setupNotifications();
    void checkDueTasks();
    void showNotification(const QString &title, const QString &message);
};

#endif // MAINWINDOW_H

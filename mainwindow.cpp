#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QInputDialog>
#include "taskdialog.h"
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupDatabase();
    connect(ui->listWidgetTopic, &QListWidget::currentTextChanged, this, &MainWindow::loadTasks);
    connect(ui->listWidgetTask, &QListWidget::currentItemChanged, this, &MainWindow::onTaskSelected);
    connect(ui->listWidgetTask, &QListWidget::itemChanged, this, &MainWindow::onTaskItemChanged);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::on_actionExit_triggered);
    loadTopics();
    trayIcon = new QSystemTrayIcon(this);
    QIcon icon = style()->standardIcon(QStyle::SP_ComputerIcon);
    if (icon.isNull()) {
        qDebug() << "Failed to load built-in icon.";
        icon = QIcon::fromTheme("dialog-information");
        if (icon.isNull()) {
            qDebug() << "Failed to load fallback icon.";
            icon = QIcon();
        }
    } else {
        qDebug() << "Built-in icon loaded successfully.";
    }
    trayIcon->setIcon(icon);
    if (!icon.isNull()) {
        trayIcon->show();
    } else {
        qDebug() << "Tray icon is null. setVisible will not display the tray icon.";
    }
    notificationTimer = new QTimer(this);
    connect(notificationTimer, &QTimer::timeout, this, &MainWindow::checkDueTasks);
    notificationTimer->start(30000);
    QTimer::singleShot(5000, this, &MainWindow::checkDueTasks);
}

MainWindow::~MainWindow()
{
    db.close();
    delete ui;
}

void MainWindow::setupDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("tasks.db");
    if (!db.open()) {
        QMessageBox::critical(this, "Error", "Database failed to open: " + db.lastError().text());
        return;
    }
    QSqlQuery query;
    query.exec("PRAGMA foreign_keys = ON;");
    query.exec("CREATE TABLE IF NOT EXISTS topics (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)");
    query.exec("CREATE TABLE IF NOT EXISTS tasks ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "topic_id INTEGER, "
               "name TEXT, "
               "description TEXT, "
               "due_date TEXT, "
               "notify INTEGER, "
               "notified INTEGER DEFAULT 0, "
               "done INTEGER DEFAULT 0, "
               "FOREIGN KEY(topic_id) REFERENCES topics(id) ON DELETE CASCADE)");
    QSqlQuery checkDoneQuery("PRAGMA table_info(tasks)");
    bool hasDone = false;
    while (checkDoneQuery.next()) {
        if (checkDoneQuery.value(1).toString() == "done") {
            hasDone = true;
            break;
        }
    }
    if (!hasDone) {
        QSqlQuery alterDoneQuery("ALTER TABLE tasks ADD COLUMN done INTEGER DEFAULT 0");
        if (!alterDoneQuery.exec()) {
            qDebug() << "Failed to add done column:" << alterDoneQuery.lastError().text();
        }
    }
    QSqlQuery checkDescriptionQuery("PRAGMA table_info(tasks)");
    bool hasDescription = false;
    while (checkDescriptionQuery.next()) {
        if (checkDescriptionQuery.value(1).toString() == "description") {
            hasDescription = true;
            break;
        }
    }
    if (!hasDescription) {
        QSqlQuery alterDescriptionQuery("ALTER TABLE tasks ADD COLUMN description TEXT");
        if (!alterDescriptionQuery.exec()) {
            qDebug() << "Failed to add description column:" << alterDescriptionQuery.lastError().text();
        }
    }
}

void MainWindow::loadTopics()
{
    QString currentTopic = ui->listWidgetTopic->currentItem()
    ? ui->listWidgetTopic->currentItem()->text()
    : QString();
    ui->listWidgetTopic->clear();
    QSqlQuery query("SELECT name FROM topics");
    if (query.exec()) {
        while (query.next()) {
            ui->listWidgetTopic->addItem(query.value(0).toString());
        }
    }
    if (!currentTopic.isEmpty()) {
        QList<QListWidgetItem*> items = ui->listWidgetTopic->findItems(currentTopic, Qt::MatchExactly);
        if (!items.isEmpty()) {
            ui->listWidgetTopic->setCurrentItem(items.first());
        }
    }
    if (ui->listWidgetTopic->count() > 0 && !ui->listWidgetTopic->currentItem()) {
        ui->listWidgetTopic->setCurrentRow(0);
    }
}

void MainWindow::loadTasks(const QString &topic)
{
    ui->listWidgetTask->blockSignals(true);
    ui->listWidgetTask->clear();
    ui->textEditDescriptionDisplay->clear();
    if (topic.isEmpty()) {
        ui->listWidgetTask->blockSignals(false);
        return;
    }
    QSqlQuery query;
    query.prepare("SELECT t.id, t.name, t.done FROM tasks t JOIN topics tp ON t.topic_id = tp.id WHERE tp.name = :topic");
    query.bindValue(":topic", topic);
    if (query.exec()) {
        while (query.next()) {
            QString taskName = query.value(1).toString();
            bool done = query.value(2).toInt() == 1;
            QListWidgetItem *item = new QListWidgetItem(taskName);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(done ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, query.value(0).toInt());
            ui->listWidgetTask->addItem(item);
        }
    }
    ui->listWidgetTask->blockSignals(false);
    if (ui->listWidgetTask->count() > 0) {
        ui->listWidgetTask->setCurrentRow(0);
    }
}

void MainWindow::on_pushButtonAddTopic_clicked()
{
    QString topic = QInputDialog::getText(this, "New Topic", "Enter topic name:");
    if (topic.isEmpty()) return;
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO topics (name) VALUES (:name)");
    query.bindValue(":name", topic);
    if (query.exec()) {
        loadTopics();
        if (ui->listWidgetTopic->count() > 0 && !ui->listWidgetTopic->currentItem()) {
            ui->listWidgetTopic->setCurrentRow(0);
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to add topic: " + query.lastError().text());
    }
}

void MainWindow::on_pushButtonDeleteTopic_clicked()
{
    QListWidgetItem *currentTopic = ui->listWidgetTopic->currentItem();
    if (!currentTopic) {
        QMessageBox::warning(this, "Warning", "Select a topic to delete");
        return;
    }
    QString topicName = currentTopic->text();
    QMessageBox::StandardButton confirm = QMessageBox::question(this, "Confirm Delete",
                                                                "Delete topic '" + topicName + "' and all its tasks?",
                                                                QMessageBox::Yes | QMessageBox::No);
    if (confirm == QMessageBox::Yes) {
        QSqlDatabase::database().transaction();
        QSqlQuery deleteTasksQuery;
        deleteTasksQuery.prepare("DELETE FROM tasks WHERE topic_id = (SELECT id FROM topics WHERE name = :topic)");
        deleteTasksQuery.bindValue(":topic", topicName);
        QSqlQuery deleteTopicQuery;
        deleteTopicQuery.prepare("DELETE FROM topics WHERE name = :topic");
        deleteTopicQuery.bindValue(":topic", topicName);
        if (deleteTasksQuery.exec() && deleteTopicQuery.exec()) {
            QSqlDatabase::database().commit();
            loadTopics();
            if (ui->listWidgetTopic->count() > 0) {
                ui->listWidgetTopic->setCurrentRow(0);
            } else {
                ui->listWidgetTask->clear();
            }
        } else {
            QSqlDatabase::database().rollback();
            QMessageBox::critical(this, "Error", "Deletion failed: " + deleteTopicQuery.lastError().text());
        }
    }
}

void MainWindow::on_pushButtonAddTask_clicked()
{
    QStringList topics;
    for (int i = 0; i < ui->listWidgetTopic->count(); ++i) {
        topics << ui->listWidgetTopic->item(i)->text();
    }
    if (topics.isEmpty()) {
        QSqlQuery insertTopicQuery;
        insertTopicQuery.prepare("INSERT INTO topics (name) VALUES (:name)");
        insertTopicQuery.bindValue(":name", "Default");
        if (!insertTopicQuery.exec()) {
            QMessageBox::warning(this, "Error", "Failed to add default topic: " + insertTopicQuery.lastError().text());
            return;
        }
        topics << "Default";
        loadTopics();
    }
    TaskDialog dialog(topics, this);
    if (dialog.exec() != QDialog::Accepted) return;
    QSqlQuery topicQuery;
    topicQuery.prepare("SELECT id FROM topics WHERE name = :name");
    topicQuery.bindValue(":name", dialog.topic());
    if (!topicQuery.exec() || !topicQuery.next()) {
        QMessageBox::warning(this, "Error", "Selected topic not found in database.");
        return;
    }
    int topicId = topicQuery.value(0).toInt();
    QString description = dialog.description();
    QSqlQuery query;
    query.prepare("INSERT INTO tasks (topic_id, name, description, due_date, notify, notified, done) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(topicId);
    query.addBindValue(dialog.taskName());
    query.addBindValue(description);
    query.addBindValue(dialog.dueDate().toString(Qt::ISODate));
    query.addBindValue(dialog.notifyEnabled() ? 1 : 0);
    query.addBindValue(0);
    query.addBindValue(0);
    if (query.exec()) {
        loadTasks(dialog.topic());
    } else {
        QMessageBox::warning(this, "Error", "Failed to add task: " + query.lastError().text());
    }
}

void MainWindow::on_pushButtonEditTask_clicked()
{
    QListWidgetItem *currentTask = ui->listWidgetTask->currentItem();
    QListWidgetItem *currentTopic = ui->listWidgetTopic->currentItem();
    if (!currentTask || !currentTopic) {
        QMessageBox::warning(this, "Warning", "Select a task to edit");
        return;
    }
    QString oldTaskName = currentTask->text();
    QString oldTopicName = currentTopic->text();
    QSqlQuery query;
    query.prepare("SELECT t.name, t.description, t.due_date, t.notify, t.done FROM tasks t "
                  "JOIN topics tp ON t.topic_id = tp.id "
                  "WHERE t.id = :task_id");
    int taskId = currentTask->data(Qt::UserRole).toInt();
    query.bindValue(":task_id", taskId);
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "Error", "Failed to load task details");
        return;
    }
    QStringList topics;
    for (int i = 0; i < ui->listWidgetTopic->count(); ++i)
        topics << ui->listWidgetTopic->item(i)->text();
    TaskDialog dialog(topics, this);
    dialog.setTaskName(query.value(0).toString());
    dialog.setDescription(query.value(1).toString());
    dialog.setDueDate(QDateTime::fromString(query.value(2).toString(), Qt::ISODate));
    dialog.setNotifyEnabled(query.value(3).toBool());
    dialog.setTopic(oldTopicName);
    if (dialog.exec() != QDialog::Accepted) return;
    QString newName = dialog.taskName();
    QString newDescription = dialog.description();
    QString newTopic = dialog.topic();
    QDateTime newDueDate = dialog.dueDate();
    bool newNotify = dialog.notifyEnabled();
    QSqlQuery topicQuery;
    topicQuery.prepare("SELECT id FROM topics WHERE name = :topic");
    topicQuery.bindValue(":topic", newTopic);
    if (!topicQuery.exec() || !topicQuery.next()) {
        QMessageBox::warning(this, "Error", "Invalid topic selected");
        return;
    }
    int newTopicId = topicQuery.value(0).toInt();
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE tasks SET "
                        "name = :name, "
                        "topic_id = :topic_id, "
                        "description = :description, "
                        "due_date = :due_date, "
                        "notify = :notify "
                        "WHERE id = :task_id");
    updateQuery.bindValue(":name", newName);
    updateQuery.bindValue(":topic_id", newTopicId);
    updateQuery.bindValue(":description", newDescription);
    updateQuery.bindValue(":due_date", newDueDate.toString(Qt::ISODate));
    updateQuery.bindValue(":notify", newNotify ? 1 : 0);
    updateQuery.bindValue(":task_id", taskId);
    if (updateQuery.exec()) {
        if (newTopic == oldTopicName) {
            loadTasks(oldTopicName);
        } else {
            loadTopics();
            loadTasks(newTopic);
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to update task: " + updateQuery.lastError().text());
    }
}

void MainWindow::on_pushButtonDeleteTask_clicked()
{
    QListWidgetItem *currentTask = ui->listWidgetTask->currentItem();
    QListWidgetItem *currentTopic = ui->listWidgetTopic->currentItem();
    if (!currentTask || !currentTopic) return;
    QString taskName = currentTask->text();
    QString topicName = currentTopic->text();
    QMessageBox::StandardButton confirm = QMessageBox::question(this, "Confirm Delete",
                                                                "Delete task '" + taskName + "' from topic '" + topicName + "'?",
                                                                QMessageBox::Yes | QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;
    QSqlQuery query;
    query.prepare("DELETE FROM tasks WHERE id = :task_id");
    int taskId = currentTask->data(Qt::UserRole).toInt();
    query.bindValue(":task_id", taskId);
    if (query.exec()) {
        loadTasks(topicName);
        if (ui->listWidgetTask->count() > 0) {
            ui->listWidgetTask->setCurrentRow(0);
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to delete task: " + query.lastError().text());
    }
}

void MainWindow::on_pushButtonEditTopic_clicked()
{
    QListWidgetItem *currentItem = ui->listWidgetTopic->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "Warning", "Select a topic to edit");
        return;
    }
    QString currentName = currentItem->text();
    bool ok;
    QString newName = QInputDialog::getText(this, "Edit Topic", "Enter new topic name:", QLineEdit::Normal, currentName, &ok);
    if (!ok || newName.isEmpty() || newName == currentName) {
        return;
    }
    QSqlQuery query;
    query.prepare("UPDATE topics SET name = :newName WHERE name = :currentName");
    query.bindValue(":newName", newName);
    query.bindValue(":currentName", currentName);
    if (query.exec()) {
        loadTopics();
        QList<QListWidgetItem*> items = ui->listWidgetTopic->findItems(newName, Qt::MatchExactly);
        if (!items.isEmpty()) {
            ui->listWidgetTopic->setCurrentItem(items.first());
        }
    } else {
        if (query.lastError().text().contains("UNIQUE constraint failed")) {
            QMessageBox::warning(this, "Error", "Topic name already exists.");
        } else {
            QMessageBox::warning(this, "Error", "Failed to update topic: " + query.lastError().text());
        }
    }
}

void MainWindow::onTaskSelected()
{
    QListWidgetItem *currentTask = ui->listWidgetTask->currentItem();
    QListWidgetItem *currentTopic = ui->listWidgetTopic->currentItem();
    if (!currentTask || !currentTopic) {
        ui->textEditDescriptionDisplay->clear();
        return;
    }
    QString taskName = currentTask->text();
    QString topicName = currentTopic->text();
    int taskId = currentTask->data(Qt::UserRole).toInt();
    QSqlQuery query;
    query.prepare("SELECT t.description FROM tasks t "
                  "JOIN topics tp ON t.topic_id = tp.id "
                  "WHERE t.id = :task_id AND tp.name = :topic");
    query.bindValue(":task_id", taskId);
    query.bindValue(":topic", topicName);
    if (query.exec() && query.next()) {
        ui->textEditDescriptionDisplay->setPlainText(query.value(0).toString());
    } else {
        ui->textEditDescriptionDisplay->clear();
    }
}

void MainWindow::onTaskItemChanged(QListWidgetItem *item)
{
    if (!item) return;
    Qt::CheckState state = item->checkState();
    int done = (state == Qt::Checked) ? 1 : 0;
    int taskId = item->data(Qt::UserRole).toInt();
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE tasks SET done = :done WHERE id = :task_id");
    updateQuery.bindValue(":done", done);
    updateQuery.bindValue(":task_id", taskId);
    if (!updateQuery.exec()) {
        QMessageBox::warning(this, "Error", "Failed to update task status: " + updateQuery.lastError().text());
    }
}

void MainWindow::checkDueTasks()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QSqlQuery query;
    query.prepare("SELECT t.id, t.name, tp.name, t.due_date FROM tasks t "
                  "JOIN topics tp ON t.topic_id = tp.id "
                  "WHERE t.notify = 1 AND t.notified = 0 AND datetime(t.due_date) <= datetime(?)");
    query.bindValue(0, currentTime.toString(Qt::ISODate));
    if (query.exec()) {
        while (query.next()) {
            int taskId = query.value(0).toInt();
            QString taskName = query.value(1).toString();
            QString topicName = query.value(2).toString();
            QDateTime dueDate = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
            showNotification(
                tr("Task Due: %1").arg(taskName),
                tr("Task '%1' in topic '%2' is due!\nDue time: %3")
                    .arg(taskName)
                    .arg(topicName)
                    .arg(dueDate.toString("dd.MM.yyyy HH:mm"))
                );
            QSqlQuery updateQuery;
            updateQuery.prepare("UPDATE tasks SET notified = 1 WHERE id = :task_id");
            updateQuery.bindValue(":task_id", taskId);
            updateQuery.exec();
        }
    }
}

void MainWindow::showNotification(const QString &title, const QString &message)
{
    if (QSystemTrayIcon::supportsMessages()) {
        trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 10000);
    }
    else {
        QMessageBox::information(this, title, message);
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

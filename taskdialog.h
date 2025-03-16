#ifndef TASKDIALOG_H
#define TASKDIALOG_H
#include <QDialog>
namespace Ui { class TaskDialog; }
class TaskDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskDialog(const QStringList &topics, QWidget *parent = nullptr);
    ~TaskDialog();
    QString taskName() const;
    QString topic() const;
    QDateTime dueDate() const;
    bool notifyEnabled() const;
    QString description() const;
    void setTaskName(const QString &name);
    void setTopic(const QString &topic);
    void setDueDate(const QDateTime &dateTime);
    void setNotifyEnabled(bool enabled);
    void setDescription(const QString &desc);
private:
    Ui::TaskDialog *ui;
};
#endif

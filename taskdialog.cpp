#include "taskdialog.h"
#include "ui_taskdialog.h"

TaskDialog::TaskDialog(const QStringList &topics, QWidget *parent)
    : QDialog(parent), ui(new Ui::TaskDialog)
{
    ui->setupUi(this);
    ui->comboBoxTopic->addItems(topics);
    ui->dateTimeEditDue->setDateTime(QDateTime::currentDateTime());
}

TaskDialog::~TaskDialog()
{
    delete ui;
}

QString TaskDialog::taskName() const { return ui->lineEditTaskName->text(); }

QString TaskDialog::topic() const { return ui->comboBoxTopic->currentText(); }

QDateTime TaskDialog::dueDate() const { return ui->dateTimeEditDue->dateTime(); }

bool TaskDialog::notifyEnabled() const { return ui->checkBoxNotify->isChecked(); }

QString TaskDialog::description() const { return ui->textEditDescription->toPlainText(); }

void TaskDialog::setTaskName(const QString &name) { ui->lineEditTaskName->setText(name); }

void TaskDialog::setTopic(const QString &topic) {
    int index = ui->comboBoxTopic->findText(topic);
    if (index >= 0) ui->comboBoxTopic->setCurrentIndex(index);
}

void TaskDialog::setDueDate(const QDateTime &dateTime) { ui->dateTimeEditDue->setDateTime(dateTime); }

void TaskDialog::setNotifyEnabled(bool enabled) { ui->checkBoxNotify->setChecked(enabled); }

void TaskDialog::setDescription(const QString &desc) { ui->textEditDescription->setPlainText(desc); }

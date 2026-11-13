// SPDX-License-Identifier: GPL-2.0-or-later
#include "externaleditor.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>

namespace
{
    constexpr auto programKey = "editor/program";
    constexpr auto argumentsKey = "editor/arguments";

    QString launch(const QString& path, const QString& program, QStringList arguments)
    {
        const QFileInfo file(path);
        if (!file.isFile())
            return QObject::tr("The file is missing or is not a regular file.");

        const auto executable = QDir::isAbsolutePath(program) ? program : QStandardPaths::findExecutable(program);
        if (executable.isEmpty())
            return QObject::tr("Editor executable not found: %1").arg(program);

        arguments.append(path);
        QProcess process;
        process.setProgram(executable);
        process.setArguments(arguments);
        process.setWorkingDirectory(file.absolutePath());
        process.setStandardInputFile(QProcess::nullDevice());
        process.setStandardOutputFile(QProcess::nullDevice());
        process.setStandardErrorFile(QProcess::nullDevice());
        if (!process.startDetached())
            return QObject::tr("Could not start %1. %2").arg(executable, process.errorString());
        return {};
    }
} // namespace

bool ExternalEditor::configure(QSettings& settings, QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setObjectName("editorSettings");
    dialog.setWindowTitle(QObject::tr("External editor"));
    auto* layout = new QFormLayout(&dialog);
    auto* program = new QLineEdit(settings.value(programKey).toString(), &dialog);
    program->setObjectName("editorProgram");
    program->setPlaceholderText(QObject::tr("Executable name or absolute path"));
    auto* browse = new QPushButton(QObject::tr("Browse…"), &dialog);
    auto* programRow = new QHBoxLayout;
    programRow->addWidget(program);
    programRow->addWidget(browse);
    layout->addRow(QObject::tr("&Program"), programRow);
    auto* arguments = new QLineEdit(settings.value(argumentsKey).toString(), &dialog);
    arguments->setObjectName("editorArguments");
    layout->addRow(QObject::tr("&Arguments"), arguments);
    auto* help = new QLabel(QObject::tr("Use double quotes around arguments that contain spaces. The file path is added as the last argument."), &dialog);
    help->setWordWrap(true);
    layout->addRow(help);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    auto* save = buttons->button(QDialogButtonBox::Save);
    const auto updateSave = [&]
    {
        const auto value = program->text().trimmed();
        save->setEnabled(!value.isEmpty() && (!value.contains('/') || QDir::isAbsolutePath(value)));
    };
    updateSave();
    QObject::connect(program, &QLineEdit::textChanged, &dialog, updateSave);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&]
                     {
        const auto path = QFileDialog::getOpenFileName(&dialog, QObject::tr("Choose editor executable"), "/usr/bin");
        if (!path.isEmpty())
            program->setText(path); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(460, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted || !save->isEnabled())
        return false;
    settings.setValue(programKey, program->text().trimmed());
    settings.setValue(argumentsKey, arguments->text());
    return true;
}

void ExternalEditor::open(const QString& path, QSettings& settings, QWidget* parent)
{
    if (settings.value(programKey).toString().isEmpty() && !configure(settings, parent))
        return;

    const auto program = settings.value(programKey).toString();
    const auto arguments = QProcess::splitCommand(settings.value(argumentsKey).toString());
    auto* watcher = new QFutureWatcher<QString>(parent);
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, parent, [watcher, parent, path]
                     {
        const auto error = watcher->result();
        watcher->deleteLater();
        if (error.isEmpty())
            return;
        auto* message = new QMessageBox(QMessageBox::Warning, QObject::tr("Could not open editor"), QString(), QMessageBox::Ok, parent);
        message->setObjectName("editorError");
        message->setTextFormat(Qt::PlainText);
        message->setText(path + "\n\n" + error);
        message->setAttribute(Qt::WA_DeleteOnClose);
        message->open(); });
    watcher->setFuture(QtConcurrent::run([path, program, arguments]
                                         { return launch(path, program, arguments); }));
}

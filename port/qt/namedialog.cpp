// SPDX-License-Identifier: GPL-2.0-or-later
#include "namedialog.h"
#include "fileoperations.h"
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
NameDialog::NameDialog(const QString& title, const QString& description, const QString& initialName, QWidget* parent) : QDialog(parent)
{
    setObjectName("operationNameDialog");
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel(description, this);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    layout->addWidget(label);
    input_ = new QLineEdit(initialName, this);
    input_->setObjectName("operationName");
    input_->setAccessibleName(tr("Name"));
    layout->addWidget(input_);
    error_ = new QLabel(this);
    error_->setObjectName("nameValidationError");
    error_->setTextFormat(Qt::PlainText);
    error_->setWordWrap(true);
    layout->addWidget(error_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &NameDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &NameDialog::reject);
    input_->setFocus();
    input_->selectAll();
    const QFileInfo info(initialName);
    if (!info.suffix().isEmpty() && !info.completeBaseName().isEmpty())
        input_->setSelection(0, info.completeBaseName().size());
    resize(480, sizeHint().height());
}
QString NameDialog::name() const { return input_->text(); }
void NameDialog::accept()
{
    const auto error = FileOperations::validateName(name());
    if (!error.isEmpty())
    {
        error_->setText(error);
        input_->setFocus();
        return;
    }
    QDialog::accept();
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "copydialogs.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
CopyDialog::CopyDialog(const QString& source, const QStringList& names, const QString& destination, QWidget* parent) : QDialog(parent)
{
    setObjectName("copyDialog");
    setWindowTitle(tr("Copy"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel(tr("Items to copy: %1\nSource directory:\n%2\n\n%3\n\nDestination directory:").arg(names.size()).arg(source, (names.mid(0, 8).join("\n") + (names.size() > 8 ? tr("\n…and %1 more").arg(names.size() - 8) : QString()))), this);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    layout->addWidget(label);
    destination_ = new QLineEdit(destination, this);
    destination_->setObjectName("copyDestination");
    layout->addWidget(destination_);
    error_ = new QLabel(this);
    error_->setTextFormat(Qt::PlainText);
    layout->addWidget(error_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Copy"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &CopyDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CopyDialog::reject);
    destination_->setFocus();
    destination_->selectAll();
    resize(560, sizeHint().height());
}
QString CopyDialog::destination() const { return QDir::cleanPath(destination_->text()); }
void CopyDialog::accept()
{
    if (destination_->text().isEmpty() || !QDir::isAbsolutePath(destination_->text()) || destination_->text().contains(QChar::Null))
    {
        error_->setText(tr("Enter an absolute destination directory."));
        return;
    }
    QDialog::accept();
}
CopyProgressDialog::CopyProgressDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName("copyProgressDialog");
    setWindowTitle(tr("Copying"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(this);
    label_ = new QLabel(tr("Scanning source items…"), this);
    label_->setTextFormat(Qt::PlainText);
    label_->setWordWrap(true);
    layout->addWidget(label_);
    bar_ = new QProgressBar(this);
    bar_->setRange(0, 0);
    layout->addWidget(bar_);
    cancel_ = new QPushButton(tr("Cancel"), this);
    cancel_->setObjectName("cancelCopy");
    layout->addWidget(cancel_);
    connect(cancel_, &QPushButton::clicked, this, &CopyProgressDialog::reject);
    resize(560, 170);
}
void CopyProgressDialog::updateProgress(const CopyOperations::Progress& progress)
{
    if (canceled_)
        return;
    label_->setText((progress.scanning ? tr("Scanning source items…") : tr("Processed %1 of %2\nCompleted: %3; skipped: %4; selected: %5").arg(QLocale().formattedDataSize(progress.bytesDone), QLocale().formattedDataSize(progress.bytesTotal)).arg(progress.completed).arg(progress.skipped).arg(progress.total)) + "\n" + progress.current);
    if (!progress.scanning)
    {
        bar_->setRange(0, 1000);
        const auto fraction = progress.bytesTotal > 0 ? static_cast<long double>(progress.bytesDone) / progress.bytesTotal : static_cast<long double>(progress.completed + progress.skipped) / qMax(1, progress.total);
        bar_->setValue(int(qBound<long double>(0, fraction, 1) * 1000));
    }
}
void CopyProgressDialog::reject()
{
    if (canceled_)
        return;
    canceled_ = true;
    cancel_->setEnabled(false);
    label_->setText(tr("Cancelling and removing the unfinished copy…"));
    emit cancelRequested();
}
void CopyProgressDialog::complete() { QDialog::accept(); }

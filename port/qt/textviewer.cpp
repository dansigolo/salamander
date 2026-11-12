// SPDX-License-Identifier: GPL-2.0-or-later
#include "textviewer.h"
#include "textpreview.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>
#include <QtConcurrent>

TextViewer::TextViewer(const QString& path, QWidget* parent) : QDialog(parent)
{
    setObjectName("textViewer");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("%1 — View").arg(QFileInfo(path).fileName()));
    resize(850, 560);

    auto* layout = new QVBoxLayout(this);
    auto* address = new QLineEdit(path, this);
    address->setReadOnly(true);
    address->setAccessibleName(tr("File path"));
    layout->addWidget(address);

    auto* tools = new QHBoxLayout;
    query_ = new QLineEdit(this);
    query_->setObjectName("viewerSearch");
    query_->setAccessibleName(tr("Find in file"));
    query_->setPlaceholderText(tr("Find in file"));
    auto* previous = new QPushButton(tr("Previous"), this);
    auto* next = new QPushButton(tr("Next"), this);
    auto* wrap = new QCheckBox(tr("Wrap lines"), this);
    for (auto* button : {previous, next})
        button->setAutoDefault(false);
    tools->addWidget(query_, 1);
    tools->addWidget(previous);
    tools->addWidget(next);
    tools->addWidget(wrap);
    layout->addLayout(tools);

    text_ = new QPlainTextEdit(this);
    text_->setObjectName("viewerText");
    text_->setAccessibleName(tr("File contents"));
    text_->setReadOnly(true);
    text_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text_->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(text_, 1);

    auto* footer = new QHBoxLayout;
    auto* status = new QLabel(tr("Loading file…"), this);
    status->setObjectName("viewerStatus");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    searchStatus_ = new QLabel(this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setAutoDefault(false);
    footer->addWidget(status, 1);
    footer->addWidget(searchStatus_);
    footer->addWidget(buttons);
    layout->addLayout(footer);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(wrap, &QCheckBox::toggled, this, [this](bool enabled)
            { text_->setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap); });
    connect(next, &QPushButton::clicked, this, [this]
            { find(false); });
    connect(previous, &QPushButton::clicked, this, [this]
            { find(true); });
    connect(query_, &QLineEdit::returnPressed, this, [this]
            { find(false); });
    connect(query_, &QLineEdit::textChanged, this, [this]
            { searchStatus_->clear(); });
    auto* search = new QShortcut(QKeySequence::Find, this);
    connect(search, &QShortcut::activated, this, [this]
            { query_->setFocus(); query_->selectAll(); });
    auto* findNext = new QShortcut(QKeySequence(Qt::Key_F3), this);
    auto* findPrevious = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    connect(findNext, &QShortcut::activated, this, [this]
            { find(false); });
    connect(findPrevious, &QShortcut::activated, this, [this]
            { find(true); });

    query_->setEnabled(false);
    previous->setEnabled(false);
    next->setEnabled(false);
    auto* watcher = new QFutureWatcher<TextPreview::Result>(this);
    connect(watcher, &QFutureWatcher<TextPreview::Result>::finished, this, [this, watcher, status, previous, next]
            {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (!result.error.isEmpty())
        {
            status->setText(tr("Cannot display this file. %1").arg(result.error));
            return;
        }
        text_->setPlainText(result.text);
        status->setText(result.truncated ? tr("%1 · Showing the first 1 MiB").arg(result.encoding)
                                        : result.text.isEmpty() ? tr("%1 · Empty file").arg(result.encoding) : result.encoding);
        query_->setEnabled(!result.text.isEmpty());
        previous->setEnabled(query_->isEnabled());
        next->setEnabled(query_->isEnabled()); });
    watcher->setFuture(QtConcurrent::run([path]
                                         { return TextPreview::read(path); }));
    text_->setFocus();
}

void TextViewer::find(bool backward)
{
    if (!query_->isEnabled() || query_->text().isEmpty())
        return;
    const QTextDocument::FindFlags flags = backward ? QTextDocument::FindBackward : QTextDocument::FindFlags();
    const auto original = text_->textCursor();
    if (!text_->find(query_->text(), flags))
    {
        text_->moveCursor(backward ? QTextCursor::End : QTextCursor::Start);
        if (!text_->find(query_->text(), flags))
        {
            text_->setTextCursor(original);
            searchStatus_->setText(tr("No matches"));
            return;
        }
    }
    searchStatus_->clear();
    text_->setFocus();
}

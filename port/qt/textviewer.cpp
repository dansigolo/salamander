// SPDX-License-Identifier: GPL-2.0-or-later
#include "textviewer.h"
#include "textpreview.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QScrollArea>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <utility>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace
{
    struct Preview
    {
        TextPreview::Result text;
        QImage image;
        QByteArray format;
    };

    Preview readPreview(const QString& path)
    {
        Preview result;
        QFile file;
        result.text.error = TextPreview::openRegularFile(path, file);
        if (!result.text.error.isEmpty())
            return result;

        QImageReader reader(&file);
        reader.setDecideFormatFromContent(true);
        if (!reader.canRead())
        {
            if (file.seek(0))
                result.text = TextPreview::read(file);
            else
                result.text.error = file.errorString();
            return result;
        }

        result.format = reader.format();
        if (result.format != "png" && result.format != "jpeg" && result.format != "bmp" &&
            result.format != "gif" && result.format != "webp")
        {
            result.text.error = QObject::tr("This image format is not supported by the preview.");
            return result;
        }
        constexpr qint64 maxImageBytes = 64 * 1024 * 1024;
        constexpr qint64 maxPixels = 24 * 1024 * 1024;
        const auto size = reader.size();
        if (file.size() > maxImageBytes || size.width() > 16384 || size.height() > 16384 ||
            qint64(size.width()) * size.height() > maxPixels)
        {
            result.text.error = QObject::tr("This image exceeds the preview size limit.");
            return result;
        }
        if (size.isEmpty())
        {
            result.text.error = QObject::tr("Could not read the image dimensions.");
            return result;
        }
        reader.setAutoTransform(true);
        result.image = reader.read();
        if (result.image.isNull())
            result.text.error = reader.errorString();
        return result;
    }

    class ImageCanvas final : public QWidget
    {
    public:
        explicit ImageCanvas(QImage image) : image_(std::move(image))
        {
            setObjectName("viewerImage");
            setAccessibleName(tr("Image preview"));
        }

        QSize sizeHint() const override { return image_.size(); }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            auto target = image_.size();
            if (target.width() > width() || target.height() > height())
                target.scale(size(), Qt::KeepAspectRatio);
            const QRect area = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter, target, rect());
            QPainter painter(this);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(area, image_);
        }

    private:
        QImage image_;
    };
} // namespace

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

    auto* textTools = new QWidget(this);
    auto* tools = new QHBoxLayout(textTools);
    tools->setContentsMargins(0, 0, 0, 0);
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
    layout->addWidget(textTools);

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
    auto* watcher = new QFutureWatcher<Preview>(this);
    connect(watcher, &QFutureWatcher<Preview>::finished, this, [this, watcher, layout, textTools, status, previous, next]
            {
        auto preview = watcher->result();
        watcher->deleteLater();
        if (!preview.image.isNull())
        {
            const auto dimensions = preview.image.size();
            auto* scroll = new QScrollArea(this);
            scroll->setAlignment(Qt::AlignCenter);
            scroll->setWidgetResizable(true);
            auto* canvas = new ImageCanvas(std::move(preview.image));
            scroll->setWidget(canvas);
            auto* actual = new QCheckBox(tr("Actual size"), this);
            actual->setObjectName("viewerActualSize");
            connect(actual, &QCheckBox::toggled, scroll, [scroll, canvas](bool enabled)
            {
                scroll->setWidgetResizable(!enabled);
                if (enabled)
                    canvas->resize(canvas->sizeHint());
            });
            textTools->hide();
            text_->hide();
            layout->insertWidget(1, actual);
            layout->insertWidget(2, scroll, 1);
            status->setText(tr("%1 · %2 × %3 pixels").arg(QString::fromLatin1(preview.format.toUpper())).arg(dimensions.width()).arg(dimensions.height()));
            scroll->setFocus();
            return;
        }
        const auto& result = preview.text;
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
                                         { return readPreview(path); }));
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

// SPDX-License-Identifier: GPL-2.0-or-later
#include "appearance.h"
#include <QApplication>
#include <QPalette>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QPixmap>
#include <QIcon>
#include <QUuid>
#include <QVBoxLayout>
#include <array>
#include <cmath>

namespace
{
    constexpr int maxThemes = 64;
    constexpr qint64 maxThemeBytes = 8192;
    const QString themeKey = QStringLiteral("appearance/theme");
    const QString accentKey = QStringLiteral("appearance/accent");
    const QString libraryKey = QStringLiteral("appearance/customThemes");

    enum Color
    {
        Window,
        Base,
        Text,
        Selection,
        Accent,
        ColorCount
    };
    const std::array<QString, ColorCount> colorKeys{"window", "base", "text", "selection", "accent"};

    struct Theme
    {
        QString id;
        QString name;
        std::array<QColor, ColorCount> colors;
    };

    QList<Theme> builtInThemes()
    {
        return {
            {"system", QObject::tr("System"), {}},
            {"light", QObject::tr("Light"), {QColor("#f0f1f3"), QColor("#ffffff"), QColor("#252932"), QColor("#cee1fd"), QColor("#245caf")}},
            {"dark", QObject::tr("Dark"), {QColor("#26282d"), QColor("#1e2024"), QColor("#ecedf0"), QColor("#394965"), QColor("#95bfff")}},
            // Color values from https://draculatheme.com/contribute#color-palette.
            {"dracula", QStringLiteral("Dracula"), {QColor("#282a36"), QColor("#21222c"), QColor("#f8f8f2"), QColor("#44475a"), QColor("#bd93f9")}},
        };
    }

    QColor parseColor(const QString& value)
    {
        static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        return pattern.match(value).hasMatch() ? QColor(value) : QColor();
    }

    double luminance(QColor color)
    {
        const auto linear = [](double channel)
        { return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    }

    double contrast(QColor first, QColor second)
    {
        const double a = luminance(first), b = luminance(second);
        return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
    }

    QColor readableText(QColor background)
    {
        return contrast(Qt::black, background) >= contrast(Qt::white, background) ? Qt::black : Qt::white;
    }

    QColor blend(QColor first, QColor second, double fraction)
    {
        return QColor::fromRgbF(first.redF() * (1 - fraction) + second.redF() * fraction,
                                first.greenF() * (1 - fraction) + second.greenF() * fraction,
                                first.blueF() * (1 - fraction) + second.blueF() * fraction);
    }

    QString validate(const Theme& theme)
    {
        if (theme.name.trimmed().isEmpty() || theme.name.size() > 60)
            return QObject::tr("Enter a theme name of 1 to 60 characters.");
        for (const auto& color : theme.colors)
            if (!color.isValid())
                return QObject::tr("Use #RRGGBB for every color.");
        if (contrast(theme.colors[Text], theme.colors[Window]) < 4.5 || contrast(theme.colors[Text], theme.colors[Base]) < 4.5)
            return QObject::tr("Text must have at least 4.5:1 contrast against the window and list backgrounds.");
        return {};
    }

    QJsonObject serialize(const Theme& theme)
    {
        QJsonObject colors;
        for (int index = 0; index < ColorCount; ++index)
            colors.insert(colorKeys[index], theme.colors[index].name());
        return {{"version", 1}, {"name", theme.name}, {"colors", colors}};
    }

    bool deserialize(const QJsonObject& object, Theme& theme, QString& error)
    {
        if (object.value("version") != QJsonValue(1))
        {
            error = QObject::tr("This file must use Salamander theme format version 1.");
            return false;
        }
        theme.name = object.value("name").toString().trimmed();
        const auto colors = object.value("colors").toObject();
        for (int index = 0; index < ColorCount; ++index)
            theme.colors[index] = parseColor(colors.value(colorKeys[index]).toString());
        error = validate(theme);
        return error.isEmpty();
    }

    QList<Theme> loadThemes(const QSettings& settings)
    {
        auto themes = builtInThemes();
        const auto stored = QJsonDocument::fromJson(settings.value(libraryKey).toByteArray()).array();
        for (const auto& entry : stored)
        {
            if (themes.size() >= builtInThemes().size() + maxThemes)
                break;
            const auto object = entry.toObject();
            Theme theme;
            QString error;
            const QUuid id(object.value("id").toString());
            if (id.isNull() || !deserialize(object, theme, error))
                continue;
            theme.id = id.toString(QUuid::WithoutBraces);
            themes.append(theme);
        }
        return themes;
    }

    void saveThemes(QSettings& settings, const QList<Theme>& themes)
    {
        QJsonArray stored;
        for (const auto& theme : themes)
        {
            if (QUuid(theme.id).isNull())
                continue;
            auto object = serialize(theme);
            object.insert("id", theme.id);
            stored.append(object);
        }
        settings.setValue(libraryKey, QJsonDocument(stored).toJson(QJsonDocument::Compact));
    }

    void setApplicationPalette(const QPalette& palette)
    {
        QApplication::setPalette(palette);
        // Refresh the existing windows and their children on Qt 6.4.
        for (auto* window : QApplication::topLevelWidgets())
        {
            QEvent changed(QEvent::ApplicationPaletteChange);
            QCoreApplication::sendEvent(window, &changed);
        }
    }

    QPalette desktopPalette()
    {
        // An empty resolve mask restores the Qt platform palette.
        setApplicationPalette(QPalette());
        return QApplication::palette();
    }

    void applyTheme(const Theme& theme, QColor accent)
    {
        QPalette palette;
        if (theme.id == "system")
        {
            palette = desktopPalette();
            if (!accent.isValid())
                return;
            palette.setColor(QPalette::Link, accent);
            const QColor selection = blend(palette.color(QPalette::Base), accent, 0.35);
            palette.setColor(QPalette::Highlight, selection);
            palette.setColor(QPalette::HighlightedText, readableText(selection));
        }
        else
        {
            const auto& colors = theme.colors;
            const QColor selection = accent.isValid() ? blend(colors[Base], accent, 0.35) : colors[Selection];
            const QColor text = colors[Text];
            palette.setColor(QPalette::Window, colors[Window]);
            palette.setColor(QPalette::Base, colors[Base]);
            palette.setColor(QPalette::AlternateBase, blend(colors[Base], colors[Window], 0.5));
            palette.setColor(QPalette::Button, colors[Window]);
            for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::ToolTipText})
                palette.setColor(role, text);
            palette.setColor(QPalette::ToolTipBase, colors[Window]);
            palette.setColor(QPalette::BrightText, readableText(colors[Window]));
            palette.setColor(QPalette::Highlight, selection);
            palette.setColor(QPalette::HighlightedText, contrast(text, selection) >= 4.5 ? text : readableText(selection));
            palette.setColor(QPalette::Link, accent.isValid() ? accent : colors[Accent]);
            palette.setColor(QPalette::LinkVisited, palette.color(QPalette::Link));
            palette.setColor(QPalette::Light, blend(colors[Window], text, 0.28));
            palette.setColor(QPalette::Midlight, blend(colors[Window], text, 0.18));
            palette.setColor(QPalette::Mid, blend(colors[Window], text, 0.12));
            palette.setColor(QPalette::Dark, blend(colors[Window], Qt::black, 0.28));
            palette.setColor(QPalette::Shadow, blend(colors[Window], Qt::black, 0.5));
            palette.setColor(QPalette::PlaceholderText, blend(colors[Base], text, 0.65));
            for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
                palette.setColor(QPalette::Disabled, role, blend(colors[Window], text, 0.5));
        }
        setApplicationPalette(palette);
    }

    bool editTheme(Theme& theme, QWidget* parent)
    {
        QDialog dialog(parent);
        dialog.setWindowTitle(QObject::tr("Edit theme"));
        auto* layout = new QFormLayout(&dialog);
        auto* name = new QLineEdit(theme.name, &dialog);
        name->setMaxLength(60);
        layout->addRow(QObject::tr("Name"), name);
        const QString labels[] = {QObject::tr("Window"), QObject::tr("File list"), QObject::tr("Text"), QObject::tr("Selection"), QObject::tr("Accent")};
        std::array<QLineEdit*, ColorCount> fields;
        for (int index = 0; index < ColorCount; ++index)
        {
            auto* row = new QHBoxLayout;
            auto* field = new QLineEdit(theme.colors[index].name(), &dialog);
            field->setMaxLength(7);
            fields[index] = field;
            auto* choose = new QPushButton(QObject::tr("Pick color…"), &dialog);
            QObject::connect(choose, &QPushButton::clicked, &dialog, [field, &dialog]
                             {
            const QColor color = QColorDialog::getColor(parseColor(field->text()), &dialog);
            if (color.isValid())
                field->setText(color.name()); });
            row->addWidget(field);
            row->addWidget(choose);
            layout->addRow(labels[index], row);
        }
        auto* error = new QLabel(&dialog);
        error->setWordWrap(true);
        layout->addRow(error);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]
                         {
        Theme candidate = theme;
        candidate.name = name->text().trimmed();
        for (int index = 0; index < ColorCount; ++index)
            candidate.colors[index] = parseColor(fields[index]->text());
        const auto problem = validate(candidate);
        if (!problem.isEmpty())
        {
            error->setText(problem);
            return;
        }
        theme = candidate;
        dialog.accept(); });
        return dialog.exec() == QDialog::Accepted;
    }
} // namespace

void Appearance::apply(const QSettings& settings)
{
    const auto themes = loadThemes(settings);
    const auto id = settings.value(themeKey, "system").toString();
    for (const auto& theme : themes)
        if (theme.id == id)
        {
            applyTheme(theme, parseColor(settings.value(accentKey).toString()));
            return;
        }
    applyTheme(themes.first(), {});
}

void Appearance::configure(QSettings& settings, QWidget* parent)
{
    auto themes = loadThemes(settings);
    const int builtInCount = builtInThemes().size();
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Appearance"));
    dialog.setObjectName("appearanceDialog");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto* choice = new QComboBox(&dialog);
    choice->setObjectName("themeChoice");
    auto* accents = new QComboBox(&dialog);
    accents->setObjectName("themeAccent");
    accents->addItem(QObject::tr("Theme default"), QString());
    const std::pair<QString, QString> presets[] = {
        {QObject::tr("Blue"), "#5796e6"}, {QObject::tr("Teal"), "#32a799"}, {QObject::tr("Green"), "#64a954"}, {QObject::tr("Orange"), "#e18753"}, {QObject::tr("Pink"), "#d7679b"}, {QObject::tr("Purple"), "#a788e2"}};
    for (const auto& [label, value] : presets)
    {
        QPixmap swatch(16, 16);
        swatch.fill(QColor(value));
        accents->addItem(QIcon(swatch), label, value);
    }
    accents->setCurrentIndex(qMax(0, accents->findData(settings.value(accentKey).toString())));
    form->addRow(QObject::tr("Theme"), choice);
    form->addRow(QObject::tr("Accent color"), accents);
    auto* actions = new QHBoxLayout;
    layout->addLayout(actions);
    auto* create = new QPushButton(QObject::tr("New…"), &dialog);
    auto* edit = new QPushButton(QObject::tr("Edit…"), &dialog);
    auto* importTheme = new QPushButton(QObject::tr("Import…"), &dialog);
    importTheme->setObjectName("importTheme");
    auto* exportTheme = new QPushButton(QObject::tr("Export…"), &dialog);
    exportTheme->setObjectName("exportTheme");
    auto* remove = new QPushButton(QObject::tr("Remove"), &dialog);
    for (auto* button : {create, edit, importTheme, exportTheme, remove})
        actions->addWidget(button);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    const auto refresh = [&]
    {
        const int index = choice->currentIndex();
        if (index < 0)
            return;
        edit->setEnabled(index >= builtInCount);
        remove->setEnabled(index >= builtInCount);
        exportTheme->setEnabled(index != 0);
        create->setEnabled(themes.size() < builtInCount + maxThemes);
        importTheme->setEnabled(create->isEnabled());
        applyTheme(themes.at(index), parseColor(accents->currentData().toString()));
    };
    const auto populate = [&](const QString& id)
    {
        const QSignalBlocker blocked(choice);
        choice->clear();
        for (const auto& theme : themes)
            choice->addItem(theme.name, theme.id);
        choice->setCurrentIndex(qMax(0, choice->findData(id)));
        refresh();
    };
    const auto add = [&](Theme theme)
    {
        theme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        themes.append(theme);
        populate(theme.id);
    };
    QObject::connect(choice, &QComboBox::currentIndexChanged, &dialog, refresh);
    QObject::connect(accents, &QComboBox::currentIndexChanged, &dialog, refresh);
    QObject::connect(create, &QPushButton::clicked, &dialog, [&]
                     {
        auto theme = themes.at(qMax(1, choice->currentIndex()));
        theme.name = QObject::tr("My theme");
        if (editTheme(theme, &dialog))
            add(theme); });
    QObject::connect(edit, &QPushButton::clicked, &dialog, [&]
                     {
        auto& theme = themes[choice->currentIndex()];
        if (editTheme(theme, &dialog))
            populate(theme.id); });
    const auto showError = [&](const QString& message)
    {
        QMessageBox::warning(&dialog, QObject::tr("Theme file"), message);
    };
    QObject::connect(importTheme, &QPushButton::clicked, &dialog, [&]
                     {
        const auto path = QFileDialog::getOpenFileName(&dialog, QObject::tr("Import theme"), {}, QObject::tr("Salamander themes (*.json)"));
        if (path.isEmpty())
            return;
        if (!QFileInfo(path).isFile())
        {
            showError(QObject::tr("Choose a regular theme file."));
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            showError(file.errorString());
            return;
        }
        const auto data = file.read(maxThemeBytes + 1);
        if (file.error() != QFileDevice::NoError || data.size() > maxThemeBytes)
        {
            showError(QObject::tr("Could not read the theme. Theme files must fit in 8 KiB."));
            return;
        }
        Theme theme;
        QString error;
        if (!deserialize(QJsonDocument::fromJson(data).object(), theme, error))
        {
            showError(error);
            return;
        }
        add(theme); });
    QObject::connect(exportTheme, &QPushButton::clicked, &dialog, [&]
                     {
        const auto path = QFileDialog::getSaveFileName(&dialog, QObject::tr("Export theme"), "salamander-theme.json", QObject::tr("Salamander themes (*.json)"));
        if (path.isEmpty())
            return;
        auto theme = themes.at(choice->currentIndex());
        const auto accent = parseColor(accents->currentData().toString());
        if (accent.isValid())
        {
            theme.colors[Accent] = accent;
            theme.colors[Selection] = blend(theme.colors[Base], accent, 0.35);
        }
        const auto bytes = QJsonDocument(serialize(theme)).toJson();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            showError(file.errorString()); });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&]
                     {
        themes.removeAt(choice->currentIndex());
        populate("system"); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    populate(settings.value(themeKey, "system").toString());
    if (dialog.exec() == QDialog::Accepted)
    {
        settings.setValue(themeKey, choice->currentData());
        settings.setValue(accentKey, accents->currentData());
        saveThemes(settings, themes);
    }
    apply(settings);
}

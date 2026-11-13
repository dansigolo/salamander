// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>

class QSettings;
class QWidget;

namespace ExternalEditor
{
    bool configure(QSettings& settings, QWidget* parent);
    void open(const QString& path, QSettings& settings, QWidget* parent);
} // namespace ExternalEditor

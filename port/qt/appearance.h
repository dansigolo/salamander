// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class QSettings;
class QWidget;

namespace Appearance
{
    void apply(const QSettings& settings);
    void configure(QSettings& settings, QWidget* parent);
} // namespace Appearance

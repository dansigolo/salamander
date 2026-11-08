// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace salamander
{
    // Extracted from src/masks.cpp. Destination requires strlen(source) + 1 bytes.
    // Source and destination must not overlap. This is byte-oriented normalization,
    // not Unicode-aware matching; no matching semantics have been ported yet.
    void prepareMask(char* destination, const char* source);
} // namespace salamander

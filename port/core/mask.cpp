// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "mask.h"
namespace salamander
{
    void prepareMask(char* mask, const char* src)
    {
        char* begMask = mask;
        char lastChar = 0;
        // remove spaces at the beginning of the mask
        while (*src == ' ')
            src++;
        while (*src != 0)
        {
            if (*src == '*' && lastChar == '*')
                src++;                               // "**" -> "*"
            else if (*src == '?' && lastChar == '*') // "*?" -> "?*"
            {
                *(mask - 1) = '?';
                *mask++ = '*';
                src++;
            }
            else
                *mask++ = (lastChar = *src++);
        }
        // trim spaces at the end of the mask
        while (mask > begMask && *(mask - 1) == ' ')
            mask--;
        *mask = 0;
    }

} // namespace salamander

// SPDX-License-Identifier: GPL-2.0-or-later
#include "mask.h"
#include <cstring>
#include <iostream>
int main()
{
    const char* cases[][2] = {{"", ""}, {"   ", ""}, {"  *.cpp  ", "*.cpp"}, {"***", "*"}, {"*?", "?*"}, {"**??.txt", "??*.txt"}, {"file name", "file name"}, {"?.*", "?.*"}};
    for (const auto& entry : cases)
    {
        char result[128];
        salamander::prepareMask(result, entry[0]);
        if (std::strcmp(result, entry[1]) != 0)
        {
            std::cerr << "Mask normalization mismatch: " << entry[0] << '\n';
            return 1;
        }
    }
}

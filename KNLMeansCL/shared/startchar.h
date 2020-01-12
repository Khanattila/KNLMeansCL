/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2002		thejam79,
*    Copyright(C) 2003		minamina,
*    Copyright(C) 2007		Donald A. Graft,
*    Copyright(C) 2014-2020 Edoardo Brunetti.
*
*    KNLMeansCL is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    KNLMeansCL is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with KNLMeansCL. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __STARTCHAR_H
#define __STARTCHAR_H

#include <cstdint>

//////////////////////////////////////////
// Functions
void DrawDigit(
    uint8_t* dst,
    int pitch,
    int x,
    int y,
    int num
);

void DrawString(
    uint8_t* dst,
    int pitch,
    int x,
    int y,
    const char *s
);

#endif //__STARTCHAR_H__

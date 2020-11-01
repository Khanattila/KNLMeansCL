/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2020  Edoardo Brunetti.
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

#ifndef __COMMON_H
#define __COMMON_H

#include <cstddef>

//////////////////////////////////////////
// Functions
size_t mrounds(
    const size_t number,
    const size_t multiple
);

int min(
    const int a,
    const int b
);

int max(
    const int a,
    const int b
);

#endif //__COMMON_H__

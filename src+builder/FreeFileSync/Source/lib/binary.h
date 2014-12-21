// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef BINARY_H_INCLUDED
#define BINARY_H_INCLUDED

#include <functional>
#include <zen/zstring.h>
#include <zen/file_error.h>

namespace zen
{
bool filesHaveSameContent(const Zstring& filepath1,  //throw FileError
                          const Zstring& filepath2,
                          const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus); //may be nullptr
}

#endif // BINARY_H_INCLUDED

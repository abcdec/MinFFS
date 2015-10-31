// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef BINARY_H_INCLUDED_3941281398513241134
#define BINARY_H_INCLUDED_3941281398513241134

#include <functional>
#include <zen/file_error.h>
#include "../fs/abstract.h"


namespace zen
{
bool filesHaveSameContent(const AbstractPathRef& filePath1, //throw FileError
                          const AbstractPathRef& filePath2,
                          const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus); //may be nullptr
}

#endif //BINARY_H_INCLUDED_3941281398513241134

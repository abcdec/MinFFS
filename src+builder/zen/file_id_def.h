// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILE_ID_INTERNAL_HEADER_013287632486321493
#define FILE_ID_INTERNAL_HEADER_013287632486321493

#include <utility>

#ifdef ZEN_WIN
    #include "win.h" //includes "windows.h"

#elif defined ZEN_LINUX || defined ZEN_MAC
    #include <sys/stat.h>
#endif


namespace zen
{
#ifdef ZEN_WIN
typedef DWORD DeviceId;
typedef ULONGLONG FileIndex;

typedef std::pair<DeviceId, FileIndex> FileId; //optional! (however, always set on Linux, and *generally* available on Windows)


inline
FileId extractFileId(const BY_HANDLE_FILE_INFORMATION& fileInfo)
{
    ULARGE_INTEGER fileIndex = {};
    fileIndex.HighPart = fileInfo.nFileIndexHigh;
    fileIndex.LowPart  = fileInfo.nFileIndexLow;

    return fileInfo.dwVolumeSerialNumber != 0 && fileIndex.QuadPart != 0 ?
           FileId(fileInfo.dwVolumeSerialNumber, fileIndex.QuadPart) : FileId();
}

inline
FileId extractFileId(DWORD volumeSerialNumber, ULONGLONG fileIndex)
{
    return volumeSerialNumber != 0 && fileIndex != 0 ?
           FileId(volumeSerialNumber, fileIndex) : FileId();
}

static_assert(sizeof(FileId().first ) == sizeof(BY_HANDLE_FILE_INFORMATION().dwVolumeSerialNumber), "");
static_assert(sizeof(FileId().second) == sizeof(BY_HANDLE_FILE_INFORMATION().nFileIndexHigh) + sizeof(BY_HANDLE_FILE_INFORMATION().nFileIndexLow), "");
static_assert(sizeof(FileId().second) == sizeof(ULARGE_INTEGER), "");


#elif defined ZEN_LINUX || defined ZEN_MAC
namespace impl { typedef struct ::stat StatDummy; } //sigh...

typedef decltype(impl::StatDummy::st_dev) DeviceId;
typedef decltype(impl::StatDummy::st_ino) FileIndex;

typedef std::pair<DeviceId, FileIndex> FileId;

inline
FileId extractFileId(const struct ::stat& fileInfo)
{
    return fileInfo.st_dev != 0 && fileInfo.st_ino != 0 ?
           FileId(fileInfo.st_dev, fileInfo.st_ino) : FileId();
}
#endif
}

#endif //FILE_ID_INTERNAL_HEADER_013287632486321493

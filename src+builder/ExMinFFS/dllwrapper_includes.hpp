// **************************************************************************
// * Copyright (C) 2014 abcdec @GitHub - All Rights Reserved                *
// * This file is part of a modified version of FreeFileSync, MinFFS.       *
// * The original FreeFileSync program and source code are distributed by   *
// * the FreeFileSync project: http://www.freefilesync.org/                 *
// * This particular file is created by by abcdec @GitHub as part of the    *
// * MinFFS project: https://github.com/abcdec/MinFFS                       *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This file is distributed under GNU General Public License:             *
// * http://www.gnu.org/licenses/gpl-3.0 per the FreeFileSync License.      *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************
#ifndef MINFFS_DLLWRAPPER_INCLUDES_HPP_INCLUDED
#define MINFFS_DLLWRAPPER_INCLUDES_HPP_INCLUDED

#include <string>
#include <iostream>
#include <windows.h>
#include <Shlobj.h>
#include <uxtheme.h>
#include <winbase.h>
#include "zen/FindFilePlus/find_file_plus.h"
#include "zen/IFileOperation/file_op.h"
#include "FreeFileSync/Source/dll/IFileDialog_Vista/ifile_dialog.h"
#include "FreeFileSync/Source/dll/Taskbar_Seven/taskbar.h"
#include "FreeFileSync/Source/dll/Thumbnail/thumbnail.h"

#ifndef HTHEME
// MinGW needs OS version 5.1 (WINVER > 0x0501) to define HTHEME.
// WINVER is set to 0x0500 from Makefile
typedef HANDLE HTHEME;
#endif//HTHEME

// For definition of:
// FreeFileSync/Source/lib/resolve_path.cpp : SHGetKnownFolderPath
// REFKNOWNFOLDERID need be defined in default namespace.
//typedef GUID KNOWNFOLDERID;
//typedef KNOWNFOLDERID *REFKNOWNFOLDERID;
//extern REFKNOWNFOLDERID FOLDERID_Downloads;
//extern REFKNOWNFOLDERID FOLDERID_PublicDownloads;
//extern REFKNOWNFOLDERID FOLDERID_QuickLaunch;

#endif//MINFFS_DLLWRAPPER_INCLUDES_HPP_INCLUDED

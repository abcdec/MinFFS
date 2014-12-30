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

#include <windows.h>
#include <Shlobj.h>
#include <string>
#include <iostream>
#include "zen/FindFilePlus/find_file_plus.h"
#include "zen/IFileOperation/file_op.h"
#include "FreeFileSync/Source/dll/IFileDialog_Vista/ifile_dialog.h"
#include "FreeFileSync/Source/dll/Taskbar_Seven/taskbar.h"
#include "FreeFileSync/Source/dll/Thumbnail/thumbnail.h"

// For definition of:
// FreeFileSync/Source/lib/resolve_path.cpp : SHGetKnownFolderPath
// REFKNOWNFOLDERID need be defined in default namespace.
typedef GUID KNOWNFOLDERID;
typedef KNOWNFOLDERID *REFKNOWNFOLDERID;
// {374DE290-123F-4565-9164-39C4925E467B} - %USERPROFILE%\Downloads
static KNOWNFOLDERID FOLDERID_Downloads_data       = {0x374DE290, 0x123F, 0x4565, {0x91, 0x64, 0x39, 0xC4, 0x92, 0x5E, 0x46, 0x7B}};
static REFKNOWNFOLDERID FOLDERID_Downloads         = &FOLDERID_Downloads_data;
// {3D644C9B-1FB8-4f30-9B45-F670235F79C0} - %PUBLIC%\Downloads
static KNOWNFOLDERID FOLDERID_PublicDownloads_data = {0x3D644C9B, 0x1FB8, 0x4f30, {0x9B, 0x45, 0xF6, 0x70, 0x23, 0x5F, 0x79, 0xC0}};
static REFKNOWNFOLDERID FOLDERID_PublicDownloads   = &FOLDERID_PublicDownloads_data;
// {52a4f021-7b75-48a9-9f6b-4b87a210bc8f} - %APPDATA%\Microsoft\Internet Explorer\Quick Launch
static KNOWNFOLDERID FOLDERID_QuickLaunch_data     = {0x52a4f021, 0x7b75, 0x48a9, {0x9f, 0x6b, 0x4b, 0x87, 0xa2, 0x10, 0xbc, 0x8f}};
static REFKNOWNFOLDERID FOLDERID_QuickLaunch       = &FOLDERID_QuickLaunch_data;

#endif//MINFFS_DLLWRAPPER_INCLUDES_HPP_INCLUDED

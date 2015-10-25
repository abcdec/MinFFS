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
#ifndef MINFFS_DLLWRAPPER_HPP_INCLUDED
#define MINFFS_DLLWRAPPER_HPP_INCLUDED

// Name space and dependent headers should be specified in a source
// file including this header file.
template <typename T>
struct DllFun {
public:
    DllFun() {};
    
    DllFun(std::wstring dllNameIn, std::string functionNameIn)
	: functionPtr_(nullptr), moduleHandle_(nullptr) {
	moduleHandle_ = LoadLibraryW(dllNameIn.data());
	if (moduleHandle_ != NULL) {
	    FARPROC proc = GetProcAddress(moduleHandle_, functionNameIn.c_str());
	    if (proc != NULL) {
		functionPtr_ = reinterpret_cast<T>(proc);
	    }
	}
    };
    
    ~DllFun() {if (moduleHandle_) FreeLibrary(moduleHandle_);};
    
    inline operator bool () const {
	return (functionPtr_ != nullptr);
    };
    inline bool operator!=(T functionPtrIn) const {
	return (functionPtr_ != functionPtrIn);
    };
    
    // =================================================================================
    // zen/FindFilePlus/find_file_plus.h

    // zen/FindFilePlus/find_file_plus.h : openDir
    inline findplus::FindHandle operator()(Zstring pathNameIn) const {
	return functionPtr_(pathNameIn);
    };
    // zen/FindFilePlus/find_file_plus.h : readDir
    inline bool operator()(findplus::FindHandle findHandleIn, findplus::FileInformation& findDataIn) const {
	return functionPtr_(findHandleIn, findDataIn);
    };
    // zen/FindFilePlus/find_file_plus.h : closeDir()
    inline void operator()(findplus::FindHandle findHandleIn) const {
	functionPtr_(findHandleIn);
    };


    // =================================================================================
    // zen/IFileOperation/file_op.h

    // zen/IFileOperation/file_op.h : freeString
    // NOTE: Definition conflict with zen/IFileOperation/file_op.h : getLockingProcesses
    // use fileop::getLastErrorMessage one as it is luckily compatible. (also return
    // value shoud be void but for now keep wchar_t*)
    // inline wchar_t* operator()(const wchar_t*) const { return functionPtr_(); };

    // zen/IFileOperation/file_op.h : getLastErrorMessage
    inline wchar_t* operator()() const { return functionPtr_(); };
    // zen/IFileOperation/file_op.h : getLockingProcesses
    inline wchar_t* operator()(const wchar_t*a, const wchar_t*b) const { return functionPtr_(a, b); };
    // zen/IFileOperation/file_op.h : getRecycleBinStatus
    inline bool operator()(const wchar_t *a, bool &b) const { return functionPtr_(a, b); };
    // zen/IFileOperation/file_op.h : moveToRecycleBin
    inline bool operator()(const wchar_t**a, std::vector<const wchar_t*>::size_type b,
			   bool (&c)(const wchar_t*, void *), void *d) const {
	return functionPtr_(a, b, c, d);
    };


    // =================================================================================
    // FreeFileSync/Source/dll/IFileDialog_Vista/ifile_dialog.h

    // FolderPicker FreeString
    // NOTE: Definition conflict with zen/IFileOperation/file_op.h : getLockingProcesses
    // use fileop::getLastErrorMessage one as it is luckily compatible. (also return
    // value shoud be void but for now keep wchar_t*)
    // inline wchar_t* operator()(const wchar_t*a) const { return functionPtr_(a); };

    // FolderPicker FolderPicker
    typedef char GuidProxy[16];
    inline void operator()(HWND winHandleIn, const wchar_t* defaultDirPathIn,
			   const GuidProxy* guidIn, wchar_t* &selectedFolderOut,
			   bool &cancelledOut, wchar_t* &errorMsgOut) const {
	functionPtr_(winHandleIn, defaultDirPathIn, guidIn,
		     selectedFolderOut, cancelledOut, errorMsgOut);
    };
    
    // =================================================================================
    // FreeFileSync/Source/dll/Taskbar_Seven/taskbar.h

    // FreeFileSync/Source/dll/Taskbar_Seven/taskbar.h : setStatus
    inline void operator()(void* winHandleIn, tbseven::TaskBarStatus taskBarStatusIn) {
	functionPtr_(winHandleIn, taskBarStatusIn);
    };

    // FreeFileSync/Source/dll/Taskbar_Seven/taskbar.h : setProgress
    inline void operator()(void* winHandleIn, double a, double b) {
	functionPtr_(winHandleIn, a, b);
    };


    // =================================================================================
    // FreeFileSync/Source/dll/Thumbnail/thumbnail.h

    // FreeFileSync/Source/dll/Thumbnail/thumbnail.h : getIconByIndex
    inline thumb::ImageData* operator()(int indexForShellIconIn, thumb::IconSizeType iconSizeTypeIn) const {
	return functionPtr_(indexForShellIconIn, iconSizeTypeIn);
    };
    // FreeFileSync/Source/dll/Thumbnail/thumbnail.h : getThumbnail
    inline thumb::ImageData* operator()(const wchar_t *iconFilePath, int& sizeIn) const {
	return functionPtr_(iconFilePath, sizeIn);
    };
    //  FreeFileSync/Source/dll/Thumbnail/thumbnail.h : releaseImageData
    inline void operator()(const thumb::ImageData *imgeDataPtrIn) const {
	functionPtr_(imgeDataPtrIn);
    };

private:
    T functionPtr_;
    HMODULE moduleHandle_;
};


template <typename T>
struct SysDllFun {
public:
    SysDllFun() {};
    
    SysDllFun(std::wstring dllNameIn, std::string functionNameIn)
	: functionPtr_(nullptr), moduleHandle_(nullptr) {
	moduleHandle_ = LoadLibraryW(dllNameIn.data());
	if (moduleHandle_ != NULL) {
	    FARPROC proc = GetProcAddress(moduleHandle_, functionNameIn.c_str());
	    if (proc != NULL) {
		functionPtr_ = reinterpret_cast<T>(proc);
	    }
	}
    };

    SysDllFun(std::wstring dllNameIn, LPCSTR functionNamePtrIn)
	: functionPtr_(nullptr), moduleHandle_(nullptr) {
	moduleHandle_ = LoadLibraryW(dllNameIn.data());
	if (moduleHandle_ != NULL) {
	    FARPROC proc = GetProcAddress(moduleHandle_, functionNamePtrIn);
	    if (proc != NULL) {
		functionPtr_ = reinterpret_cast<T>(proc);
	    }
	}
    };
    
    ~SysDllFun() {FreeLibrary(moduleHandle_);};
    
    inline operator bool() const {
	return (functionPtr_ != nullptr);
    };
    
    inline bool operator!=(T functionPtrIn) const {
	return (functionPtr_ != functionPtrIn);
    };
    

    // =================================================================================
    // FreeFileSync/Source/application.cpp

    // FreeFileSync/Source/application.cpp : GetProcessUserModeExceptionPolicy
    inline BOOL operator()(DWORD* dwFlagsPtrIn) const { return functionPtr_(dwFlagsPtrIn); };
    // FreeFileSync/Source/application.cpp : SetProcessUserModeExceptionPolicy
    inline BOOL operator()(DWORD dwFlagsIn) const { return functionPtr_(dwFlagsIn); };


    // =================================================================================
    // FreeFileSync/Source/lib/resolve_path.cpp

    // FreeFileSync/Source/lib/resolve_path.cpp : SHGetKnownFolderPath
    // REFKNOWNFOLDERID is not defined in MinGW headers.
    // Will define in dllwrapper_include.hpp
    // See http://msdn.microsoft.com/en-us/library/bb762584.aspx 
    inline HRESULT operator()(REFKNOWNFOLDERID rfidIn, DWORD dwFlagsIn,
			      HANDLE hTokenIn, PWSTR* ppszPathIn) const {
	return functionPtr_(rfidIn, dwFlagsIn, hTokenIn, ppszPathIn);
    };


    // =================================================================================
    // wx+/font_size.h
    
    // wx+/font_size.h : OpenThemeData
    inline HTHEME operator()(HWND hwndIn, LPCWSTR pszClassListIn) const {
	return functionPtr_(hwndIn, pszClassListIn);
    };
    // wx+/font_size.h : CloseThemeData
    inline HRESULT operator()(HTHEME hwndIn) const {
	return functionPtr_(hwndIn);
    };
    // wx+/font_size.h : GetThemeColor
    inline HRESULT operator()(HTHEME hwndIn, int iPartIdIn, int iStateIdIn,
			     int iPropIdIn, COLORREF *pColorIn) const {
	return functionPtr_(hwndIn, iPartIdIn, iStateIdIn, iPropIdIn, pColorIn);
    };


    // =================================================================================
    // zen/symlink_target.h

    // zen/symlink_target.h : GetFinalPathNameByHandleW
    inline DWORD operator()(HANDLE hFileIn, LPTSTR lpszFilePathIn,
			    DWORD cchFilePathIn, DWORD dwFlagsIn) const {
	return functionPtr_(hFileIn, lpszFilePathIn, cchFilePathIn, dwFlagsIn);
    };


    // =================================================================================
    // zen/win_ver.h

    // zen/win_ver.h : IsWow64Process
    inline BOOL operator()(HANDLE hProcessIn, PBOOL Wow64ProcessIn) const {
	return functionPtr_(hProcessIn, Wow64ProcessIn);
    };


    // =================================================================================
    // zen/zstring.cpp

    // zen/zstring.cpp : CompareStringOrdinal
    inline int operator()(LPCWSTR string1In, int size1In,
			  LPCWSTR string2In, int size2In, BOOL ignoreCaseIn) const {
	return functionPtr_(string1In, size1In, string2In, size2In, ignoreCaseIn);
    };


    // =================================================================================
    // FreeFileSync/Source/lib/icon_buffer.cpp

    // FreeFileSync/Source/lib/icon_buffer.cpp : FileIconInit
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776418%28v=vs.85%29.aspx
    inline BOOL operator()(BOOL fRestoreCacheIn) const {
	return functionPtr_(fRestoreCacheIn);
    };


    // =================================================================================
    // zen/file_access.cpp

    // zen/file_access.cpp : SetFileInformationByHandle
    inline BOOL operator()(HANDLE hFileIn, FILE_INFO_BY_HANDLE_CLASS FileInformationClassIn,
			   LPVOID lpFileInformationIn, DWORD dwBufferSizeIn) const {
	return functionPtr_(hFileIn, FileInformationClassIn, lpFileInformationIn, dwBufferSizeIn);
    };
    // zen/file_access.cpp : CreateSymbolicLinkW
    inline BOOLEAN operator()(LPCTSTR lpSymlinkFileNameIn, LPCTSTR lpTargetFileNameIn,
			      DWORD dwFlagsIn) const {
	return functionPtr_(lpSymlinkFileNameIn, lpTargetFileNameIn, dwFlagsIn);
    };

    
private:
    T functionPtr_;
    HMODULE moduleHandle_;
};


#endif//MINFFS_DLLWRAPPER_HPP_INCLUDED

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
    DllFun() {
	functionPtr_ = nullptr;
    };
    
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
    inline wchar_t* operator()(const wchar_t*a) const { return functionPtr_(a); };
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
    SysDllFun() {
	functionPtr_ = nullptr;
    };
    
    SysDllFun(std::wstring dllNameIn, std::string functionNameIn) {
	moduleHandle_ = LoadLibraryW(dllNameIn.data());
	if (moduleHandle_ != NULL) {
	    FARPROC proc = GetProcAddress(moduleHandle_, functionNameIn.c_str());
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
    
    inline typename std::result_of<T> operator()(int n) const {return functionPtr_(n);};


    // CompareStringOrdinal
    inline int operator()(LPCWSTR string1,  //__in  LPCWSTR lpString1,
			  int size1,        //__in  int cchCount1,
			  LPCWSTR string2,  //__in  LPCWSTR lpString1,
			  int size2,        //__in  int cchCount2,
			  BOOL ignoreCase   //__in  BOOL bIgnoreCase
	) const {
	return functionPtr_(string1, size1, string2, size2, ignoreCase);
    };

    
private:
    T functionPtr_;
    HMODULE moduleHandle_;
};

#endif//MINFFS_DLLWRAPPER_HPP_INCLUDED

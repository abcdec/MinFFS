// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILEIO_89578342758342572345
#define FILEIO_89578342758342572345

#include "file_error.h"

#ifdef ZEN_WIN
    #include "win.h" //includes "windows.h"
#endif


namespace zen
{
#ifdef ZEN_WIN
    static const char LINE_BREAK[] = "\r\n";
#elif defined ZEN_LINUX || defined ZEN_MAC
    static const char LINE_BREAK[] = "\n"; //since OS X apple uses newline, too
#endif

//OS-buffered file IO optimized for sequential read/write accesses + better error reporting + long path support + following symlinks

#ifdef ZEN_WIN
    typedef HANDLE FileHandle;
#elif defined ZEN_LINUX || defined ZEN_MAC
    typedef int FileHandle;
#endif

class FileBase
{
public:
    const Zstring& getFilePath() const { return filename_; }

protected:
    FileBase(const Zstring& filename) : filename_(filename)  {}

private:
    FileBase           (const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;

    const Zstring filename_;
};

//-----------------------------------------------------------------------------------------------

class FileInput : public FileBase
{
public:
    FileInput(const Zstring& filepath);                    //throw FileError, ErrorFileLocked
    FileInput(FileHandle handle, const Zstring& filepath); //takes ownership!
    ~FileInput();

    size_t read(void* buffer, size_t bytesToRead); //throw FileError; returns "bytesToRead", unless end of file!
    FileHandle getHandle() { return fileHandle; }
    size_t optimalBlockSize() const { return 128 * 1024; }

private:
    FileHandle fileHandle;
};


class FileOutput : public FileBase
{
public:
    enum AccessFlag
    {
        ACC_OVERWRITE,
        ACC_CREATE_NEW
    };

    FileOutput(const Zstring& filepath, AccessFlag access); //throw FileError, ErrorTargetExisting
    FileOutput(FileHandle handle, const Zstring& filepath); //takes ownership!
    ~FileOutput();
    void close(); //throw FileError   -> optional, but good place to catch errors when closing stream!

    void write(const void* buffer, size_t bytesToWrite); //throw FileError
    FileHandle getHandle() { return fileHandle; }
    size_t optimalBlockSize() const { return 128 * 1024; }

    FileOutput(FileOutput&& tmp);

private:
    FileHandle fileHandle;
};
}

#endif //FILEIO_89578342758342572345

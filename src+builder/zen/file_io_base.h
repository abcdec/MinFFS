// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef FILEIO_BASE_H_INCLUDED_23432431789615314
#define FILEIO_BASE_H_INCLUDED_23432431789615314

#include "zstring.h"

namespace zen
{
class FileBase
{
public:
    const Zstring& getFilename() const { return filename_; }

protected:
    FileBase(const Zstring& filename) : filename_(filename)  {}
    ~FileBase() {}

private:
    FileBase           (const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;

    const Zstring filename_;
};


class FileInputBase : public FileBase
{
public:
    virtual size_t read(void* buffer, size_t bytesToRead) = 0; //throw FileError; returns actual number of bytes read
    bool eof() const { return eofReached; } //end of file reached

protected:
    FileInputBase(const Zstring& filename) : FileBase(filename), eofReached(false) {}
    ~FileInputBase() {}
    void setEof() { eofReached = true; }

private:
    bool eofReached;
};


class FileOutputBase : public FileBase
{
public:
    enum AccessFlag
    {
        ACC_OVERWRITE,
        ACC_CREATE_NEW
    };
    virtual void write(const void* buffer, size_t bytesToWrite) = 0; //throw FileError

protected:
    FileOutputBase(const Zstring& filename) : FileBase(filename) {}
    ~FileOutputBase() {}
};

}

#endif //FILEIO_BASE_H_INCLUDED_23432431789615314

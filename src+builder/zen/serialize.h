// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SERIALIZE_H_INCLUDED_83940578357
#define SERIALIZE_H_INCLUDED_83940578357

#include <functional>
#include <cstdint>
#include "string_base.h"
#include "file_io.h"


namespace zen
{
//high-performance unformatted serialization (avoiding wxMemoryOutputStream/wxMemoryInputStream inefficiencies)

/*
--------------------------
|Binary Container Concept|
--------------------------
binary container for data storage: must support "basic" std::vector interface (e.g. std::vector<char>, std::string, Zbase<char>)
*/

//binary container reference implementations
typedef Zbase<char> Utf8String; //ref-counted + COW text stream + guaranteed performance: exponential growth
class ByteArray;                //ref-counted       byte stream + guaranteed performance: exponential growth -> no COW, but 12% faster than Utf8String (due to no null-termination?)


class ByteArray //essentially a std::vector<char> with ref-counted semantics, but no COW! => *almost* value type semantics, but not quite
{
public:
    ByteArray() : buffer(std::make_shared<std::vector<char>>()) {}

    typedef std::vector<char>::value_type value_type;
    typedef std::vector<char>::iterator iterator;
    typedef std::vector<char>::const_iterator const_iterator;

    iterator begin() { return buffer->begin(); }
    iterator end  () { return buffer->end  (); }

    const_iterator begin() const { return buffer->begin(); }
    const_iterator end  () const { return buffer->end  (); }

    void resize(size_t len) { buffer->resize(len); }
    size_t size() const { return buffer->size(); }
    bool empty() const { return buffer->empty(); }

    inline friend bool operator==(const ByteArray& lhs, const ByteArray& rhs) { return *lhs.buffer == *rhs.buffer; }

private:
    std::shared_ptr<std::vector<char>> buffer; //always bound!
    //perf: shared_ptr indirection irrelevant: less than 1% slower!
};

//----------------------------------------------------------------------
//functions based on binary container abstraction
template <class BinContainer>         void saveBinStream(const Zstring& filepath, const BinContainer& cont, const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus); //throw FileError
template <class BinContainer> BinContainer loadBinStream(const Zstring& filepath,                           const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus); //throw FileError


/*
-----------------------------
|Binary Input Stream Concept|
-----------------------------
struct BinInputStream
{
    size_t read(void* data, size_t len); //return "len" bytes unless end of stream!
};

------------------------------
|Binary Output Stream Concept|
------------------------------
struct BinOutputStream
{
    void write(const void* data, size_t len);
};
*/

//binary input/output stream reference implementations:

template <class BinContainer>
struct MemoryStreamIn
{
    MemoryStreamIn(const BinContainer& cont) : buffer(cont), pos(0) {} //this better be cheap!

    size_t read(void* data, size_t len) //return "len" bytes unless end of stream!
    {
        static_assert(sizeof(typename BinContainer::value_type) == 1, ""); //expect: bytes
        const size_t bytesRead = std::min(len, buffer.size() - pos);
        auto itFirst = buffer.begin() + pos;
        std::copy(itFirst, itFirst + bytesRead, static_cast<char*>(data));
        pos += bytesRead;
        return bytesRead;
    }

private:
    const BinContainer buffer;
    size_t pos;
};

template <class BinContainer>
struct MemoryStreamOut
{
    void write(const void* data, size_t len)
    {
        static_assert(sizeof(typename BinContainer::value_type) == 1, ""); //expect: bytes
        const size_t oldSize = buffer.size();
        buffer.resize(oldSize + len);
        std::copy(static_cast<const char*>(data), static_cast<const char*>(data) + len, buffer.begin() + oldSize);
    }

    const BinContainer& ref() const { return buffer; }

private:
    BinContainer buffer;
};

//----------------------------------------------------------------------
//functions based on binary stream abstraction
template <class BinInputStream, class BinOutputStream>
void copyStream(BinInputStream& streamIn, BinOutputStream& streamOut, const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus); //optional

template <class N, class BinOutputStream> void writeNumber   (BinOutputStream& stream, const N& num);                 //
template <class C, class BinOutputStream> void writeContainer(BinOutputStream& stream, const C& str);                 //throw ()
template <         class BinOutputStream> void writeArray    (BinOutputStream& stream, const void* data, size_t len); //

//----------------------------------------------------------------------
class UnexpectedEndOfStreamError {};
template <class N, class BinInputStream> N    readNumber   (BinInputStream& stream); //throw UnexpectedEndOfStreamError (corrupted data)
template <class C, class BinInputStream> C    readContainer(BinInputStream& stream); //
template <         class BinInputStream> void readArray    (BinInputStream& stream, void* data, size_t len); //








//-----------------------implementation-------------------------------
template <class BinInputStream, class BinOutputStream> inline
void copyStream(BinInputStream& streamIn, BinOutputStream& streamOut, size_t blockSize,
                const std::function<void(std::int64_t bytesDelta)>& onNotifyCopyStatus) //optional
{
    assert(blockSize > 0);
    std::vector<char> buffer(blockSize);
    for (;;)
    {
        const size_t bytesRead = streamIn.read(&buffer[0], buffer.size());
        streamOut.write(&buffer[0], bytesRead);

        if (onNotifyCopyStatus)
            onNotifyCopyStatus(bytesRead); //throw X!

        if (bytesRead != buffer.size()) //end of file
            break;
    }
}


template <class BinContainer> inline
void saveBinStream(const Zstring& filepath, //throw FileError
                   const BinContainer& cont,
                   const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus) //optional
{
    MemoryStreamIn<BinContainer> streamIn(cont);
    FileOutput streamOut(filepath, zen::FileOutput::ACC_OVERWRITE); //throw FileError, (ErrorTargetExisting)
    if (onUpdateStatus) onUpdateStatus(0); //throw X!
    copyStream(streamIn, streamOut, streamOut.optimalBlockSize(), onUpdateStatus); //throw FileError
}


template <class BinContainer> inline
BinContainer loadBinStream(const Zstring& filepath, //throw FileError
                           const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus) //optional
{
    FileInput streamIn(filepath); //throw FileError, ErrorFileLocked
    if (onUpdateStatus) onUpdateStatus(0); //throw X!
    MemoryStreamOut<BinContainer> streamOut;
    copyStream(streamIn, streamOut, streamIn.optimalBlockSize(), onUpdateStatus); //throw FileError
    return streamOut.ref();
}


template <class BinOutputStream> inline
void writeArray(BinOutputStream& stream, const void* data, size_t len)
{
    stream.write(data, len);
}


template <class N, class BinOutputStream> inline
void writeNumber(BinOutputStream& stream, const N& num)
{
    static_assert(IsArithmetic<N>::value || IsSameType<N, bool>::value, "");
    writeArray(stream, &num, sizeof(N));
}


template <class C, class BinOutputStream> inline
void writeContainer(BinOutputStream& stream, const C& cont) //don't even consider UTF8 conversions here, we're handling arbitrary binary data!
{
    const auto len = cont.size();
    writeNumber(stream, static_cast<std::uint32_t>(len));
    if (len > 0)
        writeArray(stream, &*cont.begin(), sizeof(typename C::value_type) * len); //don't use c_str(), but access uniformly via STL interface
}


template <class BinInputStream> inline
void readArray(BinInputStream& stream, void* data, size_t len) //throw UnexpectedEndOfStreamError
{
    const size_t bytesRead = stream.read(data, len);
    if (bytesRead < len)
        throw UnexpectedEndOfStreamError();
}


template <class N, class BinInputStream> inline
N readNumber(BinInputStream& stream) //throw UnexpectedEndOfStreamError
{
    static_assert(IsArithmetic<N>::value || IsSameType<N, bool>::value, "");
    N num = 0;
    readArray(stream, &num, sizeof(N)); //throw UnexpectedEndOfStreamError
    return num;
}


template <class C, class BinInputStream> inline
C readContainer(BinInputStream& stream) //throw UnexpectedEndOfStreamError
{
    C cont;
    auto strLength = readNumber<std::uint32_t>(stream);
    if (strLength > 0)
    {
        try
        {
            cont.resize(strLength); //throw std::bad_alloc
        }
        catch (std::bad_alloc&) //most likely this is due to data corruption!
        {
            throw UnexpectedEndOfStreamError();
        }
        readArray(stream, &*cont.begin(), sizeof(typename C::value_type) * strLength); //throw UnexpectedEndOfStreamError
    }
    return cont;
}
}

#endif //SERIALIZE_H_INCLUDED_83940578357

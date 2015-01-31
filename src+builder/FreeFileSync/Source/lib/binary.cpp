// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.13        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "binary.h"
#include <zen/tick_count.h>
#include <vector>
#include <zen/file_io.h>
#include <boost/thread/tss.hpp>

using namespace zen;


namespace
{
inline
void setMinSize(std::vector<char>& buffer, size_t minSize)
{
    if (buffer.size() < minSize) //this is similar to reserve(), but we need a "properly initialized" array here
        buffer.resize(minSize);
}

class BufferSize
{
public:
    BufferSize() : bufSize(BUFFER_SIZE_START) {}

    void inc()
    {
        if (bufSize < BUFFER_SIZE_MAX)
            bufSize *= 2;
    }

    void dec()
    {
        if (bufSize > BUFFER_SIZE_MIN)
            bufSize /= 2;
    }

    operator size_t() const { return bufSize; }

private:
    static const size_t BUFFER_SIZE_MIN   =        64 * 1024;
    static const size_t BUFFER_SIZE_START =       128 * 1024; //initial buffer size
    static const size_t BUFFER_SIZE_MAX   = 16 * 1024 * 1024;

    /*Tests on Win7 x64 show that buffer size does NOT matter if files are located on different physical disks!
    Impact of buffer size when files are on same disk:

    buffer  MB/s
    ------------
    64      10
    128     19
    512     40
    1024    48
    2048    56
    4096    56
    8192    56
    */

    size_t bufSize;
};


const std::int64_t TICKS_PER_SEC = ticksPerSec();
}


bool zen::filesHaveSameContent(const Zstring& filepath1, const Zstring& filepath2, const std::function<void(std::int64_t bytesDelta)>& onUpdateStatus)
{
    static boost::thread_specific_ptr<std::vector<char>> cpyBuf1;
    static boost::thread_specific_ptr<std::vector<char>> cpyBuf2;
    if (!cpyBuf1.get())
        cpyBuf1.reset(new std::vector<char>());
    if (!cpyBuf2.get())
        cpyBuf2.reset(new std::vector<char>());

    std::vector<char>& memory1 = *cpyBuf1;
    std::vector<char>& memory2 = *cpyBuf2;

    FileInput file1(filepath1); //throw FileError
    FileInput file2(filepath2); //

    BufferSize bufferSize;

    TickVal lastDelayViolation = getTicks();

    do
    {
        setMinSize(memory1, bufferSize);
        setMinSize(memory2, bufferSize);

        const TickVal startTime = getTicks();

        const size_t length1 = file1.read(&memory1[0], bufferSize); //throw FileError
        const size_t length2 = file2.read(&memory2[0], bufferSize); //returns actual number of bytes read
        //send progress updates immediately after reading to reliably allow speed calculations for our clients!
        if (onUpdateStatus)
            onUpdateStatus(std::max(length1, length2));

        if (length1 != length2 || ::memcmp(&memory1[0], &memory2[0], length1) != 0)
            return false;

        //-------- dynamically set buffer size to keep callback interval between 100 - 500ms ---------------------
        if (TICKS_PER_SEC > 0)
        {
            const TickVal now = getTicks();

            const std::int64_t loopTime = dist(startTime, now) * 1000 / TICKS_PER_SEC; //unit: [ms]
            if (loopTime < 100)
            {
                if (dist(lastDelayViolation, now) / TICKS_PER_SEC > 2) //avoid "flipping back": e.g. DVD-Roms read 32MB at once, so first read may be > 500 ms, but second one will be 0ms!
                {
                    lastDelayViolation = now;
                    bufferSize.inc();
                }
            }
            else if (loopTime > 500)
            {
                lastDelayViolation = now;
                bufferSize.dec();
            }
        }
        //------------------------------------------------------------------------------------------------
    }
    while (!file1.eof());

    if (!file2.eof()) //highly unlikely, but possible! (but then again, not in this context where both files have same size...)
        return false;

    return true;
}

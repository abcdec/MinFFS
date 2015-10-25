// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "xml_io.h"
#include "file_access.h"
#include "file_io.h"
#include "serialize.h"

using namespace zen;


XmlDoc zen::loadXmlDocument(const Zstring& filepath) //throw FileError
{
    //can't simply use zen::loadBinStream() due to the short-circuit xml-validation below!

    std::string stream;

    FileInput inputFile(filepath); //throw FileError
    {
        //quick test whether input is an XML: avoid loading large binary files up front!
        const std::string xmlBegin = "<?xml version=";
        stream.resize(strLength(BYTE_ORDER_MARK_UTF8) + xmlBegin.size());

        const size_t bytesRead = inputFile.read(&stream[0], stream.size()); //throw FileError
        stream.resize(bytesRead);

        if (!startsWith(stream, xmlBegin) &&
            !startsWith(stream, BYTE_ORDER_MARK_UTF8 + xmlBegin)) //allow BOM!
            throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtFileName(filepath)));
    }

    const size_t blockSize = 128 * 1024;
    do
    {
        stream.resize(stream.size() + blockSize);

        const size_t bytesRead = inputFile.read(&*stream.begin() + stream.size() - blockSize, blockSize); //throw FileError
        if (bytesRead < blockSize)
            stream.resize(stream.size() - (blockSize - bytesRead)); //caveat: unsigned arithmetics
    }
    while (!inputFile.eof());

    try
    {
        return parse(stream); //throw XmlParsingError
    }
    catch (const XmlParsingError& e)
    {
        throw FileError(
            replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                             L"%x", fmtFileName(filepath)),
                                  L"%y", numberTo<std::wstring>(e.row + 1)),
                       L"%z", numberTo<std::wstring>(e.col + 1)));
    }
}


void zen::saveXmlDocument(const XmlDoc& doc, const Zstring& filepath) //throw FileError
{
    std::string stream = serialize(doc); //noexcept

    //only update xml file if there are real changes
    try
    {
        if (getFilesize(filepath) == stream.size()) //throw FileError
            if (loadBinStream<std::string>(filepath, nullptr) == stream) //throw FileError
                return;
    }
    catch (FileError&) {}

    FileOutput outputFile(filepath, FileOutput::ACC_OVERWRITE); //throw FileError
    outputFile.write(stream.c_str(), stream.length());          //
}


void zen::checkForMappingErrors(const XmlIn& xmlInput, const Zstring& filepath) //throw FileError
{
    if (xmlInput.errorsOccured())
    {
        std::wstring msg = _("Cannot read the following XML elements:") + L"\n";
        for (const std::wstring& elem : xmlInput.getErrorsAs<std::wstring>())
            msg += L"\n" + elem;

        throw FileError(replaceCpy(_("Configuration file %x loaded partially only."), L"%x", fmtFileName(filepath)) + L"\n\n" + msg);
    }
}

// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl.html       *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "concrete.h"
#include "native.h"
#ifdef ZEN_WIN_VISTA_AND_LATER
    #include "mtp.h"
    #include "sftp.h"
#endif

using namespace zen;
using ABF = AbstractBaseFolder;


std::unique_ptr<AbstractBaseFolder> zen::createAbstractBaseFolder(const Zstring& folderPathPhrase) //noexcept
{
    //greedy: try native evaluation first
    if (acceptsFolderPathPhraseNative(folderPathPhrase)) //noexcept
        return createBaseFolderNative(folderPathPhrase); //noexcept

    //then the rest:
#ifdef ZEN_WIN_VISTA_AND_LATER
    if (acceptsFolderPathPhraseMtp(folderPathPhrase)) //noexcept
        return createBaseFolderMtp(folderPathPhrase); //noexcept

    if (acceptsFolderPathPhraseSftp(folderPathPhrase)) //noexcept
        return createBaseFolderSftp(folderPathPhrase); //noexcept
#endif

    //no idea? => native!
    return createBaseFolderNative(folderPathPhrase);
}

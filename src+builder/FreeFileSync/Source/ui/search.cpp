// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "search.h"
#include <zen/string_tools.h>


using namespace zen;

namespace
{
template <bool respectCase>
class ContainsMatch
{
public:
    ContainsMatch(const wxString& textToFind) : textToFind_(textToFind) {}
    bool operator()(const wxString& phrase) const { return contains(phrase, textToFind_); }

private:
    wxString textToFind_;
};


template <>
class ContainsMatch<false>
{
public:
    ContainsMatch(const wxString& textToFind) : textToFind_(textToFind) { textToFind_.MakeUpper(); }
    bool operator()(wxString&& phrase) const
    {
        //wxWidgets::MakeUpper() is inefficient! But performance is not THAT important for this high-level search functionality
        phrase.MakeUpper();
        return contains(phrase, textToFind_);
    }

private:
    wxString textToFind_;
};

//###########################################################################################

template <bool respectCase>
ptrdiff_t findRow(const Grid& grid, //return -1 if no matching row found
                  const wxString& searchString,
                  size_t rowFirst, //specify area to search:
                  size_t rowLast)  // [rowFirst, rowLast)
{
    if (auto prov = grid.getDataProvider())
    {
        std::vector<Grid::ColumnAttribute> colAttr = grid.getColumnConfig();
        vector_remove_if(colAttr, [](const Grid::ColumnAttribute& ca) { return !ca.visible_; });
        if (!colAttr.empty())
        {
            const ContainsMatch<respectCase> containsMatch(searchString);

            for (size_t row = rowFirst; row < rowLast; ++row)
                for (auto iterCol = colAttr.begin(); iterCol != colAttr.end(); ++iterCol)
                    if (containsMatch(prov->getValue(row, iterCol->type_)))
                        return row;
        }
    }
    return -1;
}
}


std::pair<const Grid*, ptrdiff_t> zen::findGridMatch(const Grid& grid1, const Grid& grid2, const wxString& searchString, bool respectCase)
{
    const size_t rowCountL = grid1.getRowCount();
    const size_t rowCountR = grid2.getRowCount();

    std::pair<const Grid*, ptrdiff_t> result(nullptr, -1);

    size_t cursorRowL = grid1.getGridCursor();
    if (cursorRowL >= rowCountL)
        cursorRowL = 0;
    {
        auto finishSearch = [&](const Grid& grid, size_t rowFirst, size_t rowLast) -> bool
        {
            const ptrdiff_t targetRow = respectCase ?
            findRow<true>( grid, searchString, rowFirst, rowLast) :
            findRow<false>(grid, searchString, rowFirst, rowLast);
            if (targetRow >= 0)
            {
                result = std::make_pair(&grid, targetRow);
                return true;
            }
            return false;
        };

        if (!finishSearch(grid1, cursorRowL + 1, rowCountL))
            if (!finishSearch(grid2, 0, rowCountR))
                finishSearch(grid1, 0, cursorRowL + 1);
    }
    return result;
}
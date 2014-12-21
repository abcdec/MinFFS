// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "structures.h"
#include <iterator>
#include <stdexcept>
#include <ctime>
#include <zen/i18n.h>
#include <zen/time.h>

using namespace zen;


std::wstring zen::getVariantName(CompareVariant var)
{
    switch (var)
    {
        case CMP_BY_CONTENT:
            return _("File content");
        case CMP_BY_TIME_SIZE:
            return _("File time and size");
    }
    assert(false);
    return _("Error");
}


std::wstring zen::getVariantName(DirectionConfig::Variant var)
{
    switch (var)
    {
        case DirectionConfig::TWOWAY:
            return L"<- " + _("Two way") + L" ->";
        case DirectionConfig::MIRROR:
            return _("Mirror") + L" ->";
        case DirectionConfig::UPDATE:
            return _("Update") + L" >";
        case DirectionConfig::CUSTOM:
            return _("Custom");
    }
    assert(false);
    return _("Error");
}


DirectionSet zen::extractDirections(const DirectionConfig& cfg)
{
    DirectionSet output;
    switch (cfg.var)
    {
        case DirectionConfig::TWOWAY:
            throw std::logic_error("there are no predefined directions for automatic mode!");

        case DirectionConfig::MIRROR:
            output.exLeftSideOnly  = SyncDirection::RIGHT;
            output.exRightSideOnly = SyncDirection::RIGHT;
            output.leftNewer       = SyncDirection::RIGHT;
            output.rightNewer      = SyncDirection::RIGHT;
            output.different       = SyncDirection::RIGHT;
            output.conflict        = SyncDirection::RIGHT;
            break;

        case DirectionConfig::UPDATE:
            output.exLeftSideOnly  = SyncDirection::RIGHT;
            output.exRightSideOnly = SyncDirection::NONE;
            output.leftNewer       = SyncDirection::RIGHT;
            output.rightNewer      = SyncDirection::NONE;
            output.different       = SyncDirection::RIGHT;
            output.conflict        = SyncDirection::NONE;
            break;

        case DirectionConfig::CUSTOM:
            output = cfg.custom;
            break;
    }
    return output;
}


bool zen::detectMovedFilesSelectable(const DirectionConfig& cfg)
{
    if (cfg.var == DirectionConfig::TWOWAY)
        return false; //moved files are always detected since we have the database file anyway

    const DirectionSet tmp = zen::extractDirections(cfg);
    return (tmp.exLeftSideOnly  == SyncDirection::RIGHT &&
            tmp.exRightSideOnly == SyncDirection::RIGHT) ||
           (tmp.exLeftSideOnly  == SyncDirection::LEFT&&
            tmp.exRightSideOnly == SyncDirection::LEFT);
}


bool zen::detectMovedFilesEnabled(const DirectionConfig& cfg)
{
    return detectMovedFilesSelectable(cfg) ? cfg.detectMovedFiles : cfg.var == DirectionConfig::TWOWAY;
}


DirectionSet zen::getTwoWayUpdateSet()
{
    DirectionSet output;
    output.exLeftSideOnly  = SyncDirection::RIGHT;
    output.exRightSideOnly = SyncDirection::LEFT;
    output.leftNewer       = SyncDirection::RIGHT;
    output.rightNewer      = SyncDirection::LEFT;
    output.different       = SyncDirection::NONE;
    output.conflict        = SyncDirection::NONE;
    return output;
}


std::wstring MainConfiguration::getCompVariantName() const
{
    const CompareVariant firstVariant = firstPair.altCmpConfig.get() ?
                                        firstPair.altCmpConfig->compareVar :
                                        cmpConfig.compareVar; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const FolderPairEnh& fp : additionalPairs)
    {
        const CompareVariant thisVariant = fp.altCmpConfig.get() ?
                                           fp.altCmpConfig->compareVar :
                                           cmpConfig.compareVar; //fallback to main sync cfg
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
}


std::wstring MainConfiguration::getSyncVariantName() const
{
    const DirectionConfig::Variant firstVariant = firstPair.altSyncConfig.get() ?
                                                  firstPair.altSyncConfig->directionCfg.var :
                                                  syncCfg.directionCfg.var; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const FolderPairEnh& fp : additionalPairs)
    {
        const DirectionConfig::Variant thisVariant = fp.altSyncConfig.get() ?
                                                     fp.altSyncConfig->directionCfg.var :
                                                     syncCfg.directionCfg.var;
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
}


std::wstring zen::getSymbol(CompareFilesResult cmpRes)
{
    switch (cmpRes)
    {
        case FILE_LEFT_SIDE_ONLY:
            return L"only <-";
        case FILE_RIGHT_SIDE_ONLY:
            return L"only ->";
        case FILE_LEFT_NEWER:
            return L"newer <-";
        case FILE_RIGHT_NEWER:
            return L"newer ->";
        case FILE_DIFFERENT:
            return L"!=";
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA: //= sub-category of equal!
            return L"'=="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case FILE_CONFLICT:
            return L"conflict";
    }
    assert(false);
    return std::wstring();
}


std::wstring zen::getSymbol(SyncOperation op)
{
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
            return L"create <-";
        case SO_CREATE_NEW_RIGHT:
            return L"create ->";
        case SO_DELETE_LEFT:
            return L"delete <-";
        case SO_DELETE_RIGHT:
            return L"delete ->";
        case SO_MOVE_LEFT_SOURCE:
            return L"move from <-";
        case SO_MOVE_LEFT_TARGET:
            return L"move to <-";
        case SO_MOVE_RIGHT_SOURCE:
            return L"move from ->";
        case SO_MOVE_RIGHT_TARGET:
            return L"move to ->";
        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            return L"update <-";
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            return L"update ->";
        case SO_DO_NOTHING:
            return L" -";
        case SO_EQUAL:
            return L"'=="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case SO_UNRESOLVED_CONFLICT:
            return L"conflict";
    };
    assert(false);
    return std::wstring();
}


namespace
{
/*
int daysSinceBeginOfWeek(int dayOfWeek) //0-6, 0=Monday, 6=Sunday
{
    assert(0 <= dayOfWeek && dayOfWeek <= 6);
#ifdef ZEN_WIN
    DWORD firstDayOfWeek = 0;
    if (::GetLocaleInfo(LOCALE_USER_DEFAULT,                 //__in   LCID Locale,
                        LOCALE_IFIRSTDAYOFWEEK |             // first day of week specifier, 0-6, 0=Monday, 6=Sunday
                        LOCALE_RETURN_NUMBER,                //__in   LCTYPE LCType,
                        reinterpret_cast<LPTSTR>(&firstDayOfWeek),    //__out  LPTSTR lpLCData,
                        sizeof(firstDayOfWeek) / sizeof(TCHAR)) > 0) //__in   int cchData
    {
        assert(firstDayOfWeek <= 6);
        return (dayOfWeek + (7 - firstDayOfWeek)) % 7;
    }
    else //default
#endif
        return dayOfWeek; //let all weeks begin with monday
}
*/


std::int64_t resolve(size_t value, UnitTime unit, std::int64_t defaultVal)
{
    TimeComp locTimeStruc = zen::localTime();

    switch (unit)
    {
        case UTIME_NONE:
            return defaultVal;

        case UTIME_TODAY:
            locTimeStruc.second = 0; //0-61
            locTimeStruc.minute = 0; //0-59
            locTimeStruc.hour   = 0; //0-23
            return localToTimeT(locTimeStruc); //convert local time back to UTC

        //case UTIME_THIS_WEEK:
        //{
        //    localTimeFmt->tm_sec  = 0; //0-61
        //    localTimeFmt->tm_min  = 0; //0-59
        //    localTimeFmt->tm_hour = 0; //0-23
        //    const time_t timeFrom = ::mktime(localTimeFmt);

        //    int dayOfWeek = (localTimeFmt->tm_wday + 6) % 7; //tm_wday := days since Sunday	0-6
        //    // +6 == -1 in Z_7

        //    return std::int64_t(timeFrom) - daysSinceBeginOfWeek(dayOfWeek) * 24 * 3600;
        //}

        case UTIME_THIS_MONTH:
            locTimeStruc.second = 0; //0-61
            locTimeStruc.minute = 0; //0-59
            locTimeStruc.hour   = 0; //0-23
            locTimeStruc.day    = 1; //1-31
            return localToTimeT(locTimeStruc);

        case UTIME_THIS_YEAR:
            locTimeStruc.second = 0; //0-61
            locTimeStruc.minute = 0; //0-59
            locTimeStruc.hour   = 0; //0-23
            locTimeStruc.day    = 1; //1-31
            locTimeStruc.month  = 1; //1-12
            return localToTimeT(locTimeStruc);

        case UTIME_LAST_X_DAYS:
            locTimeStruc.second = 0; //0-61
            locTimeStruc.minute = 0; //0-59
            locTimeStruc.hour   = 0; //0-23
            return localToTimeT(locTimeStruc) - std::int64_t(value) * 24 * 3600;
    }

    assert(false);
    return localToTimeT(locTimeStruc);
}


std::uint64_t resolve(size_t value, UnitSize unit, std::uint64_t defaultVal)
{
    const std::uint64_t maxVal = std::numeric_limits<std::uint64_t>::max();

    switch (unit)
    {
        case USIZE_NONE:
            return defaultVal;
        case USIZE_BYTE:
            return value;
        case USIZE_KB:
            return value > maxVal / 1024U ? maxVal : //prevent overflow!!!
                   1024U * value;
        case USIZE_MB:
            return value > maxVal / (1024 * 1024U) ? maxVal : //prevent overflow!!!
                   1024 * 1024U * value;
    }
    assert(false);
    return defaultVal;
}
}

void zen::resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                       size_t sizeMin,  UnitSize unitSizeMin,
                       size_t sizeMax,  UnitSize unitSizeMax,
                       std::int64_t&  timeFrom,  //unit: UTC time, seconds
                       std::uint64_t& sizeMinBy, //unit: bytes
                       std::uint64_t& sizeMaxBy) //unit: bytes
{
    timeFrom  = resolve(timeSpan, unitTimeSpan, std::numeric_limits<std::int64_t>::min());
    sizeMinBy = resolve(sizeMin,  unitSizeMin, 0U);
    sizeMaxBy = resolve(sizeMax,  unitSizeMax, std::numeric_limits<std::uint64_t>::max());
}


namespace
{
FilterConfig mergeFilterConfig(const FilterConfig& global, const FilterConfig& local)
{
    FilterConfig out = local;

    //hard filter
    if (out.includeFilter == FilterConfig().includeFilter)
        out.includeFilter = global.includeFilter;
    //else: if both global and local include filter contain data, only local filter is preserved

    trim(out.excludeFilter, true, false);
    out.excludeFilter = global.excludeFilter + Zstr("\n") + out.excludeFilter;
    trim(out.excludeFilter, true, false);

    //soft filter
    std::int64_t  loctimeFrom  = 0;
    std::uint64_t locSizeMinBy = 0;
    std::uint64_t locSizeMaxBy = 0;
    resolveUnits(out.timeSpan, out.unitTimeSpan,
                 out.sizeMin,  out.unitSizeMin,
                 out.sizeMax,  out.unitSizeMax,
                 loctimeFrom,   //unit: UTC time, seconds
                 locSizeMinBy,  //unit: bytes
                 locSizeMaxBy); //unit: bytes

    //soft filter
    std::int64_t  glotimeFrom  = 0;
    std::uint64_t gloSizeMinBy = 0;
    std::uint64_t gloSizeMaxBy = 0;
    resolveUnits(global.timeSpan, global.unitTimeSpan,
                 global.sizeMin,  global.unitSizeMin,
                 global.sizeMax,  global.unitSizeMax,
                 glotimeFrom,
                 gloSizeMinBy,
                 gloSizeMaxBy);

    if (glotimeFrom > loctimeFrom)
    {
        out.timeSpan     = global.timeSpan;
        out.unitTimeSpan = global.unitTimeSpan;
    }
    if (gloSizeMinBy > locSizeMinBy)
    {
        out.sizeMin     = global.sizeMin;
        out.unitSizeMin = global.unitSizeMin;
    }
    if (gloSizeMaxBy < locSizeMaxBy)
    {
        out.sizeMax     = global.sizeMax;
        out.unitSizeMax = global.unitSizeMax;
    }
    return out;
}


inline
bool effectivelyEmpty(const FolderPairEnh& fp)
{
    auto isEmpty = [](Zstring dirpath)
    {
        trim(dirpath);
        return dirpath.empty();
    };
    return isEmpty(fp.dirpathPhraseLeft) && isEmpty(fp.dirpathPhraseRight);
}
}


MainConfiguration zen::merge(const std::vector<MainConfiguration>& mainCfgs)
{
    assert(!mainCfgs.empty());
    if (mainCfgs.empty())
        return MainConfiguration();

    if (mainCfgs.size() == 1) //mergeConfigFilesImpl relies on this!
        return mainCfgs[0];   //

    //merge folder pair config
    std::vector<FolderPairEnh> fpMerged;
    for (const MainConfiguration& mainCfg : mainCfgs)
    {
        std::vector<FolderPairEnh> fpTmp;

        //skip empty folder pairs
        if (!effectivelyEmpty(mainCfg.firstPair))
            fpTmp.push_back(mainCfg.firstPair);
        for (const FolderPairEnh& fp : mainCfg.additionalPairs)
            if (!effectivelyEmpty(fp))
                fpTmp.push_back(fp);

        //move all configuration down to item level
        for (FolderPairEnh& fp : fpTmp)
        {
            if (!fp.altCmpConfig.get())
                fp.altCmpConfig = std::make_shared<CompConfig>(mainCfg.cmpConfig);

            if (!fp.altSyncConfig.get())
                fp.altSyncConfig = std::make_shared<SyncConfig>(mainCfg.syncCfg);

            fp.localFilter = mergeFilterConfig(mainCfg.globalFilter, fp.localFilter);
        }
        vector_append(fpMerged, fpTmp);
    }

    if (fpMerged.empty())
        return MainConfiguration();

    //optimization: remove redundant configuration

    //########################################################################################################################
    //find out which comparison and synchronization setting are used most often and use them as new "header"
    std::vector<std::pair<CompConfig, int>> cmpCfgStat;
    std::vector<std::pair<SyncConfig, int>> syncCfgStat;
    for (const FolderPairEnh& fp : fpMerged)
    {
        //a rather inefficient algorithm, but it does not require a less-than operator:
        {
            const CompConfig& cmpCfg = *fp.altCmpConfig;

            auto it = std::find_if(cmpCfgStat.begin(), cmpCfgStat.end(),
            [&](const std::pair<CompConfig, int>& entry) { return effectivelyEqual(entry.first, cmpCfg); });
            if (it == cmpCfgStat.end())
                cmpCfgStat.emplace_back(cmpCfg, 1);
            else
                ++(it->second);
        }
        {
            const SyncConfig& syncCfg = *fp.altSyncConfig;

            auto it = std::find_if(syncCfgStat.begin(), syncCfgStat.end(),
            [&](const std::pair<SyncConfig, int>& entry) { return effectivelyEqual(entry.first, syncCfg); });
            if (it == syncCfgStat.end())
                syncCfgStat.emplace_back(syncCfg, 1);
            else
                ++(it->second);
        }
    }

    //set most-used comparison and synchronization settings as new header options
    const CompConfig cmpCfgHead = cmpCfgStat.empty() ? CompConfig() :
                                  std::max_element(cmpCfgStat.begin(), cmpCfgStat.end(),
    [](const std::pair<CompConfig, int>& lhs, const std::pair<CompConfig, int>& rhs) { return lhs.second < rhs.second; })->first;

    const SyncConfig syncCfgHead = syncCfgStat.empty() ? SyncConfig() :
                                   std::max_element(syncCfgStat.begin(), syncCfgStat.end(),
    [](const std::pair<SyncConfig, int>& lhs, const std::pair<SyncConfig, int>& rhs) { return lhs.second < rhs.second; })->first;
    //########################################################################################################################

    FilterConfig globalFilter;
    const bool allFiltersEqual = std::all_of(fpMerged.begin(), fpMerged.end(), [&](const FolderPairEnh& fp) { return fp.localFilter == fpMerged[0].localFilter; });
    if (allFiltersEqual)
        globalFilter = fpMerged[0].localFilter;

    //strip redundancy...
    for (FolderPairEnh& fp : fpMerged)
    {
        //if local config matches output global config we don't need local one
        if (fp.altCmpConfig &&
            effectivelyEqual(*fp.altCmpConfig, cmpCfgHead))
            fp.altCmpConfig.reset();

        if (fp.altSyncConfig &&
            effectivelyEqual(*fp.altSyncConfig, syncCfgHead))
            fp.altSyncConfig.reset();

        if (allFiltersEqual) //use global filter in this case
            fp.localFilter = FilterConfig();
    }

    //final assembly
    zen::MainConfiguration cfgOut;
    cfgOut.cmpConfig    = cmpCfgHead;
    cfgOut.syncCfg      = syncCfgHead;
    cfgOut.globalFilter = globalFilter;
    cfgOut.firstPair    = fpMerged[0];
    cfgOut.additionalPairs.assign(fpMerged.begin() + 1, fpMerged.end());
    cfgOut.onCompletion = mainCfgs[0].onCompletion;
    return cfgOut;
}

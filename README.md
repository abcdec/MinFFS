# MinFFS

A FreeFileSync modified for MinGW Build with Extensions


## Project Goal

- Make it possible to build binaries of FreeFileSync using MinGW.
- Make it possible to add features that would not be added to original FreeFileSync anytime soon.
- Provide better open source development environment
  - Complete set of source codes to be able to build. What you run is what you see in source code.
  - Automated testing and test result visibility.
  - Improve internal documentation
  - Adware free installer.
- Implement features like:
  - Optimize FFS for large volume file system, large size binary files, many name changed files.
  - Background indexing and resume interrupted comparison/synchronization.

## Source and binary distributions

- The program binaries and sources of MinFFS are distributed under [GNU General Public License Version 3.0] (http://www.gnu.org/licenses/gpl-3.0.en.html) per original FreeFileSync License. 
- You can download from GitHub Release page. https://github.com/abcdec/MinFFS/releases
  - Windows installer (MinFFS-Setup.exe) and Portable (MinFFS-Portable.exe) binary versions are availble. (Built binaries are uploaded.)
    - Windows installer will require admin privilege and will update Windows Registry to tell Windows that a uninstaller is available.
    - Portable installer will not require admin priviledge and by default it will install on Desktop\MinFFS-Portable. Remove directory to uninstall.
  - ZIP and tar.gz source bundles are avaialble as well (GitHub automatically create one from repo).
- If you need original FreeFileSync binary and source distribution, please go to http://www.freefilesync.org/

## How to build

### Prerequisites
- A modern PC running Microsoft Windows 7, 8.1 or 10 OS with sufficient diskspace
- Internet connection to download required build toolsets
- Some ideas how Windows, MinGW, Boost and other building environment work, and skill set to be able to troubleshoot.
  - It would be quite frustrating and time consuming experience if something goes wrong.

### List of dependent toolsets

The following toolsets are used to build MinFFS.  Down below you can find brief explanations and tips for installing these toolsets.

  - MinGW-w64 (required)
  - wxWidgets (required)
  - Boost (required)
  - Unicode NSIS (Optional for creating a binary installer package)
  - AutoIt (Optional for automated testing)
  - Doxygen (Optional for documentation generation)


### Installation Step 1. Install MinGW

MinGW-w64 must be installed first because it will be used to build wxWidgets and Boost libraries.

  - Download MinGW-w64 installer (mingw-w64-install.exe) from SourceForge http://sourceforge.net/projects/mingw-w64/
  - Run mingw-w64-install.exe and follow instructions to download and install actual MinGW Software packages. This README assumes MinGW is installed in C:\MinGW-w64 directory. (If not, please substitue all "C:\MinGW-w64" in this README with absolute path of your actual installation directory.  It is strongly suggested to install MinGW in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files (x86)".)
    - When installing, Settings dialogue box shows up.  Please select latest version (as of this writing version 5.2.0) and posix for thread.

NOTE: If MinGW-w64 has been installed already on your PC, please check the MinGW-w64 version.  The original FreeFileSync uses C++14 features and posix thread thus MinFFS follow the same. Your version of MinGW g++.exe needs to support C++14 option and posix thread to build MinFFS successfully.  See Toolset Versions section for versions that was used for successful builds.


### Installation Step 2. Install wxWidgets

Using MinGW, build wxWidgets libraries.  The following is the step by step instruction.

  - Download wxWidget installer https://www.wxwidgets.org/
  - Run installer.  It will place distribution to specified directory.  This README assumes wxWidgets is installed in C:\wxWidgets dirctory. (If not, please substitue all "C:\wxWidget" in this README with absolute path of your actual installation directory.    It is strongly suggested to install wxWidgets in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files".)
  - Set up environment variable WXWIN to wxWidget installation directory, and PATH to include MinGW binary path (C:\MinGW-w64\mingw32\bin)
```
    C:\> set WXWIN=C:\wxWidget-w64
    C:\> set PATH=C:\MinGW-w64\mingw32\bin;%PATH%
```

  - Change current directory to wxWidgets build directory and run make. (With MinGW-w64 5.2.0, need to somehow use -std=gnu++11 per http://stackoverflow.com/questions/27285706/trouble-using-wxwidgets-3-0-2-library-under-mingw-64 here we use gnu+14)
```
    D:\> C:
    C:\> cd C:\wxWidgets-w64\build\msw
    C:\wxWidgets-w64\build\msw> C:\MinGW-w64\mingw32\bin\mingw32-make.exe -f makefile.gcc CXXFLAGS="-std=gnu++14"
```

NOTE: It is strongly recommended to set WXWIN environment variable when building wxWidgets libraries although some wxWidgets documents say to set up the variable is "not necessary".  Some strange run-time exceptions ("Application stopped unexpectedly" due to Segmentation Fault) were observed and fix was to rebuild wxWidget with WXWIN defiend.

NOTE: After initial installation, if MinGW toolset version is updated for any reason, it is strongly recommended to rebuild wxWidgets libraries from scratch to avoid unnecessary troubles.


### Installation Step 3. Install Boost

Using MinGW, build Boost libraries.  The following is the step by step instruction.

  - Download Boost source zip https://www.boost.org/.  Pre-built binaries are not for MinGW, you will need to download source and build for MinGW.
    - The download package is like boost_1_58_0.zip found at http://sourceforge.net/projects/boost/files/boost/1.58.0. Note that the size of distribution package is relatively large (for example, boost-1_58_0.zip is 110.5MB) so download might take some time.
  - Create a tempoary build directory and unpack ZIP.  This README assumes the temporary build directory as D:\Builds and unpacked sources are under D:\Builds\boost_1_58_0.  Intallation target directory will be C:\Boost by default. (If changed, please substitue all "C:\Boost" in this README with absolute path of your actual installation directory.    It is strongly suggested to install Boost in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files".)
    - Expanding zip package also takes quite long time.  Please be patient.
  - If not done yet, set up environment variable PATH to include MinGW binary path (C:\MinGW-w64\mingw32\bin)
```
    C:\> set PATH=C:\MinGW-w64\mingw32\bin;%PATH%
```
  - Change current directory to boost build directory and run bootstrap.bat.
```
    C:\> D:
    D:\> cd D:\Builds\boost-1_58_0
    D:\Builds\boost-1_58_0> bootstrap.bat mingw
```
  - bootstrap.bat will create b2.exe.  Run b2 to build and install package.
    - Building boost libraries also takes quite long time.  Please be patient.
```
    D:\Builds\boost-1_58_0> b2 toolset=gcc variant=release threading=multi --prefix=C:\Boost-w64 --without-mpi --without-python install
```

NOTE: If MinGW toolset version is updated for any reason, it is recommended to rebuild Boost libraries from scratch to avoid unnecessary troubles.


### Installation Step 4. Install Unicode NSIS (Optional)

Installing Unicode NSIS is relatively strait forward.  Just install from normal installer, and place distribution to default location ("C:\Program Files (x86)\NSIS\Unicode" as of version 2.46.5).  Unicode NSIS can be downlaoded from http://www.scratchpaper.com/.


### Installation Step 5. Install AutoIt (Optional)

AutoIt is used to run pre-created test scripts and generates test results.  Installing AutoIt is relatively strait forward.  Just install from normal installer, and place distribution to default location ("C:\Program Files (x86)\AutoIt\" as of version v3.3.14.2).  AutoIt can be downlaoded from https://www.autoitscript.com/site/.


### Installation Step 6. Install Doxygen (Optional)

Doxygen is used to generate documentation for MinFFS.  Installing Doxygent is relatively strait forward.  Just install from normal installer, and place distribution to default location ("C:\Program Files\doxygen" as of version 1.18.10).  AutoIt can be downlaoded from http://www.doxygen.org/ (Download Windows Installer version like doxygen-1.8.10-setup.exe, and run to install.)


### Build MinFFS

Once all dependent toolsets have been installed, use the following steps to build MinFFS.


  - Place source code tree in a tempoary build directory.  In this README, D:\Builds\MinFFS is assumed.
  - Source tree would look like this:
```
    D:\Builds\MinFFS
                   + src+builder
                              + FreeFileSync
                                           + Build
                                           + Platforms
                                                    + MinGW
                                                    + MswCommon
                                           + Source
                              + wx+
                              + zen
                              + zenXml
			      + ExMinFFS
```
  - Chagne directory to src+builder\ExMinFFS
  - Set up environment variable WXWIN to wxWidget installation directory, and PATH to include MinGW binary path (C:\MinGW-w64\mingw32\bin)
```
    C:\> set WXWIN=C:\wxWidget
    C:\> set PATH=C:\MinGW-w64\mingw32\bin;%PATH%
```
  - Also adjust Makefile-cmdexe.mk MINGW_ROOT, WXWGT_ROOT, BOOST_ROOT, BOOST_VER, BOOST_MINGW according to your build toolset installation.
    - MINGW_ROOT: mingw32 directory under the MinGW installation directory (C:\MinGW-w64\mingw32 in this README)
    - WXWGT_ROOT: wxWidget installation directory (C:\wxWidgets-w64 in this README)
    - BOOST_ROOT: Boost installation directory (C:\Boost-w64 in this README)
    - BOOST_VER: Boost version installed. (1_58 in this README)
    - BOOST_MINGW: MinGW version string in Boost libarry name. (mgw52 for MinGW-w64 5.2 in this README)
  - Run b.bat to build binary MinFFS.exe
  - Run x.bat to run built MinFFS.exe.  It will copy necessary files to bin-debug and launch MinFFS.exe from bin-debug.
  - Run p.bat to create distributable binary package installer
    - p.bat may need to be modified if Unicode NSIS packager is not in installed in default location.  Update following line if needed.
  - Run t.bat to run pre-defined test scripts. The test requires AutoIt.  Test summary is stored as ExMinFFS_Test_Result_Summary.txt, and detailed logs are stored in ExMinFFS_Test_Result_Log.txt
  - Run g.bat to generate internal documentation

```
    "C:\Program Files (x86)\NSIS\Unicode\makensis.exe" MinFFS-Setup.nsi
```
  - Run c.bat to clean up build.


### Toolset Versions

As of Nov 15, 2015, MinFFS binary distribution is built by the following toolsets versions.

  - MinGW-w64 5.2.0
  - wxWidgets 3.0.2
  - Boost 1.58
  - Unicode NSIS 2.46.5
  - AutoIt v3.3.14.2
  - Doxygent 1.8.10

## Contact

For any questions related to MinFFS ditribution, build questions, bug report, please use MinFFS issues page at GitHub, https://github.com/abcdec/MinFFS/issues.

Please avoid contacting the original author of FreeFileSync for any bugs/issues seen when using MinFFS unless the same problem is reproducible with original FreeFileSync. If a problem is observed in the orginal FreeFileSync, it would be appropriate to contact the original author but only in context of the original FreeFileSync in order to avoid confusion.


## Links

- FreeFileSync http://www.freefilesync.org/
- MinGW-w64 http://mingw-w64.org/
- wxWidget https://www.wxwidgets.org/
- Boost https://www.boost.org/
- Unicode NSIS http://www.scratchpaper.com/
- AutoIt https://www.autoitscript.com/site/
- Doxygen http://www.doxygen.org/

## Development Notes

### Documentation

Documentation is to follow a "Low-Commitment Markup" strategy.
     - http://blog.hostilefork.com/low-commitment-doxygen-markup-cpp/



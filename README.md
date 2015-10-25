# MinFFS

A FreeFileSync modified for MinGW Build


## Project Goal

- Make it possible to build binaries of FreeFileSync using MinGW.
- Make it possible to add features that would not be added to original FreeFileSync anytime soon.
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
- A modern PC running Microsoft Windows 7 or 8.1 OS with sufficient diskspace
- Internet connection to download required build toolsets
- Some ideas how Windows, MinGW, Boost and other building environment work, and skill set to be able to troubleshoot.
  - It would be quite frustrating and time consuming experience if something goes wrong.

### List of dependent toolsets

The following toolsets are used to build MinFFS.  Down below you can find brief explanations and tips for installing these toolsets.

  - MinGW (required)
  - wxWidgets (required)
  - Boost (required)
  - Unicode NSIS (Optional for creating a binary installer package)


### Installation Step 1. Install MinGW

MinGW must be installed first because it will be used to build wxWidgets and Boost libraries.

  - Download MinGW installer mingw-get-setup.exe from http://www.mingw.org/
    - Latest mingw-get-setup.exe should be found at http://sourceforge.net/projects/mingw/files/Installer/
  - Run mingw-get-setup.exe and follow instructions to download and install actual MinGW Software packages. This README assumes MinGW is installed in C:\MinGW directory. (If not, please substitue all "C:\MinGW" in this README with absolute path of your actual installation directory.  It is strongly suggested to install MinGW in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files".)
    - MinFFS build requies only the MinGW base and binutil packages.  (Base includes compiler and linkers, binutils includes tools like windres.exe.)   For example, it does not require MSYS. No need to download and install MSYS and optional packages other than base binaries and binutils.

NOTE: If MinGW has been installed already on your PC, please check the MinGW version.  The original FreeFileSync uses C++11 features thus MinFFS follow the same. Your version of MinGW g++.exe needs to support C++11 option to build MinFFS successfully.  See Toolset Versions section for versions used for successful builds.


### Installation Step 2. Install wxWidgets

Using MinGW, build wxWidgets libraries.  The following is the step by step instruction.

  - Download wxWidget installer https://www.wxwidgets.org/
  - Run installer.  It will place distribution to specified directory.  This README assumes wxWidgets is installed in C:\wxWidgets dirctory. (If not, please substitue all "C:\wxWidget" in this README with absolute path of your actual installation directory.    It is strongly suggested to install wxWidgets in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files".)
  - Set up environment variable WXWIN to wxWidget installation directory, and PATH to include MinGW binary path (C:\MinGW\bin)
```
    C:\> set WXWIN=C:\wxWidget
    C:\> set PATH=C:\MinGW\bin;%PATH%
```
  - Change current directory to wxWidgets build directory and run make.
```
    D:\> C:
    C:\> cd C:\wxWidgets\build\msw
    C:\wxWidgets\build\msw> C:\MinGW\bin\mingw32-make.exe -f makefile.gcc
```

NOTE: It is strongly recommended to set WXWIN environment variable when building wxWidgets libraries although some wxWidgets documents say to set up the variable is "not necessary".  Some strange run-time exceptions ("Application stopped unexpectedly" due to Segmentation Fault) were observed and fix was to rebuild wxWidget with WXWIN defiend.

NOTE: After initial installation, if MinGW toolset version is updated for any reason, it is strongly recommended to rebuild wxWidgets libraries from scratch to avoid unnecessary troubles.


### Installation Step 3. Install Boost

Using MinGW, build Boost libraries.  The following is the step by step instruction.

  - Download Boost source zip https://www.boost.org/.  Pre-built binaries are not for MinGW, you will need to download source and build for MinGW.
    - The download package is like boost_1.57.0.zip found at http://sourceforge.net/projects/boost/files/boost/1.57.0. Note that the size of distribution package is relatively large (for example, boost-1.57.0.zip is 110.5MB) so download might take some time.
    - NOTE: MinFFS compile error occurs with boost_1.59.0 due to boost bug: https://svn.boost.org/trac/boost/ticket/11693. Use known working version (1.57.0.)
  - Create a tempoary build directory and unpack ZIP.  This README assumes the temporary build directory as D:\Builds and unpacked sources are under D:\Builds\boost_1_57_0.  Intallation target directory will be C:\Boost by default. (If changed, please substitue all "C:\Boost" in this README with absolute path of your actual installation directory.    It is strongly suggested to install Boost in a directory without white space in the path name string. e.g. avoid installing under "C:\Program Files".)
    - Expanding zip package also takes quite long time.  Please be patient.
  - If not done yet, set up environment variable PATH to include MinGW binary path (C:\MinGW\bin)
```
    C:\> set PATH=C:\MinGW\bin;%PATH%
```
  - Change current directory to boost build directory and run bootstrap.bat.
```
    C:\> D:
    D:\> cd D:\Builds\boost-1.57.0
    D:\Builds\boost-1.57.0> bootstrap.bat mingw
```
  - bootstrap.bat will create b2.exe.  Run b2 to build and install package.
    - Building boost libraries also takes quite long time.  Please be patient.
```
    D:\Builds\boost-1.57.0> b2 toolset=gcc variant=release threading=multi --without-mpi --without-python install
```

NOTE: If MinGW toolset version is updated for any reason, it is recommended to rebuild Boost libraries from scratch to avoid unnecessary troubles.


### Installation Step 4. Install Unicode NSIS

Installing Unicode NSIS is relatively strait forward.  Just install from normal installer, and place distribution to default location ("C:\Program Files (x86)\NSIS\Unicode" as of version 2.46.5).  Unicode NSIS can be downlaoded from http://www.scratchpaper.com/.


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
```
  - Chagne directory to src+builder\FreeFileSync\Platforms\MinGW
  - Set up environment variable WXWIN to wxWidget installation directory, and PATH to include MinGW binary path (C:\MinGW\bin)
```
    C:\> set WXWIN=C:\wxWidget
    C:\> set PATH=C:\MinGW\bin;%PATH%
```
  - Also adjust Makefile-cmdexe.mk MINGW_ROOT, WXWGT_ROOT, BOOST_ROOT, BOOST_VER, BOOST_MINGW according to your build toolset installation.
    - MINGW_ROOT: MinGW installation directory (C:\MinGW in this README)
    - WXWGT_ROOT: wxWidget installation directory (C:\wxWidgets in this README)
    - BOOST_ROOT: Boost installation directory (C:\Boost in this README)
    - BOOST_VER: Boost version installed. (1_57 in this README)
    - BOOST_MINGW: MinGW version string in Boost libarry name. (mgw48 for MinGW 4.8 in this README)
  - Run b.bat to build binary MinFFS.exe
  - Run x.bat to run built MinFFS.exe.  It will copy necessary files to bin-debug and launch MinFFS.exe from bin-debug.
  - Run p.bat to create distributable binary package installer
    - p.bat may need to be modified if Unicode NSIS packager is not in installed in default location.  Update following line if needed.

```
    "C:\Program Files (x86)\NSIS\Unicode\makensis.exe" MinFFS-Setup.nsi
```
  - Run c.bat to clean up build.


### Toolset Versions

As of Dec 31, 2014, MinFFS binary distribution is built by the following toolsets versions.

  - MinGW 4.8.1
  - wxWidgets 3.0.2
  - Boost 1.57
  - Unicode NSIS 2.46.5


## Contact

For any questions related to MinFFS ditribution, build questions, bug report, please use MinFFS issues page at GitHub, https://github.com/abcdec/MinFFS/issues.

Please avoid contacting the original author of FreeFileSync for any bugs/issues seen when using MinFFS unless the same problem is reproducible with original FreeFileSync. If a problem is observed in the orginal FreeFileSync, it would be appropriate to contact the original author but only in context of the original FreeFileSync in order to avoid confusion.


## Links

- FreeFileSync http://www.freefilesync.org/
- MinGW http://www.mingw.org/
- wxWidget https://www.wxwidgets.org/
- Boost https://www.boost.org/
- Unicode NSIS http://www.scratchpaper.com/
# Microsoft Developer Studio Project File - Name="MPEG2" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=MPEG2 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MPEG2.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MPEG2.mak" CFG="MPEG2 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MPEG2 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "MPEG2 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MPEG2 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MPEG2_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MPEG2_EXPORTS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /out:"..\bin\MPEG2.vdplugin"

!ELSEIF  "$(CFG)" == "MPEG2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MPEG2_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MPEG2_EXPORTS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"E:\Tools\VirtualDub-1.10.1\plugins32\MPEG2.vdplugin" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "MPEG2 - Win32 Release"
# Name "MPEG2 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\append.cpp
# End Source File
# Begin Source File

SOURCE=..\src\audio.cpp
# End Source File
# Begin Source File

SOURCE=..\src\DataVector.cpp
# End Source File
# Begin Source File

SOURCE=..\src\decode.cpp
# End Source File
# Begin Source File

SOURCE=..\src\index.cpp
# End Source File
# Begin Source File

SOURCE=..\src\infile.cpp
# End Source File
# Begin Source File

SOURCE=..\src\InfoDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\src\InputFileMPEG2.cpp
# End Source File
# Begin Source File

SOURCE=..\src\InputOptionsMPEG2.cpp
# End Source File
# Begin Source File

SOURCE=..\src\mpeg2_x64.asm
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\mpeg2_x86.asm

!IF  "$(CFG)" == "MPEG2 - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=..\src\mpeg2_x86.asm
InputName=mpeg2_x86

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	yasm -X vc -f win32 -o "$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "MPEG2 - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=..\src\mpeg2_x86.asm
InputName=mpeg2_x86

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	yasm -X vc -f win32 -o "$(IntDir)\$(InputName).obj" $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\MPEG2Decoder.cpp
# End Source File
# Begin Source File

SOURCE=..\src\parser.cpp
# End Source File
# Begin Source File

SOURCE=..\src\video.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\append.h
# End Source File
# Begin Source File

SOURCE=..\src\audio.h
# End Source File
# Begin Source File

SOURCE=..\src\DataVector.h
# End Source File
# Begin Source File

SOURCE=..\src\decode.h
# End Source File
# Begin Source File

SOURCE=..\src\idct_macros.inc
# End Source File
# Begin Source File

SOURCE=..\src\idct_tables.inc
# End Source File
# Begin Source File

SOURCE=..\src\infile.h
# End Source File
# Begin Source File

SOURCE=..\src\InputFileMPEG2.h
# End Source File
# Begin Source File

SOURCE=..\src\InputOptionsMPEG2.h
# End Source File
# Begin Source File

SOURCE=..\src\MPEG2.def
# End Source File
# Begin Source File

SOURCE=..\src\mpeg2_asm.h
# End Source File
# Begin Source File

SOURCE=..\src\mpeg2_tables.h
# End Source File
# Begin Source File

SOURCE=..\src\MPEG2Decoder.h
# End Source File
# Begin Source File

SOURCE=..\src\parser.h
# End Source File
# Begin Source File

SOURCE=..\src\resource.h
# End Source File
# Begin Source File

SOURCE=..\src\Unknown.h
# End Source File
# Begin Source File

SOURCE=..\src\vd2\plugin\vdinputdriver.h
# End Source File
# Begin Source File

SOURCE=..\src\vd2\plugin\vdplugin.h
# End Source File
# Begin Source File

SOURCE=..\src\video.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\src\MPEG2.rc
# End Source File
# End Group
# Begin Group "Priss"

# PROP Default_Filter "cpp;h;asm"
# Begin Source File

SOURCE=..\src\Priss\a64_polyphase.asm
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\Priss\bitreader.h
# End Source File
# Begin Source File

SOURCE=..\src\Priss\convert.cpp
# End Source File
# Begin Source File

SOURCE=..\src\vd2\Priss\convert.h
# End Source File
# Begin Source File

SOURCE=..\src\vd2\system\cpuaccel.h
# End Source File
# Begin Source File

SOURCE=..\src\vd2\Priss\decoder.h
# End Source File
# Begin Source File

SOURCE=..\src\Priss\engine.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Priss\engine.h
# End Source File
# Begin Source File

SOURCE=..\src\Priss\layer1.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Priss\layer2.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Priss\layer3.cpp
# ADD CPP /W1
# End Source File
# Begin Source File

SOURCE=..\src\Priss\layer3tables.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Priss\polyphase.cpp
# End Source File
# Begin Source File

SOURCE=..\src\Priss\polyphase.h
# End Source File
# Begin Source File

SOURCE=..\src\vd2\system\vdtypes.h
# End Source File
# End Group
# End Target
# End Project

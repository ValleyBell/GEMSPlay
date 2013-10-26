# Microsoft Developer Studio Project File - Name="GEMSPlay" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=GEMSPlay - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "GEMSPlay.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "GEMSPlay.mak" CFG="GEMSPlay - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "GEMSPlay - Win32 Release" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE "GEMSPlay - Win32 Debug" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE "GEMSPlay - Win32 Release 2xFM" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GEMSPlay - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_VC6"
# PROP BASE Intermediate_Dir "Release_VC6"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_VC6"
# PROP Intermediate_Dir "Release_VC6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "zlib" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /FR /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 winmm.lib /nologo /subsystem:console /machine:I386
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "GEMSPlay - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_VC6"
# PROP BASE Intermediate_Dir "Debug_VC6"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_VC6"
# PROP Intermediate_Dir "Debug_VC6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "DUAL_SUPPORT" /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "GEMSPlay - Win32 Release 2xFM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_VC6_2xFM"
# PROP BASE Intermediate_Dir "Release_VC6_2xFM"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_VC6_2xFM"
# PROP Intermediate_Dir "Release_VC6_2xFM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "zlib" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "DUAL_SUPPORT" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /W3 /GX /O2 /I "zlib" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "DUAL_SUPPORT" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib /nologo /subsystem:console /machine:I386
# SUBTRACT BASE LINK32 /nodefaultlib
# ADD LINK32 winmm.lib /nologo /subsystem:console /machine:I386 /out:"Release_VC6_2xFM/GEMSPlay_2x.exe"
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "GEMSPlay - Win32 Release"
# Name "GEMSPlay - Win32 Debug"
# Name "GEMSPlay - Win32 Release 2xFM"
# Begin Group "Quellcodedateien"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\2612intf.c
# End Source File
# Begin Source File

SOURCE=.\src\dirent.c
# End Source File
# Begin Source File

SOURCE=.\src\fm2612.c
# End Source File
# Begin Source File

SOURCE=.\GemsPlay.c
# End Source File
# Begin Source File

SOURCE=.\src\loader.c
# End Source File
# Begin Source File

SOURCE=.\src\main.c
# End Source File
# Begin Source File

SOURCE=.\src\sn76489.c
# End Source File
# Begin Source File

SOURCE=.\src\sn764intf.c
# End Source File
# Begin Source File

SOURCE=.\src\sound.c
# End Source File
# Begin Source File

SOURCE=.\src\Stream.c
# End Source File
# Begin Source File

SOURCE=.\src\vgmwrite.c
# End Source File
# End Group
# Begin Group "Header-Dateien"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\2612intf.h
# End Source File
# Begin Source File

SOURCE=.\src\common.h
# End Source File
# Begin Source File

SOURCE=.\src\dirent.h
# End Source File
# Begin Source File

SOURCE=.\src\fm.h
# End Source File
# Begin Source File

SOURCE=.\GemsPlay.h
# End Source File
# Begin Source File

SOURCE=.\src\loader.h
# End Source File
# Begin Source File

SOURCE=.\src\mamedef.h
# End Source File
# Begin Source File

SOURCE=.\src\sn76489.h
# End Source File
# Begin Source File

SOURCE=.\src\sn764intf.h
# End Source File
# Begin Source File

SOURCE=.\src\sound.h
# End Source File
# Begin Source File

SOURCE=.\src\stdbool.h
# End Source File
# Begin Source File

SOURCE=.\src\stdtype.h
# End Source File
# Begin Source File

SOURCE=.\src\Stream.h
# End Source File
# Begin Source File

SOURCE=.\src\types.h
# End Source File
# Begin Source File

SOURCE=.\src\vgmwrite.h
# End Source File
# End Group
# Begin Group "Ressourcendateien"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project

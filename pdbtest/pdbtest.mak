# Microsoft Visual C++ Generated NMAKE File, Format Version 30003
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "pdbtest.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "Win32 Debug"
RSC=rc.exe
CPP=cl.exe
F90=fl32.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir WinRel
# PROP BASE Intermediate_Dir WinRel
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir WinRel
# PROP Intermediate_Dir WinRel
OUTDIR=.\WinRel
INTDIR=.\WinRel

ALL : ".\WinRel\pdbtest.exe" ".\WinRel\pdbtest.bsc"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/nul" mkdir "$(OUTDIR)"

# ADD BASE F90 /I "WinRel/"
# ADD F90 /I "WinRel/"
F90_OBJS=.\WinRel/
# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
CPP_PROJ=/nologo /ML /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE"\
 /FR"$(INTDIR)/" /Fp"$(INTDIR)/pdbtest.pch" /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\WinRel/
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /Iu /o"$(OUTDIR)/pdbtest.bsc" 
BSC32_SBRS= \
	".\WinRel\pdbtest.sbr"

".\WinRel\pdbtest.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 mspdb.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386
LINK32_FLAGS=mspdb.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/pdbtest.pdb" /machine:I386\
 /out:"$(OUTDIR)/pdbtest.exe" 
DEF_FILE=
LINK32_OBJS= \
	".\WinRel\pdbtest.obj"

".\WinRel\pdbtest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir WinDebug
# PROP BASE Intermediate_Dir WinDebug
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir WinDebug
# PROP Intermediate_Dir WinDebug
OUTDIR=.\WinDebug
INTDIR=.\WinDebug

ALL : ".\WinDebug\pdbtest.exe" ".\WinDebug\pdbtest.bsc"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/nul" mkdir "$(OUTDIR)"

# ADD BASE F90 /I "WinDebug/"
# ADD F90 /I "WinDebug/"
F90_OBJS=.\WinDebug/
# ADD BASE CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
CPP_PROJ=/nologo /MLd /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE"\
 /FR"$(INTDIR)/" /Fp"$(INTDIR)/pdbtest.pch" /Fo"$(INTDIR)/"\
 /Fd"$(OUTDIR)/pdbtest.pdb" /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /Iu /o"$(OUTDIR)/pdbtest.bsc" 
BSC32_SBRS= \
	".\WinDebug\pdbtest.sbr"

".\WinDebug\pdbtest.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 mspdb.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386
LINK32_FLAGS=mspdb.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/pdbtest.pdb" /debug\
 /machine:I386 /out:"$(OUTDIR)/pdbtest.exe" 
DEF_FILE=
LINK32_OBJS= \
	".\WinDebug\pdbtest.obj"

".\WinDebug\pdbtest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

F90_PROJ=/I "WinRel/" /Fo"WinRel/" 

.for{$(F90_OBJS)}.obj:
   $(F90) $(F90_PROJ) $<  

.f{$(F90_OBJS)}.obj:
   $(F90) $(F90_PROJ) $<  

.f90{$(F90_OBJS)}.obj:
   $(F90) $(F90_PROJ) $<  

################################################################################
# Begin Target

# Name "Win32 Release"
# Name "Win32 Debug"
################################################################################
# Begin Source File

SOURCE=.\pdbtest.cpp

!IF  "$(CFG)" == "Win32 Release"

".\WinRel\pdbtest.obj" : $(SOURCE) "$(INTDIR)"

!ELSEIF  "$(CFG)" == "Win32 Debug"

".\WinDebug\pdbtest.obj" : $(SOURCE) "$(INTDIR)"

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################

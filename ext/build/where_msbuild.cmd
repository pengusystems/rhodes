:: Copyright 2017 - Refael Ackermann
:: Distributed under MIT style license
:: See accompanying file LICENSE at https://github.com/node4good/windows-autoconf
:: version: 2.0.0

@REM GD modified from vswhere_usability_wrapper.cmd

@if not defined DEBUG_HELPER @ECHO OFF
setlocal
if "%~1"=="prerelease" set VSWHERE_WITH_PRERELEASE=1
set "InstallerPath=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%InstallerPath%" set "InstallerPath=%ProgramFiles%\Microsoft Visual Studio\Installer"
if not exist "%InstallerPath%" goto :no-vswhere
:: Manipulate %Path% for easier " handeling
set "Path=%Path%;%InstallerPath%"
where vswhere 2> nul > nul
if errorlevel 1 goto :no-vswhere
set VSWHERE_REQ=-requires Microsoft.Component.MSBuild
set VSWHERE_PRP=-find MSBuild
vswhere -prerelease > nul
if not errorlevel 1 if "%VSWHERE_WITH_PRERELEASE%"=="1" set "VSWHERE_LMT=%VSWHERE_LMT% -prerelease"
SET VSWHERE_ARGS=-latest %VSWHERE_REQ% %VSWHERE_PRP%
for /f "usebackq tokens=*" %%i in (`vswhere %VSWHERE_ARGS%`) do (
	endlocal

	set "MSBUILD_X64=%%i\Current\Bin\amd64\MSBuild.exe"
	set "MSBUILD_X86=%%i\Current\Bin\MSBuild.exe"

	echo set "MSBUILD_X64=%%i\Current\Bin\amd64\MSBuild.exe"
	echo set "MSBUILD_X86=%%i\Current\Bin\MSBuild.exe"

	exit /B 0
)

:no-vswhere
endlocal
echo could not find "msbuild"
exit /B 1

:: Builds and creates a package folder with a zip for iris
@echo off
setlocal enabledelayedexpansion
set repo_root=%~dp0..
set output_dir=%repo_root%\deployed

:: Build and deploy the executables
pushd ..\ext\build\
call "where_msbuild.cmd"
"%MSBUILD_X64%" /m %repo_root%/src/apps/iris/iris.sln /property:Configuration="ReleaseNoOpt";Platform="x64"
call "copy_dlls_to_dev_folders.cmd" 1>nul 2>nul
popd
set files_to_zip= ^
	iris.exe ^
	alazar_daq.dll ^
	glv.dll ^
	ATSApi.dll

pushd ..\ReleaseNoOpt_x64
if exist .%output_dir%/shared.zip del /f %output_dir%/shared.zip
"..\ext\7zip\7z.exe" a -tzip -mx9 -r %output_dir%/shared.zip %files_to_zip%
popd
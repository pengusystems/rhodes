rem Copy .DLL files from \ext folder to build target folder
@rem Expects to be run from folder ext\Build
@rem note: Destination folders must have trailing slash or they will be considered file names if folder is missing

rem restore path to batch file folder
cd %~dp0 

@rem optionally copy to release only
if "%~1" NEQ "" (
	call:function_Release_x64 %1
) ELSE (
	call:function_ReleaseNoOpt_x64 "..\..\ReleaseNoOpt_x64\"
	call:function_Release_x64 "..\..\Release_x64\"
	call:function_Debug_x64 "..\..\Debug_x64\"
)


goto:eof

:function_Debug_x64
rem ----------Debug_x64----------
set dst_folder=%~1
rem [ alazartech ]
copy /Y "..\alazartech\ATS-SDK\7.1.5\Bin\ATSApi.dll" %dst_folder%
goto:eof

:function_Release_x64
rem ----------Release_x64----------
set dst_folder=%~1
rem [ redrapids ]
copy /Y "..\red_rapids\win\drivers\win_x86-64\lib\RRadapter.dll" %dst_folder%
copy /Y "..\red_rapids\win\drivers\win_x86-64\lib\wdapi1260.dll" %dst_folder%
goto:eof

:function_ReleaseNoOpt_x64
rem ----------ReleaseNoOpt_x64----------
set dst_folder=%~1
rem [ alazartech ]
copy /Y "..\alazartech\ATS-SDK\7.1.5\Bin\ATSApi.dll" %dst_folder%
goto:eof


goto:eof

@echo off
REM Signing script
REM Arguments: sign.bat exe_to_sign certificate_subject_name

REM This is a loose wrapper around the Microsoft signtool application (included in the Windows SDK).
REM See https://docs.microsoft.com/en-us/dotnet/framework/tools/signtool-exe for more details.

REM Path to signtool.exe
IF NOT DEFINED SIGNTOOL_PATH (SET SIGNTOOL_PATH=signtool)

REM URL of the timestamp server
IF NOT DEFINED SIGNTOOL_TIMESTAMP_URL (SET SIGNTOOL_TIMESTAMP_URL=http://timestamp.digicert.com)

REM Sign with SHA-1 for Windows 7 and below
"%SIGNTOOL_PATH%" sign -v -n %2 -t %SIGNTOOL_TIMESTAMP_URL% -fd sha1 %1

REM Sign with SHA-256 for Windows 8 and above
"%SIGNTOOL_PATH%" sign -v -n %2 -tr %SIGNTOOL_TIMESTAMP_URL% -fd sha256 -td sha256 -as %1

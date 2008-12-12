@echo off

set foo=a b

if "%COMPUTERNAME%"=="REALITY-IQTNB9Z" (
  echo Setting for REALITY-IQTNB9Z
@rem  SET ITK_PATH=%HOME%\build\itk-3.6.0-review-vs2005\bin\Release
@rem  SET PLASTIMATCH_PATH=%HOME%\build\plastimatch-vs2005\Release
  SET ITK_PATH=%HOME%\build\itk-3.8-vs2005\bin\Release
  SET PLASTIMATCH_PATH=%HOME%\build\plastimatch-itk-3.8-vs2005\Release
) else if "%COMPUTERNAME%"=="COTTONTAIL" (
  echo Setting for COTTONTAIL
  SET ITK_PATH=%HOME%\build\itk-3.8.0-vse2005\bin\Release
  SET PLASTIMATCH_PATH=%HOME%\build\plastimatch-vse2005\Release
  SET DXSDK_UTILS_DIR="C:\Program Files\Microsoft DirectX SDK (June 2007)\Utilities\Bin\x86"
) else if "%COMPUTERNAME%"=="SLUMBER" (
  echo Setting for SLUMBER
  SET ITK_PATH=%HOME%\build\itk-3.6.0\bin\Release
  SET PLASTIMATCH_PATH=%HOME%\build\plastimatch\Release
) else if "%COMPUTERNAME%"=="ROFOVIA" (
  echo Setting for ROFOVIA
  SET ITK_PATH=C:\deform\build\InsightToolkit-3.6.0-build\bin\Release
  SET PLASTIMATCH_PATH=c:\deform\build\plastimatch\Release
) else (
  echo "Sorry, couldn't recognize host"
  exit /b
)

SET CTTOOLS_PATH=%HOME%\projects\plastimatch\cttools

if NOT %DXSDK_UTILS_DIR%=="" PATH=%PATH%;%DXSDK_UTILS_DIR%
@rem if NOT "%DXSDK_UTILS_DIR%"=="" PATH=%PATH%;%DXSDK_UTILS_DIR%

PATH=%PATH%;%ITK_PATH%
PATH=%PATH%;%PLASTIMATCH_PATH%
PATH=%PATH%;%CTTOOLS_PATH%

echo PATH set to:
PATH

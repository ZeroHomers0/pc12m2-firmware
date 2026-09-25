@echo off
setlocal
chcp 65001 >nul

rem Always run from the extracted package directory so .\release\firmware.bin is found.
pushd "%~dp0"
echo Starting PC12M-2 firmware flash...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_release.ps1" %*
set "FLASH_EXIT_CODE=%ERRORLEVEL%"

echo.
if "%FLASH_EXIT_CODE%"=="0" (
  color 2F
  echo Flash tool finished successfully.
) else (
  color 4F
  echo Flash tool failed with exit code %FLASH_EXIT_CODE%.
)
echo Press any key to close this window...
pause >nul
popd
exit /b %FLASH_EXIT_CODE%

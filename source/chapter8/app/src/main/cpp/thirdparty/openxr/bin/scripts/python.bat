@rem This script attempts to locate python.exe and executes it
@rem with the requested command-line parameters.

@setlocal enableextensions enabledelayedexpansion

@set P=

@python --version 2>NUL
@if errorlevel 1 (
    @echo.
    @echo ERROR^: Cannot find python. Make sure it is installed and has been added to your 'Path' system environment variable.
) else (
    @python %1 %2 %3 %4 %5 %6
)

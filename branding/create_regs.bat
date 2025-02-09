@echo off
setlocal EnableDelayedExpansion

:: Get the root project directory (parent of branding folder)
set PROJECT_DIR=%~dp0..

:: Convert to absolute path
pushd %PROJECT_DIR%
set PROJECT_DIR=%CD%
popd

:: Create the .reg file with clean path
(
echo Windows Registry Editor Version 5.00
echo.
echo [HKEY_CLASSES_ROOT\.willmodel]
echo @="WillEngineModel"
echo.
echo [HKEY_CLASSES_ROOT\WillEngineModel]
echo @="WillEngine Model File"
echo.
echo [HKEY_CLASSES_ROOT\WillEngineModel\DefaultIcon]
echo @="%PROJECT_DIR:\=\\%\\assets\\icons\\WillModel.ico"
) > "willmodel_association.reg"
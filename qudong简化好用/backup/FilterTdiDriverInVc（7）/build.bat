if "%1" == "" goto InvalidParameter

if not exist %1\bin\setenv.bat goto SetenvNotFound

call %1\bin\setenv.bat %1 %2
%3
cd %4
build
goto exit

:InvalidParameter
echo Invalid Parameter.
goto exit

:SetenvNotFound
echo Can't found Setenv.bat.
goto exit

:exit
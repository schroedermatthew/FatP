@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d "C:\Users\mtthw\Desktop\AI Projects\FatP"
cl /std:c++20 /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\include\fat_p components\Xml\tests\test_XmlLite.cpp /Fe:_verify_xmllite\test.exe /link advapi32.lib
if errorlevel 1 exit /b 1
_verify_xmllite\test.exe
if errorlevel 1 exit /b 1
cl /std:c++20 /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\include\fat_p components\Xml\tests\test_XmlLite_HeaderSelfContained.cpp /Fe:_verify_xmllite\test_header.exe /link advapi32.lib
if errorlevel 1 exit /b 1
_verify_xmllite\test_header.exe
exit /b %ERRORLEVEL%
@echo off
rem @tool Math ライブラリの単体テストを、規約（行 / 列ベクトル、左手 / 右手系、深度の向き）と SIMD の全組み合わせでビルドして実行する
rem @tool 使い方：run_math_tests.bat [Debug | Release]（省略時は Release）
setlocal

set CONFIGURATION=%~1
if "%CONFIGURATION%"=="" set CONFIGURATION=Release

set PROJECT=%~dp0MathTest.vcxproj
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do set MSBUILD=%%i
if not defined MSBUILD (
	echo MSBuild が見つかりません
	exit /b 1
)

set FAILED=0
rem @tool 名前 / MSBuild プロパティ（math_config.h の設定）/ 追加のコンパイラオプション
call :run row_lh              ""                                                                         ""
call :run row_rh              "/p:MathRightHanded=1"                                                     ""
call :run column_lh           "/p:MathColumnVector=1"                                                    ""
call :run column_rh           "/p:MathColumnVector=1 /p:MathRightHanded=1"                               ""
call :run row_lh_normal_depth "/p:MathReverseDepth=0"                                                    ""
call :run column_rh_normal    "/p:MathColumnVector=1 /p:MathRightHanded=1 /p:MathReverseDepth=0"         ""
call :run scalar              "/p:MathDisableSimd=true"                                                  ""
call :run avx2                ""                                                                         "/arch:AVX2"
echo.
if %FAILED%==0 (
	echo すべての構成でテストに成功しました
	exit /b 0
)
echo %FAILED% 個の構成でテストに失敗しました
exit /b 1

:run
echo.
echo ==== %~1 ====
"%MSBUILD%" "%PROJECT%" /nologo /v:minimal /p:Configuration=%CONFIGURATION% /p:Platform=x64 /p:MathTestVariant=%~1 %~2 "/p:MathTestOptions=%~3" > "%TEMP%\math_test_build_%~1.log"
if errorlevel 1 (
	type "%TEMP%\math_test_build_%~1.log"
	echo ビルドに失敗しました：%~1
	set /a FAILED+=1
	exit /b 0
)
"%~dp0x64\%CONFIGURATION%\%~1\MathTest.exe"
if errorlevel 1 set /a FAILED+=1
exit /b 0

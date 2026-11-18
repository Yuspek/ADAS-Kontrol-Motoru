@echo off
chcp 65001 >nul
echo ====================================================
echo    ADAS Karar ve Kontrol Motoru Baslatiliyor...
echo ====================================================
echo Tarayicida aciliyor: http://localhost:8080
start "" "http://localhost:8080"

if exist "build\Release\AdasDecisionEngine.exe" (
    "build\Release\AdasDecisionEngine.exe"
) else if exist "Release\AdasDecisionEngine.exe" (
    "Release\AdasDecisionEngine.exe"
) else (
    echo [HATA] AdasDecisionEngine.exe bulunamadi! Lutfen once projeyi derleyin.
    pause
)

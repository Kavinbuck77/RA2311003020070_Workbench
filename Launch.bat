@echo off
title PASS Science Works - Unified Engineering Workbench
echo =======================================================================
echo   PASS SCIENCE WORKS R&D BENCH 2026-2027
echo   Unified Engineering Workbench
echo   Candidate: KAVIN K K (RA2311003020070)
echo =======================================================================
echo.
cd /d "%~dp0"

echo [*] Initializing Native Engine (running baseline FEA, multicore, neural checks)...
echo [*] Browser will open automatically once the engine is ready...
echo.
workbench_server.exe 8080
pause

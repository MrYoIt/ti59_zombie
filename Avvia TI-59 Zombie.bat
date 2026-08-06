@echo off
REM Avvia TI-59 Zombie.bat — doppio click per trovare l'emulatore
REM sulla rete e aprire la sua pagina web. Non serve installare nulla:
REM usa PowerShell, gia' incluso in Windows.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch_ti59.ps1"

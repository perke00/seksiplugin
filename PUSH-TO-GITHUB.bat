@echo off
setlocal
cd /d "%~dp0"
echo ============================================
echo   KITSUNE EQ - upload to GitHub (starts build)
echo ============================================
echo.
where git >nul 2>nul
if errorlevel 1 (
  echo Git is not installed. Download it from https://git-scm.com/download/win , install, then run this file again.
  pause & exit /b 1
)
if not exist ".git" (
  git init
  git branch -M main
)
git remote get-url origin >nul 2>nul
if errorlevel 1 (
  echo Create an EMPTY repo on github.com first, then paste its URL below.
  echo Example: https://github.com/yourname/KitsuneEQ.git
  echo.
  set /p REPO=Repo URL: 
  git remote add origin %REPO%
)
git add .
git commit -m "KITSUNE EQ update"
git push -u origin main
if errorlevel 1 (
  echo.
  echo Push failed. Check the repo URL / login window and try again.
  pause & exit /b 1
)
echo.
echo Done! Open your repo on GitHub - Actions tab - wait for the green check - download the installer from Artifacts.
pause

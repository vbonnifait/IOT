@echo off
REM Script d'installation et de configuration du projet (Windows)
REM Détection de Crises Épileptiques - ESP32 + BITalino

setlocal enabledelayedexpansion

echo ╔══════════════════════════════════════════════════════════════╗
echo ║                                                              ║
echo ║     INSTALLATION - DÉTECTION CRISES ÉPILEPTIQUES            ║
echo ║          BITalino EEG + ESP32 + TinyML                      ║
echo ║                                                              ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

REM 1. Vérifier Python
echo 🔍 Vérification de Python...
where python >nul 2>nul
if %errorlevel% equ 0 (
    for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
    echo    ✓ Python !PYTHON_VERSION! trouvé
) else (
    echo    ❌ Python non trouvé. Veuillez l'installer:
    echo       https://www.python.org/downloads/
    pause
    exit /b 1
)

REM 2. Vérifier/Installer PlatformIO
echo.
echo 🔍 Vérification de PlatformIO...
where pio >nul 2>nul
if %errorlevel% equ 0 (
    for /f "tokens=*" %%i in ('pio --version 2^>^&1') do set PIO_VERSION=%%i
    echo    ✓ PlatformIO trouvé: !PIO_VERSION!
) else (
    echo    ⚠️  PlatformIO non trouvé. Installation...
    python -m pip install platformio
    if %errorlevel% equ 0 (
        echo    ✓ PlatformIO installé avec succès
    ) else (
        echo    ❌ Erreur lors de l'installation de PlatformIO
        pause
        exit /b 1
    )
)

REM 3. Installer les dépendances Python (optionnel)
echo.
echo 🔍 Installation des outils Python (optionnel)...
if exist "tools\requirements.txt" (
    set /p INSTALL_TOOLS="   Installer les outils Python de visualisation? (o/N) "
    if /i "!INSTALL_TOOLS!"=="o" (
        python -m pip install -r tools\requirements.txt
        echo    ✓ Outils Python installés
    ) else (
        echo    ⊘ Outils Python ignorés
    )
) else (
    echo    ⊘ Fichier requirements.txt non trouvé
)

REM 4. Détecter les ports série disponibles
echo.
echo 🔍 Détection des ports série...
echo    Ports disponibles:
pio device list | findstr "COM"
echo.

REM 5. Configuration du port
echo ⚙️  Configuration du port série...
set /p PORT="   Entrer le port série (ex: COM3) ou laisser vide pour garder COM3: "
if "!PORT!"=="" set PORT=COM3
echo    Port configuré: !PORT!

REM Mettre à jour platformio.ini
if exist "platformio.ini" (
    copy /y platformio.ini platformio.ini.bak >nul
    
    REM Créer un fichier temporaire avec le nouveau contenu
    powershell -Command "(Get-Content platformio.ini) -replace '^upload_port = .*', 'upload_port = !PORT!' | Set-Content platformio.ini.tmp"
    move /y platformio.ini.tmp platformio.ini >nul
    
    echo    ✓ Port mis à jour dans platformio.ini
)

REM 6. Test de compilation
echo.
echo 🔨 Test de compilation du projet...
set /p COMPILE="   Compiler le projet maintenant? (o/N) "
if /i "!COMPILE!"=="o" (
    pio run
    if %errorlevel% equ 0 (
        echo    ✓ Compilation réussie!
    ) else (
        echo    ❌ Erreur de compilation
        echo    Vérifiez les erreurs ci-dessus
    )
) else (
    echo    ⊘ Compilation ignorée
)

REM 7. Résumé
echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                                                              ║
echo ║  ✓ INSTALLATION TERMINÉE                                     ║
echo ║                                                              ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.
echo 📝 PROCHAINES ÉTAPES:
echo.
echo    1. Connecter votre ESP32 au port !PORT!
echo    2. Compiler et uploader:
echo       $ pio run --target upload
echo.
echo    3. Ouvrir le moniteur série:
echo       $ pio device monitor
echo.
echo    4. (Optionnel) Visualisation en temps réel:
echo       $ python tools\visualize_eeg.py !PORT!
echo.
echo 📚 DOCUMENTATION:
echo    • Guide rapide:  QUICK_START.md
echo    • Documentation: README_PLATFORMIO.md
echo    • Filtrage:      FILTERING_GUIDE.md
echo.
echo ⚠️  IMPORTANT:
echo    Ce projet est destiné à la RECHERCHE et l'ÉDUCATION uniquement.
echo    NE PAS utiliser sans supervision médicale.
echo.
echo ✨ Bon développement!
echo.
pause

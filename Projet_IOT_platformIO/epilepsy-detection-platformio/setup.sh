#!/bin/bash
# Script d'installation et de configuration du projet
# Détection de Crises Épileptiques - ESP32 + BITalino

set -e  # Arrêter en cas d'erreur

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║     INSTALLATION - DÉTECTION CRISES ÉPILEPTIQUES            ║"
echo "║          BITalino EEG + ESP32 + TinyML                      ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Fonction pour vérifier si une commande existe
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 1. Vérifier Python
echo "🔍 Vérification de Python..."
if command_exists python3; then
    PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
    echo "   ✓ Python $PYTHON_VERSION trouvé"
else
    echo "   ❌ Python 3 non trouvé. Veuillez l'installer:"
    echo "      https://www.python.org/downloads/"
    exit 1
fi

# 2. Vérifier/Installer PlatformIO
echo ""
echo "🔍 Vérification de PlatformIO..."
if command_exists pio; then
    PIO_VERSION=$(pio --version 2>&1)
    echo "   ✓ PlatformIO trouvé: $PIO_VERSION"
else
    echo "   ⚠️  PlatformIO non trouvé. Installation..."
    python3 -m pip install platformio
    if [ $? -eq 0 ]; then
        echo "   ✓ PlatformIO installé avec succès"
    else
        echo "   ❌ Erreur lors de l'installation de PlatformIO"
        exit 1
    fi
fi

# 3. Installer les dépendances Python (optionnel)
echo ""
echo "🔍 Installation des outils Python (optionnel)..."
if [ -f "tools/requirements.txt" ]; then
    read -p "   Installer les outils Python de visualisation? (o/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[OoYy]$ ]]; then
        python3 -m pip install -r tools/requirements.txt
        echo "   ✓ Outils Python installés"
    else
        echo "   ⊘ Outils Python ignorés (vous pouvez les installer plus tard)"
    fi
else
    echo "   ⊘ Fichier requirements.txt non trouvé"
fi

# 4. Détecter les ports série disponibles
echo ""
echo "🔍 Détection des ports série..."
if command_exists pio; then
    echo "   Ports disponibles:"
    pio device list | grep -E "(/dev/tty|COM)" || echo "   ⚠️  Aucun port détecté"
    echo ""
fi

# 5. Configuration du port
echo "⚙️  Configuration du port série..."
read -p "   Entrer le port série (ex: COM3, /dev/ttyUSB0) ou laisser vide pour garder COM3: " PORT
if [ -z "$PORT" ]; then
    PORT="COM3"
fi
echo "   Port configuré: $PORT"

# Mettre à jour platformio.ini
if [ -f "platformio.ini" ]; then
    # Sauvegarder l'original
    cp platformio.ini platformio.ini.bak
    
    # Remplacer le port
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        sed -i '' "s|^upload_port = .*|upload_port = $PORT|g" platformio.ini
    else
        # Linux
        sed -i "s|^upload_port = .*|upload_port = $PORT|g" platformio.ini
    fi
    echo "   ✓ Port mis à jour dans platformio.ini"
fi

# 6. Test de compilation
echo ""
echo "🔨 Test de compilation du projet..."
read -p "   Compiler le projet maintenant? (o/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[OoYy]$ ]]; then
    pio run
    if [ $? -eq 0 ]; then
        echo "   ✓ Compilation réussie!"
    else
        echo "   ❌ Erreur de compilation"
        echo "   Vérifiez les erreurs ci-dessus"
    fi
else
    echo "   ⊘ Compilation ignorée"
fi

# 7. Résumé
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║  ✓ INSTALLATION TERMINÉE                                     ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📝 PROCHAINES ÉTAPES:"
echo ""
echo "   1. Connecter votre ESP32 au port $PORT"
echo "   2. Compiler et uploader:"
echo "      $ pio run --target upload"
echo ""
echo "   3. Ouvrir le moniteur série:"
echo "      $ pio device monitor"
echo ""
echo "   4. (Optionnel) Visualisation en temps réel:"
echo "      $ python tools/visualize_eeg.py $PORT"
echo ""
echo "📚 DOCUMENTATION:"
echo "   • Guide rapide:  QUICK_START.md"
echo "   • Documentation: README_PLATFORMIO.md"
echo "   • Filtrage:      FILTERING_GUIDE.md"
echo ""
echo "⚠️  IMPORTANT:"
echo "   Ce projet est destiné à la RECHERCHE et l'ÉDUCATION uniquement."
echo "   NE PAS utiliser sans supervision médicale."
echo ""
echo "✨ Bon développement!"

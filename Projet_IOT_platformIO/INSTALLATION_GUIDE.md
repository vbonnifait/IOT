# 📦 Guide d'Installation Visuel

## Installation Complète en 10 Minutes

---

## 🎯 Étape 1 : Télécharger le Projet

### Option A : Archive ZIP (Recommandé pour Windows)
```
📁 epilepsy-detection-platformio.zip (7.3 MB)
```

### Option B : Archive TAR.GZ (Linux/macOS)
```
📁 epilepsy-detection-platformio.tar.gz (7.3 MB)
```

**Vérification d'intégrité** (optionnel):
```bash
# Linux/macOS
sha256sum -c checksums.txt

# Windows PowerShell
Get-FileHash epilepsy-detection-platformio.zip -Algorithm SHA256
```

---

## 🔧 Étape 2 : Extraire l'Archive

### Windows
```
1. Clic droit sur epilepsy-detection-platformio.zip
2. "Extraire tout..." → Choisir un dossier
3. Ouvrir le dossier extrait
```

### Linux/macOS
```bash
tar -xzf epilepsy-detection-platformio.tar.gz
cd epilepsy-detection-platformio
```

---

## 📂 Étape 3 : Structure du Projet

Vous devriez voir cette structure :

```
epilepsy-detection-platformio/
│
├── 📄 README.md                    ← Lisez-moi d'abord !
├── 📄 QUICK_START.md               ← Guide rapide
├── 📄 README_PLATFORMIO.md         ← Documentation complète
├── 📄 FILTERING_GUIDE.md           ← Guide technique
│
├── ⚙️  platformio.ini              ← Configuration
├── 🔧 setup.sh / setup.bat         ← Scripts d'installation
│
├── 📁 src/                         ← Code source
│   └── main.cpp
│
├── 📁 include/                     ← Headers
│   ├── model_data.h
│   ├── scaler_params.h
│   └── BITalinoEEG_Preprocessor.h
│
├── 📁 lib/                         ← Bibliothèques
│   └── BITalinoEEG_Preprocessor/
│
├── 📁 test/                        ← Tests
│   └── test_preprocessing.cpp
│
├── 📁 tools/                       ← Outils Python
│   ├── visualize_eeg.py
│   └── requirements.txt
│
└── 📁 docs/                        ← Documentation originale
    ├── epilepsy_model.h5
    ├── training_history.png
    └── ...
```

---

## 🚀 Étape 4 : Installation Automatique

### Windows
```cmd
Faire un double-clic sur:
    setup.bat
```

### Linux/macOS
```bash
chmod +x setup.sh
./setup.sh
```

Le script va :
- ✅ Vérifier Python
- ✅ Installer PlatformIO
- ✅ Configurer le port série
- ✅ Compiler le projet

---

## 🔌 Étape 5 : Connexion Matérielle

### Matériel Nécessaire

| Composant | Quantité |
|-----------|----------|
| ESP32 DevKitC | 1 |
| BITalino EEG | 1 |
| LED Rouge 5mm | 1 |
| Résistance 220Ω | 1 |
| Buzzer 5V | 1 |
| Câbles jumper | ~10 |
| Breadboard | 1 |

### Schéma de Connexion

```
    ┌──────────────────────────────────────┐
    │         ESP32 DevKitC                │
    │                                      │
    │  ┌────────────────────────────────┐ │
    │  │                                │ │
    │  │  GPIO36 ←─────────────────────┼─┼─ BITalino EEG OUT
    │  │   (ADC)                        │ │
    │  │                                │ │
    │  │  GPIO2  ──► LED Verte (board) │ │
    │  │                                │ │
    │  │  GPIO4  ──┐                    │ │
    │  │           │                    │ │
    │  └───────────┼────────────────────┘ │
    │              │                      │
    └──────────────┼──────────────────────┘
                   │
                   ├──► Résistance 220Ω ──► LED Rouge ──► GND
                   │
                   └──────────────────────────┐
    ┌──────────────────────────────────────┐  │
    │         ESP32 DevKitC                │  │
    │                                      │  │
    │  GPIO5  ──────────────────────────────┼──┘
    │           │                           │
    │           └──► Buzzer ──► GND         │
    │                                       │
    │  GND ──────────────────────────────────── BITalino GND
    │                                       │
    └───────────────────────────────────────┘
```

### Connexions Détaillées

1. **BITalino EEG → ESP32**
   ```
   BITalino OUT  →  ESP32 GPIO36 (ADC1_CH0)
   BITalino GND  →  ESP32 GND
   ```

2. **LED Rouge**
   ```
   ESP32 GPIO4 → Résistance 220Ω → LED Rouge (anode)
   LED Rouge (cathode) → GND
   ```

3. **Buzzer**
   ```
   ESP32 GPIO5 → Buzzer (+)
   Buzzer (-)  → GND
   ```

---

## 🎛️ Étape 6 : Configuration

### 1. Ouvrir VS Code

Si vous utilisez VS Code avec PlatformIO:

```
1. Fichier → Ouvrir Dossier
2. Sélectionner epilepsy-detection-platformio/
3. PlatformIO devrait détecter le projet automatiquement
```

### 2. Configurer le Port

Éditer `platformio.ini`:

```ini
; Trouver cette ligne et modifier selon votre système
upload_port = COM3      ; Windows
; upload_port = /dev/ttyUSB0  ; Linux
; upload_port = /dev/cu.usbserial-*  ; macOS
```

**Trouver votre port**:
```bash
# Via PlatformIO
pio device list

# Via Arduino IDE
Outils → Port

# Windows Device Manager
Ports (COM & LPT)
```

---

## 🔨 Étape 7 : Compilation et Upload

### Via Terminal

```bash
# Compiler
pio run

# Compiler + Uploader
pio run --target upload

# Ouvrir le moniteur série
pio device monitor
```

### Via VS Code

```
1. Cliquer sur l'icône PlatformIO (alien) dans la barre latérale
2. PROJECT TASKS
   → env:esp32dev
     → General
       → Build          (Compiler)
       → Upload         (Uploader)
       → Monitor        (Moniteur série)
```

---

## 🩺 Étape 8 : Test avec BITalino

### Placement des Électrodes

**Vue de Dessus de la Tête**:

```
        AVANT
          
     FP1  REF  FP2
      ●────○────●
      │         │
   T3 ●SIG   SIGNAL
      │         
  GND ●         
  (mastoïde)    
      
     ARRIÈRE
```

**Configuration Standard**:
- **Ground (Noir)**: Derrière l'oreille gauche (mastoïde)
- **Reference (Blanc)**: Front centre (Fpz) ou front gauche (Fp1)
- **Signal (Rouge)**: Temporal gauche (T3)

### Vérification du Signal

1. **Connecter le BITalino** à l'ESP32
2. **Ouvrir le moniteur série** (115200 baud)
3. **Observer les tests ADC**:

```
Test 1: ADC =  512 → EEG =     0.00 µV
Test 2: ADC =  645 → EEG = +6500.00 µV
Test 3: ADC =  389 → EEG = -6000.00 µV
```

✅ **Bon signal**: Les valeurs ADC varient (400-650)
❌ **Mauvais signal**: Les valeurs sont figées (512-512-512)

---

## 📊 Étape 9 : Visualisation (Optionnel)

### Installation des Outils Python

```bash
# Installer les dépendances
pip install -r tools/requirements.txt
```

### Lancer la Visualisation

```bash
# Windows
python tools\visualize_eeg.py COM3

# Linux/macOS
python tools/visualize_eeg.py /dev/ttyUSB0
```

Vous verrez 3 graphiques en temps réel:
- 📈 Signal brut
- 📉 Signal filtré (0.5-40 Hz)
- 📊 Probabilité de crise

---

## ✅ Étape 10 : Vérification Finale

### Checklist

- [ ] Python installé (3.7+)
- [ ] PlatformIO installé
- [ ] Projet compilé sans erreur
- [ ] ESP32 détecté sur le port série
- [ ] BITalino connecté à GPIO36
- [ ] LED et Buzzer connectés
- [ ] Signal EEG visible dans le moniteur
- [ ] Moniteur série affiche "SYSTÈME PRÊT"

### Sortie Console Normale

```
╔══════════════════════════════════════════════════════════════╗
║     SYSTÈME DE DÉTECTION DE CRISES ÉPILEPTIQUES             ║
║          BITalino EEG + ESP32 + TinyML                      ║
╠══════════════════════════════════════════════════════════════╣
║  Modèle: TensorFlow Lite Micro (INT8 Quantized)             ║
║  Taille: 20.46 KB                                            ║
║  Accuracy: 99.46%                                            ║
╚══════════════════════════════════════════════════════════════╝

✓ Configuration matérielle terminée
✓ Préprocesseur EEG BITalino initialisé
✓ TensorFlow Lite Micro initialisé

╔══════════════════════════════════════════════════════════════╗
║  SYSTÈME PRÊT - Début de la détection                       ║
╚══════════════════════════════════════════════════════════════╝

✓ Normal (P(crise)=3.2%)
✓ Normal (P(crise)=4.5%)
⚡ Inférence #50: P(crise)=4.1% (latence: 85 µs)
```

---

## 🎉 Félicitations !

Votre système de détection de crises épileptiques est maintenant opérationnel !

### Prochaines Étapes

1. **Tester avec des données réelles**
   - Commencer avec des sessions courtes
   - Vérifier la qualité du signal
   - Noter les fausses alarmes

2. **Ajuster les paramètres**
   - Modifier `SEIZURE_THRESHOLD` si nécessaire
   - Ajuster `CONFIRMATION_COUNT`
   - Voir [README_PLATFORMIO.md](README_PLATFORMIO.md)

3. **Explorer les fonctionnalités**
   - Visualisation temps réel
   - Tests unitaires
   - Logging des données

---

## 🆘 Besoin d'Aide ?

### Documentation
- 📘 [Guide Rapide](QUICK_START.md)
- 📗 [Documentation Complète](README_PLATFORMIO.md)
- 📙 [Guide du Filtrage](FILTERING_GUIDE.md)

### Problèmes Courants
- **Port série non détecté**: Vérifier les pilotes USB
- **Compilation échoue**: Mettre à jour PlatformIO
- **Signal plat**: Vérifier électrodes et gel conducteur
- **Fausses alarmes**: Augmenter le seuil de détection

### Support
- 🐛 Issues GitHub
- 💬 Discussions
- 📧 Contact (voir README.md)

---

## ⚠️ Rappel Important

**Ce système est destiné à la RECHERCHE et l'ÉDUCATION uniquement.**

- ❌ NE PAS utiliser sans validation clinique
- ❌ NE PAS remplacer la supervision médicale
- ✅ TOUJOURS consulter un professionnel de santé

---

<div align="center">

**✨ Bon développement et bonne recherche ! ✨**

*Développé avec ❤️ pour la recherche sur l'épilepsie*

</div>

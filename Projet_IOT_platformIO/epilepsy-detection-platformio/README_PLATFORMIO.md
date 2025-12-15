# Système de Détection de Crises Épileptiques - PlatformIO

## 📋 Description

Système embarqué de détection de crises épileptiques en temps réel utilisant:
- **Capteur EEG BITalino** pour l'acquisition des signaux cérébraux
- **Microcontrôleur ESP32** pour le traitement
- **TinyML (TensorFlow Lite Micro)** pour l'intelligence artificielle embarquée

## 🎯 Performances

| Métrique | Valeur |
|----------|--------|
| **Accuracy** | 99.46% |
| **Precision (Crise)** | 100.00% |
| **Recall (Crise)** | 98.91% |
| **Taille du modèle** | 20.46 KB (INT8 quantized) |
| **Latence d'inférence** | < 100 ms |
| **RAM requise** | ~40 KB |

## 🔧 Matériel Requis

### Composants Principaux
- **ESP32 DevKitC** (ou compatible)
- **Capteur EEG BITalino** avec électrodes
- **LED Rouge** (pour alerte crise)
- **Buzzer** (pour alerte sonore)
- **Boutons** (pour réinitialisation)

### Connexions

```
BITalino EEG  →  ESP32 GPIO36 (ADC1_CH0)
LED Normale   →  ESP32 GPIO2  (LED intégrée)
LED Crise     →  ESP32 GPIO4  (via résistance 220Ω)
Buzzer        →  ESP32 GPIO5  (via transistor)
Bouton Reset  →  ESP32 GPIO0  (BOOT button)
```

### Schéma de Connexion

```
            ┌─────────────────┐
            │                 │
  BITalino  │     ESP32       │  LED Rouge
  EEG OUT ──┤ GPIO36 (ADC)    ├──┤>├─── GND
            │                 │
            │     GPIO4 ──────┤
            │                 │
            │     GPIO5 ──────┼─── Buzzer
            │                 │
            │     GPIO2 (LED) │
            │                 │
            └─────────────────┘
```

## 📦 Installation

### 1. Prérequis

- **PlatformIO** (extension VS Code ou CLI)
- **Python 3.x** (pour certaines dépendances)
- **Pilotes USB ESP32** (CH340/CP2102)

### 2. Installation de PlatformIO

#### Via VS Code
```bash
# Installer l'extension PlatformIO IDE dans VS Code
# Extensions → Rechercher "PlatformIO IDE" → Installer
```

#### Via CLI
```bash
pip install platformio
```

### 3. Cloner le Projet

```bash
git clone <votre-repo>
cd epilepsy-detection-platformio
```

### 4. Configuration du Port

Modifier dans `platformio.ini`:
```ini
upload_port = COM3  # Windows
# ou
upload_port = /dev/ttyUSB0  # Linux
# ou  
upload_port = /dev/cu.usbserial-*  # macOS
```

### 5. Compiler et Uploader

```bash
# Compiler le projet
pio run

# Uploader sur ESP32
pio run --target upload

# Ouvrir le moniteur série
pio device monitor
```

## 🏗️ Structure du Projet

```
.
├── platformio.ini              # Configuration PlatformIO
├── src/
│   └── main.cpp               # Code principal
├── include/
│   ├── model_data.h           # Modèle TFLite en C
│   ├── scaler_params.h        # Paramètres de normalisation
│   └── BITalinoEEG_Preprocessor.h
├── lib/
│   └── BITalinoEEG_Preprocessor/
│       ├── BITalinoEEG_Preprocessor.h
│       └── BITalinoEEG_Preprocessor.cpp
└── README.md
```

## 🔬 Pipeline de Traitement

```
┌─────────────┐
│ BITalino    │
│ EEG Sensor  │
└──────┬──────┘
       │ ADC 10-bit @ 178 Hz
       ▼
┌─────────────────────────────────────┐
│ 1. Conversion ADC → Microvolts      │
│    V = (ADC/1024) * 3.3V            │
│    EEG = (V - 1.65V) / 1000         │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 2. Filtrage Passe-Bande             │
│    - Passe-Haut: 0.5 Hz             │
│    - Passe-Bas:  40 Hz              │
│    (Butterworth 4ème ordre)         │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 3. Buffer Fenêtre Glissante         │
│    - Taille: 178 samples (1 sec)    │
│    - Overlap: 50%                   │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 4. Extraction de Features           │
│    - Temporelles: 16 features       │
│    - Statistiques: 10 features      │
│    - Par segment: 7 × 26 features   │
│    Total: 194 features              │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 5. Normalisation (Z-score)          │
│    z = (x - mean) / std             │
│    (scaler du dataset)              │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 6. Inférence TFLite Micro           │
│    - Réseau: 64-32-16-1             │
│    - Output: P(crise) ∈ [0,1]       │
└──────┬──────────────────────────────┘
       ▼
┌─────────────────────────────────────┐
│ 7. Décision                         │
│    Si P(crise) > 0.7 → ALERTE       │
│    Confirmation: 3 détections       │
└─────────────────────────────────────┘
```

## 📊 Features Extraites

### Features Temporelles (16)
1. **Statistiques de base**: mean, median, std, variance
2. **Amplitude**: min, max, range, peak-to-peak
3. **Énergie**: RMS, energy
4. **Forme**: skewness, kurtosis
5. **Dynamique**: zero_crossings, mean_diff, std_diff, entropy

### Features par Segment (7 segments × 26 features)
- Division du signal en 7 segments
- Extraction des 16 features temporelles + 10 statistiques par segment
- Capture la dynamique temporelle du signal

## 🎛️ Configuration du Capteur BITalino

### Paramètres EEG
```cpp
Résolution ADC:    10-bit (0-1023)
Tension:           3.3V
Gain:              1000
Fréquence:         178 Hz
Électrodes:        3 (Ground, Ref, Signal)
```

### Placement des Électrodes

Placement **frontal-temporal** recommandé:
```
     (Front de la tête)
           
    REF ●────────● SIGNAL
         │
         │
    GND ●
```

- **Ground (GND)**: Derrière l'oreille (mastoïde)
- **Reference (REF)**: Front (Fp1 ou Fp2)
- **Signal**: Temporal (T3 ou T4)

## 🚀 Utilisation

### 1. Préparation

```bash
# 1. Connecter le BITalino à l'ESP32
# 2. Placer les électrodes EEG sur le patient
# 3. Vérifier que le signal est de bonne qualité
# 4. Connecter l'ESP32 via USB
```

### 2. Lancement

```bash
# Upload et moniteur série
pio run --target upload && pio device monitor
```

### 3. Sortie Console

```
╔══════════════════════════════════════════════════════════════╗
║     SYSTÈME DE DÉTECTION DE CRISES ÉPILEPTIQUES             ║
║          BITalino EEG + ESP32 + TinyML                      ║
╠══════════════════════════════════════════════════════════════╣
║  Modèle: TensorFlow Lite Micro (INT8 Quantized)             ║
║  Taille: 20.46 KB                                            ║
║  Accuracy: 99.46%                                            ║
╚══════════════════════════════════════════════════════════════╝

✓ Normal (P(crise)=5.2%)
✓ Normal (P(crise)=3.8%)
⚡ Inférence #50: P(crise)=4.1% (latence: 85 µs)

╔══════════════════════════════════════════════════════════════╗
║            ⚠️  ALERTE CRISE DÉTECTÉE ⚠️                      ║
╠══════════════════════════════════════════════════════════════╣
║  Probabilité: 94.3%                                          ║
║  Détections consécutives: 5                                  ║
╚══════════════════════════════════════════════════════════════╝
```

## 🔧 Configuration Avancée

### Ajuster le Seuil de Détection

Dans `main.cpp`:
```cpp
#define SEIZURE_THRESHOLD 0.70f  // Changer entre 0.5-0.9
```

- **0.5-0.6**: Plus sensible (plus de fausses alarmes)
- **0.7**: Valeur recommandée (équilibre)
- **0.8-0.9**: Plus conservateur (moins de fausses alarmes)

### Ajuster la Confirmation

```cpp
#define CONFIRMATION_COUNT 3  // Nombre de détections consécutives
```

### Modifier la Fréquence d'Échantillonnage

⚠️ **Attention**: Doit correspondre au dataset d'entraînement!

Dans `BITalinoEEG_Preprocessor.h`:
```cpp
#define SAMPLE_RATE 178  // Hz
```

## 🐛 Dépannage

### Problème: Pas de signal EEG

**Causes possibles**:
- Électrodes mal placées
- Gel conducteur insuffisant
- Connexion ADC incorrecte
- BITalino non alimenté

**Solutions**:
```bash
# Tester la lecture ADC
pio device monitor

# Vérifier les valeurs ADC (doivent varier)
Test 1: ADC =  512 → EEG =     0.00 µV
Test 2: ADC =  645 → EEG = +6500.00 µV
```

### Problème: Fausses Détections

**Solutions**:
- Augmenter `SEIZURE_THRESHOLD` (0.8)
- Augmenter `CONFIRMATION_COUNT` (5)
- Vérifier la qualité du signal (pas trop de bruit)

### Problème: Erreur de Compilation

```bash
# Nettoyer et recompiler
pio run --target clean
pio run

# Mettre à jour les librairies
pio pkg update
```

### Problème: Upload échoue

```bash
# Vérifier le port
pio device list

# Forcer le mode bootloader (maintenir BOOT button)
pio run --target upload
```

## 📈 Optimisations Futures

### 1. Communication Sans Fil
- [ ] Bluetooth Low Energy (BLE) pour notifications
- [ ] WiFi pour logging cloud
- [ ] Application mobile

### 2. Amélioration du Modèle
- [ ] Apprentissage adaptatif par patient
- [ ] Détection des phases pré-ictales
- [ ] Classification multi-classes

### 3. Capteurs Additionnels
- [ ] EMG pour mouvements musculaires
- [ ] Accéléromètre pour détection de chutes
- [ ] SpO2 pour surveillance respiratoire

## ⚠️ Avertissements Importants

### Usage Médical

⚠️ **CE PROJET EST UN PROTOTYPE DE RECHERCHE**

- **NE PAS utiliser sans validation clinique**
- **NE PAS remplacer la supervision médicale**
- **TOUJOURS consulter un neurologue**
- **Respecter les normes de dispositifs médicaux**

### Réglementation

- 🔒 Dispositif Médical Classe IIa (Europe) / Class II (USA)
- 🔒 Marquage CE requis
- 🔒 Conformité RGPD / HIPAA
- 🔒 ISO 13485 (Système qualité)

## 📚 Ressources

### Documentation Technique
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP32 Documentation](https://docs.espressif.com/)
- [BITalino User Manual](https://bitalino.com/documentation)

### Dataset
- [Epileptic Seizure Recognition Dataset](https://archive.ics.uci.edu/ml/datasets/Epileptic+Seizure+Recognition)

### Publications Scientifiques
- Detection of epileptic seizures using EEG signals (IEEE)
- Real-time epilepsy detection on wearable devices (ACM)

## 👥 Contribution

Développé dans le cadre du cours d'IoT à ISIS Castres.

**Parcours**: Double diplôme Pharmacie/Ingénieur Informatique  
**Année**: 2025

## 📄 License

Ce projet est développé à des fins éducatives et de recherche.

**Usage commercial interdit sans**:
- ✅ Validation clinique complète
- ✅ Certifications réglementaires (CE, FDA)
- ✅ Conformité aux normes de sécurité médicale

---

**Dernière mise à jour**: Décembre 2025

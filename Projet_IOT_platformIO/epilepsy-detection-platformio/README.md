# 🧠 Détection de Crises Épileptiques avec TinyML

## Système embarqué de détection en temps réel - BITalino EEG + ESP32

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D)](https://www.arduino.cc/)
[![AI](https://img.shields.io/badge/AI-TensorFlow_Lite-FF6F00)](https://www.tensorflow.org/lite)
[![Accuracy](https://img.shields.io/badge/Accuracy-99.46%25-success)](/)

---

## 📋 Vue d'Ensemble

Système complet de détection de crises épileptiques utilisant l'intelligence artificielle embarquée (TinyML) sur microcontrôleur ESP32 avec capteur EEG BITalino.

### 🎯 Performances du Modèle

| Métrique | Valeur | Notes |
|----------|--------|-------|
| **Accuracy** | 99.46% | Sur dataset de test |
| **Precision** | 100.00% | Aucun faux positif |
| **Recall** | 98.91% | Détecte 98.9% des crises |
| **Taille Modèle** | 20.46 KB | INT8 quantized |
| **RAM Utilisée** | ~40 KB | Buffers + TFLite |
| **Latence** | < 100 ms | Par fenêtre d'1 sec |

### ✨ Caractéristiques

- ✅ **Détection en temps réel** (178 Hz)
- ✅ **Filtrage passe-bande** (0.5-40 Hz, Butterworth 4ème ordre)
- ✅ **194 features** temporelles et statistiques
- ✅ **Normalisation automatique** (scaler pré-entraîné)
- ✅ **Alertes multiples** (LED, buzzer, série)
- ✅ **Faible consommation** (optimisé pour batterie)
- ✅ **Open source** et extensible

---

## 📦 Contenu du Projet

```
epilepsy-detection-platformio/
│
├── 📄 README.md                    # Ce fichier
├── 📄 QUICK_START.md               # Guide de démarrage rapide
├── 📄 README_PLATFORMIO.md         # Documentation complète PlatformIO
├── 📄 FILTERING_GUIDE.md           # Guide technique du filtrage
├── ⚙️  platformio.ini              # Configuration PlatformIO
│
├── 📁 src/
│   └── main.cpp                    # Code principal ESP32
│
├── 📁 include/
│   ├── model_data.h                # Modèle TFLite en C
│   ├── scaler_params.h             # Paramètres de normalisation
│   └── BITalinoEEG_Preprocessor.h  # Header prétraitement
│
├── 📁 lib/
│   └── BITalinoEEG_Preprocessor/
│       ├── BITalinoEEG_Preprocessor.h
│       └── BITalinoEEG_Preprocessor.cpp
│
├── 📁 test/
│   └── test_preprocessing.cpp      # Tests unitaires
│
├── 📁 tools/
│   ├── visualize_eeg.py            # Visualisation temps réel
│   └── requirements.txt            # Dépendances Python
│
└── 📁 docs/                        # (Fichiers originaux du projet)
    ├── epilepsy_model.h5
    ├── epilepsy_model_quantized.tflite
    ├── scaler.pkl
    ├── training_history.png
    └── ...
```

---

## 🚀 Installation Rapide

### Prérequis

- **PlatformIO** (VS Code ou CLI)
- **ESP32 DevKitC** ou compatible
- **Capteur EEG BITalino** avec électrodes
- **Python 3.x** (pour outils optionnels)

### Installation en 3 Étapes

```bash
# 1. Installer PlatformIO
pip install platformio

# 2. Naviguer vers le projet
cd epilepsy-detection-platformio

# 3. Compiler et uploader
pio run --target upload
```

**📖 Guide détaillé**: Voir [QUICK_START.md](QUICK_START.md)

---

## 🔌 Connexions Matérielles

### Schéma de Base

```
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│   BITalino   │         │    ESP32     │         │   Alertes    │
│   EEG        │  ADC    │  DevKitC     │         │              │
├──────────────┤ ─────►  ├──────────────┤  ─────► ├──────────────┤
│              │         │              │         │              │
│  OUT ────────┼────────►│ GPIO36 (ADC) │         │ LED Rouge    │
│              │         │              │         │ GPIO4        │
│  GND ────────┼────────►│ GND          │         │              │
│              │         │              │         │ Buzzer       │
└──────────────┘         │ GPIO5 ───────┼────────►│ GPIO5        │
                         │              │         │              │
                         └──────────────┘         └──────────────┘
```

### Tableau de Connexions

| BITalino | ESP32 | Description |
|----------|-------|-------------|
| OUT | GPIO36 (ADC1_CH0) | Signal EEG |
| GND | GND | Masse |
| - | GPIO2 | LED verte (intégrée) |
| - | GPIO4 | LED rouge (externe) |
| - | GPIO5 | Buzzer d'alerte |

---

## 🎓 Guides et Documentation

### Pour Commencer
- 📘 **[Guide de Démarrage Rapide](QUICK_START.md)** - Installation en 5 minutes
- 📗 **[Documentation PlatformIO](README_PLATFORMIO.md)** - Guide complet

### Technique
- 📙 **[Guide du Filtrage](FILTERING_GUIDE.md)** - Détails sur le traitement du signal
- 📕 **API Reference** - Documentation du code (commentaires inline)

### Outils
- 🔧 **Visualisation Temps Réel** - `tools/visualize_eeg.py`
- 🧪 **Tests Unitaires** - `test/test_preprocessing.cpp`

---

## 💻 Utilisation

### 1. Upload du Firmware

```bash
# Via PlatformIO
pio run --target upload

# Moniteur série
pio device monitor
```

### 2. Placement des Électrodes

**Configuration Standard** (Frontal-Temporal):

```
     (Avant de la tête)
           
    FP1 ●──REF──● FP2
        │
        │
    T3 ●SIG  GND● Mastoïde
```

- **Ground (Noir)**: Derrière l'oreille (mastoïde)
- **Reference (Blanc)**: Front (Fp1 ou Fp2)  
- **Signal (Rouge)**: Temporal (T3 ou T4)

### 3. Interprétation des Résultats

**État Normal** ✅:
```
✓ Normal (P(crise)=3.2%)
✓ Normal (P(crise)=4.5%)
```

**Alerte Crise** ⚠️:
```
╔════════════════════════════════════════╗
║  ⚠️  ALERTE CRISE DÉTECTÉE ⚠️           ║
╠════════════════════════════════════════╣
║  Probabilité: 94.3%                    ║
║  Détections consécutives: 5            ║
╚════════════════════════════════════════╝
```

---

## 🔬 Pipeline de Traitement

```
┌─────────────────┐
│ BITalino EEG    │ ADC 10-bit @ 178 Hz
│ Capteur         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Conversion      │ ADC → Microvolts
│ ADC             │ V = (ADC/1024) × 3.3V
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Filtrage        │ Passe-Bande 0.5-40 Hz
│ Butterworth 4e  │ (Butterworth 4ème ordre)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Buffer          │ Fenêtre glissante
│ Fenêtre         │ 178 samples (1 sec)
└────────┬────────┘ Overlap 50%
         │
         ▼
┌─────────────────┐
│ Extraction      │ 194 features:
│ Features        │ • Temporelles (16)
└────────┬────────┘ • Statistiques (10)
         │           • Par segment (7×26)
         ▼
┌─────────────────┐
│ Normalisation   │ Z-score standardization
│ (Scaler)        │ z = (x - μ) / σ
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Inférence       │ TensorFlow Lite Micro
│ TFLite Micro    │ Réseau: 64-32-16-1
└────────┬────────┘ Modèle: 20.46 KB
         │
         ▼
┌─────────────────┐
│ Décision        │ Si P(crise) > 0.7
│ & Alerte        │ → Déclencher alerte
└─────────────────┘
```

---

## 🛠️ Outils Additionnels

### Visualisation Temps Réel

Visualiser le signal EEG en direct depuis l'ESP32:

```bash
# Installer les dépendances
pip install -r tools/requirements.txt

# Lancer la visualisation
python tools/visualize_eeg.py COM3  # Windows
python tools/visualize_eeg.py /dev/ttyUSB0  # Linux
```

**Fonctionnalités**:
- 📊 Signal brut et filtré
- 📈 Probabilité de crise en temps réel
- ⚠️ Détection visuelle des alertes
- 📉 3 graphiques synchronisés

### Tests Unitaires

Tester le prétraitement sans capteur:

```bash
# Compiler les tests
pio run -e test --target upload

# Monitorer les résultats
pio device monitor
```

---

## ⚙️ Configuration Avancée

### Ajuster la Sensibilité

```cpp
// Dans src/main.cpp

// Moins de fausses alarmes (plus conservateur)
#define SEIZURE_THRESHOLD 0.85f
#define CONFIRMATION_COUNT 5

// Plus sensible (détecte plus de crises)
#define SEIZURE_THRESHOLD 0.60f
#define CONFIRMATION_COUNT 2
```

### Modifier les Pins

```cpp
#define EEG_ADC_PIN 36        // Pin ADC
#define LED_SEIZURE_PIN 4     // LED alerte
#define BUZZER_PIN 5          // Buzzer
```

---

## 📊 Dataset et Entraînement

### Dataset Original

- **Source**: [UCI Machine Learning Repository](https://archive.ics.uci.edu/ml/datasets/Epileptic+Seizure+Recognition)
- **Taille**: 11,500 échantillons
- **Classes**: 2 (crise / normal)
- **Fréquence**: 178 Hz
- **Durée**: 1 seconde par échantillon

### Architecture du Modèle

```python
# Réseau de neurones dense
Input(194 features)
    ↓
Dense(64, ReLU) + Dropout(0.3)
    ↓
Dense(32, ReLU) + Dropout(0.3)
    ↓
Dense(16, ReLU) + Dropout(0.2)
    ↓
Dense(1, Sigmoid)
```

### Réentraînement

Pour réentraîner avec vos propres données:

```bash
# 1. Préparer vos données EEG
# 2. Extraire les features
python extract_features.py

# 3. Entraîner le modèle
python train_model.py

# 4. Convertir en TFLite
python convert_model.py

# 5. Générer les headers C
python extract_scaler.py
```

---

## ⚠️ Avertissements Importants

### ⚕️ Usage Médical

**CE PROJET EST UN PROTOTYPE DE RECHERCHE**

- ❌ **NE PAS** utiliser sans validation clinique
- ❌ **NE PAS** remplacer la supervision médicale
- ❌ **TOUJOURS** consulter un neurologue
- ⚠️ Destiné à la recherche et l'éducation uniquement

### 🔒 Réglementation

- Dispositif Médical **Classe IIa** (Europe) / **Class II** (USA)
- **Marquage CE** requis pour usage clinique
- Conformité **RGPD** / **HIPAA** pour données patients
- Norme **ISO 13485** (Système qualité)

---

## 🤝 Contribution

### Auteur

**Projet développé dans le cadre du cours d'IoT**
- 🏫 **École**: ISIS Castres
- 📚 **Parcours**: Double diplôme Pharmacie/Ingénieur Informatique
- 📅 **Année**: 2025

### Comment Contribuer

1. Fork le projet
2. Créer une branche (`git checkout -b feature/amélioration`)
3. Commit vos changements (`git commit -m 'Ajout fonctionnalité'`)
4. Push vers la branche (`git push origin feature/amélioration`)
5. Ouvrir une Pull Request

### Idées d'Améliorations

- [ ] Application mobile (Flutter/React Native)
- [ ] Communication Bluetooth Low Energy
- [ ] Logging cloud (MQTT/HTTP)
- [ ] Multi-canaux EEG
- [ ] Détection phases pré-ictales
- [ ] Apprentissage adaptatif par patient
- [ ] Support d'autres capteurs (EMG, SpO2)

---

## 📚 Ressources

### Documentation
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [BITalino Documentation](https://bitalino.com/documentation)

### Publications Scientifiques
- *Automated detection of epileptic seizures using EEG signals* (IEEE)
- *Real-time seizure detection on edge devices* (ACM)
- *TinyML for healthcare applications* (Nature)

### Standards Médicaux
- [Règlement UE 2017/745](https://eur-lex.europa.eu/legal-content/FR/TXT/?uri=CELEX:32017R0745) (Dispositifs médicaux)
- [FDA Medical Device Guidelines](https://www.fda.gov/medical-devices)
- [ISO 13485:2016](https://www.iso.org/standard/59752.html) (Qualité dispositifs médicaux)

---

## 📄 License

Ce projet est développé à des fins **éducatives et de recherche**.

**Usage commercial strictement interdit sans**:
- ✅ Validation clinique complète
- ✅ Certifications réglementaires (CE, FDA)
- ✅ Conformité normes de sécurité médicale
- ✅ Autorisation écrite

---

## 📞 Support et Contact

### Issues Techniques
- 🐛 **Bugs**: Ouvrir une issue sur GitHub
- 💡 **Suggestions**: Ouvrir une discussion
- 📖 **Documentation**: Consulter les guides

### Questions Médicales
⚠️ **Pour toute question médicale, consulter un professionnel de santé qualifié**

---

<div align="center">

**⭐ Si ce projet vous aide, n'hésitez pas à laisser une étoile ! ⭐**

Made with ❤️ for epilepsy research

</div>

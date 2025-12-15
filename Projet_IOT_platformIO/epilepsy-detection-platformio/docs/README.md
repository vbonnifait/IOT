# Projet : Bracelet de Détection de Crises Épileptiques avec tinyML

## 📋 Description

Ce projet développe un système de détection de crises épileptiques embarqué utilisant :
- **Capteurs EEG/EMG de surface** pour l'acquisition des signaux biomédicaux
- **Intelligence artificielle embarquée (tinyML)** pour la détection en temps réel
- **Microcontrôleur** (ESP32 ou Arduino Nano 33 BLE Sense)
- **Apprentissage adaptatif** pour personnalisation au patient

## Performances du Modèle

| Métrique | Valeur |
|----------|--------|
| **Accuracy Test Set** | **99.46%** |
| **Precision (Crise)** | 100.00% |
| **Recall (Crise)** | 98.91% |
| **Taille du modèle** | **20.46 KB** (quantizé INT8) |
| **Réduction de taille** | 66.7% |

## Fichiers Générés

### 1. Données d'Entraînement
- **`epilepsy_data_prepared.npz`** : Dataset préparé (Train/Val/Test)
- **`scaler.pkl`** : Paramètres de normalisation
- **`epilepsy_features.csv`** : Features extraites pour analyse

### 2. Modèles
- **`epilepsy_model.h5`** : Modèle TensorFlow complet (59 KB)
- **`epilepsy_model.tflite`** : Modèle TFLite Float32 (61.38 KB)
- **`epilepsy_model_quantized.tflite`** : ⭐ **Modèle optimisé pour déploiement** (20.46 KB)

### 3. Code Arduino/ESP32
- **`arduino_epilepsy_detector.cpp`** : Code de déploiement sur microcontrôleur

### 4. Visualisations
- **`training_history.png`** : Graphiques d'entraînement
- **`model_size_comparison.png`** : Comparaison des tailles de modèle

### 5. Documentation
- **`Projet_Bracelet_Detection_Crises_Epileptiques.docx`** : Document complet du projet

## Guide de Démarrage Rapide

### Étape 1 : Installation des Dépendances

```bash
# Python
pip install tensorflow numpy scikit-learn pandas matplotlib seaborn joblib --break-system-packages

# Arduino/ESP32
# Installer Arduino IDE : https://www.arduino.cc/en/software
# Installer TensorFlow Lite Micro pour Arduino
```

### Étape 2 : Chargement du Modèle

```python
import numpy as np
import joblib

# Charger les données
data = np.load('epilepsy_data_prepared.npz')
X_test = data['X_test']
y_test = data['y_test']

# Charger le scaler
scaler = joblib.load('scaler.pkl')

# Charger le modèle TensorFlow
from tensorflow import keras
model = keras.models.load_model('epilepsy_model.h5')

# Faire une prédiction
prediction = model.predict(X_test[0:1])
print(f"Probabilité de crise : {prediction[0][0]:.2%}")
```

### Étape 3 : Déploiement sur Microcontrôleur

1. **Convertir le modèle en fichier C++** :
```bash
xxd -i epilepsy_model_quantized.tflite > model_data.h
```

2. **Intégrer dans Arduino** :
   - Copier `arduino_epilepsy_detector.cpp` dans votre projet Arduino
   - Ajouter `model_data.h` au projet
   - Modifier les fonctions de lecture des capteurs selon votre hardware

3. **Configuration des capteurs** :
   - Fréquence d'échantillonnage : **178 Hz**
   - Fenêtre glissante : **1 seconde**
   - Overlap : **50%**

## 📊 Architecture du Système

```
┌─────────────────┐
│  Capteur EEG    │──┐
└─────────────────┘  │
                     │
┌─────────────────┐  │  ┌──────────────────┐  ┌─────────────────┐
│  Capteur EMG    │──┼─>│  Microcontrôleur │─>│  Système        │
└─────────────────┘  │  │  (ESP32/Arduino) │  │  d'Alerte       │
                     │  └──────────────────┘  └─────────────────┘
┌─────────────────┐  │           │
│  Autres         │──┘           │
│  Capteurs       │              v
└─────────────────┘     ┌─────────────────┐
                        │  Application    │
                        │  Mobile (BLE)   │
                        └─────────────────┘
```

## 🔬 Pipeline de Traitement

1. **Acquisition** : Échantillonnage 178 Hz
2. **Prétraitement** : Filtrage passe-bande 0.5-40 Hz
3. **Extraction** : 16 features temporelles/fréquentielles
4. **Normalisation** : Z-score standardisation
5. **Inférence** : Réseau de neurones (64-32-16-1)
6. **Décision** : Seuil à 70% → Alerte

## 📈 Features Extraites

| Catégorie | Features |
|-----------|----------|
| **Statistiques temporelles** | mean, median, std, variance, min, max, range |
| **Mesures d'amplitude** | peak-to-peak, rms, energy |
| **Analyse de forme** | skewness, kurtosis, zero_crossings |
| **Détection de changements** | mean_diff, std_diff, entropy |

## Configuration Recommandée

### Hardware
- **Microcontrôleur** : ESP32 DevKitC ou Arduino Nano 33 BLE Sense
- **RAM** : Minimum 256 KB
- **Flash** : Minimum 100 KB (pour le modèle + code)
- **Batterie** : LiPo 500-1000 mAh
- **Capteur EEG** : ADS1299 ou compatible
- **Capteur EMG** : MyoWare ou compatible

### Software
- **Framework** : Arduino ou ESP-IDF
- **IA** : TensorFlow Lite for Microcontrollers
- **Communication** : Bluetooth Low Energy (BLE)

## 🎯 Prochaines Étapes

### Phase 1 : Prototype (3-6 mois)
- [ ] Intégration du modèle sur ESP32
- [ ] Implémentation lecture capteurs
- [ ] Développement pipeline temps réel
- [ ] Tests validation

### Phase 2 : Validation (6-12 mois)
- [ ] Tests avec données réelles
- [ ] Calibration seuil optimal
- [ ] Application mobile
- [ ] Système d'alerte

### Phase 3 : Étude Pilote (12-18 mois)
- [ ] Recrutement patients (n=30-50)
- [ ] Suivi clinique
- [ ] Collection feedback
- [ ] Amélioration itérative

### Phase 4 : Certification (18-36 mois)
- [ ] Essai clinique multicentrique
- [ ] Marquage CE / FDA
- [ ] Industrialisation
- [ ] Commercialisation

## ⚠️ Avertissements Importants

### Considérations Médicales
- ⚠️ **Ce projet est un PROTOTYPE de recherche**
- ⚠️ **Validation clinique OBLIGATOIRE avant usage réel**
- ⚠️ **Ne JAMAIS utiliser sans supervision médicale**
- ⚠️ **Consulter un neurologue pour l'interprétation**

### Réglementation
- 🔒 Dispositif Médical Classe IIa (Europe) / Class II (USA)
- 🔒 Marquage CE requis
- 🔒 Conformité RGPD / HIPAA
- 🔒 ISO 13485 (Système qualité)

## 📚 Ressources Utiles

### Documentation
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP32 Guide](https://docs.espressif.com/)
- [Arduino Nano 33 BLE](https://docs.arduino.cc/)

### Dataset
- [Epileptic Seizure Recognition](https://archive.ics.uci.edu/ml/datasets/Epileptic+Seizure+Recognition)

### Standards Médicaux
- [Règlement UE 2017/745](https://eur-lex.europa.eu/legal-content/FR/TXT/?uri=CELEX:32017R0745)
- [FDA Medical Devices](https://www.fda.gov/medical-devices)

## 👥 Contribution

Ce projet a été réalisé dans le cadre du cours d'IoT à ISIS Castres (Double diplôme Pharmacie/Ingénieur Informatique).

### Contact
- **École** : ISIS Castres
- **Parcours** : Double diplôme Pharmacie/Ingénieur Informatique
- **Année** : 2025

## 📄 License

Ce projet est développé à des fins éducatives et de recherche. L'utilisation commerciale nécessite :
- Validation clinique complète
- Certifications réglementaires (CE, FDA)
- Respect des normes de sécurité médicale

---

**Dernière mise à jour** : Décembre 2025

**Note** : Ce README accompagne le document Word complet du projet pour une compréhension approfondie du système, de son architecture et de ses performances.

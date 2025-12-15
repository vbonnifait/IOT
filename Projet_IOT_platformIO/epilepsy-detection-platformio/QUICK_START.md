# 🚀 Guide de Démarrage Rapide - Détection de Crises Épileptiques

## ⚡ Installation Express (5 minutes)

### 1. Installer PlatformIO

**Via VS Code (Recommandé)**:
```bash
# 1. Ouvrir VS Code
# 2. Extensions (Ctrl+Shift+X)
# 3. Rechercher "PlatformIO IDE"
# 4. Cliquer "Install"
```

**Via CLI**:
```bash
pip install platformio
```

### 2. Télécharger le Projet

```bash
# Cloner ou extraire le projet
cd epilepsy-detection-platformio

# Vérifier la structure
ls
# Vous devez voir: platformio.ini, src/, include/, lib/
```

### 3. Connecter le Matériel

```
BITalino EEG OUT  →  ESP32 GPIO36
LED Rouge         →  ESP32 GPIO4 (via résistance 220Ω)
Buzzer           →  ESP32 GPIO5
```

### 4. Configurer le Port Série

Éditer `platformio.ini`:
```ini
upload_port = COM3      # Windows
# OU
upload_port = /dev/ttyUSB0  # Linux
# OU
upload_port = /dev/cu.usbserial-*  # macOS
```

Trouver votre port:
```bash
# Liste des ports disponibles
pio device list
```

### 5. Compiler et Uploader

```bash
# En une seule commande
pio run --target upload

# Ouvrir le moniteur série
pio device monitor
```

## 🎯 Utilisation

### Démarrage

1. **Préparer le patient**:
   - Nettoyer la peau avec de l'alcool
   - Appliquer du gel conducteur
   - Placer les électrodes (voir schéma ci-dessous)

2. **Allumer le système**:
   - Connecter l'ESP32 via USB
   - Attendre le message "SYSTÈME PRÊT"

3. **Vérifier le signal**:
   ```
   Test 1: ADC =  512 → EEG =     0.00 µV
   Test 2: ADC =  645 → EEG = +6500.00 µV
   ```
   ✅ Les valeurs doivent varier

### Placement des Électrodes

```
Vue de dessus de la tête:

        (Avant)
           
    FP1 ●──REF──● FP2
        │
        │
    T3 ●SIG  GND● T4
        
   (Oreille)  (Oreille)
```

**Configuration Standard**:
- **Ground (Noir)**: Derrière l'oreille gauche
- **Reference (Blanc)**: Front (Fp1)
- **Signal (Rouge)**: Temporal gauche (T3)

### Interprétation des Résultats

#### État Normal ✅
```
✓ Normal (P(crise)=3.2%)
✓ Normal (P(crise)=4.5%)
✓ Normal (P(crise)=2.8%)
```

#### Alerte Crise ⚠️
```
╔══════════════════════════════════════════════╗
║     ⚠️  ALERTE CRISE DÉTECTÉE ⚠️             ║
╠══════════════════════════════════════════════╣
║  Probabilité: 94.3%                          ║
║  Détections consécutives: 5                  ║
╚══════════════════════════════════════════════╝
```

**Actions**:
1. LED rouge clignote
2. Buzzer sonne (3 bips)
3. Message dans le moniteur série
4. (Optionnel) Notification Bluetooth

## 🔧 Résolution de Problèmes

### Problème: "Upload failed"

**Solution 1**: Vérifier le port
```bash
pio device list
# Mettre à jour platformio.ini avec le bon port
```

**Solution 2**: Mode bootloader
```bash
# 1. Maintenir le bouton BOOT sur l'ESP32
# 2. Lancer l'upload:
pio run --target upload
# 3. Relâcher BOOT quand l'upload commence
```

### Problème: Signal EEG plat (pas de variation)

**Causes**:
- ❌ Électrodes mal placées
- ❌ Gel conducteur sec
- ❌ Mauvais contact

**Solutions**:
1. Nettoyer la peau à l'alcool
2. Réappliquer du gel conducteur frais
3. Vérifier les connexions
4. Tester avec un multimètre (résistance < 10kΩ)

### Problème: Trop de fausses alarmes

**Solutions**:
```cpp
// Dans main.cpp, augmenter le seuil:
#define SEIZURE_THRESHOLD 0.80f  // Au lieu de 0.70f

// Augmenter les confirmations:
#define CONFIRMATION_COUNT 5  // Au lieu de 3
```

### Problème: "Tensor allocation failed"

**Solution**: Augmenter la mémoire
```cpp
// Dans main.cpp:
constexpr int kTensorArenaSize = 40 * 1024;  // Au lieu de 30KB
```

## 📊 Commandes Utiles

### Compilation
```bash
# Compiler sans uploader
pio run

# Compiler en mode verbose
pio run -v

# Nettoyer avant recompiler
pio run --target clean
pio run
```

### Moniteur Série
```bash
# Moniteur simple
pio device monitor

# Moniteur avec filtre
pio device monitor --filter esp32_exception_decoder

# Changer la vitesse
pio device monitor -b 115200
```

### Debug
```bash
# Logs détaillés
pio run -v

# Taille du firmware
pio run --target size

# Upload avec logs
pio run --target upload -v
```

## 🎨 Personnalisation

### Changer les Pins

Dans `main.cpp`:
```cpp
#define EEG_ADC_PIN 36        // Pin ADC BITalino
#define LED_NORMAL_PIN 2      // LED verte
#define LED_SEIZURE_PIN 4     // LED rouge
#define BUZZER_PIN 5          // Buzzer
#define RESET_BUTTON_PIN 0    // Bouton reset
```

### Ajuster la Sensibilité

```cpp
// Moins sensible (moins de fausses alarmes)
#define SEIZURE_THRESHOLD 0.85f
#define CONFIRMATION_COUNT 5

// Plus sensible (détecte plus de crises)
#define SEIZURE_THRESHOLD 0.60f
#define CONFIRMATION_COUNT 2
```

### Désactiver le Buzzer

```cpp
void triggerSeizureAlert() {
    digitalWrite(LED_SEIZURE_PIN, HIGH);
    // digitalWrite(BUZZER_PIN, HIGH);  // ← Commenter cette ligne
}
```

## 📈 Monitoring en Temps Réel

### Statistiques

Le système affiche automatiquement des statistiques toutes les 10 secondes:

```
╔══════════════════════════════════════════════╗
║  STATISTIQUES                                ║
╠══════════════════════════════════════════════╣
║  Inférences totales: 1234                    ║
║  Crises détectées: 3                         ║
║  État actuel: ✓ Normal                       ║
║  Uptime: 456 secondes                        ║
║  Mémoire libre: 125684 bytes                 ║
╚══════════════════════════════════════════════╝
```

### Exporter les Données

Pour logger dans un fichier:
```bash
# Rediriger vers fichier
pio device monitor > logs.txt

# Avec timestamp (Linux/macOS)
pio device monitor | ts '[%Y-%m-%d %H:%M:%S]' > logs.txt

# Avec timestamp (Windows PowerShell)
pio device monitor | ForEach-Object {"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $_"} > logs.txt
```

## 🧪 Mode Test

Pour tester le système sans patient réel:

```bash
# Compiler le test
pio run -e test

# Uploader et monitorer
pio run -e test --target upload
pio device monitor
```

Le mode test génère un signal EEG simulé et vérifie:
- ✅ Conversion ADC
- ✅ Filtrage
- ✅ Extraction de features
- ✅ Performance temps réel

## ⚡ Performances

### Temps de Traitement

```
Acquisition:           ~5 µs
Filtrage:             ~15 µs
Extraction features:  ~500 µs
Normalisation:        ~50 µs
Inférence TFLite:     ~800 µs
─────────────────────────────
Total par fenêtre:    ~1.4 ms
```

### Consommation Mémoire

```
Flash:     ~200 KB (code + modèle)
RAM:       ~40 KB (buffers + TFLite)
Heap free: ~120 KB disponible
```

## 🔄 Mise à Jour

### Nouveau Modèle

1. Entraîner le nouveau modèle (Python)
2. Convertir en TFLite quantized
3. Générer `model_data.h`:
   ```bash
   python convert_model.py
   ```
4. Générer `scaler_params.h`:
   ```bash
   python extract_scaler.py
   ```
5. Copier dans `include/`
6. Recompiler:
   ```bash
   pio run --target upload
   ```

## ⚠️ Sécurité

### ❌ NE PAS
- Utiliser sur patients sans supervision médicale
- Remplacer un dispositif médical certifié
- Ignorer les alertes médicales
- Modifier le seuil sans validation

### ✅ FAIRE
- Consulter un neurologue
- Valider avec des données réelles
- Logger toutes les détections
- Maintenir le système à jour

## 📞 Support

### Problèmes Techniques
- Vérifier le README complet
- Consulter la documentation PlatformIO
- Vérifier les issues GitHub

### Problèmes Médicaux
- **Toujours consulter un professionnel de santé**
- Ce système est expérimental
- Ne remplace pas un suivi médical

---

**Temps total d'installation**: ~5 minutes  
**Temps jusqu'à première détection**: ~2 minutes  
**Difficulté**: ⭐⭐⚡⚡⚡ (Facile)

✅ **Vous êtes prêt!** Le système est maintenant opérationnel.

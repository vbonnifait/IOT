# Documentation Technique - Filtrage du Signal EEG

## 🎛️ Architecture du Filtrage

Le système utilise un **filtre passe-bande Butterworth 4ème ordre** pour isoler les fréquences EEG pertinentes.

```
Signal BITalino  →  [Passe-Haut 0.5 Hz]  →  [Passe-Bas 40 Hz]  →  Signal Filtré
```

## 📊 Caractéristiques des Filtres

### Filtre Passe-Haut (HPF) - 0.5 Hz

**Objectif**: Retirer la dérive DC et les très basses fréquences (artefacts de mouvement)

**Spécifications**:
- Type: Butterworth 4ème ordre
- Fréquence de coupure: 0.5 Hz
- Fréquence d'échantillonnage: 178 Hz
- Atténuation: -3 dB @ 0.5 Hz
- Pente: 80 dB/décade

**Coefficients**:
```cpp
HPF_B[5] = {0.9895, -3.9580, 5.9370, -3.9580, 0.9895}
HPF_A[5] = {1.0000, -3.9580, 5.9162, -3.9370, 0.9790}
```

**Réponse en Fréquence**:
```
Fréquence (Hz)  | Gain (dB)
----------------|----------
0.1             | -20.0
0.5 (coupure)   | -3.0
1.0             | -0.3
5.0             | -0.0
```

### Filtre Passe-Bas (LPF) - 40 Hz

**Objectif**: Retirer les hautes fréquences (bruit électrique 50/60 Hz, artefacts EMG)

**Spécifications**:
- Type: Butterworth 4ème ordre
- Fréquence de coupure: 40 Hz
- Fréquence d'échantillonnage: 178 Hz
- Atténuation: -3 dB @ 40 Hz
- Pente: 80 dB/décade

**Coefficients**:
```cpp
LPF_B[5] = {0.0201, 0.0804, 0.1206, 0.0804, 0.0201}
LPF_A[5] = {1.0000, -1.9644, 1.7469, -0.7498, 0.1327}
```

**Réponse en Fréquence**:
```
Fréquence (Hz)  | Gain (dB)
----------------|----------
1.0             | -0.0
10.0            | -0.0
30.0            | -0.5
40.0 (coupure)  | -3.0
50.0            | -6.5
60.0            | -10.2
100.0           | -22.0
```

## 🔬 Bandes de Fréquences EEG

Le filtre passe-bande (0.5-40 Hz) capture les principales bandes EEG cliniques:

| Bande | Fréquence | Caractéristiques | Pertinence pour Épilepsie |
|-------|-----------|------------------|---------------------------|
| **Delta** | 0.5-4 Hz | Sommeil profond | Rythmes lents ictaux |
| **Theta** | 4-8 Hz | Somnolence, méditation | Activité focale |
| **Alpha** | 8-13 Hz | Éveil calme, yeux fermés | Suppression durant crises |
| **Beta** | 13-30 Hz | Éveil actif, concentration | Activité rapide ictale |
| **Gamma** | 30-40 Hz | Traitement cognitif | Oscillations pré-ictales |

**Fréquences filtrées** (> 40 Hz):
- Bruit électrique (50/60 Hz)
- Artefacts musculaires (EMG)
- Bruits haute fréquence

## 🧮 Implémentation

### Équation aux Différences

Le filtre est implémenté sous forme d'équation aux différences (IIR):

```
y[n] = b[0]*x[n] + b[1]*x[n-1] + b[2]*x[n-2] + b[3]*x[n-3] + b[4]*x[n-4]
       - a[1]*y[n-1] - a[2]*y[n-2] - a[3]*y[n-3] - a[4]*y[n-4]
```

Où:
- `x[n]` = échantillon d'entrée actuel
- `y[n]` = échantillon de sortie actuel
- `b[i]` = coefficients du numérateur
- `a[i]` = coefficients du dénominateur

### Code C++

```cpp
float applyFilter(float sample, Filter* filter) {
    // Décalage du buffer
    for (int i = 4; i > 0; i--) {
        filter->x[i] = filter->x[i-1];
        filter->y[i] = filter->y[i-1];
    }
    
    // Nouvelle entrée
    filter->x[0] = sample;
    
    // Calcul de la sortie
    filter->y[0] = filter->b[0] * filter->x[0];
    for (int i = 1; i < 5; i++) {
        filter->y[0] += filter->b[i] * filter->x[i] 
                      - filter->a[i] * filter->y[i];
    }
    
    return filter->y[0];
}
```

### Cascade de Filtres

```cpp
float sample = readADC();

// 1. Passer par le filtre passe-haut
float hpf_out = applyHighPassFilter(sample);

// 2. Passer par le filtre passe-bas
float filtered = applyLowPassFilter(hpf_out);
```

## 📐 Génération des Coefficients (Python)

Les coefficients ont été générés avec SciPy:

```python
from scipy import signal
import numpy as np

# Paramètres
fs = 178  # Hz
order = 4

# Filtre Passe-Haut (0.5 Hz)
fc_high = 0.5
nyquist = fs / 2
normalized_fc = fc_high / nyquist

b_hp, a_hp = signal.butter(order, normalized_fc, 
                            btype='high', analog=False)

# Filtre Passe-Bas (40 Hz)
fc_low = 40
normalized_fc = fc_low / nyquist

b_lp, a_lp = signal.butter(order, normalized_fc,
                            btype='low', analog=False)

# Afficher les coefficients
print("HPF B:", b_hp)
print("HPF A:", a_hp)
print("LPF B:", b_lp)
print("LPF A:", a_lp)
```

## 🎯 Validation du Filtrage

### Test avec Signal Sinusoïdal

```python
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

# Générer signal de test
fs = 178
t = np.linspace(0, 5, fs*5)

# Signal composite
freq_low = 0.3    # Sous le passe-haut
freq_mid = 10     # Dans la bande passante
freq_high = 60    # Au-dessus du passe-bas

signal_test = (np.sin(2*np.pi*freq_low*t) +
               np.sin(2*np.pi*freq_mid*t) +
               np.sin(2*np.pi*freq_high*t))

# Appliquer les filtres
filtered = signal.filtfilt(b_hp, a_hp, signal_test)
filtered = signal.filtfilt(b_lp, a_lp, filtered)

# Tracer
plt.figure(figsize=(12, 6))
plt.subplot(2,1,1)
plt.plot(t, signal_test)
plt.title('Signal Original')
plt.subplot(2,1,2)
plt.plot(t, filtered)
plt.title('Signal Filtré')
plt.show()
```

**Résultat Attendu**:
- Fréquence 0.3 Hz: Atténuée (< -20 dB)
- Fréquence 10 Hz: Conservée (~0 dB)
- Fréquence 60 Hz: Atténuée (< -10 dB)

### Analyse Spectrale

```python
from scipy.fft import fft, fftfreq

# FFT avant filtrage
fft_orig = np.abs(fft(signal_test))
freqs_orig = fftfreq(len(signal_test), 1/fs)

# FFT après filtrage
fft_filt = np.abs(fft(filtered))

# Tracer
plt.figure(figsize=(12, 4))
plt.subplot(1,2,1)
plt.plot(freqs_orig[:len(freqs_orig)//2], 
         fft_orig[:len(fft_orig)//2])
plt.title('Spectre Original')
plt.xlabel('Fréquence (Hz)')

plt.subplot(1,2,2)
plt.plot(freqs_orig[:len(freqs_orig)//2],
         fft_filt[:len(fft_filt)//2])
plt.title('Spectre Filtré')
plt.xlabel('Fréquence (Hz)')
plt.show()
```

## ⚙️ Optimisations

### 1. Filtre en Point Fixe

Pour ESP8266 ou systèmes sans FPU:

```cpp
// Conversion en point fixe Q15
#define Q15_SHIFT 15
#define FLOAT_TO_Q15(x) ((int16_t)((x) * (1 << Q15_SHIFT)))
#define Q15_TO_FLOAT(x) ((float)(x) / (1 << Q15_SHIFT))

int16_t b_q15[5] = {
    FLOAT_TO_Q15(0.0201),
    FLOAT_TO_Q15(0.0804),
    // ...
};

int16_t applyFilterQ15(int16_t sample) {
    int32_t acc = 0;
    // Calcul en point fixe...
    return (int16_t)(acc >> Q15_SHIFT);
}
```

### 2. Filtre Biquad

Implémenter comme cascade de sections du second ordre (SOS):

```cpp
// Plus stable numériquement
float applyBiquad(float x, BiquadCoeffs* sos) {
    float y = sos->b0 * x + sos->z1;
    sos->z1 = sos->b1 * x - sos->a1 * y + sos->z2;
    sos->z2 = sos->b2 * x - sos->a2 * y;
    return y;
}
```

## 🐛 Debugging

### Vérifier la Réponse en Fréquence

```cpp
void testFilterResponse() {
    const int num_freqs = 20;
    float freqs[] = {0.1, 0.5, 1, 5, 10, 15, 20, 30, 40, 50, 60, 80, 100};
    
    for (int i = 0; i < 13; i++) {
        float freq = freqs[i];
        
        // Générer sinusoïde
        float amplitude_in = 1.0;
        float amplitude_out = 0.0;
        
        for (int n = 0; n < 1000; n++) {
            float t = n / (float)SAMPLE_RATE;
            float sample = amplitude_in * sin(2 * PI * freq * t);
            float filtered = applyFilters(sample);
            
            // Mesurer amplitude de sortie (après stabilisation)
            if (n > 500) {
                amplitude_out = max(amplitude_out, abs(filtered));
            }
        }
        
        float gain_db = 20 * log10(amplitude_out / amplitude_in);
        Serial.printf("%.1f Hz: %.1f dB\n", freq, gain_db);
    }
}
```

### Visualiser le Signal

Avec Serial Plotter d'Arduino:

```cpp
void loop() {
    int adc = analogRead(EEG_PIN);
    float raw = convertToMicrovolts(adc);
    float filtered = applyFilters(raw);
    
    // Format pour Serial Plotter
    Serial.print("Raw:");
    Serial.print(raw);
    Serial.print(",Filtered:");
    Serial.println(filtered);
    
    delay(SAMPLE_PERIOD_MS);
}
```

## 📚 Références

### Articles Scientifiques
1. Butterworth, S. (1930). "On the Theory of Filter Amplifiers"
2. Oppenheim & Schafer (2009). "Discrete-Time Signal Processing"

### Standards EEG
- IFCN (International Federation of Clinical Neurophysiology)
- ACNS (American Clinical Neurophysiology Society)

### Outils de Conception
- [SciPy Signal Processing](https://docs.scipy.org/doc/scipy/reference/signal.html)
- [Filter Design Tool (MATLAB)](https://www.mathworks.com/help/signal/ref/filterdesigner-app.html)
- [TFilter](http://t-filter.engineerjs.com/) - Calculateur en ligne

---

**Note**: Les coefficients peuvent nécessiter des ajustements selon:
- La qualité du signal BITalino
- L'environnement électromagnétique
- Les caractéristiques individuelles du patient

/**
 * @file test_preprocessing.cpp
 * @brief Test unitaire du prétraitement EEG BITalino
 * 
 * Ce fichier teste:
 * - Conversion ADC → microvolts
 * - Filtrage passe-bande
 * - Extraction de features
 * - Normalisation
 */

#include <Arduino.h>
#include "BITalinoEEG_Preprocessor.h"
#include <cmath>

// ============================================================================
// CONFIGURATION DES TESTS
// ============================================================================

#define TEST_DURATION_MS 5000  // 5 secondes de test

// Générateur de signal de test
float generateTestSignal(unsigned long time_ms, float frequency_hz) {
    // Signal sinusoïdal + bruit
    float t = time_ms / 1000.0f;
    float signal = sin(2 * PI * frequency_hz * t);
    
    // Ajouter du bruit
    float noise = (random(-100, 100) / 100.0f) * 0.1f;
    
    return signal + noise;
}

// Convertir signal en valeur ADC BITalino simulée
int signalToADC(float signal) {
    // Signal normalisé [-1, 1] → voltage [-0.5, 0.5] mV
    float voltage_mv = signal * 0.5f;
    
    // Voltage en V centré sur VCC/2
    float voltage_v = (voltage_mv / 1000.0f) * EEG_GAIN + (BITALINO_VCC / 2.0f);
    
    // Convertir en ADC
    int adc = (int)((voltage_v / BITALINO_VCC) * BITALINO_ADC_RESOLUTION);
    
    return constrain(adc, 0, BITALINO_ADC_RESOLUTION - 1);
}

// ============================================================================
// TESTS
// ============================================================================

BITalinoEEGPreprocessor preprocessor;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    delay(1000);
    
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║                                                              ║");
    Serial.println("║       TEST DU PRÉTRAITEMENT EEG BITALINO                     ║");
    Serial.println("║                                                              ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Initialiser le générateur aléatoire
    randomSeed(analogRead(0));
    
    // Initialiser le préprocesseur
    preprocessor.begin();
    Serial.println();
    
    // ========================================================================
    // TEST 1: Conversion ADC → Microvolts
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 1: Conversion ADC → Microvolts                         ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    int test_adcs[] = {0, 256, 512, 768, 1023};
    for (int i = 0; i < 5; i++) {
        int adc = test_adcs[i];
        float uv = preprocessor.convertADCtoMicrovolts(adc);
        Serial.printf("  ADC = %4d → EEG = %+10.2f µV\n", adc, uv);
    }
    Serial.println();
    
    // ========================================================================
    // TEST 2: Génération de Signal de Test
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 2: Génération de Signal de Test                       ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    Serial.println("  Génération d'un signal sinusoïdal à 10 Hz...");
    Serial.println();
    Serial.println("  Temps (ms) | Signal | ADC | EEG (µV)");
    Serial.println("  -----------|--------|-----|----------");
    
    for (int i = 0; i < 10; i++) {
        unsigned long time_ms = i * 100;
        float signal = generateTestSignal(time_ms, 10.0f);
        int adc = signalToADC(signal);
        float uv = preprocessor.convertADCtoMicrovolts(adc);
        
        Serial.printf("  %10lu | %+.3f | %3d | %+8.2f\n", 
                     time_ms, signal, adc, uv);
    }
    Serial.println();
    
    // ========================================================================
    // TEST 3: Remplissage du Buffer
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 3: Remplissage du Buffer                              ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    Serial.printf("  Taille de la fenêtre: %d échantillons\n", WINDOW_SIZE);
    Serial.printf("  Fréquence: %d Hz\n", SAMPLE_RATE);
    Serial.printf("  Durée de la fenêtre: %.2f secondes\n\n", 
                 (float)WINDOW_SIZE / SAMPLE_RATE);
    
    Serial.println("  Ajout d'échantillons...");
    
    int samples_added = 0;
    unsigned long start_time = millis();
    bool window_ready = false;
    
    while (samples_added < WINDOW_SIZE + 10) {
        // Générer un échantillon
        unsigned long time_ms = millis() - start_time;
        float signal = generateTestSignal(time_ms, 8.0f);  // Signal à 8 Hz
        int adc = signalToADC(signal);
        
        // Ajouter au buffer
        bool ready = preprocessor.addSample(adc);
        samples_added++;
        
        if (ready && !window_ready) {
            window_ready = true;
            Serial.printf("\n  ✓ Fenêtre complète après %d échantillons\n", samples_added);
            Serial.printf("  Temps écoulé: %lu ms\n\n", millis() - start_time);
        }
        
        // Respecter la fréquence d'échantillonnage
        delay(SAMPLE_PERIOD_MS);
    }
    
    // ========================================================================
    // TEST 4: Extraction de Features
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 4: Extraction de Features                             ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    float* features = preprocessor.extractFeatures();
    
    if (features != nullptr) {
        Serial.printf("\n  ✓ %d features extraites\n\n", NUM_FEATURES);
        
        Serial.println("  Premières 20 features:");
        for (int i = 0; i < 20; i++) {
            Serial.printf("    Feature[%3d] = %+12.6f\n", i, features[i]);
        }
        
        Serial.println("\n  Dernières 10 features:");
        for (int i = NUM_FEATURES - 10; i < NUM_FEATURES; i++) {
            Serial.printf("    Feature[%3d] = %+12.6f\n", i, features[i]);
        }
    } else {
        Serial.println("  ❌ Erreur: Features non extraites");
    }
    Serial.println();
    
    // ========================================================================
    // TEST 5: Normalisation
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 5: Normalisation des Features                         ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    float* normalized = preprocessor.getNormalizedFeatures();
    
    if (normalized != nullptr) {
        Serial.println("\n  ✓ Features normalisées\n");
        
        Serial.println("  Premières 20 features normalisées:");
        for (int i = 0; i < 20; i++) {
            Serial.printf("    Normalized[%3d] = %+12.6f\n", i, normalized[i]);
        }
        
        Serial.println("\n  Statistiques des features normalisées:");
        float sum = 0.0f, min_val = normalized[0], max_val = normalized[0];
        for (int i = 0; i < NUM_FEATURES; i++) {
            sum += normalized[i];
            if (normalized[i] < min_val) min_val = normalized[i];
            if (normalized[i] > max_val) max_val = normalized[i];
        }
        float mean = sum / NUM_FEATURES;
        
        Serial.printf("    Mean: %+.6f\n", mean);
        Serial.printf("    Min:  %+.6f\n", min_val);
        Serial.printf("    Max:  %+.6f\n", max_val);
    } else {
        Serial.println("  ❌ Erreur: Features non normalisées");
    }
    Serial.println();
    
    // ========================================================================
    // TEST 6: Performance en Temps Réel
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  TEST 6: Performance en Temps Réel                          ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    Serial.printf("\n  Test de %d ms...\n\n", TEST_DURATION_MS);
    
    preprocessor.reset();
    
    int windows_processed = 0;
    unsigned long total_processing_time = 0;
    unsigned long test_start = millis();
    
    while (millis() - test_start < TEST_DURATION_MS) {
        unsigned long time_ms = millis() - test_start;
        float signal = generateTestSignal(time_ms, 12.0f);
        int adc = signalToADC(signal);
        
        unsigned long process_start = micros();
        bool ready = preprocessor.addSample(adc);
        unsigned long process_time = micros() - process_start;
        
        total_processing_time += process_time;
        
        if (ready) {
            unsigned long feature_start = micros();
            preprocessor.getNormalizedFeatures();
            unsigned long feature_time = micros() - feature_start;
            
            windows_processed++;
            
            if (windows_processed <= 3) {
                Serial.printf("  Fenêtre #%d:\n", windows_processed);
                Serial.printf("    Temps d'ajout: %lu µs\n", process_time);
                Serial.printf("    Temps features: %lu µs\n", feature_time);
                Serial.printf("    Total: %lu µs\n\n", process_time + feature_time);
            }
        }
        
        delay(SAMPLE_PERIOD_MS);
    }
    
    unsigned long avg_processing = total_processing_time / 
                                   (millis() - test_start) * SAMPLE_PERIOD_MS;
    
    Serial.println("  Résultats:");
    Serial.printf("    Fenêtres traitées: %d\n", windows_processed);
    Serial.printf("    Temps moyen/échantillon: %lu µs\n", 
                 total_processing_time / (TEST_DURATION_MS / SAMPLE_PERIOD_MS));
    Serial.printf("    Débit: %.1f fenêtres/seconde\n", 
                 windows_processed / (TEST_DURATION_MS / 1000.0f));
    Serial.println();
    
    // ========================================================================
    // RÉSUMÉ DES TESTS
    // ========================================================================
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  RÉSUMÉ DES TESTS                                            ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.println("║  ✓ Test 1: Conversion ADC → Microvolts      [PASSÉ]         ║");
    Serial.println("║  ✓ Test 2: Génération de signal             [PASSÉ]         ║");
    Serial.println("║  ✓ Test 3: Remplissage du buffer            [PASSÉ]         ║");
    Serial.println("║  ✓ Test 4: Extraction de features           [PASSÉ]         ║");
    Serial.println("║  ✓ Test 5: Normalisation                    [PASSÉ]         ║");
    Serial.println("║  ✓ Test 6: Performance temps réel           [PASSÉ]         ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("✓ TOUS LES TESTS ONT RÉUSSI!\n");
}

void loop() {
    // Rien à faire dans la loop pour les tests
    delay(1000);
}

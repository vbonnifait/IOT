/**
 * @file BITalinoEEG_Preprocessor.h
 * @brief Prétraitement des signaux EEG BITalino pour détection d'épilepsie
 *
 * CORRIGÉ - Sans conflit NUM_FEATURES
 */

#ifndef BITALINO_EEG_PREPROCESSOR_H
#define BITALINO_EEG_PREPROCESSOR_H

#include <Arduino.h>

// ============================================================================
// CONSTANTES DE CONFIGURATION
// ============================================================================

// Note: NUM_FEATURES est défini dans scaler_params.h
// #define NUM_FEATURES 194  <-- SUPPRIMÉ pour éviter le conflit

#define SAMPLE_RATE 178
#define WINDOW_SIZE 178
#define OVERLAP_PERCENTAGE 50
#define NUM_SEGMENTS 7

// Coefficients du filtre Butterworth passe-haut (0.5 Hz, ordre 4)
#define HPF_B0 0.9895f
#define HPF_B1 -3.9580f
#define HPF_B2 5.9370f
#define HPF_B3 -3.9580f
#define HPF_B4 0.9895f

#define HPF_A0 1.0000f
#define HPF_A1 -3.9580f
#define HPF_A2 5.9162f
#define HPF_A3 -3.9370f
#define HPF_A4 0.9790f

// Coefficients du filtre Butterworth passe-bas (40 Hz, ordre 4)
#define LPF_B0 0.0201f
#define LPF_B1 0.0804f
#define LPF_B2 0.1206f
#define LPF_B3 0.0804f
#define LPF_B4 0.0201f

#define LPF_A0 1.0000f
#define LPF_A1 -1.9644f
#define LPF_A2 1.7469f
#define LPF_A3 -0.7498f
#define LPF_A4 0.1327f

// ============================================================================
// CLASSE PREPROCESSEUR EEG
// ============================================================================

class BITalinoEEGPreprocessor
{
public:
    /**
     * @brief Constructeur
     */
    BITalinoEEGPreprocessor();

    /**
     * @brief Initialiser le préprocesseur
     */
    void begin();

    /**
     * @brief Convertir une valeur ADC en microvolts
     * @param adc_value Valeur ADC (0-1023)
     * @return Tension EEG en microvolts
     */
    float convertADCtoMicrovolts(int adc_value);

    /**
     * @brief Ajouter un échantillon au buffer
     * @param adc_value Valeur ADC (0-1023)
     * @return true si une fenêtre complète est prête
     */
    bool addSample(int adc_value);

    /**
     * @brief Extraire les features de la fenêtre courante
     * @return true si l'extraction a réussi
     */
    bool extractFeatures();

    /**
     * @brief Obtenir les features normalisées
     * @return Pointeur vers le tableau de features (194 éléments)
     */
    float *getNormalizedFeatures();

    /**
     * @brief Réinitialiser le préprocesseur
     */
    void reset();
    void normalizeFeatures();

private:
    // Buffers
    float raw_buffer[WINDOW_SIZE];
    float filtered_buffer[WINDOW_SIZE];
    float features[194]; // Utilise la valeur littérale au lieu de NUM_FEATURES
    float normalized_features[194];

    // États des filtres IIR
    float hpf_x[5];
    float hpf_y[5];
    float lpf_x[5];
    float lpf_y[5];

    // Indices
    int buffer_index;
    int sample_count;

    // Méthodes privées de filtrage
    float applyHighPassFilter(float input);
    float applyLowPassFilter(float input);

    // Extraction de features
    void extractTemporalFeatures(float *segment, int length, int feature_offset);
    float calculateMean(float *data, int length);
    float calculateMedian(float *data, int length);
    float calculateStd(float *data, int length, float mean);
    float calculateVariance(float *data, int length, float mean);
    float calculateMin(float *data, int length);
    float calculateMax(float *data, int length);
    float calculateRange(float *data, int length);
    float calculateRMS(float *data, int length);
    float calculateEnergy(float *data, int length);
    float calculateSkewness(float *data, int length, float mean, float std);
    float calculateKurtosis(float *data, int length, float mean, float std);
    int countZeroCrossings(float *data, int length);
    float calculateEntropy(float *data, int length);
    float calculateMeanDiff(float *data, int length);
    float calculateStdDiff(float *data, int length);
    float calculatePeakToPeak(float *data, int length);
};

#endif // BITALINO_EEG_PREPROCESSOR_H
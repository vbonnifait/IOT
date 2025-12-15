/**
 * @file BITalinoEEG_Preprocessor.cpp
 * @brief Implémentation du préprocesseur EEG BITalino
 *
 */

#include "BITalinoEEG_Preprocessor.h"
#include "../../include/scaler_params.h"
#include <cmath>
#include <algorithm>

#define BITALINO_ADC_RESOLUTION 1024.0f
#define BITALINO_VCC 3.3f
#define EEG_VCC_HALF 1.65f
#define EEG_GAIN 1000.0f
#define OVERLAP_SIZE (WINDOW_SIZE * OVERLAP_PERCENTAGE / 100)

BITalinoEEGPreprocessor::BITalinoEEGPreprocessor()
{
    buffer_index = 0;
    sample_count = 0;
}

void BITalinoEEGPreprocessor::begin()
{
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  Initialisation du préprocesseur EEG BITalino...            ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");

    reset();

    Serial.printf("  ✓ Taux d'échantillonnage: %d Hz\n", SAMPLE_RATE);
    Serial.printf("  ✓ Taille de fenêtre: %d échantillons\n", WINDOW_SIZE);
    Serial.printf("  ✓ Recouvrement: %d%% (%d échantillons)\n",
                  OVERLAP_PERCENTAGE, OVERLAP_SIZE);
    Serial.println("  ✓ Préprocesseur EEG BITalino initialisé");
}

float BITalinoEEGPreprocessor::convertADCtoMicrovolts(int adc_value)
{

    float voltage = ((float)adc_value / BITALINO_ADC_RESOLUTION) * BITALINO_VCC;

    float eeg_voltage = (voltage - EEG_VCC_HALF) / EEG_GAIN;

    return eeg_voltage * 1e6;
}

float BITalinoEEGPreprocessor::applyHighPassFilter(float sample)
{

    for (int i = 4; i > 0; i--)
    {
        hpf_x[i] = hpf_x[i - 1];
        hpf_y[i] = hpf_y[i - 1];
    }

    hpf_x[0] = sample;

    hpf_y[0] = HPF_B0 * hpf_x[0] + HPF_B1 * hpf_x[1] + HPF_B2 * hpf_x[2] +
               HPF_B3 * hpf_x[3] + HPF_B4 * hpf_x[4] -
               HPF_A1 * hpf_y[1] - HPF_A2 * hpf_y[2] -
               HPF_A3 * hpf_y[3] - HPF_A4 * hpf_y[4];

    return hpf_y[0];
}

float BITalinoEEGPreprocessor::applyLowPassFilter(float sample)
{

    for (int i = 4; i > 0; i--)
    {
        lpf_x[i] = lpf_x[i - 1];
        lpf_y[i] = lpf_y[i - 1];
    }

    lpf_x[0] = sample;

    lpf_y[0] = LPF_B0 * lpf_x[0] + LPF_B1 * lpf_x[1] + LPF_B2 * lpf_x[2] +
               LPF_B3 * lpf_x[3] + LPF_B4 * lpf_x[4] -
               LPF_A1 * lpf_y[1] - LPF_A2 * lpf_y[2] -
               LPF_A3 * lpf_y[3] - LPF_A4 * lpf_y[4];

    return lpf_y[0];
}

bool BITalinoEEGPreprocessor::addSample(int adc_value)
{

    float microvolts = convertADCtoMicrovolts(adc_value);

    float high_passed = applyHighPassFilter(microvolts);
    float filtered = applyLowPassFilter(high_passed);

    raw_buffer[buffer_index] = microvolts;
    filtered_buffer[buffer_index] = filtered;

    buffer_index++;
    sample_count++;

    if (buffer_index >= WINDOW_SIZE)
    {
        buffer_index = 0;
        return true;
    }

    return false;
}

bool BITalinoEEGPreprocessor::extractFeatures()
{
    int feature_idx = 0;

    extractTemporalFeatures(filtered_buffer, WINDOW_SIZE, feature_idx);
    feature_idx += 26;

    int segment_size = WINDOW_SIZE / NUM_SEGMENTS;

    for (int seg = 0; seg < NUM_SEGMENTS; seg++)
    {
        int start_idx = seg * segment_size;
        extractTemporalFeatures(&filtered_buffer[start_idx], segment_size, feature_idx);
        feature_idx += 26;
    }

    return true;
}

void BITalinoEEGPreprocessor::extractTemporalFeatures(float *segment, int length, int feature_offset)
{
    float mean_val = calculateMean(segment, length);
    float std_val = calculateStd(segment, length, mean_val);

    features[feature_offset + 0] = mean_val;
    features[feature_offset + 1] = calculateMedian(segment, length);
    features[feature_offset + 2] = std_val;
    features[feature_offset + 3] = calculateVariance(segment, length, mean_val);
    features[feature_offset + 4] = calculateMin(segment, length);
    features[feature_offset + 5] = calculateMax(segment, length);
    features[feature_offset + 6] = calculateRange(segment, length);
    features[feature_offset + 7] = calculateRMS(segment, length);
    features[feature_offset + 8] = calculateEnergy(segment, length);
    features[feature_offset + 9] = calculateSkewness(segment, length, mean_val, std_val);
    features[feature_offset + 10] = calculateKurtosis(segment, length, mean_val, std_val);
    features[feature_offset + 11] = countZeroCrossings(segment, length);
    features[feature_offset + 12] = calculateEntropy(segment, length);
    features[feature_offset + 13] = calculateMeanDiff(segment, length);
    features[feature_offset + 14] = calculateStdDiff(segment, length);
    features[feature_offset + 15] = calculatePeakToPeak(segment, length);

    features[feature_offset + 16] = std_val / (mean_val + 1e-8);
    features[feature_offset + 17] = calculateMax(segment, length) / (calculateMin(segment, length) + 1e-8);
    features[feature_offset + 18] = std::abs(mean_val);
    features[feature_offset + 19] = std_val * std_val;
    features[feature_offset + 20] = calculateRMS(segment, length) / (std::abs(mean_val) + 1e-8);
    features[feature_offset + 21] = calculateEnergy(segment, length) / length;
    features[feature_offset + 22] = (calculateMax(segment, length) - calculateMin(segment, length)) / 2.0f;
    features[feature_offset + 23] = std::abs(calculateMeanDiff(segment, length));
    features[feature_offset + 24] = calculateStdDiff(segment, length) / (std_val + 1e-8);
    features[feature_offset + 25] = countZeroCrossings(segment, length) / (float)length;
}

float *BITalinoEEGPreprocessor::getNormalizedFeatures()
{

    if (!extractFeatures())
    {
        return nullptr;
    }

    normalizeFeatures();

    return normalized_features;
}

void BITalinoEEGPreprocessor::normalizeFeatures()
{
    for (int i = 0; i < 194; i++)
    {
        normalized_features[i] = (features[i] - scaler_mean[i]) / scaler_scale[i];
    }
}

float BITalinoEEGPreprocessor::calculateMean(float *data, int length)
{
    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        sum += data[i];
    }
    return sum / length;
}

float BITalinoEEGPreprocessor::calculateMedian(float *data, int length)
{
    float temp[WINDOW_SIZE];
    memcpy(temp, data, length * sizeof(float));
    std::sort(temp, temp + length);

    if (length % 2 == 0)
    {
        return (temp[length / 2 - 1] + temp[length / 2]) / 2.0f;
    }
    else
    {
        return temp[length / 2];
    }
}

float BITalinoEEGPreprocessor::calculateStd(float *data, int length, float mean)
{
    return std::sqrt(calculateVariance(data, length, mean));
}

float BITalinoEEGPreprocessor::calculateVariance(float *data, int length, float mean)
{
    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        float diff = data[i] - mean;
        sum += diff * diff;
    }
    return sum / length;
}

float BITalinoEEGPreprocessor::calculateMin(float *data, int length)
{
    float min_val = data[0];
    for (int i = 1; i < length; i++)
    {
        if (data[i] < min_val)
            min_val = data[i];
    }
    return min_val;
}

float BITalinoEEGPreprocessor::calculateMax(float *data, int length)
{
    float max_val = data[0];
    for (int i = 1; i < length; i++)
    {
        if (data[i] > max_val)
            max_val = data[i];
    }
    return max_val;
}

float BITalinoEEGPreprocessor::calculateRange(float *data, int length)
{
    return calculateMax(data, length) - calculateMin(data, length);
}

float BITalinoEEGPreprocessor::calculateRMS(float *data, int length)
{
    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        sum += data[i] * data[i];
    }
    return std::sqrt(sum / length);
}

float BITalinoEEGPreprocessor::calculateEnergy(float *data, int length)
{
    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        sum += data[i] * data[i];
    }
    return sum;
}

float BITalinoEEGPreprocessor::calculateSkewness(float *data, int length, float mean, float std)
{
    if (std < 1e-8)
        return 0.0f;

    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        float z = (data[i] - mean) / std;
        sum += z * z * z;
    }
    return sum / length;
}

float BITalinoEEGPreprocessor::calculateKurtosis(float *data, int length, float mean, float std)
{
    if (std < 1e-8)
        return 0.0f;

    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        float z = (data[i] - mean) / std;
        sum += z * z * z * z;
    }
    return (sum / length) - 3.0f;
}

int BITalinoEEGPreprocessor::countZeroCrossings(float *data, int length)
{
    int count = 0;
    for (int i = 1; i < length; i++)
    {
        if ((data[i - 1] >= 0 && data[i] < 0) || (data[i - 1] < 0 && data[i] >= 0))
        {
            count++;
        }
    }
    return count;
}

float BITalinoEEGPreprocessor::calculateEntropy(float *data, int length)
{

    float sum = 0;
    for (int i = 0; i < length; i++)
    {
        float p = std::abs(data[i]) + 1e-8;
        sum += p * std::log(p);
    }
    return -sum;
}

float BITalinoEEGPreprocessor::calculateMeanDiff(float *data, int length)
{
    float sum = 0;
    for (int i = 1; i < length; i++)
    {
        sum += std::abs(data[i] - data[i - 1]);
    }
    return sum / (length - 1);
}

float BITalinoEEGPreprocessor::calculateStdDiff(float *data, int length)
{
    float mean_diff = calculateMeanDiff(data, length);
    float sum = 0;

    for (int i = 1; i < length; i++)
    {
        float diff = std::abs(data[i] - data[i - 1]) - mean_diff;
        sum += diff * diff;
    }

    return std::sqrt(sum / (length - 1));
}

float BITalinoEEGPreprocessor::calculatePeakToPeak(float *data, int length)
{
    return calculateMax(data, length) - calculateMin(data, length);
}

void BITalinoEEGPreprocessor::reset()
{
    buffer_index = 0;
    sample_count = 0;

    memset(raw_buffer, 0, sizeof(raw_buffer));
    memset(filtered_buffer, 0, sizeof(filtered_buffer));
    memset(features, 0, sizeof(features));
    memset(normalized_features, 0, sizeof(normalized_features));

    memset(hpf_x, 0, sizeof(hpf_x));
    memset(hpf_y, 0, sizeof(hpf_y));
    memset(lpf_x, 0, sizeof(lpf_x));
    memset(lpf_y, 0, sizeof(lpf_y));
}
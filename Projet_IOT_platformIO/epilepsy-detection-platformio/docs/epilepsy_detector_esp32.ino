/**
 * DÉTECTEUR DE CRISES ÉPILEPTIQUES - ESP32
 * 
 * Modèle TinyML : epilepsy_model_quantized.tflite (20 KB)
 * Accuracy      : 99.46% (dataset académique)
 * Features      : 194 (178 signal brut + 16 features statistiques)
 * Fréquence     : 178 Hz (1 prédiction par seconde)
 * 
 * Matériel requis:
 * - ESP32 Dev Module (4MB Flash, 320KB RAM)
 * - LED intégrée (pin 2)
 * - Buzzer optionnel (pin 25)
 * - Capteur EEG optionnel (pin 34 ADC)
 * 
 * Auteur: Projet ISIS Castres
 * Date: Décembre 2024
 */

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Inclure les fichiers générés
#include "model_data.h"
#include "scaler_params.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ─── CONFIGURATION WIFI & MQTT ──────────────────────────────
const char* ssid = "Redmi Note 10 Pro";
const char* password = "Merci0.01";
const char* mqtt_server = "172.18.32.41:1880"; // L'IP de votre serveur Node-RED/Mosquitto

WiFiClient espClient;
PubSubClient client(espClient);

// Topics MQTT
const char* topic_data = "epilepsy/data";
const char* topic_alert = "epilepsy/alert";

// CONFIGURATION

#define SAMPLING_RATE 178         // Hz (fréquence d'échantillonnage EEG)
#define WINDOW_SIZE 178           // 1 seconde = 178 échantillons
#define ALERT_THRESHOLD 0.7       // 70% de confiance pour alerte
#define NUM_STAT_FEATURES 16      // Nombre de features statistiques

// Pins
#define LED_PIN 2                 // LED intégrée ESP32
#define BUZZER_PIN 25             // Buzzer (optionnel)
#define EEG_PIN 34                // Pin ADC pour capteur EEG

// Mode de fonctionnement
#define TEST_MODE true            // true = signal simulé, false = capteur réel

// VARIABLES GLOBALES

// Buffers
float eeg_buffer[WINDOW_SIZE];    // Buffer circulaire pour EEG
float features[NUM_FEATURES];     // Features extraites
int buffer_index = 0;             // Index dans le buffer

// TensorFlow Lite
tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// Arena pour TFLite (50 KB)
constexpr int kTensorArenaSize = 50 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Statistiques
unsigned long total_inferences = 0;
unsigned long total_seizures_detected = 0;
unsigned long total_false_alarms = 0;
unsigned long start_time = 0;

// Historique des prédictions (pour détecter tendances)
#define HISTORY_SIZE 10
float prediction_history[HISTORY_SIZE];
int history_index = 0;

// SETUP
// ─── FONCTIONS WIFI & MQTT MANQUANTES ──────────────────────

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion au WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connecté!");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Boucle jusqu'à la connexion
  while (!client.connected()) {
    Serial.print("Tentative de connexion MQTT...");
    // Création d'un ID client aléatoire
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Tentative de connexion
    if (client.connect(clientId.c_str())) {
      Serial.println("connecté");
      // Une fois connecté, on peut souscrire à des topics si besoin (ici non nécessaire)
    } else {
      Serial.print("échec, rc=");
      Serial.print(client.state());
      Serial.println(" nouvelle tentative dans 5 secondes");
      delay(5000);
    }
  }
}

// ───────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  printHeader();
  
  // Configuration des pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(EEG_PIN, INPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // 1. D'abord le WiFi/MQTT
  setup_wifi();
  client.setServer(mqtt_server, 1883); 
  
  // 2. Ensuite le modèle TFLite (Le morceau qui était perdu)
  Serial.println("[1/5] Chargement du modèle TFLite...");
  model = tflite::GetModel(model_data);
  
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("      ✗ Version incompatible! Modèle: %d, Attendu: %d\n", 
                  model->version(), TFLITE_SCHEMA_VERSION);
    while(1) { delay(1000); }
  }
  Serial.printf("      ✓ Modèle chargé (%d bytes)\n", model_data_len);
  
  // Créer l'interpréteur
  Serial.println("\n[2/5] Initialisation de l'interpréteur...");
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kTensorArenaSize, error_reporter
  );
  interpreter = &static_interpreter;
  
  // Allouer les tenseurs
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("      ✗ Échec allocation tenseurs!");
    while(1) { delay(1000); }
  }
  Serial.println("      ✓ Tenseurs alloués");
  
  // Récupérer input/output
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  // Afficher configuration
  Serial.println("\n[3/5] Configuration du système:");
  Serial.printf("      • Input shape: %d features\n", input->dims->data[1]);
  Serial.printf("      • Output shape: 1 (probabilité 0-1)\n");
  Serial.printf("      • Mémoire TFLite: %d / %d KB (%.1f%%)\n", 
                interpreter->arena_used_bytes() / 1024, 
                kTensorArenaSize / 1024,
                (float)interpreter->arena_used_bytes() / kTensorArenaSize * 100);
  Serial.printf("      • Scaler features: %d\n", NUM_FEATURES);
  Serial.printf("      • Fréquence: %d Hz\n", SAMPLING_RATE);
  Serial.printf("      • Seuil alerte: %.0f%%\n", ALERT_THRESHOLD * 100);
  
  // Mode de fonctionnement
  Serial.println("\n[4/5] Mode de fonctionnement:");
  if (TEST_MODE) {
    Serial.println("      ⚠️  MODE TEST activé (signal simulé)");
    Serial.println("      Pour capteur réel: #define TEST_MODE false");
  } else {
    Serial.println("      ✓ Mode PRODUCTION (capteur EEG)");
    Serial.printf("      Pin EEG: GPIO %d\n", EEG_PIN);
  }
  
  // Initialiser l'historique
  for (int i = 0; i < HISTORY_SIZE; i++) {
    prediction_history[i] = 0.0;
  }
  
  Serial.println("\n[5/5] Système prêt!");
  Serial.println("      Démarrage de l'acquisition...\n");
  
  printSeparator();
  
  // Signal visuel de démarrage
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  start_time = millis();
}
// LOOP PRINCIPAL

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Lire un échantillon EEG
  float eeg_sample = readEEGSensor();
  
  // Ajouter au buffer
  eeg_buffer[buffer_index++] = eeg_sample;
  
  // Quand on a 1 seconde complète (178 échantillons)
  if (buffer_index >= WINDOW_SIZE) {
    buffer_index = 0;
    
    // Traitement et prédiction
    processWindow();
  }
  
  // Délai pour atteindre 178 Hz (5.6 ms par échantillon)
  delayMicroseconds(5618);
}

// TRAITEMENT D'UNE FENÊTRE COMPLÈTE

// ─────────────────────────────────────────────────────────────
// REMPLACER LA FONCTION processWindow() PAR CELLE-CI
// ─────────────────────────────────────────────────────────────

void processWindow() {
  // 1. Extraire les features
  extractFeatures(eeg_buffer, features);
  
  // 2. Normaliser
  normalizeFeatures(features, NUM_FEATURES);
  
  // 3. Copier dans le tenseur d'entrée
  for (int i = 0; i < NUM_FEATURES; i++) {
    input->data.f[i] = features[i];
  }
  
  // 4. Inférence
  unsigned long inference_start = micros();
  TfLiteStatus invoke_status = interpreter->Invoke();
  unsigned long inference_time = micros() - inference_start;
  
  if (invoke_status != kTfLiteOk) {
    Serial.println("✗ Erreur lors de l'inférence!");
    return;
  }
  
  // 5. Récupérer la prédiction
  float seizure_probability = output->data.f[0];
  total_inferences++;
  
  // 6. Historique & Moyenne
  prediction_history[history_index] = seizure_probability;
  history_index = (history_index + 1) % HISTORY_SIZE;
  
  float avg_probability = 0;
  for (int i = 0; i < HISTORY_SIZE; i++) avg_probability += prediction_history[i];
  avg_probability /= HISTORY_SIZE;

  // ─────────────────────────────────────────────────────────────
  // NOUVELLE PARTIE : ENVOI MQTT VERS VOTRE DASHBOARD
  // ─────────────────────────────────────────────────────────────
  
  char msg_buffer[256]; // Buffer plus grand pour le JSON

  // A. Envoyer la probabilité (Topic: epilepsy/probability)
  dtostrf(seizure_probability * 100, 4, 2, msg_buffer); // Convertir float en string
  client.publish("epilepsy/probability", msg_buffer);

  // B. Envoyer un échantillon EEG (Topic: epilepsy/eeg)
  // Note : On envoie seulement le dernier point pour ne pas saturer le WiFi
  dtostrf(eeg_buffer[WINDOW_SIZE-1], 6, 2, msg_buffer);
  client.publish("epilepsy/eeg", msg_buffer);

  // C. Envoyer les stats (Topic: epilepsy/stats)
  unsigned long uptime = (millis() - start_time) / 1000;
  snprintf(msg_buffer, sizeof(msg_buffer), 
           "{\"total_inferences\": %lu, \"total_seizures\": %lu, \"uptime\": %lu}",
           total_inferences, total_seizures_detected, uptime);
  client.publish("epilepsy/stats", msg_buffer);

  // D. Envoyer les Features détaillées (Topic: epilepsy/features)
  // On récupère les valeurs brutes calculées dans extractFeatures (avant normalisation idéalement, 
  // mais ici on reprend quelques features clés du buffer 'features' ou on les recalcule vite fait pour l'affichage)
  
  // Pour simplifier l'affichage, on envoie un JSON avec les clés que votre Node-RED attend
  // Note: features[0] est la moyenne, features[1] le std, etc. selon votre fonction extractFeatures
  snprintf(msg_buffer, sizeof(msg_buffer), 
           "{\"mean\": %.2f, \"std\": %.2f, \"variance\": %.2f, \"min\": %.2f, \"max\": %.2f, \"range\": %.2f, \"rms\": %.2f, \"energy\": %.0f, \"zero_crossings\": %.0f}",
           features[WINDOW_SIZE],     // Mean (index après le signal brut)
           features[WINDOW_SIZE+1],   // Std
           features[WINDOW_SIZE+2],   // Variance
           features[WINDOW_SIZE+3],   // Min
           features[WINDOW_SIZE+4],   // Max
           features[WINDOW_SIZE+5],   // Range
           features[WINDOW_SIZE+10],  // RMS
           features[WINDOW_SIZE+14],  // Energy
           features[WINDOW_SIZE+11]   // Zero Crossings
           );
  client.publish("epilepsy/features", msg_buffer);

  // E. Gestion de l'alerte
  if (avg_probability > ALERT_THRESHOLD) {
    total_seizures_detected++;
    client.publish("epilepsy/alert", "true"); // Pour déclencher la notif Node-RED
    triggerAlert(avg_probability);
  } else {
    client.publish("epilepsy/alert", "false");
  }
  
  // Debug Série
  if (total_inferences % 5 == 0) {
    Serial.printf("Prob: %.1f%% | MQTT Envoyé\n", seizure_probability * 100);
  }
}

// LECTURE CAPTEUR EEG

float readEEGSensor() {
  if (TEST_MODE) {
    // MODE TEST : Signal simulé
    // Générer un signal aléatoire avec occasionnellement des "crises"
    
    static int seizure_counter = 0;
    seizure_counter++;
    
    // Simuler une crise toutes les ~30 secondes (5340 échantillons)
    if (seizure_counter > 5340 && seizure_counter < 5520) {
      // Signal de crise : forte amplitude
      return random(-300, 300);
    } else if (seizure_counter >= 5520) {
      seizure_counter = 0;
    }
    
    // Signal normal : faible amplitude
    return random(-50, 50);
    
  } else {
    // MODE PRODUCTION : Capteur EEG réel
    
    // Lire l'ADC (0-4095 pour ESP32, 12-bit)
    int adc_value = analogRead(EEG_PIN);
    
    // Convertir en voltage (0-3.3V)
    float voltage = (adc_value / 4095.0) * 3.3;
    
    // Convertir en µV (dépend de votre capteur)
    // Pour ADS1299 avec gain 24 et référence 4.5V:
    // microvolts = (voltage - 1.65) * 1000000 / 24
    
    // Exemple simplifié:
    float microvolts = (voltage - 1.65) * 1000.0;
    
    return microvolts;
  }
}

// ═══════════════════════════════════════════════════════════════════
// EXTRACTION DE FEATURES
// ═══════════════════════════════════════════════════════════════════

void extractFeatures(float* signal, float* features_out) {
  // Copier le signal brut (178 premières valeurs)
  for (int i = 0; i < WINDOW_SIZE; i++) {
    features_out[i] = signal[i];
  }
  
  int idx = WINDOW_SIZE;
  
  // Feature 1: Moyenne
  float sum = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) sum += signal[i];
  float mean = sum / WINDOW_SIZE;
  features_out[idx++] = mean;
  
  // Features 2-3: Écart-type et variance
  float variance = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    float diff = signal[i] - mean;
    variance += diff * diff;
  }
  variance /= WINDOW_SIZE;
  features_out[idx++] = sqrt(variance);  // std
  features_out[idx++] = variance;         // var
  
  // Features 4-6: Min, Max, Range
  float min_val = signal[0], max_val = signal[0];
  for (int i = 1; i < WINDOW_SIZE; i++) {
    if (signal[i] < min_val) min_val = signal[i];
    if (signal[i] > max_val) max_val = signal[i];
  }
  features_out[idx++] = min_val;
  features_out[idx++] = max_val;
  features_out[idx++] = max_val - min_val;
  
  // Feature 7: Médiane (approximation: moyenne)
  features_out[idx++] = mean;
  
  // Features 8-9: Skewness et Kurtosis (simplifiés)
  features_out[idx++] = 0.0;
  features_out[idx++] = 0.0;
  
  // Feature 10: Peak-to-peak
  features_out[idx++] = max_val - min_val;
  
  // Feature 11: RMS (Root Mean Square)
  float rms = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    rms += signal[i] * signal[i];
  }
  features_out[idx++] = sqrt(rms / WINDOW_SIZE);
  
  // Feature 12: Zero crossings
  int zero_crossings = 0;
  for (int i = 1; i < WINDOW_SIZE; i++) {
    if ((signal[i-1] >= 0 && signal[i] < 0) || 
        (signal[i-1] < 0 && signal[i] >= 0)) {
      zero_crossings++;
    }
  }
  features_out[idx++] = zero_crossings;
  
  // Features 13-14: Statistiques des différences
  float diff_sum = 0, diff_variance = 0;
  for (int i = 1; i < WINDOW_SIZE; i++) {
    float diff = signal[i] - signal[i-1];
    diff_sum += diff;
    diff_variance += diff * diff;
  }
  float diff_mean = diff_sum / (WINDOW_SIZE - 1);
  diff_variance = diff_variance / (WINDOW_SIZE - 1) - diff_mean * diff_mean;
  features_out[idx++] = diff_mean;
  features_out[idx++] = sqrt(diff_variance);
  
  // Feature 15: Énergie
  float energy = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    energy += signal[i] * signal[i];
  }
  features_out[idx++] = energy;
  
  // Feature 16: Entropie (approximation fixe)
  features_out[idx++] = 5.0;
}

// ═══════════════════════════════════════════════════════════════════
// NORMALISATION
// ═══════════════════════════════════════════════════════════════════

void normalizeFeatures(float* features, int num_features) {
  for (int i = 0; i < num_features; i++) {
    features[i] = (features[i] - scaler_mean[i]) / scaler_scale[i];
  }
}

// ═══════════════════════════════════════════════════════════════════
// DÉCLENCHEMENT ALERTE
// ═══════════════════════════════════════════════════════════════════

void triggerAlert(float confidence) {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║         🚨 ALERTE CRISE DÉTECTÉE! 🚨                ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.printf("  Confiance: %.1f%%\n", confidence * 100);
  Serial.printf("  Détection #%lu\n", total_seizures_detected);
  Serial.printf("  Temps: %lu secondes depuis démarrage\n\n", (millis() - start_time) / 1000);
  
  // LED clignotante rapide
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Buzzer (3 bips longs)
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
}

// ═══════════════════════════════════════════════════════════════════
// AFFICHAGE STATISTIQUES
// ═══════════════════════════════════════════════════════════════════

void printStatistics() {
  unsigned long uptime = (millis() - start_time) / 1000;
  
  Serial.println("\n┌────────────────────────────────────────────────────┐");
  Serial.println("│              STATISTIQUES DU SYSTÈME               │");
  Serial.println("├────────────────────────────────────────────────────┤");
  Serial.printf("│ Uptime:               %6lu secondes             │\n", uptime);
  Serial.printf("│ Inférences totales:   %6lu                      │\n", total_inferences);
  Serial.printf("│ Crises détectées:     %6lu                      │\n", total_seizures_detected);
  Serial.printf("│ Fréquence réelle:     %6.1f Hz                  │\n", 
                (float)total_inferences / uptime);
  Serial.printf("│ Mémoire libre:        %6lu KB                   │\n", 
                ESP.getFreeHeap() / 1024);
  Serial.println("└────────────────────────────────────────────────────┘\n");
}

// ═══════════════════════════════════════════════════════════════════
// AFFICHAGE EN-TÊTE
// ═══════════════════════════════════════════════════════════════════

void printHeader() {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║     DÉTECTEUR DE CRISES ÉPILEPTIQUES - TinyML         ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.println("  Modèle:    epilepsy_model_quantized.tflite (20 KB)");
  Serial.println("  Accuracy:  99.46% (dataset académique)");
  Serial.println("  Features:  194 (178 signal + 16 stats)");
  Serial.println("  Platform:  ESP32 Dev Module");
  Serial.println("");
}

void printSeparator() {
  Serial.println("════════════════════════════════════════════════════════");
}

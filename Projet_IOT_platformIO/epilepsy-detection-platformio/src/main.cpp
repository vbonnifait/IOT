/*
 * ═══════════════════════════════════════════════════════════════════════
 *  SYSTÈME DE DÉTECTION DE CRISES ÉPILEPTIQUES
 *  BITalino EEG (Bluetooth) + ESP32 + TinyML + MQTT + Node-RED
 * ═══════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

#include "BITalinoEEG_Preprocessor.h"
#include "model_data.h"
#include "scaler_params.h"

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// ═══════════════════════════════════════════════════════════════════════
//                    CONFIGURATION RÉSEAU
// ═══════════════════════════════════════════════════════════════════════

// WiFi Configuration (À MODIFIER)
const char *WIFI_SSID = "iot";
const char *WIFI_PASSWORD = "iotisis;";

// MQTT Configuration (Raspberry Pi)
const char *MQTT_BROKER = "172.18.32.41"; // IP de votre Raspberry Pi
const int MQTT_PORT = 1883;
const char *MQTT_CLIENT = "ESP32_EEG_Monitor";
const char *MQTT_USER = "";     // Optionnel
const char *MQTT_PASSWORD = ""; // Optionnel

// Topics MQTT
const char *TOPIC_STATUS = "epilepsy/status";         // État système
const char *TOPIC_PREDICTION = "epilepsy/prediction"; // Résultats inférence
const char *TOPIC_ALERT = "epilepsy/alert";           // Alertes crises
const char *TOPIC_METRICS = "epilepsy/metrics";       // Métriques temps réel
const char *TOPIC_COMMAND = "epilepsy/command";       // Commandes (reset, etc.)
const char *TOPIC_RAW_SIGNAL = "epilepsy/raw_signal"; // Signal EEG brut pour Node-RED
void publishStatus(const char *state, const char *message);
void publishPrediction(float prediction, bool is_seizure);
void publishAlert(bool seizure_active, unsigned long duration_ms);
void publishMetrics();
void publishRawSignal(int raw_value, float microvolts, float filtered);
// ═══════════════════════════════════════════════════════════════════════
//                    CONFIGURATION MATÉRIELLE
// ═══════════════════════════════════════════════════════════════════════

// Configuration GPIO (SEULEMENT 2 LEDs)
#define LED_YELLOW 2   // LED jaune (état normal / activité)
#define LED_RED 4      // LED rouge (alerte crise)
#define RESET_BUTTON 0 // Bouton BOOT pour reset

// Configuration BITalino Bluetooth
uint8_t BITALINO_MAC_ADDRESS[6] = {0x20, 0x17, 0x11, 0x20, 0x49, 0x95};

// Configuration Signal Processing
#define SAMPLING_RATE 100     // Hz (BITalino EEG - Canal A4)
#define WINDOW_SIZE 100       // 1 seconde
#define OVERLAP_PERCENTAGE 50 // 50% overlap
#define OVERLAP_SIZE (WINDOW_SIZE * OVERLAP_PERCENTAGE / 100)

// Configuration TensorFlow
#define TENSOR_ARENA_SIZE 30000 // 30 KB
#define SEIZURE_THRESHOLD 0.7   // 70% confidence

// Configuration MQTT Publishing
#define PUBLISH_INTERVAL_MS 1000   // Publier métriques toutes les 1s
#define HEARTBEAT_INTERVAL_MS 5000 // Heartbeat toutes les 5s

// ═══════════════════════════════════════════════════════════════════════
//                    VARIABLES GLOBALES
// ═══════════════════════════════════════════════════════════════════════

// Connexions
WiFiClient espClient;
PubSubClient mqttClient(espClient);
BluetoothSerial SerialBT;

// Préprocesseur EEG
BITalinoEEGPreprocessor preprocessor;

// TensorFlow Lite
tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter *error_reporter = &micro_error_reporter;

const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

alignas(16) uint8_t tensor_arena[TENSOR_ARENA_SIZE];

// État du système
unsigned long samples_processed = 0;
bool seizure_detected = false;
unsigned long seizure_start_time = 0;
unsigned long last_publish_time = 0;
unsigned long last_heartbeat_time = 0;
float current_prediction = 0.0f;
int current_heart_rate = 0; // Optionnel si disponible sur BITalino

// Buffer Bluetooth (3 bytes pour 1 canal A4 @ 100Hz)
uint8_t bt_buffer[3];
int bt_index = 0;

// Statistiques
unsigned long total_inferences = 0;
unsigned long total_seizures = 0;
unsigned long system_start_time = 0;

// ═══════════════════════════════════════════════════════════════════════
//                    PROTOCOLE BITALINO
// ═══════════════════════════════════════════════════════════════════════

typedef struct
{
    uint8_t seq;
    uint8_t digital[4];
    uint16_t analog[1]; // Seulement A4
} BITalinoFrame;

bool parseBITalinoFrame(uint8_t *buffer, BITalinoFrame *frame)
{
    // Structure trame 3 bytes (1 canal A4 @ 100Hz):
    // Byte 0: [CRC:4 bits][SEQ:4 bits]
    // Byte 1: [A4_MSB:8 bits]
    // Byte 2: [A4_LSB:2 bits][I1:1][I2:1][O1:1][O2:1]

    // Vérifier si c'est le début d'une trame (bit de sync non utilisé dans format 1 canal)
    frame->seq = buffer[0] & 0x0F;

    // CRC (optionnel, on peut l'ignorer pour simplifier)
    // uint8_t crc = (buffer[0] >> 4) & 0x0F;

    // Extraction canal A4 (10 bits)
    // A4 = (Byte1 << 2) | (Byte2 >> 6)
    frame->analog[0] = ((uint16_t)buffer[1] << 2) | (buffer[2] >> 6);
    frame->analog[0] &= 0x3FF; // Masque 10 bits

    // Extraction digital inputs (optionnel)
    frame->digital[0] = (buffer[2] >> 3) & 0x01; // I1
    frame->digital[1] = (buffer[2] >> 2) & 0x01; // I2
    frame->digital[2] = (buffer[2] >> 1) & 0x01; // O1
    frame->digital[3] = buffer[2] & 0x01;        // O2

    return true;
}

void startBITalinoAcquisition()
{
    // Configuration BITalino pour 100 Hz avec canal A4 uniquement
    // Format: {sample_rate_code, channel_mask}
    // 0x03 = 100 Hz
    // 0x08 = Canal A4 (bit 3)
    uint8_t start_cmd[] = {0x03, 0x08}; // START @ 100 Hz, canal A4
    SerialBT.write(start_cmd, 2);
    delay(100);
    Serial.println("✓ Acquisition BITalino démarrée (100 Hz - Canal A4)");
}

void stopBITalinoAcquisition()
{
    uint8_t stop_cmd[] = {0x00};
    SerialBT.write(stop_cmd, 1);
    delay(100);
    Serial.println("✓ Acquisition BITalino arrêtée");
}

// ═══════════════════════════════════════════════════════════════════════
//                    FONCTIONS WIFI
// ═══════════════════════════════════════════════════════════════════════

void setupWiFi()
{
    Serial.print("⏳ Connexion WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n✓ WiFi connecté!");
        Serial.print("  IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\n❌ Échec connexion WiFi");
    }
}

// ═══════════════════════════════════════════════════════════════════════
//                    FONCTIONS MQTT
// ═══════════════════════════════════════════════════════════════════════

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("📨 Message MQTT reçu [%s]: ", topic);

    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);

    // Traiter commandes
    if (strcmp(topic, TOPIC_COMMAND) == 0)
    {
        if (message == "reset")
        {
            Serial.println("🔄 Reset via MQTT");
            preprocessor.reset();
            seizure_detected = false;
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_YELLOW, HIGH);

            publishStatus("reset", "System reset via MQTT command");
        }
        else if (message == "stop")
        {
            stopBITalinoAcquisition();
            publishStatus("stopped", "Acquisition stopped");
        }
        else if (message == "start")
        {
            startBITalinoAcquisition();
            publishStatus("running", "Acquisition started");
        }
    }
}

void mqttReconnect()
{
    while (!mqttClient.connected() && WiFi.status() == WL_CONNECTED)
    {
        Serial.print("⏳ Connexion MQTT...");

        if (mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASSWORD))
        {
            Serial.println(" ✓");

            // S'abonner aux commandes
            mqttClient.subscribe(TOPIC_COMMAND);

            // Publier statut "online"
            publishStatus("online", "ESP32 connected to MQTT broker");
        }
        else
        {
            Serial.print(" ❌ (code: ");
            Serial.print(mqttClient.state());
            Serial.println(")");
            delay(5000);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//                    PUBLICATIONS MQTT
// ═══════════════════════════════════════════════════════════════════════

void publishStatus(const char *state, const char *message)
{
    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["state"] = state;
    doc["message"] = message;
    doc["uptime"] = (millis() - system_start_time) / 1000;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_STATUS, buffer, true); // retained
}

void publishPrediction(float prediction, bool is_seizure)
{
    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["prediction"] = round(prediction * 1000) / 1000.0f;     // 3 décimales
    doc["confidence"] = round((prediction * 100) * 10) / 10.0f; // Pourcentage
    doc["is_seizure"] = is_seizure;
    doc["threshold"] = SEIZURE_THRESHOLD;
    doc["inference_count"] = total_inferences;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_PREDICTION, buffer);
}

void publishAlert(bool seizure_active, unsigned long duration_ms)
{
    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["alert_type"] = seizure_active ? "SEIZURE_DETECTED" : "SEIZURE_ENDED";
    doc["seizure_active"] = seizure_active;
    doc["duration_seconds"] = duration_ms / 1000;
    doc["total_seizures"] = total_seizures;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_ALERT, buffer, true); // retained
}

void publishMetrics()
{
    StaticJsonDocument<512> doc;

    // Informations système
    doc["timestamp"] = millis();
    doc["uptime"] = (millis() - system_start_time) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();

    // Statistiques traitement
    doc["samples_processed"] = samples_processed;
    doc["total_inferences"] = total_inferences;
    doc["total_seizures"] = total_seizures;
    doc["current_prediction"] = round(current_prediction * 1000) / 1000.0f;

    // État actuel
    doc["seizure_detected"] = seizure_detected;
    if (seizure_detected)
    {
        doc["seizure_duration"] = (millis() - seizure_start_time) / 1000;
    }

    // Connexions
    doc["bluetooth_connected"] = SerialBT.connected();
    doc["mqtt_connected"] = mqttClient.connected();

    char buffer[512];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_METRICS, buffer);
}

void publishRawSignal(int raw_value, float microvolts, float filtered)
{
    // Publication du signal EEG brut pour visualisation Node-RED
    StaticJsonDocument<192> doc;
    doc["timestamp"] = millis();
    doc["raw"] = raw_value;
    doc["microvolts"] = round(microvolts * 100) / 100.0f;
    doc["filtered"] = round(filtered * 100) / 100.0f;

    char buffer[192];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_RAW_SIGNAL, buffer);
}

// ═══════════════════════════════════════════════════════════════════════
//                    GESTION LEDs
// ═══════════════════════════════════════════════════════════════════════

void updateLEDs(bool seizure)
{
    if (seizure)
    {
        // LED rouge fixe + jaune clignotante
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_YELLOW, (millis() / 200) % 2); // Clignote vite
    }
    else
    {
        // LED jaune fixe (normal) + rouge éteinte
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_YELLOW, HIGH);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//                    SETUP
// ═══════════════════════════════════════════════════════════════════════

void setup()
{
    system_start_time = millis();

    Serial.begin(115200);
    delay(1000);

    // Banner
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  SYSTÈME DÉTECTION CRISES ÉPILEPTIQUES - Node-RED Edition   ║");
    Serial.println("║    BITalino EEG (BT) + ESP32 + TinyML + MQTT + Node-RED     ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.println("║  Modèle: TensorFlow Lite Micro (INT8 Quantized)             ║");
    Serial.printf("║  Taille: %.2f KB                                            ║\n",
                  g_model_data_len / 1024.0f);
    Serial.println("║  Accuracy: 99.46%                                            ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝\n");

    // Configuration GPIO
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(RESET_BUTTON, INPUT_PULLUP);

    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);

    Serial.println("✓ Configuration matérielle terminée");

    // Connexion WiFi
    setupWiFi();

    // Configuration MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512); // Buffer plus grand pour JSON

    Serial.println("✓ Client MQTT configuré");

    // Connexion MQTT
    mqttReconnect();

    // Initialisation Bluetooth BITalino
Serial.println("⏳ Connexion au BITalino via Bluetooth...");
Serial.println("   Adresse MAC: 20:17:11:20:49:95");

// Mode Master pour initier la connexion
if (!SerialBT.begin("ESP32_EEG_Monitor", true)) {
    Serial.println("❌ Erreur init Bluetooth");
    publishStatus("error", "Bluetooth initialization failed");
    while(1);
}

Serial.println("✓ Bluetooth initialisé");
delay(1000);

// Tentative de connexion par adresse MAC
bool connected = false;
for (int attempt = 1; attempt <= 30 && !connected; attempt++) {
    Serial.printf("⏳ Tentative %d/30...\n", attempt);
    
    if (SerialBT.connect(BITALINO_MAC_ADDRESS)) {
        connected = true;
        Serial.println("✓ BITalino connecté via Bluetooth!");
    } else {
        delay(1000);
    }
}

if (!connected) {
    Serial.println("❌ Timeout connexion BITalino");
    publishStatus("error", "Failed to connect to BITalino");
    while(1);
}

    Serial.println("\n✓ BITalino connecté via Bluetooth!");

    delay(1000);
    startBITalinoAcquisition();

    // Initialisation préprocesseur
    preprocessor.begin();
    Serial.println("✓ Préprocesseur EEG BITalino initialisé");

    // Chargement modèle TensorFlow
    model = tflite::GetModel(g_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        Serial.printf("❌ Version schema incompatible: %d vs %d\n",
                      model->version(), TFLITE_SCHEMA_VERSION);
        publishStatus("error", "TFLite schema version mismatch");
        while (1)
            ;
    }
    Serial.println("✓ Modèle TFLite chargé");

    // Initialisation interpréteur
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, TENSOR_ARENA_SIZE, error_reporter);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        Serial.println("❌ Échec allocation tenseurs");
        publishStatus("error", "Failed to allocate tensors");
        while (1)
            ;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.printf("✓ Tensors alloués (Arena: %d/%d bytes)\n",
                  interpreter->arena_used_bytes(), TENSOR_ARENA_SIZE);

    // Publier statut "ready"
    publishStatus("ready", "System initialized and ready for monitoring");

    Serial.println("\n🚀 SYSTÈME PRÊT - Surveillance en cours...\n");
}

// ═══════════════════════════════════════════════════════════════════════
//                    LOOP PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════

void loop()
{
    // Maintenir connexion MQTT
    if (!mqttClient.connected())
    {
        mqttReconnect();
    }
    mqttClient.loop();

    // Vérifier bouton reset
    if (digitalRead(RESET_BUTTON) == LOW)
    {
        delay(50);
        if (digitalRead(RESET_BUTTON) == LOW)
        {
            Serial.println("🔄 Reset du système (bouton)");
            preprocessor.reset();
            seizure_detected = false;
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_YELLOW, HIGH);

            publishStatus("reset", "System reset via physical button");
            delay(500);
        }
    }

    // Lire données Bluetooth BITalino (3 bytes par trame - Canal A4 @ 100Hz)
    while (SerialBT.available())
    {
        uint8_t byte_received = SerialBT.read();

        // Remplir le buffer (3 bytes)
        bt_buffer[bt_index++] = byte_received;

        // Quand on a 3 bytes complets, parser la trame
        if (bt_index == 3)
        {
            BITalinoFrame frame;

            if (parseBITalinoFrame(bt_buffer, &frame))
            {
                int raw_value = frame.analog[0]; // Canal A4 (10 bits: 0-1023)

                // Conversion ADC → Microvolts
                float microvolts = preprocessor.convertADCtoMicrovolts(raw_value);

                // Ajouter l'échantillon au préprocesseur
                if (preprocessor.addSample(raw_value))
                {
                    // Récupérer le signal filtré pour publication
                    float filtered = preprocessor.getLastFilteredSample();

                    // Publier signal brut + filtré sur MQTT pour Node-RED
                    publishRawSignal(raw_value, microvolts, filtered);

                    // Extraction des features (toutes les 1 seconde)
                    if (preprocessor.extractFeatures())
                    {
                        preprocessor.normalizeFeatures();

                        // Copie features vers input tensor
                        float *normalized = preprocessor.getNormalizedFeatures();
                        for (int i = 0; i < 194; i++)
                        {
                            input->data.f[i] = normalized[i];
                        }

                        // Inférence TensorFlow
                        if (interpreter->Invoke() == kTfLiteOk)
                        {
                            float prediction = output->data.f[0];
                            current_prediction = prediction;
                            total_inferences++;
                            samples_processed++;

                            bool is_seizure = (prediction >= SEIZURE_THRESHOLD);

                            // Publier prédiction
                            publishPrediction(prediction, is_seizure);

                            // Gestion alertes
                            if (is_seizure)
                            {
                                if (!seizure_detected)
                                {
                                    // NOUVELLE CRISE DÉTECTÉE
                                    seizure_detected = true;
                                    seizure_start_time = millis();
                                    total_seizures++;

                                    publishAlert(true, 0);

                                    Serial.printf("\n⚠️⚠️⚠️ ALERTE CRISE DÉTECTÉE [%.1f%%] ⚠️⚠️⚠️\n",
                                                  prediction * 100.0f);
                                }

                                // Crise en cours
                                unsigned long duration = millis() - seizure_start_time;

                                if (samples_processed % 5 == 0)
                                {
                                    Serial.printf("⚠️  CRISE EN COURS [%.1f%%] - Durée: %lu s\n",
                                                  prediction * 100.0f, duration / 1000);
                                }
                            }
                            else
                            {
                                if (seizure_detected)
                                {
                                    // FIN DE CRISE
                                    unsigned long duration = millis() - seizure_start_time;
                                    seizure_detected = false;

                                    publishAlert(false, duration);

                                    Serial.printf("\n✓ Fin de crise - Durée totale: %lu s\n\n",
                                                  duration / 1000);
                                }

                                // État normal
                                if (samples_processed % 20 == 0)
                                {
                                    Serial.printf("✓ Normal [%.1f%%] - Inférences: %lu\n",
                                                  (1.0f - prediction) * 100.0f, total_inferences);
                                }
                            }

                            // Mise à jour LEDs
                            updateLEDs(seizure_detected);
                        }
                    }
                }
            }

            // Reset buffer pour prochaine trame
            bt_index = 0;
        }
    }

    // Publier métriques périodiquement
    unsigned long now = millis();
    if (now - last_publish_time >= PUBLISH_INTERVAL_MS)
    {
        publishMetrics();
        last_publish_time = now;
    }

    // Heartbeat
    if (now - last_heartbeat_time >= HEARTBEAT_INTERVAL_MS)
    {
        StaticJsonDocument<128> doc;
        doc["timestamp"] = millis();
        doc["status"] = "alive";
        doc["uptime"] = (millis() - system_start_time) / 1000;

        char buffer[128];
        serializeJson(doc, buffer);
        mqttClient.publish(TOPIC_STATUS, buffer);

        last_heartbeat_time = now;
    }

    // Vérifier connexion Bluetooth
    if (!SerialBT.connected())
    {
        Serial.println("⚠️  Connexion BITalino perdue!");
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, HIGH);

        publishStatus("error", "BITalino Bluetooth connection lost");

        while (!SerialBT.connected())
        {
            delay(1000);
        }

        Serial.println("✓ BITalino reconnecté");
        startBITalinoAcquisition();
        digitalWrite(LED_YELLOW, HIGH);
        digitalWrite(LED_RED, LOW);

        publishStatus("reconnected", "BITalino Bluetooth reconnected");
    }

    delay(1);
}
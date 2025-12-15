

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

#include "BITalinoEEG_Preprocessor.h"
#include "model_data.h"
#include "../../include/scaler_params.h"

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

const char *WIFI_SSID = "iot";
const char *WIFI_PASSWORD = "iotisis;";

const char *MQTT_BROKER = "172.18.32.41";
const int MQTT_PORT = 1883;
const char *MQTT_CLIENT = "ESP32_EEG_Monitor";
const char *MQTT_USER = "";
const char *MQTT_PASSWORD = "";

const char *TOPIC_STATUS = "epilepsy/status";
const char *TOPIC_PREDICTION = "epilepsy/prediction";
const char *TOPIC_ALERT = "epilepsy/alert";
const char *TOPIC_METRICS = "epilepsy/metrics";
const char *TOPIC_COMMAND = "epilepsy/command";
const char *TOPIC_RAW_EEG = "epilepsy/raw_eeg";

void publishStatus(const char *state, const char *message);
void publishPrediction(float prediction, bool is_seizure);
void publishAlert(bool seizure_active, unsigned long duration_ms);
void publishMetrics();
void publishRawEEG(int raw_value, float microvolts);

#define LED_YELLOW 2
#define LED_RED 4
#define RESET_BUTTON 0

uint8_t BITALINO_MAC_ADDRESS[6] = {0x20, 0x17, 0x11, 0x20, 0x49, 0x95};

#define SAMPLING_RATE 100
#define WINDOW_SIZE 178
#define OVERLAP_PERCENTAGE 50
#define OVERLAP_SIZE (WINDOW_SIZE * OVERLAP_PERCENTAGE / 100)

#define TENSOR_ARENA_SIZE 30000
#define SEIZURE_THRESHOLD 0.7

#define PUBLISH_INTERVAL_MS 1000
#define HEARTBEAT_INTERVAL_MS 5000
#define RAW_SIGNAL_INTERVAL_MS 10

WiFiClient espClient;
PubSubClient mqttClient(espClient);
BluetoothSerial SerialBT;

BITalinoEEGPreprocessor preprocessor;

tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter *error_reporter = &micro_error_reporter;

const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

alignas(16) uint8_t tensor_arena[TENSOR_ARENA_SIZE];

unsigned long samples_processed = 0;
bool seizure_detected = false;
unsigned long seizure_start_time = 0;
unsigned long last_publish_time = 0;
unsigned long last_heartbeat_time = 0;
unsigned long last_raw_signal_publish = 0;
float current_prediction = 0.0f;
int current_heart_rate = 0;

uint8_t bt_buffer[6];
int bt_index = 0;

unsigned long total_inferences = 0;
unsigned long total_seizures = 0;
unsigned long system_start_time = 0;

typedef struct
{
    uint8_t seq;
    uint8_t digital[4];
    uint16_t analog[6];
} BITalinoFrame;

bool parseBITalinoFrame(uint8_t *buffer, BITalinoFrame *frame)
{
    if (!(buffer[0] & 0x80))
        return false;

    frame->seq = buffer[0] & 0x0F;
    frame->digital[0] = (buffer[0] >> 7) & 0x01;
    frame->digital[1] = (buffer[0] >> 6) & 0x01;
    frame->digital[2] = (buffer[0] >> 5) & 0x01;
    frame->digital[3] = (buffer[0] >> 4) & 0x01;

    frame->analog[0] = ((buffer[1] & 0x03) << 8) | buffer[2];

    frame->analog[1] = ((buffer[3] & 0x0F) << 6) | (buffer[4] >> 2);

    return true;
}

void startBITalinoAcquisition()
{
    uint8_t start_cmd[] = {0x01, 0x07};
    SerialBT.write(start_cmd, 2);
    delay(100);
    Serial.println("✓ Acquisition BITalino démarrée (178 Hz)");
}

void stopBITalinoAcquisition()
{
    uint8_t stop_cmd[] = {0x00};
    SerialBT.write(stop_cmd, 1);
    delay(100);
    Serial.println("✓ Acquisition BITalino arrêtée");
}

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

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("📨 Message MQTT reçu [%s]: ", topic);

    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);

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

            mqttClient.subscribe(TOPIC_COMMAND);

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

void publishStatus(const char *state, const char *message)
{
    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["state"] = state;
    doc["message"] = message;
    doc["uptime"] = (millis() - system_start_time) / 1000;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_STATUS, buffer, true);
}

void publishPrediction(float prediction, bool is_seizure)
{
    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();
    doc["prediction"] = round(prediction * 1000) / 1000.0f;
    doc["confidence"] = round((prediction * 100) * 10) / 10.0f;
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
    mqttClient.publish(TOPIC_ALERT, buffer, true);
}

void publishMetrics()
{
    StaticJsonDocument<512> doc;

    doc["timestamp"] = millis();
    doc["uptime"] = (millis() - system_start_time) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();

    doc["samples_processed"] = samples_processed;
    doc["total_inferences"] = total_inferences;
    doc["total_seizures"] = total_seizures;
    doc["current_prediction"] = round(current_prediction * 1000) / 1000.0f;

    doc["seizure_detected"] = seizure_detected;
    if (seizure_detected)
    {
        doc["seizure_duration"] = (millis() - seizure_start_time) / 1000;
    }

    doc["bluetooth_connected"] = SerialBT.connected();
    doc["mqtt_connected"] = mqttClient.connected();

    char buffer[512];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_METRICS, buffer);
}

void publishRawEEG(int raw_value, float microvolts)
{

    StaticJsonDocument<128> doc;
    doc["timestamp"] = millis();
    doc["raw"] = raw_value;
    doc["microvolts"] = round(microvolts * 100) / 100.0f;

    char buffer[128];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_RAW_EEG, buffer);
}

void updateLEDs(bool seizure)
{
    if (seizure)
    {

        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_YELLOW, (millis() / 200) % 2);
    }
    else
    {

        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_YELLOW, HIGH);
    }
}

void setup()
{
    system_start_time = millis();

    Serial.begin(115200);
    delay(1000);

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

    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(RESET_BUTTON, INPUT_PULLUP);

    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);

    Serial.println("✓ Configuration matérielle terminée");

    setupWiFi();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);

    Serial.println("✓ Client MQTT configuré");

    mqttReconnect();

    Serial.println("⏳ Connexion au BITalino via Bluetooth...");
    Serial.println("   Adresse MAC: 20:17:11:20:49:95");

    if (!SerialBT.begin("ESP32_EEG_Monitor", true))
    {
        Serial.println("❌ Erreur init Bluetooth");
        publishStatus("error", "Bluetooth initialization failed");
        while (1)
            ;
    }

    Serial.println("✓ Bluetooth initialisé");
    delay(1000);

    bool connected = false;
    for (int attempt = 1; attempt <= 30 && !connected; attempt++)
    {
        Serial.printf("⏳ Tentative %d/30...\n", attempt);

        if (SerialBT.connect(BITALINO_MAC_ADDRESS))
        {
            connected = true;
            Serial.println("✓ BITalino connecté via Bluetooth!");
        }
        else
        {
            delay(1000);
        }
    }

    if (!connected)
    {
        Serial.println("❌ Timeout connexion BITalino");
        publishStatus("error", "Failed to connect to BITalino");
        while (1)
            ;
    }

    Serial.println("\n✓ BITalino connecté via Bluetooth!");

    delay(1000);
    startBITalinoAcquisition();

    preprocessor.begin();
    Serial.println("✓ Préprocesseur EEG BITalino initialisé");

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

    publishStatus("ready", "System initialized and ready for monitoring");

    Serial.println("\n🚀 SYSTÈME PRÊT - Surveillance en cours...\n");
}

void loop()
{

    if (!mqttClient.connected())
    {
        mqttReconnect();
    }
    mqttClient.loop();

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

    while (SerialBT.available())
    {
        uint8_t byte_received = SerialBT.read();

        if ((byte_received & 0x80) && bt_index == 0)
        {
            bt_buffer[bt_index++] = byte_received;
        }
        else if (bt_index > 0 && bt_index < 6)
        {
            bt_buffer[bt_index++] = byte_received;

            if (bt_index == 6)
            {
                BITalinoFrame frame;

                if (parseBITalinoFrame(bt_buffer, &frame))
                {
                    int raw_value = frame.analog[0];

                    if (millis() - last_raw_signal_publish >= RAW_SIGNAL_INTERVAL_MS)
                    {
                        publishRawEEG(raw_value, preprocessor.convertADCtoMicrovolts(raw_value));
                        last_raw_signal_publish = millis();
                    }

                    if (preprocessor.addSample(raw_value))
                    {
                        if (preprocessor.extractFeatures())
                        {
                            preprocessor.normalizeFeatures();

                            float *normalized = preprocessor.getNormalizedFeatures();
                            for (int i = 0; i < 194; i++)
                            {
                                input->data.f[i] = normalized[i];
                            }

                            if (interpreter->Invoke() == kTfLiteOk)
                            {
                                float prediction = output->data.f[0];
                                current_prediction = prediction;
                                total_inferences++;
                                samples_processed++;

                                bool is_seizure = (prediction >= SEIZURE_THRESHOLD);

                                publishPrediction(prediction, is_seizure);

                                if (is_seizure)
                                {
                                    if (!seizure_detected)
                                    {

                                        seizure_detected = true;
                                        seizure_start_time = millis();
                                        total_seizures++;

                                        publishAlert(true, 0);

                                        Serial.printf("\n⚠️⚠️⚠️ ALERTE CRISE DÉTECTÉE [%.1f%%] ⚠️⚠️⚠️\n",
                                                      prediction * 100.0f);
                                    }

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

                                        unsigned long duration = millis() - seizure_start_time;
                                        seizure_detected = false;

                                        publishAlert(false, duration);

                                        Serial.printf("\n✓ Fin de crise - Durée totale: %lu s\n\n",
                                                      duration / 1000);
                                    }

                                    if (samples_processed % 20 == 0)
                                    {
                                        Serial.printf("✓ Normal [%.1f%%] - Inférences: %lu\n",
                                                      (1.0f - prediction) * 100.0f, total_inferences);
                                    }
                                }

                                updateLEDs(seizure_detected);
                            }
                        }
                    }
                }

                bt_index = 0;
            }
        }
        else
        {
            bt_index = 0;
        }
    }

    unsigned long now = millis();
    if (now - last_publish_time >= PUBLISH_INTERVAL_MS)
    {
        publishMetrics();
        last_publish_time = now;
    }

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
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BluetoothSerial.h>
#include <DHT.h>

#define ENABLE_DATABASE
#define ENABLE_LEGACY_TOKEN
#include <FirebaseClient.h>

#define DATABASE_URL "https://SEU-PROJETO-default-rtdb.firebaseio.com/"
#define DB_SECRET    "SEU_DATABASE_SECRET"
#define CAMINHO_TEMP "/prova/temperatura"

#define DHT_PIN  4
#define DHT_TYPE DHT11

#define LED_VERMELHO 25
#define LED_AMARELO  26
#define LED_VERDE    27

const float TEMP_FRIO_MAX   = 25.0;
const float TEMP_QUENTE_MIN = 30.0;

const unsigned long INTERVALO_MS = 3000;

BluetoothSerial SerialBT;
DHT dht(DHT_PIN, DHT_TYPE);

WiFiClientSecure ssl;
AsyncClientClass client(ssl);
FirebaseApp app;
RealtimeDatabase Database;
LegacyToken dbSecret(DB_SECRET);

String WIFI_SSID;
String WIFI_PASSWORD;

unsigned long ultimaLeitura = 0;

String lerLinhaBluetooth()
{
  String linha = "";
  while (linha.length() == 0)
  {
    if (SerialBT.available())
    {
      linha = SerialBT.readStringUntil('\n');
      linha.trim();
    }
    delay(20);
  }
  return linha;
}

void receberCredenciaisBluetooth()
{
  SerialBT.begin("ESP32_ANA_LUIZA");
  Serial.println("Bluetooth pronto para parear");

  while (!SerialBT.hasClient())
  {
    delay(200);
  }
  Serial.println("Celular conectado via Bluetooth");

  SerialBT.println("Envie o SSID da rede:");
  WIFI_SSID = lerLinhaBluetooth();
  Serial.println("SSID recebido: " + WIFI_SSID);

  SerialBT.println("Envie a SENHA da rede:");
  WIFI_PASSWORD = lerLinhaBluetooth();
  SerialBT.println("Credenciais recebidas. Encerrando Bluetooth.");
  delay(500);

  SerialBT.end();
  Serial.println("Bluetooth encerrado");
}

void conectarWiFi()
{
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);

    if (millis() - inicio > 20000)
    {
      Serial.println("\nFalha ao conectar. Tentando novamente...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
      inicio = millis();
    }
  }

  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

bool wifiOk()
{
  if (WiFi.status() == WL_CONNECTED)
    return true;

  Serial.println("Wi-Fi caiu. Reconectando...");
  WiFi.reconnect();
  return false;
}

void apagarLeds()
{
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERDE, LOW);
}

void atualizarLeds(float temp)
{
  apagarLeds();

  if (temp >= TEMP_QUENTE_MIN)
  {
    Serial.println("Faixa: QUENTE -> vermelho");
    digitalWrite(LED_VERMELHO, HIGH);
  }
  else if (temp >= TEMP_FRIO_MAX)
  {
    Serial.println("Faixa: NORMAL -> amarelo");
    digitalWrite(LED_AMARELO, HIGH);
  }
  else
  {
    Serial.println("Faixa: FRIO -> verde");
    digitalWrite(LED_VERDE, HIGH);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  apagarLeds();

  dht.begin();

  receberCredenciaisBluetooth();

  conectarWiFi();

  ssl.setInsecure();
  initializeApp(client, app, getAuth(dbSecret));
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  Serial.println("Firebase inicializado");
}

void loop()
{
  app.loop();

  if (!wifiOk() || !app.ready())
  {
    delay(500);
    return;
  }

  if (millis() - ultimaLeitura < INTERVALO_MS)
    return;
  ultimaLeitura = millis();

  float temperatura = dht.readTemperature();

  if (isnan(temperatura))
  {
    Serial.println("Falha na leitura do DHT11, ignorando");
    return;
  }

  Serial.println("--------------------------------");
  Serial.print("Temperatura lida: ");
  Serial.print(temperatura);
  Serial.println(" C");

  bool gravou = Database.set<float>(client, CAMINHO_TEMP, temperatura);
  if (gravou)
  {
    Serial.println("Gravado no Firebase");
  }
  else
  {
    Serial.print("Erro ao gravar: ");
    Serial.println(client.lastError().message());
    return;
  }

  float tempFirebase = Database.get<float>(client, CAMINHO_TEMP);

  if (client.lastError().code() != 0)
  {
    Serial.print("Erro ao ler do Firebase: ");
    Serial.println(client.lastError().message());
    return;
  }

  Serial.print("Temperatura lida do Firebase: ");
  Serial.print(tempFirebase);
  Serial.println(" C");

  atualizarLeds(tempFirebase);
}
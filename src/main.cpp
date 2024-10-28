#include <Arduino.h>
#include <credentials.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define ledPin 5
#define alarm 9

#define DHTPIN 2
#define DHTTYPE DHT11

int ledStatus = 0;
float max_temperature = 30.0; // Valor por defecto para la temperatura máxima

const unsigned long BOT_MTBS = 1000;

unsigned long bot_lasttime;

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
DHT_Unified dht(DHTPIN, DHTTYPE);

String chat_id_alert = "";

float actual_temperature;
float actual_humidity;

void bot_setup()
{
  const String commands = F("[" 
                            "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
                            "{\"command\":\"start\", \"description\":\"Message sent when you open a chat with a bot\"},"
                            "{\"command\":\"led_on\",\"description\":\"Put the LED in ON\"},"
                            "{\"command\":\"led_off\",\"description\":\"Put the LED in OFF\"},"
                            "{\"command\":\"status_led\",\"description\":\"Get the status of the LED\"},"
                            "{\"command\":\"temperature\",\"description\":\"Get temperature and humidity actual\"},"
                            "{\"command\":\"set_max_temperature\",\"description\":\"Set limit temperature for alarm\"}"
                            "]");
  bot.setMyCommands(commands);
}

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/led_on")
    {
      digitalWrite(ledPin, HIGH);
      ledStatus = 1;
      bot.sendMessage(chat_id, "Led is ON", "");
    }

    if (text == "/led_off")
    {
      ledStatus = 0;
      digitalWrite(ledPin, LOW);
      bot.sendMessage(chat_id, "Led is OFF", "");
    }

    if (text == "/status_led")
    {
      if (ledStatus)
      {
        bot.sendMessage(chat_id, "Led is ON", "");
      }
      else
      {
        bot.sendMessage(chat_id, "Led is OFF", "");
      }
    }

    if (text == "/temperature")
    {
      String tempMessage = "Temperatura actual: " + String(actual_temperature) + "°C\n";
      tempMessage += "Humedad actual: " + String(actual_humidity) + "%";
      bot.sendMessage(chat_id, tempMessage, "");
    }

    if (text.startsWith("/set_max_temperature"))
    {
      int spaceIndex = text.indexOf(' ');
      if (spaceIndex != -1)
      {
        String valueStr = text.substring(spaceIndex + 1);
        float newMaxTemp = valueStr.toFloat();
        if (newMaxTemp > 0) // Aseguramos que el valor ingresado sea positivo
        {
          max_temperature = newMaxTemp;
          bot.sendMessage(chat_id, "Temperatura máxima establecida a: " + String(max_temperature) + "°C", "");
        }
        else
        {
          bot.sendMessage(chat_id, "Por favor, ingresa un valor válido para la temperatura máxima.", "");
        }
      }
      else
      {
        bot.sendMessage(chat_id, "Uso: /set_max_temperature <valor>", "");
      }
    }

    if (text == "/start")
    {
      String welcome = "Bienvenido, " + from_name + " !!!\n";
      welcome += "Este es el sistema de monitoreo de temperatura y humedad.\n\n";
      welcome += "/help : Consulta todos los comandos que puedes utilizar.\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }

    if (text == "/help")
    {
      String message = "Comandos disponibles: \n";
      message += "Este es el sistema de monitoreo de temperatura y humedad.\n\n";
      message += "/led_on : Encender el LED.\n";
      message += "/led_off : Apagar el LED.\n";
      message += "/status_led : Estado actual del LED.\n";
      message += "/temperature : Obtener la temperatura y humedad actual.\n";
      message += "/set_max_temperature : Actualizar el valor límite de la temperatura máxima.\n";
      bot.sendMessage(chat_id, message, "Markdown");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(SSID);
  WiFi.begin(SSID, PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  dht.begin();
  bot_setup();
}

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    Serial.println(F("Error reading temperature!"));
  }
  else
  {
    actual_temperature = event.temperature;
    Serial.print(F("Temperature: "));
    Serial.print(actual_temperature);
    Serial.println(F("°C"));
    if (actual_temperature > max_temperature)
    {
      digitalWrite(alarm, HIGH);
      Serial.println(F("Alarma activada: temperatura superior al límite establecido."));
    }
    else
    {
      digitalWrite(alarm, LOW);
    }
  }

  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    Serial.println(F("Error reading humidity!"));
  }
  else
  {
    actual_humidity = event.relative_humidity;
    Serial.print(F("Humidity: "));
    Serial.print(actual_humidity);
    Serial.println(F("%"));
  }
}

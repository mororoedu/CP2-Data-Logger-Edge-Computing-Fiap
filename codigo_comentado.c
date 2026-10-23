#include <LiquidCrystal_I2C.h>  // Biblioteca para LCD I2C
#include <RTClib.h> // Biblioteca para Relógio em Tempo Real
#include <Wire.h>   // Biblioteca para comunicação I2C
#include <EEPROM.h> // Biblioteca para gravar na memória que não apaga ao desligar
#include "DHT.h"    // Biblioteca do sensor de temperatura e umidade

#define LOG_OPTION 1     // Opção para ativar a leitura do log
#define SERIAL_OPTION 0  // Opção de comunicação serial: 0 para desligado, 1 para ligado
#define UTC_OFFSET -3    // Ajuste de fuso horário para UTC-3

// Configurações do DHT22 (lembre que no laboratório tempos o DHT-11)
#define DHTPIN 2
#define DHTTYPE DHT22 //Mudar para DHT-11 no laboratório
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2); // Endereço de acesso: 0x3F ou 0x27
RTC_DS1307 RTC;

/* ===================== OTIMIZAÇÃO DA EEPROM =====================
   A EEPROM do Arduino Uno tem só 1024 bytes.

   ANTES: cada registro usava 8 bytes -> só 100 registros (800 bytes)
     - 4 bytes: data/hora em segundos desde 1970
     - 2 bytes: temperatura x 100
     - 2 bytes: umidade x 100

   AGORA: cada registro usa 5 bytes -> 204 registros (1020 bytes)
     - 3 bytes: data/hora em MINUTOS desde 01/01/2024
                (o log só grava 1 vez por minuto, então os segundos
                 não servem pra nada; 3 bytes dão para uns 31 anos)
     - 1 byte:  temperatura -> (temperatura + 40) x 2
                (-40 °C vira 0 e 80 °C vira 240, cabe em 1 byte,
                 com passo de 0,5 °C, que é a precisão do DHT22)
     - 1 byte:  umidade -> umidade x 2
                (0 % vira 0 e 100 % vira 200, passo de 0,5 %)

   Resultado: o DOBRO de registros na mesma memória.
   ================================================================ */
const int recordSize = 5;                 // Tamanho de cada registro em bytes (antes era 8)
const int maxRecords = 1024 / recordSize; // 1024 / 5 = 204 registros (antes eram 100)
int startAddress = 0;
int endAddress = maxRecords * recordSize; // 204 x 5 = 1020 bytes usados
int currentAddress = 0;

// 01/01/2024 00:00:00 em segundos desde 1970 (é o nosso "ponto zero")
const unsigned long INICIO_2024 = 1704067200;

int lastLoggedMinute = -1;

// Triggers de temperatura e umidade
float trigger_t_min = 20.0; // Exemplo: valor mínimo de temperatura
float trigger_t_max = 30.0; // Exemplo: valor máximo de temperatura
float trigger_u_min = 30.0; // Exemplo: valor mínimo de umidade
float trigger_u_max = 60.0; // Exemplo: valor máximo de umidade

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    dht.begin();
    Serial.begin(9600);
    lcd.init();   // Inicialização do LCD
    lcd.backlight();  // Ligando o backlight do LCD
    RTC.begin();    // Inicialização do Relógio em Tempo Real
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // RTC.adjust(DateTime(2024, 5, 6, 08, 15, 00));  // Ajustar a data e hora apropriadas uma vez inicialmente, depois comentar
    EEPROM.begin();
}

void loop() {
    DateTime now = RTC.now();

    // Calculando o deslocamento do fuso horário
    int offsetSeconds = UTC_OFFSET * 3600; // Convertendo horas para segundos
    now = now.unixtime() + offsetSeconds; // Adicionando o deslocamento ao tempo atual

    // Convertendo o novo tempo para DateTime
    DateTime adjustedTime = DateTime(now);

    if (LOG_OPTION) get_log();

    // Verifica se o minuto atual é diferente do minuto do último registro
    if (adjustedTime.minute() != lastLoggedMinute) {
        lastLoggedMinute = adjustedTime.minute();

        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(1000);                       // wait for a second
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
        delay(1000);                       // wait for a second

        // Ler os valores de temperatura e umidade
        float humidity = dht.readHumidity();
        float temperature = dht.readTemperature();

        // Verificar se os valores estão fora dos triggers
        if (temperature < trigger_t_min || temperature > trigger_t_max || humidity < trigger_u_min || humidity > trigger_u_max) {

            // ---- OTIMIZADO: data/hora em minutos desde 2024 (cabe em 3 bytes) ----
            unsigned long minutos = (now.unixtime() - INICIO_2024) / 60;

            // ---- OTIMIZADO: temperatura e umidade em 1 byte cada ----
            byte tempByte = round((temperature + 40) * 2); // ex: 23.5 °C -> (23.5 + 40) x 2 = 127
            byte humiByte = round(humidity * 2);           // ex: 45.0 %  -> 45 x 2 = 90

            // Escrever os 5 bytes na EEPROM
            // Um byte só guarda de 0 a 255, então os minutos são divididos em 3 pedaços:
            EEPROM.update(currentAddress,     minutos / 65536);        // pedaço de cima   (65536 = 256 x 256)
            EEPROM.update(currentAddress + 1, (minutos / 256) % 256);  // pedaço do meio
            EEPROM.update(currentAddress + 2, minutos % 256);          // pedaço de baixo
            EEPROM.update(currentAddress + 3, tempByte);               // temperatura
            EEPROM.update(currentAddress + 4, humiByte);               // umidade
            // update() só grava se o valor for diferente do que já está lá,
            // isso poupa a EEPROM (cada byte aguenta ~100.000 gravações)

            // Atualiza o endereço para o próximo registro
            getNextAddress();
        }
    }

    if (SERIAL_OPTION) {
        Serial.print(adjustedTime.day());
        Serial.print("/");
        Serial.print(adjustedTime.month());
        Serial.print("/");
        Serial.print(adjustedTime.year());
        Serial.print(" ");
        Serial.print(adjustedTime.hour() < 10 ? "0" : ""); // Adiciona zero à esquerda se hora for menor que 10
        Serial.print(adjustedTime.hour());
        Serial.print(":");
        Serial.print(adjustedTime.minute() < 10 ? "0" : ""); // Adiciona zero à esquerda se minuto for menor que 10
        Serial.print(adjustedTime.minute());
        Serial.print(":");
        Serial.print(adjustedTime.second() < 10 ? "0" : ""); // Adiciona zero à esquerda se segundo for menor que 10
        Serial.print(adjustedTime.second());
        Serial.print("\n");
    }

    lcd.setCursor(0, 0);
    lcd.print("DATA: ");
    lcd.print(adjustedTime.day() < 10 ? "0" : ""); // Adiciona zero à esquerda se dia for menor que 10
    lcd.print(adjustedTime.day());
    lcd.print("/");
    lcd.print(adjustedTime.month() < 10 ? "0" : ""); // Adiciona zero à esquerda se mês for menor que 10
    lcd.print(adjustedTime.month());
    lcd.print("/");
    lcd.print(adjustedTime.year());
    lcd.setCursor(0, 1);
    lcd.print("HORA: ");
    lcd.print(adjustedTime.hour() < 10 ? "0" : ""); // Adiciona zero à esquerda se hora for menor que 10
    lcd.print(adjustedTime.hour());
    lcd.print(":");
    lcd.print(adjustedTime.minute() < 10 ? "0" : ""); // Adiciona zero à esquerda se minuto for menor que 10
    lcd.print(adjustedTime.minute());
    lcd.print(":");
    lcd.print(adjustedTime.second() < 10 ? "0" : ""); // Adiciona zero à esquerda se segundo for menor que 10
    lcd.print(adjustedTime.second());

    delay(1000);
}

void getNextAddress() {
    currentAddress += recordSize; // Pula 5 bytes (antes pulava 8)
    if (currentAddress >= endAddress) {
        currentAddress = 0; // Volta para o começo se atingir o limite
    }
}

void get_log() {
    Serial.println("Data stored in EEPROM:");
    Serial.println("Timestamp\t\tTemperature\tHumidity");

    for (int address = startAddress; address < endAddress; address += recordSize) {
        // Ler os 5 bytes da EEPROM e juntar os 3 pedaços dos minutos de volta
        unsigned long minutos = EEPROM.read(address) * 65536UL
                              + EEPROM.read(address + 1) * 256UL
                              + EEPROM.read(address + 2);
        byte tempByte = EEPROM.read(address + 3);
        byte humiByte = EEPROM.read(address + 4);

        // Desfazer as contas da gravação
        float temperature = tempByte / 2.0 - 40; // ex: 127 / 2 - 40 = 23.5 °C
        float humidity = humiByte / 2.0;         // ex: 90 / 2 = 45.0 %

        // Verificar se os dados são válidos antes de imprimir
        // EEPROM vazia vem com todos os bytes em 255, então 3 bytes vazios = 16777215
        if (minutos != 16777215) {
            DateTime dt = DateTime(INICIO_2024 + minutos * 60); // minutos -> segundos desde 1970
            Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
            Serial.print("\t");
            Serial.print(temperature);
            Serial.print(" C\t\t");
            Serial.print(humidity);
            Serial.println(" %");
        }
    }
}

# Data Logger com EEPROM Otimizada - Edge Computing FIAP

Projeto de um **Data Logger** (registrador de dados) de temperatura e umidade com Arduino, desenvolvido como evolução do projeto de Edge Computing da **FIAP**.

* **Simulação no Wokwi:** https://wokwi.com/projects/478696391250198529
* **Video no youtub explicando o projeto:** https://youtube.com/shorts/jYuTaj59wWM?is=irxWn7SbHWnUutcb

## Descrição do projeto

O projeto usa o ATmega 328P (Arduino Uno) para medir a temperatura e a umidade do ambiente e mostrar a data e a hora em um LCD. A data e a hora vêm de um relógio de tempo real (RTC), que continua contando mesmo com o Arduino desligado.

Sempre que a temperatura ou a umidade saem dos limites definidos (triggers), o Arduino grava um registro na **EEPROM**, uma memória que **não apaga quando o aparelho é desligado**. Assim fica um histórico dos momentos em que o ambiente ficou fora do ideal.

## Materiais utilizados

Para a criação deste aparelho, utilizamos os seguintes materiais:

* 1 Arduino Uno
* 1 Sensor de temperatura e umidade DHT22 (DHT11 no laboratório)
* 1 Relógio de tempo real RTC DS1307
* 1 LCD 16x2 I2C
* 1 Mini protoboard
* Fios jumper

## Tecnologias utilizadas

Para a criação deste aparelho, utilizamos as seguintes tecnologias:

* C++
* Wokwi
* Arduino IDE

## Como funciona

1. A cada segundo o Arduino lê a hora do RTC, ajusta para o fuso de Brasília (UTC-3) e mostra a data e a hora no LCD.
2. A cada minuto ele lê a temperatura e a umidade do DHT22.
3. Se a temperatura estiver fora de **20 °C a 30 °C** ou a umidade fora de **30 % a 60 %**, ele grava um registro na EEPROM.
4. Os registros gravados são mostrados no Monitor Serial.
5. Quando a EEPROM enche, o próximo registro volta para o começo e substitui o mais antigo (buffer circular).

O LCD e o RTC usam a comunicação **I2C** e dividem os mesmos dois fios (SDA no pino A4 e SCL no pino A5). O DHT22 fica no pino 2.

## Otimização da EEPROM

A EEPROM do Arduino Uno tem só **1024 bytes**. No código base, cada registro ocupava 8 bytes, então cabiam só 100 registros. Nós diminuímos cada registro para **5 bytes**:

| Dado | Antes | Depois | Como |
|---|---|---|---|
| Data e hora | 4 bytes | **3 bytes** | Guardamos os **minutos desde 01/01/2024** em vez dos segundos desde 1970. Como o log só grava uma vez por minuto, os segundos não eram necessários |
| Temperatura | 2 bytes | **1 byte** | `(temperatura + 40) x 2`: de -40 °C a 80 °C vira de 0 a 240 |
| Umidade | 2 bytes | **1 byte** | `umidade x 2`: de 0 % a 100 % vira de 0 a 200 |
| **Total por registro** | **8 bytes** | **5 bytes** | |

### Resultado

| | Antes | Depois |
|---|---|---|
| Bytes por registro | 8 | **5** |
| Espaço para guardar 100 registros | 800 bytes | **500 bytes** (300 bytes a menos) |
| Registros que cabem na EEPROM (1024 bytes) | 100 | **204** |

Cada registro gasta **37,5 % menos memória**. Como a EEPROM do Arduino tem sempre 1024 bytes e serve justamente para guardar o histórico, usamos essa economia para guardar **o dobro de registros** no mesmo espaço.

### Por que não perdemos informação importante

* Um byte só guarda números inteiros de 0 a 255. Somar 40 tira os números negativos e multiplicar por 2 transforma o 0,5 em número inteiro. Exemplo: 23,5 °C → (23,5 + 40) x 2 = **127**. Para ler de volta: 127 / 2 - 40 = **23,5 °C**.
* Os valores ficam com passo de 0,5. O DHT22 tem precisão de ±0,5 °C e ±2 % de umidade, então as casas decimais a mais do código antigo não eram confiáveis. O DHT11 mede de 1 em 1, então para ele não se perde nada.
* Para guardar os minutos em 3 bytes, o número é dividido em 3 pedaços com divisão e resto (`/ 65536`, `/ 256 % 256` e `% 256`), como separar um número em centena, dezena e unidade, só que em base 256.
* Usamos `EEPROM.update()`, que só grava quando o valor muda. Isso aumenta a vida útil da EEPROM (cada byte aguenta cerca de 100.000 gravações).

## Como usar

1. Abra o link da simulação no Wokwi e clique no botão verde (▶).
2. O LCD vai mostrar a data e a hora.
3. Clique no sensor DHT22 e mude a temperatura para um valor fora da faixa, por exemplo **35 °C**.
4. Na virada do minuto o registro é gravado e aparece no Monitor Serial com a data, a hora, a temperatura e a umidade.

No laboratório, troque `#define DHTTYPE DHT22` por `#define DHTTYPE DHT11` no código.

## Autores

Leandro Rodrigues Barbosa

Eduardo Miranda Mororo

Victor Krause Esteves

Henrique Nascimento

# Estacionamento Inteligente (ESP32)

Maquete de estacionamento com 3 vagas, desenvolvida na disciplina de Sistemas Ciberfísicos (PUCPR, 2026). Um ESP32 monitora as vagas com sensores infravermelhos, sinaliza o tráfego atrás delas, controla a cancela por cartão RFID e publica tudo em um painel web servido pela própria placa.

## Funcionalidades

- **Ocupação das vagas:** cada vaga tem um sensor IR. Carro parado por 1 s na frente do sensor deixa o LED da vaga vermelho; sem carro, verde.
- **Tráfego cruzado:** três sensores IR, um acima de cada vaga, apontam para o caminho atrás das vagas. Quando um sensor vê um carro, o LED azul daquela vaga acende e continua aceso por 3 s depois que o carro sai. Com o carro passando, os LEDs acendem em cascata.
- **Cancela por RFID:** cartão cadastrado com vaga livre abre a cancela por 4 s. O mesmo cartão de novo conta como saída e sempre abre. Sem vaga, o LCD mostra "LOTADO". Cartão desconhecido recebe "Acesso negado".
- **Cancela pela rede:** botões no painel web ou chamada direta à rota `/cancela`. Aberta pela rede, também fecha sozinha depois de 4 s.
- **LCD 16x2:** mostra vagas livres, estado da cancela e a situação de cada vaga (`Livres: 2/3 FECH` / `V1:L V2:X V3:L`), além de mensagens temporárias.
- **Painel web:** atualiza a cada 1 s com vagas, cancela, tráfego cruzado, último acesso, dados do Wi-Fi (IP, MAC, sinal) e os últimos 25 eventos.
- **Log:** eventos com nível (`ERRO`, `AVISO`, `INFO`), data e hora, gravados na flash com LittleFS.

O `loop()` não usa `delay()`: sensores, cancela, LCD e site rodam em paralelo.

## Hardware

| Componente | Quantidade |
|---|---|
| ESP32 de 30 pinos (NodeMCU-32S) | 1 |
| Sensor IR de obstáculo (FC-51 / LM393) | 6 |
| LED RGB de 4 pernas, cátodo comum | 6 |
| Servo SG90 | 1 |
| Leitor RFID MFRC522 com cartões ou chaveiros | 1 |
| LCD 16x2 com módulo I2C | 1 |
| Resistor de 220 Ω (perna vermelha) | 3 |
| Resistor de 100 Ω (pernas verde e azul) | 6 |
| Jumpers macho-fêmea | 29 |
| Jumpers macho-macho | 26 |

Cada vaga tem **dois LEDs RGB**: um indica a ocupação (pernas vermelha e verde) e o outro indica tráfego cruzado (só a perna azul). As pernas não usadas ficam sem ligação.

O chip USB da placa é o CH9102X, que usa o driver **CH343SER** da WCH (não o do CH340).

## Pinagem

| Função | Vaga 1 | Vaga 2 | Vaga 3 |
|---|---|---|---|
| Sensor IR da vaga | GPIO 34 | GPIO 35 | GPIO 36 (VP) |
| Sensor IR de tráfego cruzado | GPIO 39 (VN) | GPIO 16 | GPIO 17 |
| LED de ocupação, perna verde | GPIO 25 | GPIO 27 | GPIO 32 |
| LED de ocupação, perna vermelha | GPIO 2 | GPIO 14 | GPIO 15 |
| LED de tráfego, perna azul | GPIO 26 | GPIO 33 | GPIO 12 |

| Peça | Ligação |
|---|---|
| Servo SG90 | sinal no GPIO 13, VCC no VIN (5 V) |
| RFID MFRC522 | SDA/SS 21, SCK 18, MISO 22, MOSI 4, RST e 3.3V no 3V3 |
| LCD 16x2 I2C | SDA 5, SCL 19, VCC no VIN (5 V) |
| Sensores IR | VCC no 3V3 |

Observações:

- O RST do RFID vai direto no 3V3; a biblioteca reinicia o leitor por SPI (`MFRC522::UNUSED_PIN`).
- Não ligue sensor IR no GPIO 12: em nível alto no boot ele impede a placa de iniciar. Como saída de LED de cátodo comum, funciona.
- Para LEDs de ânodo comum (perna longa no 3V3), mude `RGB_ANODO_COMUM` para `true`. Nesse caso o GPIO 12 não pode ser usado para LED.
- GPIO 1 e 3 (TX0/RX0) ficam livres para o USB.

## Software

### Ambiente

- Arduino IDE com o core **esp32 by Espressif 3.3.x**
- Placa: **NodeMCU-32S**

### Bibliotecas

| Biblioteca | Autor |
|---|---|
| MFRC522 | GithubCommunity |
| ESP32Servo | Kevin Harrington |
| LiquidCrystal I2C | Frank de Brabander |

`WiFi`, `WebServer`, `ESPmDNS`, `LittleFS`, `SPI` e `Wire` já vêm com o core do ESP32.

## Configuração

Tudo o que a equipe ajusta fica no início do arquivo `Estacionamento_Inteligente.ino`:

| Constante | O que define |
|---|---|
| `AP_NOME`, `AP_SENHA` | Rede Wi-Fi criada pelo ESP32 (sempre ligada) |
| `WIFI_NOME`, `WIFI_SENHA` | Wi-Fi externo, só 2,4 GHz; fornece data e hora por NTP. Deixe `WIFI_NOME` vazio para não usar |
| `LCD_ENDERECO` | Endereço I2C do LCD, normalmente `0x27` ou `0x3F` |
| `ANG_FECHADA`, `ANG_ABERTA` | Ângulos do servo da cancela |
| `cartoes[]` | UID e nome de cada cartão cadastrado |
| `IR_ATIVO_EM_LOW` | `true` se o sensor IR dá LOW ao detectar objeto |
| `RGB_ANODO_COMUM` | `false` para LED de cátodo comum |
| `T_...` | Tempos em ms: confirmação da vaga, filtro do tráfego, azul após a saída, cancela aberta, mensagem no LCD, repetição de cartão |

### Cadastro dos cartões

Os UIDs que vêm no código são de exemplo. Para descobrir os reais:

1. Grave o código e encoste o cartão no leitor.
2. O sistema responde "Acesso negado" e mostra o UID no LCD, no log do site e no Monitor Serial (115200 baud).
3. Copie o UID para `cartoes[]`, em maiúsculas e com espaço entre os pares, por exemplo `"3B 7A 91 0C"`.

## Como usar

1. Instale o driver CH343SER e conecte o ESP32 pelo USB.
2. Instale o core esp32 e as três bibliotecas na Arduino IDE.
3. Abra `Estacionamento_Inteligente.ino`, selecione a placa NodeMCU-32S e a porta, e grave.
4. Conecte o celular ou o computador à rede Wi-Fi definida em `AP_NOME`.
5. Abra **http://192.168.4.1** no navegador.

Se o ESP32 também estiver conectado a um Wi-Fi externo, o painel fica acessível pelo IP mostrado no LCD ou em `http://estacionamento.local`.

Ao ligar, os LEDs fazem um teste rápido (verdes e azuis, depois vermelhos) e o log registra se o leitor RFID e o LCD responderam.

## Rotas HTTP

| Rota | Descrição |
|---|---|
| `/` | Painel web |
| `/status` | Estado completo em JSON (vagas, cancela, tráfego, Wi-Fi, log) |
| `/cancela?estado=aberta` | Abre a cancela |
| `/cancela?estado=fechada` | Fecha a cancela |
| `/log` | Log completo gravado na flash |
| `/log?antigo=1` | Arquivo de log anterior |

O log na flash tem limite de 64 KB: ao passar disso, o arquivo atual vira o antigo e um novo começa.

## Limitações conhecidas

- A contagem de vagas livres usa só os sensores: um carro que passou o cartão e ainda não estacionou não ocupa vaga.
- A rota `/cancela` não tem autenticação; qualquer dispositivo na rede pode abrir a cancela.
- O registro de quem está dentro do estacionamento fica na memória e se perde ao reiniciar a placa.

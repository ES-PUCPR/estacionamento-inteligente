# 🚗 Estacionamento Inteligente

Maquete interativa de um estacionamento inteligente, controlada por um único microcontrolador **ESP32**, com monitoramento individual das vagas, alerta de tráfego cruzado no corredor, controle de acesso por **RFID** com cancela automatizada e um **painel web em tempo real** hospedado na própria placa.

![ESP32](https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![IoT](https://img.shields.io/badge/IoT-Internet%20das%20Coisas-23C48E?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Em%20desenvolvimento-yellow?style=for-the-badge)

> Projeto acadêmico de **Engenharia de Software / Conectividade de Sistemas Ciberfísicos** · Docente orientador: **Prof. Fábio Bettio** · Período letivo: **2026/02**

---

## 📚 Sumário

- [📌 Visão Geral](#-visão-geral)
- [❗ Problema](#-problema)
- [⚡ Funcionalidades](#-funcionalidades)
- [🏗️ Arquitetura](#️-arquitetura)
- [⚙️ Princípio de Funcionamento](#️-princípio-de-funcionamento)
- [🛠️ Componentes de Hardware](#️-componentes-de-hardware)
- [💰 Custos](#-custos)
- [🎯 Escopo](#-escopo)
- [🔎 Estado da Arte](#-estado-da-arte)
- [⚠️ Riscos e Desafios Técnicos](#️-riscos-e-desafios-técnicos)
- [📅 Cronograma](#-cronograma)
- [👥 Equipe](#-equipe)

---

## 📌 Visão Geral

A solução consiste na construção de uma maquete interativa de um estacionamento inteligente, cujo gerenciamento é centralizado em um microcontrolador **ESP32**. O sistema atua na dificuldade de localização de vagas e na sinalização do fluxo de veículos, fornecendo orientações visuais no local físico e exibindo a disponibilidade de forma remota.

O diferencial do projeto é rodar a **lógica de controle** e o **servidor web** simultaneamente na mesma placa, exibindo a disponibilidade das vagas em tempo real, sem depender de plataformas externas em nuvem.

---

## ❗ Problema

A dificuldade diária na busca por vagas em estacionamentos gera perda de tempo, estresse para os condutores e ineficiência logística para os administradores desses espaços. Sem um sistema de sinalização claro, os motoristas precisam circular aleatoriamente para encontrar um local livre e ficam expostos a riscos de pequenas colisões devido à falta de visibilidade do tráfego cruzado nos corredores durante as manobras de saída.

---

## ⚡ Funcionalidades

- 🅿️ **Monitoramento individual das vagas:** sensores infravermelhos em cada uma das 3 vagas detectam a presença de veículos. LEDs **verdes** e **vermelhos** indicam se o espaço está livre ou ocupado.
- 🚦 **Gestão de tráfego cruzado:** sensores adicionais monitoram a via principal. Ao identificar veículos passando atrás das vagas, o sistema aciona um alerta visual com LEDs **azuis**, sinalizando que a manobra de saída está temporariamente bloqueada.
- 🔐 **Controle de acesso e cancela:** leitor RFID (MFRC522). Ao aproximar um cartão ou chaveiro cadastrado, o ESP32 verifica se há vaga livre e, em caso positivo, aciona a cancela automatizada por um servomotor SG90. Se não houver vaga, a cancela permanece fechada.
- 🌐 **Interface web em tempo real:** o ESP32 atua como servidor web independente, mostrando quais das 3 vagas estão livres ou ocupadas, o estado da cancela e o último acesso registrado.
- 📝 **Log de eventos com horário:** cada evento (entrada, ocupação de vaga, saída) é registrado, formando um log de auditoria simples.

### Exemplo de log de eventos

```
14:03:21 — Cartão A52F3C entrou
14:03:22 — Vaga 03 ocupada
14:03:23 — Catraca aberta
15:17:48 — Cartão A52F3C saiu
15:17:49 — Vaga 03 liberada
15:17:50 — Catraca aberta
```

### Exemplo do painel web (ilustrativo)

| Vaga    | Status     |
| ------- | ---------- |
| Vaga 01 | 🟢 LIVRE   |
| Vaga 02 | 🔴 OCUPADA |
| Vaga 03 | 🟢 LIVRE   |

*Vagas disponíveis: 2 · Catraca: FECHADA · Último acesso: Cartão A52F3C às 14:03:21*

---

## 🏗️ Arquitetura

O ESP32 é o núcleo do sistema e concentra três funções:

1. Leitura dos sensores infravermelhos de cada vaga e do sensor de fluxo do corredor, para atualizar a sinalização por LEDs;
2. Controle de acesso: a leitura de um cartão no módulo RFID dispara a verificação de disponibilidade de vaga e, havendo vaga livre, aciona o servomotor que abre a cancela;
3. Hospedagem de um servidor web embarcado que publica um painel HTML com o status das vagas, o estado da cancela e o último acesso registrado.

### Diagrama de blocos (fluxo de dados e controle)

```
Cartão/Chaveiro RFID --> Leitor RFID (MFRC522) --> ESP32

ESP32 --> Sensores IR nas vagas (TCRT5000)
ESP32 --> Sensor de fluxo no corredor (FC-51)
ESP32 --> Servomotor SG90 (cancela)
ESP32 --> LEDs verde / vermelho / azul
ESP32 --> Servidor Web embarcado --> Painel HTML (status em tempo real)
```

### Lógica de controle de acesso via RFID

```
RFID detectado -> Existe vaga livre?
   SIM -> Abre a cancela
   NÃO -> Mantém a cancela fechada
```

---

## ⚙️ Princípio de Funcionamento

1. **Leitura do RFID:** o usuário aproxima o cartão ou chaveiro cadastrado do leitor MFRC522.
2. **Verificação de vaga:** o ESP32 consulta o estado das 3 vagas, lido pelos sensores infravermelhos.
3. **Controle da cancela:** havendo vaga livre, o servomotor SG90 abre a cancela; caso contrário, ela permanece fechada.
4. **Sinalização das vagas:** o estado de cada vaga é refletido nos LEDs verde (livre) e vermelho (ocupada).
5. **Sinalização do corredor:** o sensor de fluxo detecta veículos passando no corredor e aciona os LEDs azuis, indicando saída bloqueada.
6. **Registro de eventos:** entrada, ocupação e saída são registradas em log com horário.
7. **Painel web:** o servidor embarcado no ESP32 publica o status atualizado das vagas, da cancela e do último acesso.

---

## 🛠️ Componentes de Hardware

| Componente | Função no projeto |
| --- | --- |
| ESP32 (módulo Wi-Fi/Bluetooth, 30 pinos) | Microcontrolador central: lógica de controle, rede e servidor web |
| Sensores infravermelhos TCRT5000 | Confirmar a ocupação de cada vaga (curto alcance) |
| Módulos FC-51 (sensibilidade ajustável) | Rastrear o fluxo de veículos no corredor de tráfego |
| Leitor RFID MFRC522 (13,56 MHz) | Controle de acesso por cartão ou chaveiro |
| Micro servo motor SG90 | Acionamento da cancela |
| LEDs (verde, vermelho e azul) | Sinalização de vagas livres/ocupadas e de saída bloqueada |
| Display LCD 16x2 com I2C | Item da lista de materiais |
| Protoboard 830 pontos e jumpers | Montagem do circuito |

---

## 💰 Custos

Lista de materiais (BOM) com os itens já adquiridos em 03/09/26:

| # | Item | Qtd. | Preço Unit. (R$) | Preço Total (R$) |
| --- | --- | --- | --- | --- |
| 01 | Módulo Wi-Fi ESP32 Bluetooth, 30 pinos | 1 | 44,85 | 44,85 |
| 02 | Display LCD 16x2 com Backlight Azul e I2C | 1 | 24,90 | 24,90 |
| 03 | Protoboard 830 Pontos | 1 | 11,90 | 11,90 |
| 04 | Sensor de Obstáculos Reflexivo Infravermelho LM393 | 8 | 5,90 | 47,20 |
| 05 | Micro Servo Motor SG90 9g 180° | 1 | 14,90 | 14,90 |
| 06 | Kit Módulo RFID MFRC522 13.56 MHz | 1 | 13,40 | 13,40 |
| 07 | LED RGB Difuso 5mm — Cátodo Comum | 6 | 0,80 | 4,80 |
| 08 | Jumpers Macho/Fêmea, 20 un. | 1 | 4,90 | 4,90 |
| 09 | Jumpers Macho/Macho, 20 un. | 1 | 4,90 | 4,90 |
| 10 | Materiais para maquete (base, paredes, pista) *(não comprado)* | A definir | A definir | Estimativa: 30,00 |

| Resumo | Valor (R$) |
| --- | --- |
| Subtotal dos itens comprados (01–09) | 171,75 |
| Frete | 13,70 |
| **Consolidado com frete (itens 01–09)** | **185,45** |
| Custo total estimado da solução (com frete e estimativa do item 10) | ≈ 215,45 |

> **Nota:** o item 10 (materiais da maquete) ainda não foi adquirido; o valor será atualizado quando a equipe definir fornecedor e valor exato.

---

## 🎯 Escopo

O escopo limita-se à montagem e validação de uma **maquete funcional** com **3 vagas físicas** e um **corredor principal de cruzamento**. A camada de software e conectividade está restrita à comunicação direta via servidor embarcado local na placa, **sem bancos de dados complexos** e **sem dependência de infraestrutura robusta em nuvem**.

---

## 🔎 Estado da Arte

Projetos de referência analisados e o diferencial desta solução:

| # | Projeto | Contribuição / Diferencial do nosso projeto |
| --- | --- | --- |
| 1 | [smart-parking-system_iot-Based](https://github.com/saichaitanya-c/smart-parking-system_iot-Based) (saichaitanya-c, 2026) | Projeto mais básico: ESP32 + sensores IR exibindo status em tela no acesso, sem servidor web próprio nem controle de acesso. Nossa solução adiciona interface web embarcada, cancela por RFID e gestão do corredor de tráfego cruzado. |
| 2 | IoT-Based Smart Parking Management System (ESP32, Blynk, ThingSpeak), Roshan Sahu | Projeto mais amplo, usado só como referência lógica: depende de plataforma externa (Blynk/ThingSpeak). O nosso roda cliente e servidor web inteiramente no ESP32, sem nuvem. |
| 3 | [Real-time Parking Monitoring System](https://github.com/Arfazrll/Real-time-Parking-Monitoring-System) (Arfazrll) | Projeto com RFID e API. Nosso diferencial: dashboard e lógica embarcados no ESP32 (sem backend PHP/banco externo) e sinalização dedicada de tráfego cruzado. |
| 4 | [RFID-based Smart Parking System using ESP32](https://github.com/Muhaiminul-Hasan/RFID-based-Smart-Parking-System-using-ESP32) (Muhaiminul-Hasan) | Referência direta para o controle de acesso via RFID. Nosso diferencial: monitoramento individual das vagas com LEDs verde/vermelho e alerta de tráfego cruzado com LEDs azuis. |
| 5 | [IoT Based Smart Parking System](https://www.researchgate.net/publication/381814507_IoT_Based_Smart_Parking_System) (Abdulsaheb, Nafea, Al-Jawher & Hayyawi, 2024) | Artigo acadêmico usado como referência de arquitetura de sensores. Nosso diferencial: ESP32 (Wi-Fi nativo) em vez de NodeMCU + nuvem externa, com painel web local em tempo real. |

---

## ⚠️ Riscos e Desafios Técnicos

| Desafio | Impacto | Causa | Mitigação |
| --- | --- | --- | --- |
| Latência na transmissão via Wi-Fi | Médio | Ruído no ambiente de rede e sobrecarga de pacotes | Protocolo MQTT leve com buffer local de dados |
| Dissipação térmica na fonte chaveada | Alto | Aquecimento acima do esperado em carga máxima contínua | Redimensionar o dissipador e incluir furação de ventilação no case |
| Concorrência entre lógica de controle e servidor web no mesmo ESP32 | Médio | Leitura de sensores/RFID, acionamento do servo e servidor web simultâneos podem causar travamentos ou atrasos na interface | Tarefas paralelas via FreeRTOS e priorização das interrupções dos sensores sobre a renderização das páginas |

---

## 📅 Cronograma

| Marco | Prazo | Entregas | Responsável | Status |
| --- | --- | --- | --- | --- |
| M1: Definição & Pesquisa | Mês 1 | Escopo formalizado, levantamento de requisitos e referências | Equipe | ✅ Concluído |
| M2: Arquitetura & BOM | Mês 2 | Diagrama de blocos, esquemático inicial e lista de compras | Edson Santos | 🔄 Em andamento |
| M3: Prototipagem Alpha | Mês 3 | Testes em bancada, validação individual dos módulos/sensores | Pedro Viana | ⏳ Não iniciado |
| M4: Integração dos Sistemas | Mês 4 | Unificação de hardware, firmware e estrutura mecânica | Equipe | ⏳ Não iniciado |
| M5: Validação & Banca Final | Mês 5 | Protótipo funcional, testes reais e relatório final | Equipe | ⏳ Não iniciado |

---

## 👥 Equipe

| Nome | GitHub | Papel | Responsabilidades |
| --- | --- | --- | --- |
| Chadi Waked | [@3chadi](https://github.com/3chadi) | Estrutura de Rede e IoT | Comunicação Wi-Fi e servidor web interno no ESP32 |
| Edson Vinícius Pereira dos Santos | [@edsonvsantos](https://github.com/edsonvsantos) | Desenvolvedor de Firmware | Lógica de controle no ESP32: leitura dos sensores e acionamento dos LEDs e da cancela |
| Edward Alves Souza | [@edwsz](https://github.com/edwsz) | Hardware e Eletrônica | Montagem física do circuito (ESP32, sensores IR, LEDs e servomotor) |
| Leonardo Neves da Rocha Melo | [@Nevesleonardo](https://github.com/Nevesleonardo) | Desenvolvedor Front-end | Interface web com status de ocupação das vagas em tempo real |
| Pedro Viana Cherobim | [@vianapedro851](https://github.com/vianapedro851) | Qualidade & Documentação | Testes gerais e documentação do projeto |

---

📎 Repositório: [ES-PUCPR/estacionamento-inteligente](https://github.com/ES-PUCPR/estacionamento-inteligente)

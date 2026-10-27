// =====================================================================
//  ESTACIONAMENTO INTELIGENTE - PUCPR 2026/02
//  Conectividade de Sistemas Ciberfisicos - Prof. Fabio Bettio
//
//  Tudo roda em UM ESP32:
//   - 3 vagas com sensor IR + LED RGB (verde = livre, vermelho = ocupada)
//   - sensor IR no corredor -> LEDs azuis "em cascata" (saida bloqueada)
//   - leitor RFID na entrada -> abre a cancela (servo) se houver vaga
//   - LCD 16x2 com vagas livres e mensagens
//   - site embarcado com status em tempo real + log de eventos
//
//  Placa: NodeMCU-32S (ou "ESP32 Dev Module")  |  Core esp32 3.x
//  Bibliotecas (Gerenciador de Bibliotecas):
//   - MFRC522 (GithubCommunity)
//   - ESP32Servo (Kevin Harrington)
//   - LiquidCrystal I2C (Frank de Brabander)
//  WiFi, WebServer, ESPmDNS, SPI e Wire ja vem com a placa.
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ======================= CONFIGURACOES ===============================

// Rede do ESP32 (sempre ligada). Conecte o celular nela e abra
// http://192.168.4.1  - senha com no minimo 8 caracteres.
const char* AP_NOME  = "Estacionamento-IoT";
const char* AP_SENHA = "pucpr2026";

// (Opcional) Rede com internet, para o log ter hora certa (NTP).
// Deixe "" se nao quiser usar. So redes 2,4 GHz com senha simples (WPA2).
const char* WIFI_NOME  = "Waked 2.4g"; 
const char* WIFI_SENHA = "libano0712";

// Endereco do LCD (descubra com o teste T06_Scanner_I2C)
#define LCD_ENDERECO 0x27

// Cartoes cadastrados: coloque os UIDs que o teste T08_RFID mostrou.
struct Cartao { const char* uid; const char* nome; bool dentro; };
Cartao cartoes[] = {
  {"A5 2F 3C 11", "Cartao branco", false},   // <-- troque pelo seu UID
  {"12 34 56 78", "Chaveiro azul", false},   // <-- troque pelo seu UID
};
const int QTD_CARTOES = sizeof(cartoes) / sizeof(cartoes[0]);

// Cancela
#define ANG_FECHADA        0
#define ANG_ABERTA        90
#define TEMPO_ABERTA_MS 4000

// Quanto tempo o alerta azul fica ligado depois que o carro passou
#define ALERTA_CORREDOR_MS 3000

// Leitura precisa ficar estavel por esse tempo para mudar a vaga
// (evita "piscar" quando a mao passa na frente do sensor)
#define ESTABILIDADE_MS 500

// ======================= PINOS =======================================

const int PINO_SENSOR_VAGA[3] = {34, 35, 36};   // 36 = VP
#define   PINO_SENSOR_CORREDOR 39               // 39 = VN
const int PINO_LED_VERM[3]    = {25, 27, 32};
const int PINO_LED_VERDE[3]   = {26, 14, 33};
const int PINO_LED_AZUL[3]    = {15, 16, 17};
#define   PINO_SERVO  13
#define   RFID_SS      5
#define   RFID_RST     4
#define   RFID_SCK    18
#define   RFID_MISO   19
#define   RFID_MOSI   23
#define   LCD_SDA     21
#define   LCD_SCL     22

// ======================= OBJETOS =====================================

WebServer servidor(80);
MFRC522 rfid(RFID_SS, RFID_RST);
Servo cancela;
LiquidCrystal_I2C lcd(LCD_ENDERECO, 16, 2);

// ======================= ESTADO ======================================

bool vagaOcupada[3]     = {false, false, false};
bool leituraAnterior[3] = {false, false, false};
unsigned long mudouEm[3] = {0, 0, 0};

unsigned long ultimoCarroCorredor = 0;
bool corredorBloqueado = false;
int  passoCascata = 0;
unsigned long ultimoPassoCascata = 0;

bool cancelaAberta = false;
unsigned long fecharCancelaEm = 0;

String ultimoAcesso = "nenhum";
String mensagemLcd = "";
unsigned long mensagemLcdAte = 0;

bool ntpIniciado = false;
bool avisouIpRede = false;

// Log circular com os ultimos eventos
#define TAM_LOG 25
String logEventos[TAM_LOG];
int logInicio = 0, logQtd = 0;

// ======================= FUNCOES DE APOIO ============================

String horaAtual() {
  struct tm t;
  if (ntpIniciado && getLocalTime(&t, 10)) {
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    return String(buf);
  }
  // Sem internet: mostra o tempo desde que a placa ligou
  unsigned long s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "+%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  return String(buf);
}

void registrar(const String& texto) {
  String linha = horaAtual() + " - " + texto;
  Serial.println(linha);
  int pos = (logInicio + logQtd) % TAM_LOG;
  if (logQtd < TAM_LOG) {
    logQtd++;
  } else {
    logInicio = (logInicio + 1) % TAM_LOG;   // descarta o mais antigo
  }
  logEventos[pos] = linha;
}

void mostrarMensagem(const String& msg, unsigned long duracaoMs) {
  mensagemLcd = msg;
  mensagemLcdAte = millis() + duracaoMs;
}

int vagasOcupadas() {
  int n = 0;
  for (int i = 0; i < 3; i++) if (vagaOcupada[i]) n++;
  return n;
}

int carrosDentro() {
  int n = 0;
  for (int i = 0; i < QTD_CARTOES; i++) if (cartoes[i].dentro) n++;
  return n;
}

// Um carro que acabou de entrar ainda nao parou na vaga,
// entao consideramos o maior valor entre sensores e cartoes la dentro.
int vagasLivres() {
  int ocupadas = max(vagasOcupadas(), carrosDentro());
  return max(0, 3 - ocupadas);
}

// ======================= VAGAS =======================================

void atualizarVagas() {
  for (int i = 0; i < 3; i++) {
    bool leitura = (digitalRead(PINO_SENSOR_VAGA[i]) == LOW);  // LOW = objeto
    if (leitura != leituraAnterior[i]) {
      leituraAnterior[i] = leitura;
      mudouEm[i] = millis();
    }
    if (leitura != vagaOcupada[i] && millis() - mudouEm[i] > ESTABILIDADE_MS) {
      vagaOcupada[i] = leitura;
      registrar("Vaga 0" + String(i + 1) + (leitura ? " ocupada" : " liberada"));
    }
    digitalWrite(PINO_LED_VERM[i],  vagaOcupada[i] ? HIGH : LOW);
    digitalWrite(PINO_LED_VERDE[i], vagaOcupada[i] ? LOW  : HIGH);
  }
}

// ======================= CORREDOR (LEDs azuis em cascata) ============

void atualizarCorredor() {
  if (digitalRead(PINO_SENSOR_CORREDOR) == LOW) {
    if (!corredorBloqueado) registrar("Veiculo no corredor - saida bloqueada");
    ultimoCarroCorredor = millis();
    corredorBloqueado = true;
  } else if (corredorBloqueado && millis() - ultimoCarroCorredor > ALERTA_CORREDOR_MS) {
    corredorBloqueado = false;
    registrar("Corredor livre");
  }

  if (!corredorBloqueado) {
    for (int i = 0; i < 3; i++) digitalWrite(PINO_LED_AZUL[i], LOW);
    passoCascata = 0;
    return;
  }

  // Cascata: acende 1, depois 1+2, depois 1+2+3, apaga tudo, repete
  if (millis() - ultimoPassoCascata > 200) {
    ultimoPassoCascata = millis();
    passoCascata = (passoCascata + 1) % 4;
    for (int i = 0; i < 3; i++) {
      digitalWrite(PINO_LED_AZUL[i], (i < passoCascata) ? HIGH : LOW);
    }
  }
}

// ======================= CANCELA =====================================

void abrirCancela() {
  cancela.write(ANG_ABERTA);
  if (!cancelaAberta) registrar("Cancela aberta");
  cancelaAberta = true;
  fecharCancelaEm = millis() + TEMPO_ABERTA_MS;
}

void atualizarCancela() {
  if (cancelaAberta && (long)(millis() - fecharCancelaEm) >= 0) {
    cancela.write(ANG_FECHADA);
    cancelaAberta = false;
    registrar("Cancela fechada");
  }
}

// ======================= RFID ========================================

void atualizarRfid() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    char hex[4];
    snprintf(hex, sizeof(hex), "%02X", rfid.uid.uidByte[i]);
    if (i > 0) uid += " ";
    uid += hex;
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  int idx = -1;
  for (int i = 0; i < QTD_CARTOES; i++) {
    if (uid.equalsIgnoreCase(cartoes[i].uid)) { idx = i; break; }
  }

  if (idx < 0) {
    registrar("Cartao " + uid + " NAO cadastrado - acesso negado");
    mostrarMensagem("Acesso negado", 3000);
    return;
  }

  Cartao& c = cartoes[idx];
  if (c.dentro) {
    // Saida: sempre libera
    c.dentro = false;
    registrar("Cartao " + uid + " (" + c.nome + ") saiu");
    ultimoAcesso = "Saida " + uid + " as " + horaAtual();
    mostrarMensagem("Volte sempre!", 3000);
    abrirCancela();
  } else if (vagasLivres() > 0) {
    c.dentro = true;
    registrar("Cartao " + uid + " (" + c.nome + ") entrou");
    ultimoAcesso = "Entrada " + uid + " as " + horaAtual();
    mostrarMensagem("Bem-vindo!", 3000);
    abrirCancela();
  } else {
    registrar("Cartao " + uid + " barrado - estacionamento lotado");
    mostrarMensagem("LOTADO", 3000);
  }
}

// ======================= LCD =========================================

void atualizarLcd() {
  static String linha0Antiga = "", linha1Antiga = "";
  static unsigned long ultimo = 0;
  if (millis() - ultimo < 250) return;   // atualiza 4x por segundo, no maximo
  ultimo = millis();

  String l0 = "Livres: " + String(vagasLivres()) + "/3 " + (cancelaAberta ? "ABER" : "FECH");
  String l1;
  if (millis() < mensagemLcdAte) {
    l1 = mensagemLcd;
  } else {
    l1 = "";
    for (int i = 0; i < 3; i++) {
      l1 += "V" + String(i + 1) + ":" + (vagaOcupada[i] ? "X " : "L ");
    }
    if (corredorBloqueado) l1 += "!";
  }
  while (l0.length() < 16) l0 += " ";
  while (l1.length() < 16) l1 += " ";

  if (l0 != linha0Antiga) { lcd.setCursor(0, 0); lcd.print(l0); linha0Antiga = l0; }
  if (l1 != linha1Antiga) { lcd.setCursor(0, 1); lcd.print(l1); linha1Antiga = l1; }
}

// ======================= WI-FI =======================================

void iniciarWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NOME, AP_SENHA);
  Serial.printf("Rede do ESP32: %s  |  abra http://%s\n",
                AP_NOME, WiFi.softAPIP().toString().c_str());

  if (strlen(WIFI_NOME) > 0) {
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
    Serial.printf("Tentando conectar em \"%s\"...\n", WIFI_NOME);
  }
  if (MDNS.begin("estacionamento")) {
    Serial.println("Tambem pode abrir: http://estacionamento.local");
  }
}

void verificarWifi() {
  if (strlen(WIFI_NOME) == 0) return;
  if (WiFi.status() == WL_CONNECTED && !avisouIpRede) {
    avisouIpRede = true;
    String ip = WiFi.localIP().toString();
    Serial.println("Conectado na rede! IP: " + ip);
    mostrarMensagem(ip, 8000);
    configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br");  // horario de Brasilia
    ntpIniciado = true;
  }
}

// ======================= SITE ========================================

const char PAGINA[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Estacionamento Inteligente</title>
<style>
 body{font-family:system-ui,Arial,sans-serif;margin:0;background:#f2f4f7;color:#1d2430}
 header{background:#1d2430;color:#fff;padding:16px 20px}
 header h1{margin:0;font-size:20px} header p{margin:4px 0 0;opacity:.7;font-size:13px}
 main{max-width:720px;margin:0 auto;padding:16px}
 .vagas{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}
 .vaga{border-radius:12px;padding:18px 8px;text-align:center;color:#fff;font-weight:600}
 .vaga span{display:block;font-size:13px;font-weight:400;margin-top:6px}
 .livre{background:#1f9d55} .ocupada{background:#d64545}
 .cards{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:12px}
 .card{background:#fff;border-radius:12px;padding:12px;text-align:center;font-size:13px}
 .card b{display:block;font-size:20px;margin-top:4px}
 .alerta{background:#2457d6;color:#fff;border-radius:12px;padding:12px;margin-top:12px;
   text-align:center;font-weight:600;display:none}
 h2{font-size:15px;margin:20px 0 8px}
 #log{background:#fff;border-radius:12px;padding:8px 12px;font-family:monospace;
   font-size:13px;max-height:260px;overflow:auto}
 #log div{padding:3px 0;border-bottom:1px solid #eee}
 footer{font-size:12px;opacity:.6;text-align:center;margin:16px 0}
</style></head><body>
<header><h1>Estacionamento Inteligente</h1><p>Status em tempo real - ESP32</p></header>
<main>
 <div class="vagas" id="vagas"></div>
 <div class="alerta" id="alerta">Veiculo no corredor - aguarde para sair</div>
 <div class="cards">
  <div class="card">Vagas livres<b id="livres">-</b></div>
  <div class="card">Cancela<b id="cancela">-</b></div>
  <div class="card">Hora<b id="hora">-</b></div>
 </div>
 <div class="card" style="margin-top:12px">Ultimo acesso: <b id="ultimo" style="font-size:14px">-</b></div>
 <h2>Log de eventos</h2>
 <div id="log"></div>
 <footer id="rodape">conectando...</footer>
</main>
<script>
async function atualizar(){
 try{
  const r = await fetch('/status'); const s = await r.json();
  document.getElementById('vagas').innerHTML = s.vagas.map((o,i)=>
   `<div class="vaga ${o?'ocupada':'livre'}">Vaga 0${i+1}<span>${o?'OCUPADA':'LIVRE'}</span></div>`).join('');
  document.getElementById('livres').textContent = s.livres + ' / 3';
  document.getElementById('cancela').textContent = s.cancela ? 'ABERTA' : 'FECHADA';
  document.getElementById('hora').textContent = s.hora;
  document.getElementById('ultimo').textContent = s.ultimo;
  document.getElementById('alerta').style.display = s.corredor ? 'block' : 'none';
  document.getElementById('log').innerHTML = s.log.slice().reverse().map(l=>`<div>${l}</div>`).join('');
  document.getElementById('rodape').textContent = 'Atualizado automaticamente a cada 1 s';
 }catch(e){ document.getElementById('rodape').textContent = 'Sem conexao com a placa...'; }
}
atualizar(); setInterval(atualizar, 1000);
</script></body></html>
)HTML";

void paginaInicial() {
  servidor.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void paginaStatus() {
  String j = "{\"vagas\":[";
  for (int i = 0; i < 3; i++) {
    j += vagaOcupada[i] ? "true" : "false";
    if (i < 2) j += ",";
  }
  j += "],\"livres\":" + String(vagasLivres());
  j += ",\"cancela\":" + String(cancelaAberta ? "true" : "false");
  j += ",\"corredor\":" + String(corredorBloqueado ? "true" : "false");
  j += ",\"hora\":\"" + horaAtual() + "\"";
  j += ",\"ultimo\":\"" + ultimoAcesso + "\"";
  j += ",\"log\":[";
  for (int k = 0; k < logQtd; k++) {
    if (k > 0) j += ",";
    j += "\"" + logEventos[(logInicio + k) % TAM_LOG] + "\"";
  }
  j += "]}";
  servidor.send(200, "application/json", j);
}

// ======================= SETUP / LOOP ================================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Estacionamento Inteligente ===");

  for (int i = 0; i < 3; i++) {
    pinMode(PINO_SENSOR_VAGA[i], INPUT);
    pinMode(PINO_LED_VERM[i], OUTPUT);
    pinMode(PINO_LED_VERDE[i], OUTPUT);
    pinMode(PINO_LED_AZUL[i], OUTPUT);
  }
  pinMode(PINO_SENSOR_CORREDOR, INPUT);

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Estacionamento");
  lcd.setCursor(0, 1); lcd.print("Iniciando...");

  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();

  cancela.setPeriodHertz(50);
  cancela.attach(PINO_SERVO, 500, 2400);
  cancela.write(ANG_FECHADA);

  iniciarWifi();
  servidor.on("/", paginaInicial);
  servidor.on("/status", paginaStatus);
  servidor.begin();

  // Le o estado inicial das vagas sem esperar a estabilizacao
  for (int i = 0; i < 3; i++) {
    vagaOcupada[i] = leituraAnterior[i] = (digitalRead(PINO_SENSOR_VAGA[i]) == LOW);
  }
  registrar("Sistema iniciado");
  mostrarMensagem(WiFi.softAPIP().toString(), 6000);
}

void loop() {
  // Nada aqui usa delay(): cada funcao faz um pouquinho e devolve o
  // controle, assim o site e os sensores respondem ao mesmo tempo.
  servidor.handleClient();
  atualizarVagas();
  atualizarCorredor();
  atualizarRfid();
  atualizarCancela();
  atualizarLcd();
  verificarWifi();
}

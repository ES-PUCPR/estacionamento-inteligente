#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <LittleFS.h>

const char* AP_NOME  = "Estacionamento-IoT";
const char* AP_SENHA = "pucpr2026";

const char* WIFI_NOME  = "eps32_67";
const char* WIFI_SENHA = "123456789";

#define LCD_ENDERECO 0x27

const int ANG_FECHADA = 0;
const int ANG_ABERTA  = 90;

struct Cartao {
  const char* uid;
  const char* nome;
  bool dentro;
};
Cartao cartoes[] = {
  { "A5 2F 3C 11", "Cartao 1",   false },
  { "00 00 00 00", "Chaveiro 1", false },
};
const int NUM_CARTOES = sizeof(cartoes) / sizeof(cartoes[0]);

const bool IR_ATIVO_EM_LOW = true;

const unsigned long T_CONFIRMA_VAGA     = 1000;
const unsigned long T_CONFIRMA_CORREDOR = 500;
const unsigned long T_AZUL_APOS         = 3000;
const unsigned long T_CANCELA_ABERTA    = 4000;
const unsigned long T_MSG_LCD           = 3000;
const unsigned long T_REPETE_CARTAO     = 2500;

const int PIN_IR_VAGA[3]       = { 34, 35, 36 };
const int PIN_IR_CORREDOR[3]   = { 39, 16, 17 };
const int PIN_VAGA_VERDE[3]    = { 25, 27, 32 };
const int PIN_VAGA_VERMELHO[3] = { 2, 14, 15 };
const int PIN_TRAFEGO_AZUL[3]  = { 26, 33, 12 };
const bool RGB_ANODO_COMUM     = false;
const int PIN_SERVO            = 13;
const int RFID_SS = 21, RFID_SCK = 18, RFID_MISO = 22, RFID_MOSI = 4;
const int LCD_SDA = 5, LCD_SCL = 19;

void ledRGB(int pino, bool aceso) {
  digitalWrite(pino, (aceso != RGB_ANODO_COMUM) ? HIGH : LOW);
}

WebServer server(80);
MFRC522 rfid(RFID_SS, MFRC522::UNUSED_PIN);
Servo cancela;
LiquidCrystal_I2C lcd(LCD_ENDERECO, 16, 2);

bool vagaOcupada[3]       = { false, false, false };
bool vagaLeitura[3]       = { false, false, false };
unsigned long vagaDesde[3]   = { 0, 0, 0 };

bool corredorCarro[3]   = { false, false, false };
bool corredorLeitura[3] = { false, false, false };
unsigned long corredorDesde[3]  = { 0, 0, 0 };
unsigned long corredorSaiuEm[3] = { 0, 0, 0 };
bool azulAceso[3]       = { false, false, false };

bool cancelaAberta = false;
unsigned long cancelaAbertaEm = 0;

String ultimoUid = "";
unsigned long ultimaLeituraCartao = 0;
String ultimoAcesso = "Nenhum ainda";
bool rfidOk = false;

String lcdMsg1 = "", lcdMsg2 = "";
unsigned long lcdMsgDesde = 0, lcdMsgDuracao = 0;
bool lcdTemMsg = false;
String lcdTela1 = "", lcdTela2 = "";

bool horaSincronizada = false;

const int LOG_MAX = 25;
String logs[LOG_MAX];
int logInicio = 0, logQtd = 0;

const char* LOG_ARQUIVO = "/log.txt";
const char* LOG_ANTIGO  = "/log_antigo.txt";
const size_t LOG_TAMANHO_MAX = 64 * 1024;
bool flashOk = false;

String doisDigitos(int n) { return (n < 10 ? "0" : "") + String(n); }

String horaAtual() {
  time_t agora = time(nullptr);
  if (agora > 1700000000) {
    horaSincronizada = true;
    struct tm t;
    localtime_r(&agora, &t);
    char texto[20];
    strftime(texto, sizeof(texto), "%d/%m/%Y %H:%M:%S", &t);
    return String(texto);
  }
  unsigned long s = millis() / 1000;
  return "sem data +" + doisDigitos(s / 3600) + ":" + doisDigitos((s / 60) % 60) + ":" + doisDigitos(s % 60);
}

void gravarNaFlash(const String& linha) {
  if (!flashOk) return;
  File arquivo = LittleFS.open(LOG_ARQUIVO, FILE_APPEND);
  if (!arquivo) return;
  arquivo.println(linha);
  size_t tamanho = arquivo.size();
  arquivo.close();
  if (tamanho > LOG_TAMANHO_MAX) {
    LittleFS.remove(LOG_ANTIGO);
    LittleFS.rename(LOG_ARQUIVO, LOG_ANTIGO);
  }
}

void registrar(const char* nivel, const String& texto) {
  String linha = horaAtual() + "  [" + nivel + "] " + texto;
  Serial.println(linha);
  gravarNaFlash(linha);
  if (logQtd < LOG_MAX) {
    logs[(logInicio + logQtd) % LOG_MAX] = linha;
    logQtd++;
  } else {
    logs[logInicio] = linha;
    logInicio = (logInicio + 1) % LOG_MAX;
  }
}

void registrar(const String& texto) { registrar("INFO", texto); }

bool lerIR(int pino) {
  int v = digitalRead(pino);
  return IR_ATIVO_EM_LOW ? (v == LOW) : (v == HIGH);
}

int vagasLivres() {
  int n = 0;
  for (int i = 0; i < 3; i++) if (!vagaOcupada[i]) n++;
  return n;
}

void mostrarMsgPor(const String& l1, const String& l2, unsigned long duracao) {
  lcdMsg1 = l1; lcdMsg2 = l2;
  lcdMsgDesde = millis(); lcdMsgDuracao = duracao;
  lcdTemMsg = true;
}

void mostrarMsg(const String& l1, const String& l2) {
  mostrarMsgPor(l1, l2, T_MSG_LCD);
}

String jsonTexto(const String& s) {
  String r = "\"";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if (c >= 32) r += c;
  }
  return r + "\"";
}

void iniciarWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NOME, AP_SENHA);
  Serial.print("Rede propria: "); Serial.print(AP_NOME);
  Serial.print("  ->  http://"); Serial.println(WiFi.softAPIP());

  if (strlen(WIFI_NOME) > 0) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
  }
  configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br");
  if (MDNS.begin("estacionamento")) MDNS.addService("http", "tcp", 80);
}

void verificarWifi() {
  if (strlen(WIFI_NOME) == 0) return;
  static bool estavaConectado = false;
  static bool jaConectou = false;
  static unsigned long ultimaTentativa = 0;
  bool conectado = (WiFi.status() == WL_CONNECTED);

  if (conectado && !estavaConectado) {
    registrar("INFO", String(jaConectou ? "Wi-Fi reconectado: " : "Wi-Fi conectado: ") + WiFi.localIP().toString());
    jaConectou = true;
    mostrarMsg("Wi-Fi conectado", WiFi.localIP().toString());
    configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br");
  }
  if (!conectado && estavaConectado) {
    registrar("AVISO", "Wi-Fi caiu; tentando reconectar");
    ultimaTentativa = millis();
  }
  if (!conectado && millis() - ultimaTentativa > 30000) {
    ultimaTentativa = millis();
    int status = WiFi.status();
    registrar("ERRO", "Wi-Fi nao conectou em " + String(WIFI_NOME) + " (status " + String(status) + ")");
    WiFi.disconnect();
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
  }
  estavaConectado = conectado;
}

void atualizarVagas() {
  unsigned long agora = millis();
  for (int i = 0; i < 3; i++) {
    bool leitura = lerIR(PIN_IR_VAGA[i]);
    if (leitura != vagaLeitura[i]) {
      vagaLeitura[i] = leitura;
      vagaDesde[i] = agora;
    } else if (leitura != vagaOcupada[i] && agora - vagaDesde[i] >= T_CONFIRMA_VAGA) {
      vagaOcupada[i] = leitura;
      registrar("Vaga 0" + String(i + 1) + (leitura ? " ocupada" : " liberada"));
    }
    ledRGB(PIN_VAGA_VERMELHO[i], vagaOcupada[i]);
    ledRGB(PIN_VAGA_VERDE[i], !vagaOcupada[i]);
  }
}

void atualizarCorredor() {
  unsigned long agora = millis();
  for (int i = 0; i < 3; i++) {
    bool leitura = lerIR(PIN_IR_CORREDOR[i]);
    if (leitura != corredorLeitura[i]) {
      corredorLeitura[i] = leitura;
      corredorDesde[i] = agora;
    } else if (leitura != corredorCarro[i] && agora - corredorDesde[i] >= T_CONFIRMA_CORREDOR) {
      corredorCarro[i] = leitura;
      if (leitura) {
        azulAceso[i] = true;
        registrar("Carro atras da vaga 0" + String(i + 1));
      } else {
        corredorSaiuEm[i] = agora;
      }
    }
    if (azulAceso[i] && !corredorCarro[i] && agora - corredorSaiuEm[i] >= T_AZUL_APOS) {
      azulAceso[i] = false;
    }
    ledRGB(PIN_TRAFEGO_AZUL[i], azulAceso[i]);
  }
}

void abrirCancela() {
  cancela.write(ANG_ABERTA);
  if (!cancelaAberta) registrar("Cancela aberta");
  cancelaAberta = true;
  cancelaAbertaEm = millis();
}

void fecharCancela() {
  if (!cancelaAberta) return;
  cancela.write(ANG_FECHADA);
  cancelaAberta = false;
  registrar("Cancela fechada");
}

void atualizarCancela() {
  if (cancelaAberta && millis() - cancelaAbertaEm >= T_CANCELA_ABERTA) fecharCancela();
}

String uidTexto(const MFRC522::Uid& u) {
  String s = "";
  for (byte i = 0; i < u.size; i++) {
    if (i > 0) s += " ";
    if (u.uidByte[i] < 0x10) s += "0";
    s += String(u.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

int acharCartao(const String& uid) {
  for (int i = 0; i < NUM_CARTOES; i++) if (uid.equalsIgnoreCase(cartoes[i].uid)) return i;
  return -1;
}

bool leitorRespondendo() {
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  return v != 0x00 && v != 0xFF;
}

void atualizarRfid() {
  static unsigned long ultimaChecagem = 0;
  if (millis() - ultimaChecagem > 10000) {
    ultimaChecagem = millis();
    bool ok = leitorRespondendo();
    if (!ok) { rfid.PCD_Init(); ok = leitorRespondendo(); }
    if (ok != rfidOk) registrar(ok ? "INFO" : "ERRO", ok ? "Leitor RFID OK" : "Leitor RFID NAO respondeu");
    rfidOk = ok;
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  String uid = uidTexto(rfid.uid);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (uid == ultimoUid && millis() - ultimaLeituraCartao < T_REPETE_CARTAO) return;
  ultimoUid = uid;
  ultimaLeituraCartao = millis();

  int idx = acharCartao(uid);
  if (idx < 0) {
    registrar("AVISO", "Acesso negado: " + uid);
    ultimoAcesso = horaAtual() + " - Negado (" + uid + ")";
    mostrarMsg("Acesso negado", uid);
    return;
  }

  Cartao& c = cartoes[idx];
  if (!c.dentro) {
    if (vagasLivres() == 0) {
      registrar("AVISO", "LOTADO: " + String(c.nome) + " nao entrou");
      ultimoAcesso = horaAtual() + " - " + c.nome + " (lotado)";
      mostrarMsg("    LOTADO!", "Sem vagas livres");
      return;
    }
    c.dentro = true;
    abrirCancela();
    registrar("Entrada: " + String(c.nome) + " (" + uid + ")");
    ultimoAcesso = horaAtual() + " - Entrada " + c.nome;
    mostrarMsg("  Bem-vindo!", c.nome);
  } else {
    c.dentro = false;
    abrirCancela();
    registrar("Saida: " + String(c.nome) + " (" + uid + ")");
    ultimoAcesso = horaAtual() + " - Saida " + c.nome;
    mostrarMsg(" Volte sempre!", c.nome);
  }
}

String completar16(String s) {
  if (s.length() > 16) s = s.substring(0, 16);
  while (s.length() < 16) s += " ";
  return s;
}

void atualizarLcd() {
  static unsigned long ultima = 0;
  if (millis() - ultima < 200) return;
  ultima = millis();

  if (lcdTemMsg && millis() - lcdMsgDesde >= lcdMsgDuracao) lcdTemMsg = false;

  String l1, l2;
  if (lcdTemMsg) {
    l1 = lcdMsg1; l2 = lcdMsg2;
  } else {
    l1 = "Livres: " + String(vagasLivres()) + "/3 " + (cancelaAberta ? "ABER" : "FECH");
    l2 = "";
    for (int i = 0; i < 3; i++) {
      if (i > 0) l2 += " ";
      l2 += "V" + String(i + 1) + ":" + (vagaOcupada[i] ? "X" : "L");
    }
  }
  l1 = completar16(l1);
  l2 = completar16(l2);
  if (l1 != lcdTela1) { lcd.setCursor(0, 0); lcd.print(l1); lcdTela1 = l1; }
  if (l2 != lcdTela2) { lcd.setCursor(0, 1); lcd.print(l2); lcdTela2 = l2; }
}

const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Estacionamento Inteligente</title>
<style>
  :root{--bg:#0f172a;--card:#1e293b;--txt:#e2e8f0;--sub:#94a3b8;--verde:#22c55e;--verm:#ef4444;--azul:#3b82f6}
  *{box-sizing:border-box}
  body{margin:0;font-family:system-ui,Arial,sans-serif;background:var(--bg);color:var(--txt);padding:16px}
  h1{font-size:1.3rem;margin:0 0 4px}
  .sub{color:var(--sub);font-size:.85rem;margin-bottom:14px}
  .grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
  .card{background:var(--card);border-radius:12px;padding:14px;margin-bottom:12px}
  .vaga{text-align:center;border-radius:12px;padding:16px 6px;font-weight:700;color:#fff}
  .vaga small{display:block;font-weight:400;opacity:.9;margin-top:4px}
  .livre{background:var(--verde)} .ocupada{background:var(--verm)}
  .linha{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid #334155}
  .linha:last-child{border:0}
  .big{font-size:2rem;font-weight:800}
  .leds{display:flex;gap:10px;margin-top:8px}
  .led{width:26px;height:26px;border-radius:50%;background:#334155;transition:background .1s}
  .led.on{background:var(--azul);box-shadow:0 0 12px var(--azul)}
  .botoes{display:flex;gap:10px;margin-top:10px}
  button{flex:1;padding:12px;border:0;border-radius:10px;font:inherit;font-weight:700;color:#fff;cursor:pointer}
  button:disabled{opacity:.5;cursor:wait}
  #btAbrir{background:#16a34a} #btFechar{background:#dc2626}
  #cmd{margin:8px 0 0}
  ul{list-style:none;padding:0;margin:0;font-family:ui-monospace,monospace;font-size:.8rem}
  li{padding:4px 0;border-bottom:1px solid #334155}
  .off{color:var(--verm)}
</style>
</head>
<body>
  <h1>Estacionamento Inteligente</h1>
  <div class="sub">Atualiza a cada 1 s · <span id="hora">--</span> · <span id="con">conectando…</span></div>

  <div class="grid card" style="padding:10px">
    <div class="vaga" id="v0">Vaga 1<small>--</small></div>
    <div class="vaga" id="v1">Vaga 2<small>--</small></div>
    <div class="vaga" id="v2">Vaga 3<small>--</small></div>
  </div>

  <div class="card">
    <div class="linha"><span>Vagas livres</span><span class="big" id="livres">-</span></div>
    <div class="linha"><span>Cancela</span><b id="cancela">-</b></div>
    <div class="botoes">
      <button id="btAbrir" onclick="comandar('aberta')">Abrir cancela</button>
      <button id="btFechar" onclick="comandar('fechada')">Fechar cancela</button>
    </div>
    <div class="sub" id="cmd"></div>
    <div class="linha"><span>Tráfego cruzado</span><b id="corredor">-</b></div>
    <div class="leds"><div class="led" id="a0"></div><div class="led" id="a1"></div><div class="led" id="a2"></div></div>
  </div>

  <div class="card">
    <div class="linha"><span>Último acesso</span></div>
    <div id="acesso" style="padding-top:6px">-</div>
  </div>

  <div class="card">
    <div class="linha"><span>Wi-Fi</span><b id="wifi">-</b></div>
    <div class="linha"><span>Nome da rede</span><b id="ssid">-</b></div>
    <div class="linha"><span>IP no Wi-Fi</span><b id="ip">-</b></div>
    <div class="linha"><span>IP da rede própria</span><b id="ipAP">-</b></div>
    <div class="linha"><span>MAC</span><b id="mac">-</b></div>
  </div>

  <div class="card">
    <div class="linha"><span>Log de eventos</span><a href="/log" target="_blank" style="color:var(--azul)">Abrir log completo</a></div>
    <ul id="log"></ul>
  </div>

<script>
  async function buscar(url, opcoes = {}){
    const c = new AbortController();
    const t = setTimeout(() => c.abort(), 3000);
    try{ return await fetch(url, {...opcoes, cache:'no-store', signal:c.signal}); }
    finally{ clearTimeout(t); }
  }

  let limparMsg;
  async function comandar(estado){
    const msg = document.getElementById('cmd');
    const botoes = [document.getElementById('btAbrir'), document.getElementById('btFechar')];
    botoes.forEach(b => b.disabled = true);
    clearTimeout(limparMsg);
    msg.textContent = 'Enviando comando…';
    msg.className = 'sub';
    try{
      const r = await buscar('/cancela?estado=' + estado, {method:'POST'});
      const d = await r.json();
      if(!r.ok) throw new Error(d.erro);
      document.getElementById('cancela').textContent = d.cancela === 'aberta' ? 'Aberta' : 'Fechada';
      msg.textContent = d.cancela === 'aberta' ? 'Cancela aberta (fecha sozinha em 4 s)' : 'Cancela fechada';
      limparMsg = setTimeout(() => msg.textContent = '', 4000);
    }catch(e){
      msg.textContent = 'Comando não executado: sem resposta do ESP32';
      msg.className = 'sub off';
    }finally{
      botoes.forEach(b => b.disabled = false);
    }
  }

  async function atualizar(){
    try{
      const r = await buscar('/status');
      const d = await r.json();
      document.getElementById('con').textContent = 'online';
      document.getElementById('con').className = '';
      document.getElementById('hora').textContent = d.hora;
      d.vagas.forEach((ocup, i) => {
        const el = document.getElementById('v' + i);
        el.className = 'vaga ' + (ocup ? 'ocupada' : 'livre');
        el.querySelector('small').textContent = ocup ? 'Ocupada' : 'Livre';
      });
      document.getElementById('livres').textContent = d.livres + '/' + d.total;
      document.getElementById('cancela').textContent = d.cancela === 'aberta' ? 'Aberta' : 'Fechada';
      document.getElementById('corredor').textContent =
        d.corredor ? 'Carro passando' : (d.azul.includes(true) ? 'Liberando…' : 'Livre');
      d.azul.forEach((on, i) => document.getElementById('a' + i).classList.toggle('on', on));
      document.getElementById('acesso').textContent = d.ultimoAcesso;
      document.getElementById('wifi').textContent = d.wifi === 'conectado' ? 'Conectado (' + d.rssi + ' dBm)' : 'Desconectado';
      document.getElementById('ssid').textContent = d.ssid;
      document.getElementById('ip').textContent = d.ip;
      document.getElementById('ipAP').textContent = d.ipAP;
      document.getElementById('mac').textContent = d.mac;
      const ul = document.getElementById('log');
      ul.innerHTML = '';
      d.log.forEach(t => { const li = document.createElement('li'); li.textContent = t; ul.appendChild(li); });
    }catch(e){
      document.getElementById('con').textContent = 'sem conexão com o ESP32';
      document.getElementById('con').className = 'off';
    }
    setTimeout(atualizar, 1000);
  }
  atualizar();
</script>
</body>
</html>
)rawliteral";

void paginaInicial() {
  server.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void paginaStatus() {
  String j;
  j.reserve(3500);
  j += "{\"vagas\":[";
  for (int i = 0; i < 3; i++) { if (i) j += ","; j += vagaOcupada[i] ? "true" : "false"; }
  j += "],\"livres\":";   j += vagasLivres();
  j += ",\"total\":3";
  j += ",\"cancela\":";   j += cancelaAberta ? "\"aberta\"" : "\"fechada\"";
  j += ",\"corredor\":";  j += (corredorCarro[0] || corredorCarro[1] || corredorCarro[2]) ? "true" : "false";
  j += ",\"azul\":[";
  for (int i = 0; i < 3; i++) { if (i) j += ","; j += azulAceso[i] ? "true" : "false"; }
  j += "]";
  j += ",\"rfid\":";     j += rfidOk ? "true" : "false";
  j += ",\"ultimoAcesso\":"; j += jsonTexto(ultimoAcesso);
  j += ",\"hora\":";      j += jsonTexto(horaAtual());
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  j += ",\"wifi\":";      j += jsonTexto(wifiOk ? "conectado" : "desconectado");
  j += ",\"ssid\":";      j += jsonTexto(WIFI_NOME);
  j += ",\"ip\":";        j += jsonTexto(wifiOk ? WiFi.localIP().toString() : String("-"));
  j += ",\"ipAP\":";      j += jsonTexto(WiFi.softAPIP().toString());
  j += ",\"mac\":";       j += jsonTexto(WiFi.macAddress());
  j += ",\"rssi\":";      j += wifiOk ? (int)WiFi.RSSI() : 0;
  j += ",\"log\":[";
  for (int k = 0; k < logQtd; k++) {
    int idx = (logInicio + logQtd - 1 - k) % LOG_MAX;
    if (k) j += ",";
    j += jsonTexto(logs[idx]);
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void paginaLog() {
  const char* caminho = server.hasArg("antigo") ? LOG_ANTIGO : LOG_ARQUIVO;
  if (!flashOk || !LittleFS.exists(caminho)) {
    server.send(404, "text/plain; charset=utf-8", "Log vazio ou flash indisponivel");
    return;
  }
  File arquivo = LittleFS.open(caminho, FILE_READ);
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(arquivo, "text/plain; charset=utf-8");
  arquivo.close();
}

void comandoCancela() {
  String estado = server.arg("estado");
  if (estado == "aberta") {
    abrirCancela();
    registrar("Comando de rede: abrir cancela");
    ultimoAcesso = horaAtual() + " - Aberta pelo site";
    mostrarMsg("Comando de rede", "Cancela aberta");
  } else if (estado == "fechada") {
    fecharCancela();
    registrar("Comando de rede: fechar cancela");
    ultimoAcesso = horaAtual() + " - Fechada pelo site";
    mostrarMsg("Comando de rede", "Cancela fechada");
  } else {
    server.send(400, "application/json", "{\"erro\":\"use estado=aberta ou estado=fechada\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json",
              String("{\"ok\":true,\"cancela\":\"") + (cancelaAberta ? "aberta" : "fechada") + "\"}");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== Estacionamento Inteligente ===");
  flashOk = LittleFS.begin(true);

  for (int i = 0; i < 3; i++) {
    pinMode(PIN_IR_VAGA[i], INPUT);
    pinMode(PIN_IR_CORREDOR[i], INPUT);
  }

  for (int i = 0; i < 3; i++) {
    pinMode(PIN_VAGA_VERDE[i], OUTPUT);
    pinMode(PIN_VAGA_VERMELHO[i], OUTPUT);
    pinMode(PIN_TRAFEGO_AZUL[i], OUTPUT);
    ledRGB(PIN_VAGA_VERDE[i], true);
    ledRGB(PIN_VAGA_VERMELHO[i], false);
    ledRGB(PIN_TRAFEGO_AZUL[i], true);
  }

  Wire.begin(LCD_SDA, LCD_SCL);
  Wire.beginTransmission(LCD_ENDERECO);
  bool lcdOk = (Wire.endTransmission() == 0);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Estacionamento");
  lcd.setCursor(0, 1); lcd.print("Iniciando...");

  ESP32PWM::allocateTimer(0);
  cancela.setPeriodHertz(50);
  cancela.attach(PIN_SERVO, 500, 2400);
  cancela.write(ANG_FECHADA);

  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();
  delay(50);
  rfidOk = leitorRespondendo();

  delay(350);
  for (int i = 0; i < 3; i++) { ledRGB(PIN_VAGA_VERDE[i], false); ledRGB(PIN_VAGA_VERMELHO[i], true); }
  delay(350);
  for (int i = 0; i < 3; i++) ledRGB(PIN_TRAFEGO_AZUL[i], false);

  for (int i = 0; i < 3; i++) {
    vagaOcupada[i] = vagaLeitura[i] = lerIR(PIN_IR_VAGA[i]);
    vagaDesde[i] = millis();
  }

  iniciarWifi();
  server.on("/", paginaInicial);
  server.on("/status", paginaStatus);
  server.on("/cancela", comandoCancela);
  server.on("/log", paginaLog);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();

  registrar("Sistema iniciado");
  if (!flashOk) registrar("ERRO", "Flash indisponivel: log apenas na memoria RAM");
  registrar(rfidOk ? "INFO" : "ERRO", rfidOk ? "Leitor RFID OK" : "Leitor RFID NAO respondeu (confira fios/solda)");
  registrar(lcdOk ? "INFO" : "ERRO", lcdOk ? "LCD OK" : "LCD NAO respondeu (confira SDA/SCL e o endereco)");
  registrar("Vagas livres: " + String(vagasLivres()) + "/3");

  mostrarMsgPor("Site: rede IoT", WiFi.softAPIP().toString(), 5000);
}

void loop() {
  server.handleClient();
  verificarWifi();
  atualizarVagas();
  atualizarCorredor();
  atualizarRfid();
  atualizarCancela();
  atualizarLcd();
}

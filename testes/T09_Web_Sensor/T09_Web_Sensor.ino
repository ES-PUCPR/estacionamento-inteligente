
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

const char* WIFI_NOME  = "Galaxy A56 5G 6E33"; 
const char* WIFI_SENHA = "12345678";

const char* AP_NOME  = "Teste-Vaga";
const char* AP_SENHA = "pucpr2026";   

#define PINO_SENSOR     23
#define ESTABILIDADE_MS 500   
// ---------------------------------------------------------------------

WebServer servidor(80);

bool vagaOcupada = false;
bool leituraAnterior = false;
unsigned long mudou = 0;
unsigned long ocupou = 0; 
bool horario = false;

#define TAM_LOG 30
String logEventos[TAM_LOG];
int logInicio = 0, logQtd = 0;

String horaAtual() {
  struct tm t;
  if (horario && getLocalTime(&t, 10)) {
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    return String(buf);
  }
  unsigned long s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "+%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  return String(buf);
}

String duracaoTexto(unsigned long ms) {
  unsigned long s = ms / 1000;
  unsigned long h = s / 3600, m = (s / 60) % 60, seg = s % 60;
  String r = "";
  if (h > 0) r += String(h) + " h ";
  if (h > 0 || m > 0) r += String(m) + " min ";
  r += String(seg) + " s";
  return r;
}

void registrar(const String& texto) {
  String linha = horaAtual() + " - " + texto;
  Serial.println(linha);
  int pos = (logInicio + logQtd) % TAM_LOG;
  if (logQtd < TAM_LOG) logQtd++;
  else logInicio = (logInicio + 1) % TAM_LOG;
  logEventos[pos] = linha;
}

void atualizarSensor() {
  bool leitura = (digitalRead(PINO_SENSOR) == LOW); 
  if (leitura != leituraAnterior) {
    leituraAnterior = leitura;
    mudou = millis();
  }
  if (leitura != vagaOcupada && millis() - mudou > ESTABILIDADE_MS) {
    vagaOcupada = leitura;
    if (vagaOcupada) {
      ocupou = millis();
      registrar("Vaga 01 OCUPADA");
    } else {
      registrar("Vaga 01 LIVRE (ficou ocupada por " + duracaoTexto(millis() - ocupou) + ")");
    }
  }
}

const char PAGINA[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Teste Vaga</title>
<style>
 body{font-family:system-ui,Arial,sans-serif;margin:0;background:#f2f4f7;color:#1d2430}
 header{background:#1d2430;color:#fff;padding:14px 18px}
 header h1{margin:0;font-size:19px} header p{margin:4px 0 0;opacity:.7;font-size:13px}
 main{max-width:640px;margin:0 auto;padding:16px}
 .vaga{border-radius:14px;padding:22px;text-align:center;color:#fff}
 .vaga b{display:block;font-size:24px} .vaga span{font-size:15px}
 .livre{background:#1f9d55} .ocupada{background:#d64545}
 h2{font-size:15px;margin:20px 0 8px}
 #log{background:#fff;border-radius:12px;padding:8px 12px;font-family:monospace;font-size:13px}
 #log div{padding:4px 0;border-bottom:1px solid #eee}
 footer{font-size:12px;opacity:.6;text-align:center;margin:14px 0}
</style></head><body>
<header><h1>Teste: Vaga 01</h1><p>Sensor infravermelho + servidor web no ESP32</p></header>
<main>
 <div id="vaga" class="vaga livre"><b>Vaga 01</b><span>-</span></div>
 <h2>Log de eventos</h2>
 <div id="log"><div>Sem eventos ainda</div></div>
 <footer id="rodape">conectando...</footer>
</main>
<script>
async function atualizar(){
 try{
  const s = await (await fetch('/status')).json();
  const v = document.getElementById('vaga');
  v.className = 'vaga ' + (s.ocupada ? 'ocupada' : 'livre');
  v.querySelector('span').textContent = s.ocupada
     ? 'OCUPADA ha ' + s.tempo : 'LIVRE';
  if (s.log.length) document.getElementById('log').innerHTML =
     s.log.slice().reverse().map(l => '<div>' + l + '</div>').join('');
  document.getElementById('rodape').textContent = 'Hora da placa: ' + s.hora + ' - atualiza a cada 1 s';
 }catch(e){ document.getElementById('rodape').textContent = 'Sem conexao com a placa...'; }
}
atualizar(); setInterval(atualizar, 1000);
</script></body></html>
)HTML";

void paginaInicial() {
  servidor.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void paginaStatus() {
  String j = "{\"ocupada\":" + String(vagaOcupada ? "true" : "false");
  j += ",\"tempo\":\"" + (vagaOcupada ? duracaoTexto(millis() - ocupou) : String("")) + "\"";
  j += ",\"hora\":\"" + horaAtual() + "\"";
  j += ",\"log\":[";
  for (int k = 0; k < logQtd; k++) {
    if (k > 0) j += ",";
    j += "\"" + logEventos[(logInicio + k) % TAM_LOG] + "\"";
  }
  j += "]}";
  servidor.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PINO_SENSOR, INPUT);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NOME, AP_SENHA);

  Serial.printf("\nConectando em \"%s\"", WIFI_NOME);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br"); 
    horario = true;
    Serial.println("Conectado! Abra no navegador (mesma rede):");
    Serial.print("   http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Nao conectou no Wi-Fi. Use a rede da propria placa:");
  }
  Serial.printf("   ou conecte em \"%s\" (senha %s) e abra http://%s\n",
                AP_NOME, AP_SENHA, WiFi.softAPIP().toString().c_str());

  servidor.on("/", paginaInicial);
  servidor.on("/status", paginaStatus);
  servidor.begin();

  vagaOcupada = leituraAnterior = (digitalRead(PINO_SENSOR) == LOW);
  if (vagaOcupada) ocupou = millis();
  delay(1500);
  registrar(String("Sistema iniciado - vaga ") + (vagaOcupada ? "OCUPADA" : "LIVRE"));
}

void loop() {
  servidor.handleClient();
  atualizarSensor();
}

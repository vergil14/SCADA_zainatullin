/*
 * Лабораторная работа N8. Часть 2 — web-стенд на ESP32
 * «Простейшие методы помехоустойчивого кодирования»
 *
 * Плата: Wemos D1 R1 ESP32 (в Arduino IDE — «WEMOS D1 R32»).
 * В папке скетча должны лежать: fec_codes.h (из части 1) и page_html.h.
 *
 * Светодиоды (через резисторы 220 Ом на GND):
 *   GPIO 16 — зелёный  «принято верно»
 *   GPIO 17 — жёлтый   «ошибка обнаружена»
 *   GPIO 18 — красный  «ошибка НЕ обнаружена»
 *
 * HTTP API (все ответы — JSON):
 *   /api/info    ?code=P&k=5            | ?code=W&m=2&n=5
 *   /api/encode  ...&data=11011
 *   /api/check   ...&sent=110110&word=100110
 *   /api/exp     ...&p=0.01&N=100000
 *   /api/theory  ?k=5&m=2&n=5           — 1-K_обн(p0) для четырёх кодов
 */
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "fec_codes.h"
#include "page_html.h"

// ---------------- Настройки сети ----------------
#define USE_SOFTAP 1                 // 1 — ESP32 создаёт свою точку доступа, 0 — подключение к роутеру
const char *AP_SSID   = "ESP32-FEC-Lab";
const char *AP_PASS   = "12345678";  // не короче 8 символов
const char *STA_SSID  = "Your_SSID";
const char *STA_PASS  = "Your_PASSWORD";

// ---------------- Светодиоды ----------------
const int LED_OK  = 16;
const int LED_DET = 17;
const int LED_UND = 18;

WebServer server(80);

// Исход приёма одной комбинации
enum Outcome : uint8_t {
  OUT_CORRECT    = 0,   // искажений не было, декодер ошибок не нашёл
  OUT_DETECTED   = 1,   // искажения были и обнаружены
  OUT_UNDETECTED = 2,   // искажения были, но декодер их пропустил
  OUT_FALSE      = 3    // искажений не было, а декодер сообщил об ошибке (ошибка в программе)
};
const char *OUT_NAMES[] = {"correct", "detected", "undetected", "false_alarm"};

// -------------------------------------------------------------------
// Классификация исхода приёма.
// sent — переданная комбинация, recv — принятая, ok — решение декодера
// (true — ошибок не обнаружено).
// -------------------------------------------------------------------
Outcome classify(uint32_t sent, uint32_t recv, bool ok) {
  bool corrupted = (sent != recv);
  if (!corrupted && ok)    return OUT_CORRECT;
  if (corrupted && !ok)    return OUT_DETECTED;
  if (corrupted && ok)     return OUT_UNDETECTED;
  return OUT_FALSE;
}

// -------------------------------------------------------------------
// Индикация исхода на светодиодах: горит ровно один светодиод.
// Для OUT_FALSE мигают все три (3 раза), затем гаснут.
// -------------------------------------------------------------------
void indicate(Outcome o) {
  digitalWrite(LED_OK,  LOW);
  digitalWrite(LED_DET, LOW);
  digitalWrite(LED_UND, LOW);

  if (o == OUT_CORRECT) {
    digitalWrite(LED_OK, HIGH);
  } else if (o == OUT_DETECTED) {
    digitalWrite(LED_DET, HIGH);
  } else if (o == OUT_UNDETECTED) {
    digitalWrite(LED_UND, HIGH);
  } else if (o == OUT_FALSE) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_OK, HIGH);
      digitalWrite(LED_DET, HIGH);
      digitalWrite(LED_UND, HIGH);
      delay(150);
      digitalWrite(LED_OK, LOW);
      digitalWrite(LED_DET, LOW);
      digitalWrite(LED_UND, LOW);
      delay(150);
    }
  }
}

// -------------------------------------------------------------------
// Вспомогательные функции HTTP
// -------------------------------------------------------------------
String num(double v) {                // число для JSON (экспоненциальная форма)
  char b[24];
  snprintf(b, sizeof(b), "%.6g", v);
  return String(b);
}

String bits(uint32_t x, uint8_t n) {
  char b[33];
  return String(fecToStr(x, n, b));
}

void sendJson(const String &s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

void sendError(const String &msg) {
  server.send(400, "application/json", "{\"error\":\"" + msg + "\"}");
}

// Чтение параметров кода из запроса: code=P|W|C|I, k, m, n
bool parseCode(CodeParams &c) {
  String t = server.arg("code");
  if (t.length() == 0) return false;
  uint8_t type = toupper(t[0]);
  if (type == CODE_WEIGHT) return fecSetup(c, type, 0, server.arg("m").toInt(), server.arg("n").toInt());
  return fecSetup(c, type, server.arg("k").toInt());
}

String codeJson(const CodeParams &c) {
  double R = (double)c.k / c.n;
  double Lid = 1.0 - ((c.type == CODE_WEIGHT) ? log2((double)fecBinom(c.n, c.m)) / c.n : R);
  String s = "\"code\":\"" + String((char)c.type) + "\",\"name\":\"" + fecName(c.type) + "\"";
  s += ",\"k\":" + String(c.k) + ",\"n\":" + String(c.n) + ",\"m\":" + String(c.m);
  s += ",\"R\":" + num(R) + ",\"L\":" + num(1.0 - R) + ",\"Lid\":" + num(Lid);
  s += ",\"messages\":" + String(fecMessages(c));
  return s;
}

// -------------------------------------------------------------------
// Обработчики маршрутов
// -------------------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", PAGE_HTML);
}

void handleInfo() {
  CodeParams c;
  if (!parseCode(c)) return sendError("bad code params");
  sendJson("{" + codeJson(c) + "}");
}

void handleEncode() {
  CodeParams c;
  if (!parseCode(c)) return sendError("bad code params");
  uint32_t info;
  int len = fecFromStr(server.arg("data").c_str(), &info);
  if (len != c.k) return sendError("data must have k bits");
  if (info >= fecMessages(c)) return sendError("index exceeds C(n,m)-1");
  uint32_t w = fecEncode(c, info);
  sendJson("{" + codeJson(c) + ",\"info\":\"" + bits(info, c.k) + "\",\"word\":\"" + bits(w, c.n) +
           "\",\"weight\":" + String(fecWeight(w)) + "}");
}

void handleCheck() {
  CodeParams c;
  if (!parseCode(c)) return sendError("bad code params");
  uint32_t sent, recv;
  if (fecFromStr(server.arg("sent").c_str(), &sent) != c.n ||
      fecFromStr(server.arg("word").c_str(), &recv) != c.n) return sendError("words must have n bits");
  bool ok = fecCheck(c, recv);
  Outcome o = classify(sent, recv, ok);
  indicate(o);
  uint32_t e = sent ^ recv;
  String s = "{\"ok\":" + String(ok ? "true" : "false");
  s += ",\"outcome\":\"" + String(OUT_NAMES[o]) + "\"";
  s += ",\"evec\":\"" + bits(e, c.n) + "\",\"multiplicity\":" + String(fecWeight(e));
  s += ",\"weight\":" + String(fecWeight(recv));
  s += ",\"info\":\"" + (ok ? bits(fecExtract(c, recv), c.k) : String("")) + "\"}";
  sendJson(s);
}

void handleExp() {
  CodeParams c;
  if (!parseCode(c)) return sendError("bad code params");
  double p = server.arg("p").toFloat();
  uint32_t N = strtoul(server.arg("N").c_str(), NULL, 10);
  if (p < 0 || p > 1 || N == 0 || N > 2000000UL) return sendError("0<=p<=1, 1<=N<=2000000");
  unsigned long t0 = millis();
  ExpResult r = fecExperiment(c, p, N);
  unsigned long dt = millis() - t0;
  uint32_t errs = r.detected + r.undetected;
  String s = "{" + codeJson(c) + ",\"p\":" + num(p) + ",\"N\":" + String(r.N);
  s += ",\"noErr\":" + String(r.noErr) + ",\"detected\":" + String(r.detected);
  s += ",\"undetected\":" + String(r.undetected) + ",\"falseAlarm\":" + String(r.falseAlarm);
  s += ",\"p0meas\":" + num((double)r.bitErrors / ((double)r.N * c.n));
  s += ",\"PerrExp\":" + num((double)errs / r.N) + ",\"PerrTh\":" + num(fecPerr(c, p));
  s += ",\"PudExp\":" + num((double)r.undetected / r.N) + ",\"PudTh\":" + num(fecPundet(c, p));
  s += ",\"Kexp\":" + (errs ? num((double)r.detected / errs) : String("null"));
  s += ",\"Kth\":" + num(fecKtheor(c, p));
  s += ",\"ms\":" + String(dt) + "}";
  sendJson(s);
}

void handleTheory() {
  CodeParams cs[4];
  bool ok = fecSetup(cs[0], CODE_PARITY, server.arg("k").toInt()) &&
            fecSetup(cs[1], CODE_WEIGHT, 0, server.arg("m").toInt(), server.arg("n").toInt()) &&
            fecSetup(cs[2], CODE_CORR, server.arg("k").toInt()) &&
            fecSetup(cs[3], CODE_INV, server.arg("k").toInt());
  if (!ok) return sendError("bad params");
  String s = "{\"p\":[";
  const int NP = 41;                      // p0 от 1e-3 до 0.5 по логарифмической шкале
  for (int i = 0; i < NP; i++) { if (i) s += ','; s += num(pow(10.0, -3.0 + 2.69897 * i / (NP - 1))); }
  s += "]";
  for (int j = 0; j < 4; j++) {
    s += ",\"" + String((char)cs[j].type) + "\":{\"n\":" + String(cs[j].n) + ",\"k\":" + String(cs[j].k) + ",\"u\":[";
    for (int i = 0; i < NP; i++) {
      double p = pow(10.0, -3.0 + 2.69897 * i / (NP - 1));
      if (i) s += ',';
      s += num(1.0 - fecKtheor(cs[j], p));  // доля необнаруженных ошибок
    }
    s += "]}";
  }
  s += "}";
  sendJson(s);
}

// -------------------------------------------------------------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Отключение детектора просадки напряжения (Brownout)
  Serial.begin(115200);
  delay(1000); // пауза для стабилизации USB питания после сброса
  Serial.println("\n--- Старт ESP32 Web-стенда ---");

  pinMode(LED_OK, OUTPUT); pinMode(LED_DET, OUTPUT); pinMode(LED_UND, OUTPUT);
  digitalWrite(LED_OK, LOW); digitalWrite(LED_DET, LOW); digitalWrite(LED_UND, LOW);
  fecSeed(esp_random());

#if USE_SOFTAP
  Serial.println("Включение Wi-Fi точки доступа...");
  WiFi.mode(WIFI_AP);
  delay(200);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // снижение мощности передачи для предотвращения просадки питания
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS, 1);
  delay(500);
  if (ap_ok) {
    Serial.print("Точка доступа запущена: "); Serial.println(AP_SSID);
    Serial.print("IP адрес для браузера: ");  Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ОШИБКА запуска точки доступа!");
  }
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println();
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
#endif

  server.on("/", handleRoot);
  server.on("/api/info", handleInfo);
  server.on("/api/encode", handleEncode);
  server.on("/api/check", handleCheck);
  server.on("/api/exp", handleExp);
  server.on("/api/theory", handleTheory);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

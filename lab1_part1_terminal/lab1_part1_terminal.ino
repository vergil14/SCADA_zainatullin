/*
 * Лабораторная работа N8. Часть 1 — терминальный стенд
 * «Простейшие методы помехоустойчивого кодирования»
 *
 * Плата: Arduino Uno или ESP32 (Wemos D1 R1 ESP32). Монитор порта: 115200 бод,
 * окончание строки — «Новая строка» (NL) или «NL и CR».
 *
 * Все функции кодирования/декодирования находятся в fec_codes.h.
 * Введите  help  для списка команд.
 */
#include "fec_codes.h"

// ---- Параметры индивидуального варианта (используются командой cmp) ----
uint8_t VAR_K  = 4;   // k для кодов P, C, I
uint8_t VAR_M  = 2;   // m для кода W
uint8_t VAR_NW = 5;   // n для кода W

CodeParams code;          // текущий код
uint32_t   lastSent = 0;  // последняя закодированная комбинация
bool       haveSent = false;

char line[72];
uint8_t lineLen = 0;

// -------------------------------------------------------------------
// Печать
// -------------------------------------------------------------------
void printBits(uint32_t x, uint8_t n) {
  char b[33];
  Serial.print(fecToStr(x, n, b));
}

// Печать с разделением на группы, соответствующие структуре кода
void printWord(uint32_t x) {
  char b[34];
  fecToStr(x, code.n, b);
  for (uint8_t i = 0; i < code.n; i++) {
    bool sep = false;
    if (code.type == CODE_PARITY && i == code.k) sep = true;
    if (code.type == CODE_INV    && i == code.k) sep = true;
    if (code.type == CODE_CORR   && i > 0 && (i % 2) == 0) sep = true;
    if (sep) Serial.print(code.type == CODE_CORR ? ' ' : '|');
    Serial.print(b[i]);
  }
}

// Число в экспоненциальной форме, например 1.441e-03
void printSci(double v) {
  if (v <= 0) { Serial.print(F("0")); return; }
  int e = (int)floor(log10(v));
  double m = v / pow(10.0, e);
  if (m >= 9.9995) { m /= 10; e++; }
  Serial.print(m, 3);
  Serial.print('e');
  if (e < 0) { Serial.print('-'); e = -e; } else Serial.print('+');
  if (e < 10) Serial.print('0');
  Serial.print(e);
}

void printCodeInfo() {
  Serial.print(F("Код: "));
  Serial.print(fecName(code.type));
  Serial.print(F("  k=")); Serial.print(code.k);
  Serial.print(F("  n=")); Serial.print(code.n);
  if (code.type == CODE_WEIGHT) {
    Serial.print(F("  m=")); Serial.print(code.m);
    Serial.print(F("  C(n,m)=")); Serial.print(fecBinom(code.n, code.m));
  }
  double R = (double)code.k / code.n;
  Serial.print(F("  R=")); Serial.print(R, 3);
  Serial.print(F("  L=")); Serial.print(1.0 - R, 3);
  if (code.type == CODE_WEIGHT) {
    double Rid = log((double)fecBinom(code.n, code.m)) / log(2.0) / code.n;
    Serial.print(F("  (по log2 C: L=")); Serial.print(1.0 - Rid, 3); Serial.print(')');
  }
  Serial.println();
}

void printHelp() {
  Serial.println(F("------------------------------------------------------------"));
  Serial.println(F(" code p K      код с проверкой на чётность, k = K"));
  Serial.println(F(" code w M N    код с постоянным весом M из N"));
  Serial.println(F(" code c K      корреляционный код, k = K"));
  Serial.println(F(" code i K      инверсный код, k = K"));
  Serial.println(F(" info          параметры текущего кода"));
  Serial.println(F(" enc BITS      закодировать k информационных бит (для W - номер комбинации)"));
  Serial.println(F(" dec BITS      проверить принятую комбинацию из n бит"));
  Serial.println(F(" flip i j ...  исказить разряды i, j... (1..n слева) последней комбинации"));
  Serial.println(F(" list          таблица кодовых комбинаций (не более 64)"));
  Serial.println(F(" exp P N       эксперимент: N комбинаций через ДСК с p0 = P"));
  Serial.println(F(" sweep N       K_обн(p0) для текущего кода, CSV"));
  Serial.println(F(" var K M N     параметры варианта для cmp"));
  Serial.println(F(" cmp P N       сравнение четырёх кодов при p0 = P"));
  Serial.println(F(" seed S        начальное значение генератора"));
  Serial.println(F("------------------------------------------------------------"));
}

// -------------------------------------------------------------------
// Команды
// -------------------------------------------------------------------
void cmdCode(char *args) {
  char *t = strtok(args, " ");
  char *a = strtok(NULL, " ");
  char *b = strtok(NULL, " ");
  if (!t || !a) { Serial.println(F("Формат: code p|c|i K  или  code w M N")); return; }
  uint8_t type = toupper(t[0]);
  bool ok;
  if (type == CODE_WEIGHT) ok = b && fecSetup(code, type, 0, atoi(a), atoi(b));
  else                     ok = fecSetup(code, type, atoi(a));
  if (!ok) { Serial.println(F("Недопустимые параметры")); fecSetup(code, CODE_PARITY, 5); }
  haveSent = false;
  printCodeInfo();
}

void report(uint32_t sent, uint32_t recv) {
  uint32_t e = sent ^ recv;
  bool ok = fecCheck(code, recv);
  Serial.print(F("  передано : ")); printWord(sent); Serial.println();
  Serial.print(F("  ошибки   : ")); printWord(e);    Serial.print(F("   кратность ")); Serial.println(fecWeight(e));
  Serial.print(F("  принято  : ")); printWord(recv); Serial.println();
  Serial.print(F("  декодер  : ")); Serial.println(ok ? F("ошибок не обнаружено") : F("ОШИБКА ОБНАРУЖЕНА, комбинация бракуется"));
  Serial.print(F("  итог     : "));
  if (e == 0)  Serial.println(ok ? F("верный приём") : F("ложная тревога - проверьте программу!"));
  else         Serial.println(ok ? F("НЕОБНАРУЖЕННАЯ ОШИБКА") : F("ошибка обнаружена"));
}

void cmdEnc(char *args) {
  uint32_t info;
  int len = fecFromStr(args, &info);
  if (len != code.k) { Serial.print(F("Нужно ровно k=")); Serial.print(code.k); Serial.println(F(" бит")); return; }
  if (info >= fecMessages(code)) { Serial.println(F("Номер превышает C(n,m)-1")); return; }
  lastSent = fecEncode(code, info);
  haveSent = true;
  Serial.print(F("  ")); printBits(info, code.k); Serial.print(F("  ->  ")); printWord(lastSent);
  Serial.print(F("   вес ")); Serial.println(fecWeight(lastSent));
}

void cmdDec(char *args) {
  uint32_t w;
  int len = fecFromStr(args, &w);
  if (len != code.n) { Serial.print(F("Нужно ровно n=")); Serial.print(code.n); Serial.println(F(" бит")); return; }
  bool ok = fecCheck(code, w);
  Serial.print(F("  ")); printWord(w); Serial.print(F("  ->  "));
  if (ok) { Serial.print(F("ошибок не обнаружено, информация ")); printBits(fecExtract(code, w), code.k); Serial.println(); }
  else      Serial.println(F("ОШИБКА ОБНАРУЖЕНА"));
}

void cmdFlip(char *args) {
  if (!haveSent) { Serial.println(F("Сначала выполните enc")); return; }
  uint32_t e = 0;
  for (char *t = strtok(args, " "); t; t = strtok(NULL, " ")) {
    int i = atoi(t);
    if (i < 1 || i > code.n) { Serial.println(F("Номер разряда вне диапазона 1..n")); return; }
    e ^= 1UL << (code.n - i);
  }
  report(lastSent, lastSent ^ e);
}

void cmdList() {
  uint32_t M = fecMessages(code);
  if (M > 64) M = 64;
  for (uint32_t i = 0; i < M; i++) {
    Serial.print(F("  "));
    if (i < (1UL << code.k)) printBits(i, code.k);
    else for (uint8_t j = 0; j < code.k; j++) Serial.print('-');   // для W: комбинация сверх 2^k
    Serial.print(F("  ->  "));
    printWord(fecEncode(code, i));
    if (code.type == CODE_WEIGHT) { Serial.print(F("   №")); Serial.print(i); }
    Serial.println();
  }
}

void printExp(const CodeParams &c, double p, const ExpResult &r) {
  uint32_t errs = r.detected + r.undetected;
  double Kexp = errs ? (double)r.detected / errs : 1.0;
  Serial.print(F("  N=")); Serial.print(r.N);
  Serial.print(F("  без искажений=")); Serial.print(r.noErr);
  Serial.print(F("  обнаружено=")); Serial.print(r.detected);
  Serial.print(F("  не обнаружено=")); Serial.print(r.undetected);
  if (r.falseAlarm) { Serial.print(F("  ЛОЖНЫЕ ТРЕВОГИ=")); Serial.print(r.falseAlarm); }
  Serial.println();
  Serial.print(F("  p0 измер.=")); Serial.print((double)r.bitErrors / ((double)r.N * c.n), 5);
  Serial.print(F("  P_ош эксп=")); printSci((double)errs / r.N);
  Serial.print(F("  теор=")); printSci(fecPerr(c, p));
  Serial.println();
  Serial.print(F("  P_но эксп=")); printSci((double)r.undetected / r.N);
  Serial.print(F("  теор=")); printSci(fecPundet(c, p));
  Serial.println();
  Serial.print(F("  K_обн эксп=")); Serial.print(Kexp, 5);
  Serial.print(F("  теор=")); Serial.println(fecKtheor(c, p), 5);
}

void cmdExp(char *args) {
  char *a = strtok(args, " ");
  char *b = strtok(NULL, " ");
  double p = a ? atof(a) : 0.01;
  uint32_t N = b ? strtoul(b, NULL, 10) : 10000UL;
  if (p < 0 || p > 1 || N == 0) { Serial.println(F("Формат: exp P N")); return; }
  printCodeInfo();
  unsigned long t0 = millis();
  ExpResult r = fecExperiment(code, p, N);
  printExp(code, p, r);
  Serial.print(F("  время ")); Serial.print(millis() - t0); Serial.println(F(" мс"));
}

const double P_GRID[] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.3};

void cmdSweep(char *args) {
  uint32_t N = (args && *args) ? strtoul(args, NULL, 10) : 20000UL;
  printCodeInfo();
  Serial.println(F("code;k;n;p0;N;detected;undetected;K_exp;K_theor;Pud_exp;Pud_theor"));
  for (uint8_t j = 0; j < sizeof(P_GRID) / sizeof(P_GRID[0]); j++) {
    double p = P_GRID[j];
    ExpResult r = fecExperiment(code, p, N);
    uint32_t errs = r.detected + r.undetected;
    Serial.print((char)code.type); Serial.print(';');
    Serial.print(code.k); Serial.print(';'); Serial.print(code.n); Serial.print(';');
    Serial.print(p, 3); Serial.print(';'); Serial.print(N); Serial.print(';');
    Serial.print(r.detected); Serial.print(';'); Serial.print(r.undetected); Serial.print(';');
    Serial.print(errs ? (double)r.detected / errs : 1.0, 5); Serial.print(';');
    Serial.print(fecKtheor(code, p), 5); Serial.print(';');
    printSci((double)r.undetected / N); Serial.print(';');
    printSci(fecPundet(code, p)); Serial.println();
  }
}

void cmdVar(char *args) {
  char *a = strtok(args, " "), *b = strtok(NULL, " "), *c = strtok(NULL, " ");
  if (a && b && c) { VAR_K = atoi(a); VAR_M = atoi(b); VAR_NW = atoi(c); }
  Serial.print(F("Вариант: k=")); Serial.print(VAR_K);
  Serial.print(F("  W: ")); Serial.print(VAR_M); Serial.print(F(" из ")); Serial.println(VAR_NW);
}

void cmdCmp(char *args) {
  char *a = strtok(args, " "), *b = strtok(NULL, " ");
  double p = a ? atof(a) : 0.01;
  uint32_t N = b ? strtoul(b, NULL, 10) : 20000UL;
  CodeParams saved = code;
  const uint8_t types[4] = {CODE_PARITY, CODE_WEIGHT, CODE_CORR, CODE_INV};
  for (uint8_t i = 0; i < 4; i++) {
    bool ok = (types[i] == CODE_WEIGHT) ? fecSetup(code, CODE_WEIGHT, 0, VAR_M, VAR_NW)
                                        : fecSetup(code, types[i], VAR_K);
    if (!ok) continue;
    printCodeInfo();
    printExp(code, p, fecExperiment(code, p, N));
  }
  code = saved;
}

void execute(char *s) {
  while (*s == ' ') s++;
  char *args = strchr(s, ' ');
  if (args) { *args++ = 0; while (*args == ' ') args++; } else args = s + strlen(s);
  Serial.print(F("> ")); Serial.print(s); Serial.print(' '); Serial.println(args);

  if      (!strcmp(s, "help") || !strcmp(s, "?")) printHelp();
  else if (!strcmp(s, "code"))  cmdCode(args);
  else if (!strcmp(s, "info"))  printCodeInfo();
  else if (!strcmp(s, "enc"))   cmdEnc(args);
  else if (!strcmp(s, "dec"))   cmdDec(args);
  else if (!strcmp(s, "flip"))  cmdFlip(args);
  else if (!strcmp(s, "list"))  cmdList();
  else if (!strcmp(s, "exp"))   cmdExp(args);
  else if (!strcmp(s, "sweep")) cmdSweep(args);
  else if (!strcmp(s, "var"))   cmdVar(args);
  else if (!strcmp(s, "cmp"))   cmdCmp(args);
  else if (!strcmp(s, "seed"))  { fecSeed(strtoul(args, NULL, 10)); Serial.println(F("ok")); }
  else if (*s) Serial.println(F("Неизвестная команда, help - список команд"));
}

// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
#if defined(ESP32)
  fecSeed(esp_random());
#else
  randomSeed(analogRead(A0));
  fecSeed(((uint32_t)analogRead(A0) << 16) ^ micros() ^ random(0x7FFFFFFF));
#endif
  fecSetup(code, CODE_PARITY, VAR_K);
  Serial.println();
  Serial.println(F("ЛР N8. Простейшие коды, обнаруживающие ошибки"));
  printHelp();
  printCodeInfo();
}

void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (lineLen) { line[lineLen] = 0; execute(line); lineLen = 0; }
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = ch;
    }
  }
}

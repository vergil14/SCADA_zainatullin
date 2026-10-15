/*
 * fec_codes.h — простейшие коды, обнаруживающие ошибки
 * Лабораторная работа N8 «Простейшие методы помехоустойчивого кодирования»
 *
 * Коды:  P — с проверкой на чётность        (n = k + 1)
 *        W — с постоянным весом m из n      (k = floor(log2 C(n,m)))
 *        C — корреляционный (с удвоением)   (n = 2k)
 *        I — инверсный                      (n = 2k)
 *
 * Представление кодовой комбинации: младшие n бит переменной uint32_t.
 * Старший из них (бит n-1) — первый передаваемый, он пишется слева:
 *   комбинация "110110" (n = 6) хранится как 0b110110.
 *
 * Файл одинаково компилируется для Arduino Uno, ESP32 и для ПК (g++).
 */
#ifndef FEC_CODES_H
#define FEC_CODES_H

#include <cstdint>
#include <math.h>
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#define FEC_YIELD() yield()
#else
#define FEC_YIELD()
#endif

/* ------------------------------------------------------------------ */
/*  Типы кодов и параметры                                             */
/* ------------------------------------------------------------------ */
enum CodeType : uint8_t {
  CODE_PARITY = 'P',
  CODE_WEIGHT = 'W',
  CODE_CORR = 'C',
  CODE_INV = 'I'
};

struct CodeParams {
  uint8_t type; // CODE_PARITY / CODE_WEIGHT / CODE_CORR / CODE_INV
  uint8_t k;    // число информационных разрядов
  uint8_t n;    // длина кодовой комбинации
  uint8_t m;    // вес (только для кода с постоянным весом)
};

static const uint8_t FEC_MAX_N = 24; // ограничение длины комбинации

/* ------------------------------------------------------------------ */
/*  Вспомогательные функции (готовы, менять не нужно)                  */
/* ------------------------------------------------------------------ */

// Маска из n младших единиц
static inline uint32_t fecMask(uint8_t n) {
  return (n >= 32) ? 0xFFFFFFFFUL : ((1UL << n) - 1UL);
}

// Вес Хэмминга w(x) — число единиц в слове
static inline uint8_t fecWeight(uint32_t x) {
  uint8_t w = 0;
  while (x) {
    x &= x - 1;
    w++;
  }
  return w;
  }

// Чётность слова: 0 — чётное число единиц, 1 — нечётное
static inline uint8_t fecParity(uint32_t x) { return fecWeight(x) & 1; }

// Число сочетаний C(n, r)
static inline uint32_t fecBinom(uint8_t n, uint8_t r) {
  if (r > n)
    return 0;
  if (r > n - r)
    r = n - r;
  uint32_t c = 1;
  for (uint8_t i = 1; i <= r; i++)
    c = c * (n - r + i) / i;
  return c;
}

// Слово -> строка из '0'/'1' длиной n (buf должен вмещать n+1 символ)
static inline char *fecToStr(uint32_t x, uint8_t n, char *buf) {
  for (uint8_t i = 0; i < n; i++)
    buf[i] = ((x >> (n - 1 - i)) & 1UL) ? '1' : '0';
  buf[n] = 0;
  return buf;
}

// Строка из '0'/'1' -> слово. Возвращает число прочитанных разрядов или -1 при
// ошибке
static inline int fecFromStr(const char *s, uint32_t *out) {
  uint32_t x = 0;
  int len = 0;
  for (; *s; s++) {
    if (*s == '0' || *s == '1') {
      x = (x << 1) | (uint32_t)(*s - '0');
      len++;
    } else if (*s == ' ' || *s == '_' || *s == '|')
      continue; // разделители допускаются
    else
      return -1;
    if (len > 31)
      return -1;
  }
  *out = x;
  return len;
}

/* ------------------------------------------------------------------ */
/*  1. Код с проверкой на чётность                                     */
/* ------------------------------------------------------------------ */
// k информационных разрядов + 1 контрольный (справа): общее число единиц
// чётное. Пример: 11011 -> 110110,  11100 -> 111001
static inline uint32_t parityEncode(uint32_t info, uint8_t k) {
  info = info << 1;
  uint8_t bitPer = fecParity(info);
  info = info | bitPer;
  return info;
}

// true — ошибок не обнаружено (число единиц чётное)
static inline bool parityCheck(uint32_t word, uint8_t n) {
	return fecParity(word & fecMask(n)) == 0;
}

// Выделение информационной части
static inline uint32_t parityExtract(uint32_t word, uint8_t n) {
  return (word & fecMask(n)) >> 1;
}

/* ------------------------------------------------------------------ */
/*  2. Код с постоянным весом m из n                                   */
/* ------------------------------------------------------------------ */
// Разрешены только комбинации, содержащие ровно m единиц; всего их C(n,m).
// Сообщение — номер комбинации idx (0 .. C(n,m)-1) в порядке возрастания:
//   «2 из 5»: 0 -> 00011, 1 -> 00101, 2 -> 00110, 3 -> 01001, ... 9 -> 11000
// Алгоритм (комбинаторная система счисления): идём от старшего разряда pos =
// n-1 к младшему; c = C(pos, r) — сколько разрешённых «хвостов» начинается с 0
// в разряде pos при r оставшихся единицах. Если idx >= c, ставим 1, idx -= c,
// r--.

    static inline uint32_t weightEncode(uint32_t idx, uint8_t n, uint8_t m) {
      uint32_t res = 0;   // сюда будем собирать биты нашего ответа
      uint8_t r = m;      // сколько единиц нам ещё осталось поставить (сначала 2)

      // Идём по позициям разрядов от старшей (pos = n - 1) вниз до 0:
      for (int pos = n - 1; pos >= 0 && r > 0; pos--) {
	// Сколько комбинаций начинаются с 0 на этой позиции:
        uint32_t c = fecBinom((uint8_t)pos, r);

	// Если наш номер больше или равен c — ставим 1:
        if (idx >= c) {
          res |= (1UL << pos);  // зажигаем единицу в разряде pos
          idx -= c;             // уменьшаем номер
          r--;			// одной единицей стало меньше
        }
      }

      return res;  // возвращаем собранную комбинацию
    }

// true — ошибок не обнаружено (вес равен m)
static inline bool weightCheck(uint32_t word, uint8_t n, uint8_t m) {
  return fecWeight(word & fecMask(n)) == m;
}

// Обратное преобразование: комбинация -> номер (для принятых без ошибок)
static inline uint32_t weightExtract(uint32_t word, uint8_t n, uint8_t m) {
  uint32_t idx = 0;
  uint8_t r = m;
  for (int pos = n - 1; pos >= 0 && r > 0; pos--)
    if ((word >> pos) & 1UL) {
      idx += fecBinom((uint8_t)pos, r);
      r--;
    }
  return idx;
}

/* ------------------------------------------------------------------ */
/*  3. Корреляционный код (с удвоением)                                */
/* ------------------------------------------------------------------ */
// Каждый символ заменяется парой: 1 -> 10, 0 -> 01.
// Пример: 1010011 -> 10 01 10 01 01 10 10
static inline uint32_t corrEncode(uint32_t info, uint8_t k) {
  uint32_t res = 0;
  for (int i = k - 1; i >= 0; i--) {
    uint8_t bit = (info >> i) & 1UL;
    res = (res << 2) | (bit ? 0b10 : 0b01);
  }
  return res;
}

// true — ошибок не обнаружено (во всех парах символы различны: 01 или 10)
static inline bool corrCheck(uint32_t word, uint8_t k) {
  for (uint8_t i = 0; i < k; i++) {
    uint8_t pair = (word >> (2 * i)) & 0x03;
    if (pair != 0b01 && pair != 0b10) return false;
  }
  return true;
}

// Выделение информации: старший бит каждой пары
static inline uint32_t corrExtract(uint32_t word, uint8_t k) {
  uint32_t info = 0;
  for (int i = k - 1; i >= 0; i--)
    info = (info << 1) | ((word >> (2 * i + 1)) & 1UL);
  return info;
}

/* ------------------------------------------------------------------ */
/*  4. Инверсный код                                                   */
/* ------------------------------------------------------------------ */
// К k информационным разрядам добавляются k контрольных: при чётном числе
// единиц — повтор, при нечётном — инверсия информационной части.
// Пример: 11011 -> 11011 11011,  11100 -> 11100 00011
static inline uint32_t invEncode(uint32_t info, uint8_t k) {
  uint32_t ctrl = fecParity(info) ? ((~info) & fecMask(k)) : info;
  return (info << k) | ctrl;
}

// Декодирование в два этапа:
//  1) считаем единицы в информационной группе; если их нечётное число —
//     инвертируем контрольную группу;
//  2) складываем по модулю 2 информационную и контрольную группы:
//     нулевая сумма — ошибок нет, иначе комбинация бракуется.
static inline bool invCheck(uint32_t word, uint8_t k) {
  uint32_t info = (word >> k) & fecMask(k);
  uint32_t ctrl = word & fecMask(k);
  if (fecParity(info)) ctrl = (~ctrl) & fecMask(k);
  return (info ^ ctrl) == 0;
}

static inline uint32_t invExtract(uint32_t word, uint8_t k) {
  return (word >> k) & fecMask(k);
}

/* ------------------------------------------------------------------ */
/*  Общий интерфейс (готов)                                            */
/* ------------------------------------------------------------------ */

// Настройка параметров. Для W задаются m и n, для остальных — k.
// Возвращает false, если параметры недопустимы.
static inline bool fecSetup(CodeParams &c, uint8_t type, uint8_t k,
                            uint8_t m = 0, uint8_t n = 0) {
  c.type = type;
  c.m = 0;
  switch (type) {
  case CODE_PARITY:
    c.k = k;
    c.n = k + 1;
    break;
  case CODE_CORR:
  case CODE_INV:
    c.k = k;
    c.n = 2 * k;
    break;
  case CODE_WEIGHT: {
    if (n < 2 || m < 1 || m >= n)
      return false;
    c.n = n;
    c.m = m;
    uint32_t N = fecBinom(n, m);
    uint8_t kk = 0;
    while ((2UL << kk) <= N)
      kk++; // floor(log2 C(n,m))
    c.k = kk;
    break;
  }
  default:
    return false;
  }
  return c.k >= 1 && c.n <= FEC_MAX_N;
}

static inline uint32_t fecEncode(const CodeParams &c, uint32_t info) {
  switch (c.type) {
  case CODE_PARITY:
    return parityEncode(info, c.k);
  case CODE_WEIGHT:
    return weightEncode(info, c.n, c.m);
  case CODE_CORR:
    return corrEncode(info, c.k);
  case CODE_INV:
    return invEncode(info, c.k);
  }
  return 0;
}

// true — ошибок не обнаружено
static inline bool fecCheck(const CodeParams &c, uint32_t word) {
  switch (c.type) {
  case CODE_PARITY:
    return parityCheck(word, c.n);
  case CODE_WEIGHT:
    return weightCheck(word, c.n, c.m);
  case CODE_CORR:
    return corrCheck(word, c.k);
  case CODE_INV:
    return invCheck(word, c.k);
  }
  return false;
}

static inline uint32_t fecExtract(const CodeParams &c, uint32_t word) {
  switch (c.type) {
  case CODE_PARITY:
    return parityExtract(word, c.n);
  case CODE_WEIGHT:
    return weightExtract(word, c.n, c.m);
  case CODE_CORR:
    return corrExtract(word, c.k);
  case CODE_INV:
    return invExtract(word, c.k);
  }
  return 0;
}

// Число допустимых информационных сообщений (для W: C(n,m), иначе 2^k)
static inline uint32_t fecMessages(const CodeParams &c) {
  return (c.type == CODE_WEIGHT) ? fecBinom(c.n, c.m) : (1UL << c.k);
}

static inline const char *fecName(uint8_t type) {
  switch (type) {
  case CODE_PARITY:
    return "parity";
  case CODE_WEIGHT:
    return "const-weight";
  case CODE_CORR:
    return "correlation";
  case CODE_INV:
    return "inverse";
  }
  return "?";
}

/* ------------------------------------------------------------------ */
/*  Теория: вероятность необнаруженной ошибки в ДСК                    */
/* ------------------------------------------------------------------ */
static inline double fecPow(double x, int e) {
  double r = 1;
  while (e-- > 0)
    r *= x;
  return r;
}

// Вероятность любой ошибки в комбинации P_Σ = 1 - (1-p)^n
static inline double fecPerr(const CodeParams &c, double p) {
  return 1.0 - fecPow(1.0 - p, c.n);
}

// Вероятность необнаруженной ошибки P_но
static inline double fecPundet(const CodeParams &c, double p) {
  const double q = 1.0 - p;
  double s = 0;
  switch (c.type) {
  case CODE_PARITY: // чётное число искажений 2, 4, 6, ...
    for (uint8_t i = 2; i <= c.n; i += 2)
      s += fecBinom(c.n, i) * fecPow(p, i) * fecPow(q, c.n - i);
    break;
  case CODE_WEIGHT: // ошибки смещения: i единиц -> 0 и одновременно i нулей ->
                    // 1
    for (uint8_t i = 1; i <= c.m && i <= c.n - c.m; i++)
      s += (double)fecBinom(c.m, i) * fecBinom(c.n - c.m, i) *
           fecPow(p, 2 * i) * fecPow(q, c.n - 2 * i);
    break;
  case CODE_CORR: // ни одной пары с одиночной ошибкой, но хотя бы одна пара
                  // искажена дважды
    s = fecPow(q * q + p * p, c.k) - fecPow(q, 2 * c.k);
    break;
  case CODE_INV: { // код линейный: ошибка не обнаруживается, если сама является
                   // кодовым словом
    // кодовое слово для x веса j имеет вес 2j (j чётное) или k (j нечётное)
    for (uint8_t j = 1; j <= c.k; j++) {
      uint8_t w = (j & 1) ? c.k : 2 * j;
      s += fecBinom(c.k, j) * fecPow(p, w) * fecPow(q, c.n - w);
    }
    break;
  }
  }
  return s;
}

// Теоретический коэффициент обнаружения K_обн = P_об / P_Σ = 1 - P_но / P_Σ
static inline double fecKtheor(const CodeParams &c, double p) {
  double pe = fecPerr(c, p);
  return (pe > 0) ? 1.0 - fecPundet(c, p) / pe : 1.0;
}

/* ------------------------------------------------------------------ */
/*  Статистический эксперимент: двоичный симметричный канал            */
/* ------------------------------------------------------------------ */
struct ExpResult {
  uint32_t N;          // передано комбинаций
  uint32_t noErr;      // искажений не было
  uint32_t detected;   // искажения были и обнаружены
  uint32_t undetected; // искажения были, но не обнаружены
  uint32_t falseAlarm; // искажений не было, а декодер сообщил об ошибке
                       // (признак ошибки в программе!)
  uint32_t bitErrors;  // всего искажённых символов (для контроля p0)
};

static uint32_t fecRngState = 2463534242UL;
static inline void fecSeed(uint32_t s) { fecRngState = s ? s : 2463534242UL; }
static inline uint32_t fecRand() { // xorshift32
  uint32_t x = fecRngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return fecRngState = x;
}

// Вектор ошибок длиной n: каждый символ искажается независимо с вероятностью p
static inline uint32_t fecErrorVector(uint8_t n, uint32_t thr) {
  uint32_t e = 0;
  for (uint8_t i = 0; i < n; i++)
    if (fecRand() < thr)
      e |= (1UL << i);
  return e;
}

static inline uint32_t fecThreshold(double p) {
  if (p <= 0)
    return 0;
  if (p >= 1)
    return 0xFFFFFFFFUL;
  return (uint32_t)(p * 4294967295.0);
}

static inline ExpResult fecExperiment(const CodeParams &c, double p,
                                      uint32_t N) {
  ExpResult r = {0, 0, 0, 0, 0, 0};
  const uint32_t thr = fecThreshold(p);
  const uint32_t msgs = fecMessages(c);
  for (uint32_t t = 0; t < N; t++) {
    uint32_t info = fecRand() % msgs;      // случайное сообщение
    uint32_t sent = fecEncode(c, info);    // кодер
    uint32_t e = fecErrorVector(c.n, thr); // канал
    uint32_t recv = sent ^ e;
    bool ok = fecCheck(c, recv); // декодер
    r.bitErrors += fecWeight(e);
    if (e == 0) {
      if (ok)
        r.noErr++;
      else
        r.falseAlarm++;
    } else {
      if (ok)
        r.undetected++;
      else
        r.detected++;
    }
    if ((t & 0x0FFF) == 0)
      FEC_YIELD();
  }
  r.N = N;
  return r;
}

#endif // FEC_CODES_H

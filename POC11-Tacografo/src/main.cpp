#include <Arduino.h>
#include <driver/gpio.h>

// ---------------------------------------------------------------------------
// Simulador de tacografo - ESP32-WROOM-32D / Heltec WiFi LoRa 32 V2
//
// Gera no GPIO 23 uma onda quadrada (50% duty) cuja frequencia e proporcional
// a velocidade, igual ao sensor de velocidade de um tacografo:
//
//   f [Hz] = velocidade [km/h] * K [pulsos/km] / 3600
//
// Comandos pela serial (115200, terminados por Enter):
//   v <kmh>                 velocidade fixa
//   sim [min max [taxa]]    sobe e desce entre min e max (taxa em km/h por s)
//   stop                    para (velocidade 0, pino em nivel baixo)
//   k <pulsos/km>           constante K do tacografo
//   log [on|off]            liga/desliga o log periodico
//   status                  mostra o estado atual
//   zero                    zera o hodometro
//   help                    lista os comandos
// ---------------------------------------------------------------------------

static const gpio_num_t PIN_PULSO = GPIO_NUM_23;

static const float K_PADRAO = 4000.0f;      // pulsos/km
static const float K_MIN = 100.0f;
static const float K_MAX = 100000.0f;
static const float VEL_MAX = 300.0f;        // km/h
static const float VEL_MIN = 0.05f;         // abaixo disso considera parado
static const uint32_t MEIO_PERIODO_MIN_US = 50;   // limita a saida em 10 kHz
static const uint32_t FATIA_US = 1000;      // passo maximo do timer, para a
                                            // troca de velocidade ser rapida
                                            // mesmo em frequencias baixas

enum Modo { PARADO, FIXO, SIMULANDO };

static hw_timer_t *timer = nullptr;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Compartilhadas com a ISR
static volatile uint32_t meioPeriodoUs = 0;   // 0 = parado
static volatile uint32_t pulsos = 0;

static Modo modo = PARADO;
static float kPulsosKm = K_PADRAO;
static float velocidade = 0.0f;   // km/h atual

static float simMin = 0.0f;
static float simMax = 120.0f;
static float simTaxa = 5.0f;      // km/h por segundo
static bool simSubindo = true;

static bool logAtivo = true;
static uint32_t ultimoPassoMs = 0;
static uint32_t ultimoLogMs = 0;

static char linha[64];
static size_t linhaLen = 0;

// O alarme e reprogramado a cada interrupcao. Periodos longos sao divididos em
// fatias de ate FATIA_US para que uma nova velocidade entre em vigor logo.
void IRAM_ATTR onTimer() {
  static uint32_t decorrido = 0;
  static uint32_t alarmeAtual = FATIA_US;
  static bool nivel = false;

  uint32_t mp = meioPeriodoUs;
  uint32_t proximo;

  decorrido += alarmeAtual;

  if (mp == 0) {
    nivel = false;
    gpio_set_level(PIN_PULSO, 0);
    decorrido = 0;
    proximo = FATIA_US;
  } else {
    if (decorrido >= mp) {
      nivel = !nivel;
      gpio_set_level(PIN_PULSO, nivel);
      if (nivel) {
        pulsos++;
      }
      decorrido = 0;
    }
    uint32_t resta = mp - decorrido;
    // Nunca deixa sobrar uma fatia curta demais para o timer alcancar
    proximo = (resta > 2 * FATIA_US) ? FATIA_US : resta;
  }

  alarmeAtual = proximo;
  timerAlarmWrite(timer, proximo, true);
}

static float frequenciaHz(float kmh) {
  return kmh * kPulsosKm / 3600.0f;
}

static float velocidadeMaxima() {
  // Maior velocidade que cabe no limite de frequencia para o K atual
  float porFrequencia = 1.8e9f / (kPulsosKm * MEIO_PERIODO_MIN_US);
  return min(VEL_MAX, porFrequencia);
}

static void aplicaVelocidade(float kmh) {
  kmh = constrain(kmh, 0.0f, velocidadeMaxima());
  velocidade = kmh;

  uint32_t mp = 0;
  if (kmh >= VEL_MIN) {
    // meio periodo [us] = 1e6 / (2 * f) = 1.8e9 / (v * K)
    mp = (uint32_t)(1.8e9f / (kmh * kPulsosKm) + 0.5f);
    if (mp < MEIO_PERIODO_MIN_US) {
      mp = MEIO_PERIODO_MIN_US;
    }
  }

  portENTER_CRITICAL(&mux);
  meioPeriodoUs = mp;
  portEXIT_CRITICAL(&mux);
}

static const char *nomeModo() {
  switch (modo) {
    case FIXO:      return "FIXO";
    case SIMULANDO: return "SIMULANDO";
    default:        return "PARADO";
  }
}

static void imprimeStatus() {
  portENTER_CRITICAL(&mux);
  uint32_t p = pulsos;
  portEXIT_CRITICAL(&mux);

  Serial.printf("[%s] %.1f km/h | %.1f Hz | K=%.0f | %lu pulsos | %.3f km\n",
                nomeModo(), velocidade, frequenciaHz(velocidade), kPulsosKm,
                (unsigned long)p, p / kPulsosKm);
}

static void imprimeAjuda() {
  Serial.println();
  Serial.println("=== Simulador de tacografo (GPIO 23) ===");
  Serial.println("  v <kmh>               velocidade fixa          ex: v 80");
  Serial.println("  sim [min max [taxa]]  sobe e desce sem parar   ex: sim 0 120 5");
  Serial.println("                        (taxa em km/h por segundo)");
  Serial.println("  stop                  para a simulacao (0 km/h)");
  Serial.println("  k <pulsos/km>         constante K              ex: k 4000");
  Serial.println("  log [on|off]          log periodico a cada 1 s");
  Serial.println("  status                estado atual");
  Serial.println("  zero                  zera o hodometro");
  Serial.println("  help                  esta ajuda");
  Serial.println();
}

static void comandoVelocidade(char *arg) {
  if (!arg) {
    Serial.println("Uso: v <kmh>");
    return;
  }
  float v = atof(arg);
  if (v < 0.0f) {
    Serial.println("Velocidade invalida");
    return;
  }
  if (v > velocidadeMaxima()) {
    Serial.printf("Limitado a %.1f km/h\n", velocidadeMaxima());
  }
  modo = (v >= VEL_MIN) ? FIXO : PARADO;
  aplicaVelocidade(v);
  imprimeStatus();
}

static void comandoSimular(char *aMin, char *aMax, char *aTaxa) {
  float mn = simMin, mx = simMax, taxa = simTaxa;

  if (aMin && aMax) {
    mn = atof(aMin);
    mx = atof(aMax);
    if (aTaxa) {
      taxa = atof(aTaxa);
    }
  } else if (aMin) {
    Serial.println("Uso: sim [min max [taxa]]");
    return;
  }

  mx = min(mx, velocidadeMaxima());
  if (mn < 0.0f || mx <= mn || taxa <= 0.0f) {
    Serial.println("Parametros invalidos (precisa 0 <= min < max e taxa > 0)");
    return;
  }

  simMin = mn;
  simMax = mx;
  simTaxa = taxa;

  // Parte da velocidade atual, sem salto
  simSubindo = velocidade < simMax;
  modo = SIMULANDO;
  ultimoPassoMs = millis();
  Serial.printf("Simulando %.1f <-> %.1f km/h a %.1f km/h/s\n", simMin, simMax, simTaxa);
}

static void comandoK(char *arg) {
  if (!arg) {
    Serial.printf("K = %.0f pulsos/km\n", kPulsosKm);
    return;
  }
  float k = atof(arg);
  if (k < K_MIN || k > K_MAX) {
    Serial.printf("K deve estar entre %.0f e %.0f\n", K_MIN, K_MAX);
    return;
  }
  kPulsosKm = k;
  simMax = min(simMax, velocidadeMaxima());
  aplicaVelocidade(velocidade);   // recalcula a frequencia com o novo K
  imprimeStatus();
}

static void processaLinha(char *txt) {
  char *cmd = strtok(txt, " \t");
  if (!cmd) {
    return;
  }
  char *a1 = strtok(nullptr, " \t");
  char *a2 = strtok(nullptr, " \t");
  char *a3 = strtok(nullptr, " \t");

  for (char *c = cmd; *c; c++) {
    *c = tolower(*c);
  }

  if (!strcmp(cmd, "v") || !strcmp(cmd, "vel") || !strcmp(cmd, "set")) {
    comandoVelocidade(a1);
  } else if (!strcmp(cmd, "sim")) {
    comandoSimular(a1, a2, a3);
  } else if (!strcmp(cmd, "stop") || !strcmp(cmd, "p") || !strcmp(cmd, "parar")) {
    modo = PARADO;
    aplicaVelocidade(0.0f);
    imprimeStatus();
  } else if (!strcmp(cmd, "k")) {
    comandoK(a1);
  } else if (!strcmp(cmd, "log")) {
    if (a1) {
      logAtivo = !strcasecmp(a1, "on") || !strcmp(a1, "1");
    } else {
      logAtivo = !logAtivo;
    }
    Serial.printf("Log %s\n", logAtivo ? "ligado" : "desligado");
  } else if (!strcmp(cmd, "status") || !strcmp(cmd, "s")) {
    imprimeStatus();
  } else if (!strcmp(cmd, "zero")) {
    portENTER_CRITICAL(&mux);
    pulsos = 0;
    portEXIT_CRITICAL(&mux);
    Serial.println("Hodometro zerado");
  } else if (!strcmp(cmd, "help") || !strcmp(cmd, "h") || !strcmp(cmd, "?")) {
    imprimeAjuda();
  } else {
    Serial.printf("Comando desconhecido: %s (digite help)\n", cmd);
  }
}

static void leSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (linhaLen > 0) {
        linha[linhaLen] = '\0';
        processaLinha(linha);
        linhaLen = 0;
      }
    } else if (linhaLen < sizeof(linha) - 1) {
      linha[linhaLen++] = c;
    }
  }
}

static void passoSimulacao() {
  uint32_t agora = millis();
  uint32_t dt = agora - ultimoPassoMs;
  if (dt < 20) {
    return;
  }
  ultimoPassoMs = agora;

  float v = velocidade;
  float delta = simTaxa * dt / 1000.0f;

  if (simSubindo) {
    v += delta;
    if (v >= simMax) {
      v = simMax;
      simSubindo = false;
    }
  } else {
    v -= delta;
    if (v <= simMin) {
      v = simMin;
      simSubindo = true;
    }
  }
  aplicaVelocidade(v);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_PULSO, OUTPUT);
  digitalWrite(PIN_PULSO, LOW);

  // Timer 0 com prescaler 80 -> 1 tick = 1 us
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, FATIA_US, true);
  timerAlarmEnable(timer);

  imprimeAjuda();
  imprimeStatus();
}

void loop() {
  leSerial();

  if (modo == SIMULANDO) {
    passoSimulacao();
  }

  if (logAtivo && modo != PARADO && millis() - ultimoLogMs >= 1000) {
    ultimoLogMs = millis();
    imprimeStatus();
  }
}

/*
 * CC1101 RF Signal Sender — ESP32-C3 Super Mini
 *
 * Wiring (per project wiring diagram):
 *   CC1101 VCC -> 3.3V
 *   CC1101 GND -> GND
 *   CC1101 CE  -> GPIO20   (GDO0, also used as ASK/OOK bit-bang pin)
 *   CC1101 CNS -> GPIO21   (CSN / SPI chip select)
 *   CC1101 SCK -> GPIO4
 *   CC1101 MOSI-> GPIO6
 *   CC1101 MISO-> GPIO5
 *
 * Library: ELECHOUSE_CC1101_SRC_DRV
 *   Arduino IDE -> Library Manager -> search "SmartRC-CC1101-Driver-Lib"
 *   (https://github.com/LSatan/SmartRC-CC1101-Driver-Lib)
 *
 * Serial command interface (115200 baud):
 *   FREQ <mhz>            set carrier frequency, e.g. FREQ 433.92
 *   MOD OOK|FSK           set modulation
 *   RATE <kbps>           set data rate, e.g. RATE 4.8
 *   SEND <hex> [repeats]  transmit a hex byte payload, e.g. SEND AABBCC 5
 *   RAW <us,us,us,...> [repeats]
 *                         bit-bang a raw OOK pulse train (durations in
 *                         microseconds, alternating ON/OFF starting with ON)
 *                         e.g. RAW 320,320,640,320 3
 *   NOISE <mhz> <duration_ms> [min_us] [max_us]
 *                         contained interference test: emits randomized
 *                         OOK noise at <mhz> for <duration_ms>. ONLY run
 *                         this with the device under test inside a shielded
 *                         (Faraday) enclosure with the antenna contained -
 *                         this is a broadband disruptor and must never be
 *                         operated over the air. e.g. NOISE 433.92 2000
 *   STATUS                print current config
 */

#include <ELECHOUSE_CC1101_SRC_DRV.h>

#define PIN_GDO0 20
#define PIN_CSN  21
#define PIN_SCK  4
#define PIN_MOSI 6
#define PIN_MISO 5

float currentFreqMHz = 433.92;
bool currentModOOK = true;
float currentRateKbps = 4.8;

void applyRadioConfig() {
  ELECHOUSE_CC1101.setModulation(currentModOOK ? 2 : 0); // 2 = ASK/OOK, 0 = 2-FSK
  ELECHOUSE_CC1101.setMHZ(currentFreqMHz);
  ELECHOUSE_CC1101.setDRate(currentRateKbps);
  ELECHOUSE_CC1101.SetTx();
}

void printStatus() {
  Serial.printf("freq=%.3fMHz mod=%s rate=%.2fkbps\n",
                currentFreqMHz, currentModOOK ? "OOK" : "FSK", currentRateKbps);
}

void sendHexPayload(const String &hex, int repeats) {
  int len = hex.length() / 2;
  if (len <= 0 || len > 61) {
    Serial.println("ERR payload must be 1-61 bytes of hex");
    return;
  }
  uint8_t buf[61];
  for (int i = 0; i < len; i++) {
    buf[i] = (uint8_t)strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
  }

  ELECHOUSE_CC1101.SetTx();
  for (int r = 0; r < repeats; r++) {
    ELECHOUSE_CC1101.SendData(buf, len);
    delay(10);
  }
  Serial.printf("OK sent %d bytes x%d\n", len, repeats);
}

void sendRawPulses(const String &csv, int repeats) {
  const int MAX_PULSES = 256;
  uint32_t pulses[MAX_PULSES];
  int count = 0;

  int start = 0;
  while (start < (int)csv.length() && count < MAX_PULSES) {
    int comma = csv.indexOf(',', start);
    String token = (comma == -1) ? csv.substring(start) : csv.substring(start, comma);
    token.trim();
    if (token.length() > 0) {
      pulses[count++] = (uint32_t)token.toInt();
    }
    if (comma == -1) break;
    start = comma + 1;
  }

  if (count == 0) {
    Serial.println("ERR no pulses parsed");
    return;
  }

  // Put the radio in TX with carrier held by GDO0 bit-banging (OOK).
  ELECHOUSE_CC1101.SetTx();
  pinMode(PIN_GDO0, OUTPUT);

  for (int r = 0; r < repeats; r++) {
    bool level = HIGH; // start ON
    for (int i = 0; i < count; i++) {
      digitalWrite(PIN_GDO0, level);
      delayMicroseconds(pulses[i]);
      level = !level;
    }
    digitalWrite(PIN_GDO0, LOW);
    delay(5);
  }

  Serial.printf("OK raw sent %d pulses x%d\n", count, repeats);
}

void sendNoise(float mhz, uint32_t durationMs, uint32_t minUs, uint32_t maxUs) {
  if (minUs < 20) minUs = 20;
  if (maxUs < minUs) maxUs = minUs;

  float savedFreq = currentFreqMHz;
  currentFreqMHz = mhz;
  currentModOOK = true;
  applyRadioConfig();

  pinMode(PIN_GDO0, OUTPUT);
  uint32_t end = millis() + durationMs;
  bool level = HIGH;
  while (millis() < end) {
    digitalWrite(PIN_GDO0, level);
    uint32_t pulse = minUs + (esp_random() % (maxUs - minUs + 1));
    delayMicroseconds(pulse);
    level = !level;
  }
  digitalWrite(PIN_GDO0, LOW);

  currentFreqMHz = savedFreq;
  applyRadioConfig();

  Serial.printf("OK noise burst done: %.3fMHz for %lums\n", mhz, (unsigned long)durationMs);
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  int sp = line.indexOf(' ');
  String cmd = (sp == -1) ? line : line.substring(0, sp);
  String rest = (sp == -1) ? "" : line.substring(sp + 1);
  cmd.toUpperCase();

  if (cmd == "FREQ") {
    currentFreqMHz = rest.toFloat();
    applyRadioConfig();
    printStatus();
  } else if (cmd == "MOD") {
    rest.trim();
    rest.toUpperCase();
    currentModOOK = (rest == "OOK");
    applyRadioConfig();
    printStatus();
  } else if (cmd == "RATE") {
    currentRateKbps = rest.toFloat();
    applyRadioConfig();
    printStatus();
  } else if (cmd == "SEND") {
    int sp2 = rest.indexOf(' ');
    String hex = (sp2 == -1) ? rest : rest.substring(0, sp2);
    int repeats = (sp2 == -1) ? 1 : rest.substring(sp2 + 1).toInt();
    if (repeats < 1) repeats = 1;
    sendHexPayload(hex, repeats);
  } else if (cmd == "RAW") {
    int sp2 = rest.lastIndexOf(' ');
    String csv = rest;
    int repeats = 1;
    if (sp2 != -1) {
      String maybeRepeats = rest.substring(sp2 + 1);
      bool isNumber = maybeRepeats.length() > 0;
      for (size_t i = 0; i < maybeRepeats.length() && isNumber; i++) {
        if (!isDigit(maybeRepeats[i])) isNumber = false;
      }
      if (isNumber) {
        csv = rest.substring(0, sp2);
        repeats = maybeRepeats.toInt();
      }
    }
    if (repeats < 1) repeats = 1;
    sendRawPulses(csv, repeats);
  } else if (cmd == "NOISE") {
    float mhz = 433.92;
    uint32_t durationMs = 1000;
    uint32_t minUs = 50;
    uint32_t maxUs = 500;

    int n = 0;
    int idx = 0;
    String parts[4];
    while (idx < (int)rest.length() && n < 4) {
      int next = rest.indexOf(' ', idx);
      parts[n++] = (next == -1) ? rest.substring(idx) : rest.substring(idx, next);
      if (next == -1) break;
      idx = next + 1;
    }
    if (n >= 1) mhz = parts[0].toFloat();
    if (n >= 2) durationMs = (uint32_t)parts[1].toInt();
    if (n >= 3) minUs = (uint32_t)parts[2].toInt();
    if (n >= 4) maxUs = (uint32_t)parts[3].toInt();

    if (mhz <= 0 || durationMs == 0) {
      Serial.println("ERR usage: NOISE <mhz> <duration_ms> [min_us] [max_us]");
    } else {
      Serial.println("WARN: ensure device under test is fully enclosed in a shielded box before transmitting");
      sendNoise(mhz, durationMs, minUs, maxUs);
    }
  } else if (cmd == "STATUS") {
    printStatus();
  } else {
    Serial.println("ERR unknown command");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  ELECHOUSE_CC1101.addSpiPin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN, 0);
  ELECHOUSE_CC1101.setGDO(PIN_GDO0, PIN_GDO0);

  if (ELECHOUSE_CC1101.getCC1101()) {
    Serial.println("CC1101 connection OK");
  } else {
    Serial.println("CC1101 connection FAILED - check wiring");
  }

  ELECHOUSE_CC1101.Init();
  applyRadioConfig();

  Serial.println("CC1101 RF Sender ready. Commands: FREQ MOD RATE SEND RAW NOISE STATUS");
  printStatus();
}

void loop() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(line);
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}

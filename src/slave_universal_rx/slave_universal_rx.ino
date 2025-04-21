// Маркеры начала фрейма
const uint8_t STX = 0x02;
const uint8_t ETX = 0x03;

// Произвольный адрес ардуины
const uint8_t SLAVE_ADDR = 0x01;

// Функции
const uint8_t FUNC_TASK = 0x99;
const uint8_t FUNC_STATUS = 0x88;
const uint8_t FUNC_COMPLETE = 0x77;

// Таски
const uint8_t TASK_RESPOND_STATUS = 0x00;
const uint8_t TASK_LED_ON = 0x01;
const uint8_t TASK_LED_OFF = 0x02;

// Номер домов / светодиодов
const uint8_t NUM_HOMES = 2;
const uint8_t NUM_LEDS  = 2;

// Маппинг домов в формате [ДОМ][LED]
const uint8_t ledPins[NUM_HOMES][NUM_LEDS] = {
  {3, 4},  // Home1: LED1=pin3, LED2=pin4
  {5, 6}   // Home2: LED1=pin5, LED2=pin6
};

// Буферы под фреймы
volatile uint8_t rxBuffer[8];
volatile uint8_t txBuffer[8];
volatile uint8_t rxIndex = 0;
volatile bool packetReady = false;

// Функция вычисления контрольной суммы
uint8_t calcLRC(const uint8_t* data, uint8_t len) {
  uint8_t lrc = 0;
  for (uint8_t i = 0; i < len; ++i) lrc += data[i];
  return (~lrc) + 1;
}

// Прерывание SPI
ISR(SPI_STC_vect) {
  uint8_t b = SPDR;
  SPDR = txBuffer[rxIndex];
  if (!packetReady) {
    rxBuffer[rxIndex++] = b;
    if (rxIndex >= sizeof(rxBuffer)) packetReady = true;
  }
}

// Построение кадра
uint8_t buildFrame(uint8_t* buf, uint8_t func, uint8_t task, uint8_t hnum, uint8_t lnum) {
  buf[0] = STX;
  buf[1] = SLAVE_ADDR;
  buf[2] = func;
  buf[3] = task;
  buf[4] = hnum;
  buf[5] = lnum;
  buf[6] = calcLRC(buf + 1, 5);
  buf[7] = ETX;
  return 8;
}

// Изначальная установка пинов в режим вывода 
void setup() {
  for (uint8_t h = 0; h < NUM_HOMES; ++h)
    for (uint8_t l = 0; l < NUM_LEDS; ++l)
      pinMode(ledPins[h][l], OUTPUT), digitalWrite(ledPins[h][l], LOW);

  pinMode(MISO, OUTPUT);
  SPCR |= _BV(SPE) | _BV(SPIE);
  SPDR = 0;
}

void loop() {
  if (!packetReady) return;

  noInterrupts();
  uint8_t stx    = rxBuffer[0];
  uint8_t addr   = rxBuffer[1];
  uint8_t func   = rxBuffer[2]; // Получаем данные в регистр, переписываем их в буфер, разделяем на переменные
  uint8_t task   = rxBuffer[3];
  uint8_t hnum   = rxBuffer[4];
  uint8_t lnum   = rxBuffer[5];
  uint8_t lrcRx  = rxBuffer[6];
  uint8_t etx    = rxBuffer[7];
  packetReady    = false;
  rxIndex        = 0;
  interrupts();

  if (stx!=STX||etx!=ETX||addr!=SLAVE_ADDR||calcLRC(rxBuffer+1,5)!=lrcRx) return; // Проверяем фрейм на валидность
  if (hnum<1||hnum>NUM_HOMES||lnum<1||lnum>NUM_LEDS) return;

  uint8_t pin = ledPins[hnum-1][lnum-1]; // Маппим полученное значение из кадра в переменную
  if (func==FUNC_TASK) {
    if (task==TASK_LED_ON)      digitalWrite(pin, HIGH); // Если полученный таск - включение светодиода, включаем его 
    else if (task==TASK_LED_OFF) digitalWrite(pin, LOW);
    buildFrame(txBuffer, FUNC_COMPLETE, task, hnum, lnum);
    SPDR = txBuffer[0];
  }
  else if (func==FUNC_STATUS) { // В противном случае - отправляем текущий статус пина
    uint8_t state = digitalRead(pin);
    buildFrame(txBuffer, FUNC_COMPLETE, TASK_RESPOND_STATUS, hnum, state);
    SPDR = txBuffer[0];
  }
}

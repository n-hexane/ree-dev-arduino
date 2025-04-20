// Начало/конец кадра
const uint8_t STX = 0x02;  // Начало фрейма 
const uint8_t ETX = 0x03;  // Конец фрейма

// Адрес ведомой ардуины
const uint8_t SLAVE_ADDR = 0x01;

// Коды/айди функций 
const uint8_t FUNC_TASK = 0x99;  // Входящая функция - таска
const uint8_t FUNC_STATUS = 0x88;  // Входящая функция - статус
const uint8_t FUNC_COMPLETE = 0x77;  // Исходящая функция - ответ выполненной функци

// Входящие айди тасков
const uint8_t TASK_RESPOND_STATUS = 0x00;
const uint8_t TASK_LED_ON = 0x01;
const uint8_t TASK_LED_OFF = 0x02;

// Номер дома/светодиода
const uint8_t H1_NUM = 0x01;
const uint8_t H1_LED1_NUM = 0x01;

// Номер пина
const uint8_t H1_LED1_PIN = 3;

// Функция вычисления контрольной суммы
uint8_t calcLRC(const uint8_t* data, uint8_t len) {
  uint8_t lrc = 0;
  for (uint8_t i = 0; i < len; i++) lrc += data[i];
  return (~lrc) + 1;
}

// Буферы прерываний SPI + проверка на целостность получения данных
volatile uint8_t rxBuffer[8];  // [STX][ADDR][FUNC][TASK][HNUM][LNUM][LRC][ETX]
volatile uint8_t txBuffer[8];  // Одинаковые буферы на прием/ответ
volatile uint8_t rxIndex = 0;
volatile bool packetReady = false;

// Само прерывание SPI
ISR(SPI_STC_vect) {
  uint8_t received = SPDR; 
  SPDR = txBuffer[rxIndex];
  if (!packetReady) {
    rxBuffer[rxIndex++] = received;
    if (rxIndex >= sizeof(rxBuffer)) packetReady = true;
  }
}

// Построение фрейма
uint8_t buildFrame(uint8_t* buf, uint8_t func, uint8_t task, uint8_t hnum, uint8_t lnum) {
  buf[0] = STX;
  buf[1] = SLAVE_ADDR;
  buf[2] = func;
  buf[3] = task;
  buf[4] = hnum;
  buf[5] = lnum;
  uint8_t lrc = calcLRC(buf + 1, 5);
  buf[6] = lrc;
  buf[7] = ETX;
  return 8;
}

// Первоначальная настройка
void setup() {
  pinMode(H1_LED1_PIN, OUTPUT);
  digitalWrite(H1_LED1_PIN, LOW);

  pinMode(MISO, OUTPUT);
  SPCR |= _BV(SPE) | _BV(SPIE);
  SPDR = 0x00;
}

void loop() {
  if (!packetReady) return; // Пока не получен весь фрейм - крутимся в цикле

  noInterrupts(); // Отключаем прерывания для записи кадра
  uint8_t stx = rxBuffer[0];
  uint8_t addr = rxBuffer[1];
  uint8_t func = rxBuffer[2];
  uint8_t task = rxBuffer[3];
  uint8_t hnum = rxBuffer[4];
  uint8_t lnum = rxBuffer[5];
  uint8_t lrcRx = rxBuffer[6];
  uint8_t etx = rxBuffer[7];
  packetReady = false;
  rxIndex = 0;
  interrupts(); 

  // Проверяем фрейм на корректность
  if (stx != STX || etx != ETX || addr != SLAVE_ADDR || calcLRC(rxBuffer + 1, 5) != lrcRx) {
    return;  // invalid frame
  }

  // Выполняем таску, если есть такая функция
  if (func == FUNC_TASK) {
    switch (task) {
      case TASK_LED_ON: // Включаем светодиод
        digitalWrite(H1_LED1_PIN, HIGH);
        break;
      case TASK_LED_OFF:
        digitalWrite(H1_LED1_PIN, LOW);
        break;
      default:
        break;
    }
    // После выполнения таски отправляем фрейм обратно
    buildFrame(txBuffer, FUNC_COMPLETE, task, hnum, lnum);
    SPDR = txBuffer[0];
  }
  else if (func == FUNC_STATUS) {
    // Если функция - проверка статуса текущего светодиода - отправляем его статус обратно
    uint8_t ledState = digitalRead(H1_LED1_PIN);
    buildFrame(txBuffer, FUNC_COMPLETE, TASK_RESPOND_STATUS, hnum, ledState);
    SPDR = txBuffer[0];
  }
}

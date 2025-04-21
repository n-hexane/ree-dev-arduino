#include <SPI.h>

// Маркеры начала кадра и адрес ведомой ардуины
const uint8_t STX = 0x02, ETX = 0x03;
const uint8_t SLAVE_ADDR = 0x01;

// Функции 
const uint8_t FUNC_TASK = 0x99;
const uint8_t FUNC_STATUS = 0x88;
const uint8_t FUNC_COMPLETE = 0x77;

// Таски
const uint8_t TASK_RESPOND_STATUS = 0x00;
const uint8_t TASK_LED_ON = 0x01;
const uint8_t TASK_LED_OFF = 0x02;

// Буферы под фрейм
uint8_t txBuf[8], rxBuf[8];

// Функция вычисления контрольной суммы
uint8_t calcLRC(const uint8_t* data, uint8_t len) {
  uint8_t lrc = 0;
  for (uint8_t i = 0; i < len; ++i) lrc += data[i];
  return (~lrc) + 1;
}

uint8_t buildFrame(uint8_t* buf, uint8_t func, uint8_t task, uint8_t hnum, uint8_t lnum) {
  buf[0] = STX;
  buf[1] = SLAVE_ADDR;
  buf[2] = func;
  buf[3] = task;
  buf[4] = hnum;
  buf[5] = lnum;
  buf[6] = calcLRC(buf+1,5);
  buf[7] = ETX;
  return 8;
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  pinMode(SS, OUTPUT);
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    // Формат команды: Hx_LEDy ON/OFF; Пример: H1_LED1 ON
    int h = cmd.charAt(1) - '0';
    int l = cmd.charAt(cmd.indexOf("LED")+3) - '0';
    bool on = cmd.endsWith("ON");
    uint8_t task = on ? TASK_LED_ON : TASK_LED_OFF;
    
    buildFrame(txBuf, FUNC_TASK, task, h, l);
    digitalWrite(SS, LOW);
    for (uint8_t i = 0; i < 8; ++i) rxBuf[i] = SPI.transfer(txBuf[i]);
    digitalWrite(SS, HIGH);

    // Проверяем ответ
    if (rxBuf[0]==STX && rxBuf[7]==ETX && rxBuf[2]==FUNC_COMPLETE &&
        calcLRC(rxBuf+1,5)==rxBuf[6]) {
      Serial.println("Task completed");
    } else Serial.println("No response or invalid frame");
  }
}

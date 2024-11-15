#include <U8g2lib.h>
#include <DHT.h>
#include <Servo.h>

// Пины и типы
#define DHTPIN 7               // Пин для DHT11
#define DHTTYPE DHT11          // Тип датчика DHT
#define SOIL_MOISTURE_PIN A1   // Пин для датчика влажности почвы
#define TRIG_PIN 9             // Пин Trig для HC-SR04
#define ECHO_PIN 8             // Пин Echo для HC-SR04
#define BUZZER_PIN 4           // Пин для буззера
#define SERVO_PIN 3            // Пин для серво SG90
#define GAS_PIN A0             // Пин для газового датчика MQ-4   
#define PT100_PIN A5           // Пин для pt100

// Пины для светодиодов
#define GREEN_LED1_PIN A2      // Пин для первого зеленого светодиода
#define YELLOW_LED_PIN A3     // Пин для желтого светодиода
#define RED_LED1_PIN A4        // Пин для первого красного светодиода
#define RED_LED2_PIN 5        // Пин для второго красного светодиода (ALARM)
#define GREEN_LED2_PIN 6      // Пин для второго зеленого светодиода

const float knownResistor = 100.0; // Известное сопротивление резистора в Ом
const float referenceVoltage = 5.0; // Опорное напряжение (5V для Arduino)

// Инициализация дисплея
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 10, /* reset=*/ 6);

// Инициализация DHT11 и серво
DHT dht(DHTPIN, DHTTYPE);
Servo servo;

bool alarmTriggered = false; // Сигнал тревоги
unsigned long alarmTime = 0; // Время начала тревоги

void setup() {
  // Пины для HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Пины для светодиодов
  pinMode(GREEN_LED1_PIN, OUTPUT); // Первый зеленый
  pinMode(YELLOW_LED_PIN, OUTPUT); // Желтый
  pinMode(RED_LED1_PIN, OUTPUT);   // Первый красный
  pinMode(RED_LED2_PIN, OUTPUT);   // Второй красный (ALARM)
  pinMode(GREEN_LED2_PIN, OUTPUT); // Второй зеленый
  
  // Инициализация серийного порта и компонентов
  Serial.begin(9600);
  dht.begin();
  u8g2.begin();
  servo.attach(SERVO_PIN);
  pinMode(BUZZER_PIN, OUTPUT); // Настройка буззера как выхода
  
  // Задержка для стабилизации
  delay(2000);
}

void loop() {
  // Чтение данных с датчиков
  int rawValue = analogRead(PT100_PIN);
  float voltage = (rawValue / 1023.0) * referenceVoltage;

  // Вычисляем сопротивление PT100
  float resistance = (knownResistor * voltage) / (referenceVoltage - voltage);
  float tempPT100 = (resistance - 100.0) / 0.385;

  // Чтение данных с HC-SR04 (расстояние)
  long duration, distance;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;

  // Чтение данных с DHT11 (температура и влажность)
  float humidity = dht.readHumidity();
  float tempDHT11 = dht.readTemperature();

  // Разделение температуры на целую и десятичную части
  int tempDHT11_int = int(tempDHT11);          // Целая часть
  int tempDHT11_dec = (tempDHT11 - tempDHT11_int) * 10;  // Десятичная часть (умножаем на 10 для получения одного знака после запятой)

  // Чтение данных с датчика газа (метан)
  int gasValue = analogRead(GAS_PIN);
  float methaneLevel = gasValue * (5.0 / 1023.0);  // Преобразование в ppm (приближенно)

  // Управление светодиодами в зависимости от уровня заполненности
  if (distance <= 6.25) {  // 0% (0-6.25 см)
    digitalWrite(GREEN_LED1_PIN, HIGH);  // Включаем первый зеленый
  } else {
    digitalWrite(GREEN_LED1_PIN, LOW);  // Выключаем первый зеленый
  }

  if (distance > 6.25 && distance <= 18.75) {  // 25%-75% (6.25 см - 18.75 см)
    digitalWrite(YELLOW_LED_PIN, HIGH);  // Включаем желтый
  } else {
    digitalWrite(YELLOW_LED_PIN, LOW);  // Выключаем желтый
  }

  if (distance > 18.75) {  // 75%-100% (18.75 см - 25 см)
    digitalWrite(RED_LED1_PIN, HIGH);  // Включаем первый красный
  } else {
    digitalWrite(RED_LED1_PIN, LOW);  // Выключаем первый красный
  }

  // Управление светодиодом ALARM
  if (tempPT100 < 25.0 || tempPT100 > 55.0 || humidity < 50.0 || methaneLevel > 500.0) {
    if (!alarmTriggered) {  // Только если тревога ещё не была активирована
      alarmTriggered = true;
      alarmTime = millis(); // Сохраняем время начала тревоги
      digitalWrite(RED_LED2_PIN, HIGH);  // Включаем светодиод ALARM
      tone(BUZZER_PIN, 1000);  // Включаем буззер
    }
  } else {
    if (alarmTriggered) {
      alarmTriggered = false;
      digitalWrite(RED_LED2_PIN, LOW);   // Выключаем светодиод ALARM
      noTone(BUZZER_PIN);  // Выключаем буззер
    }
  }

  // Управление зеленым светодиодом 2 (нормальная работа)
  if (!alarmTriggered) {
    digitalWrite(GREEN_LED2_PIN, HIGH);  // Включаем второй зеленый
  } else {
    digitalWrite(GREEN_LED2_PIN, LOW);  // Выключаем второй зеленый
  }

  // Показываем сообщение на дисплее в случае тревоги:
  if (alarmTriggered) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_6x12_tf);
      if (tempPT100 < 25.0) {
        u8g2.drawStr(0, 10, "LOW TEMPERATURE");
      } else if (tempPT100 > 55.0) {
        u8g2.drawStr(0, 10, "HIGH TEMPERATURE");
      } else if (humidity < 50.0) {
        u8g2.drawStr(0, 10, "LOW HUMIDITY");
      } else if (methaneLevel > 500.0) {
        u8g2.drawStr(0, 10, "HIGH METHANE LEVEL");
      }
    } while (u8g2.nextPage());

    // Выключаем сообщение через 2 секунды
    if (millis() - alarmTime > 2000) {
      alarmTriggered = false;
      digitalWrite(RED_LED2_PIN, LOW); // Выключаем ALARM
      noTone(BUZZER_PIN);           // Выключаем буззер
    }
  }

  // Отображаем данные на экране, если нет тревоги
  if (!alarmTriggered) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_5x7_tf); // Меньший шрифт

      // Отображаем температуру с PT100 (внутреняя температура)
      char tempPT100Str[16];
      sprintf(tempPT100Str, "Temp.in: %.1f C", tempPT100);
      u8g2.drawStr(0, 10, tempPT100Str);

      // Отображаем температуру с DHT11 (внешняя температура)
      char tempDHT11Str[16];
      sprintf(tempDHT11Str, "Temp.out: %d.%d C", tempDHT11_int, tempDHT11_dec); // Выводим целую и десятичную часть
      u8g2.drawStr(0, 20, tempDHT11Str);
    } while (u8g2.nextPage());
  }
}


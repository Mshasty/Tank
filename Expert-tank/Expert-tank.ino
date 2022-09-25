/***************************************************
* Laboratory Arduino-tank. Version 1.1  (13.03.18) *
* creator: Owintsowsky Maxim            6 064 (88) *
* https://vk.com/automation4house  VK group        *
* https://t.me/aquatimer Telegram chat             *
****************************************************/

#include <Servo.h>
#include <MX1508.h>
#include "pitches.h"

//#define TEST_MODE // State for test mode

#define TriggerPin A0 // Триггер дальномера на D07
#define EchoPin A1 // Датчик эха дальномера на D02
#define LedPin LED_BUILTIN // Стоп-сигнал на D13 (для UNO)
#define ServoPin 8 // Серво на D08

// The PWM pins are: D03, D05, D06, D09, D10, and D11.
// При работе с серво на D08, выводы D09 и D10 не работают как PWM
#define motorRightForward 3 // Правый двигатель
#define motorRightBackward 5
#define motorLeftForward 11 // Левый двигатель
#define motorLeftBackward 6
#define speaker 7 // Бипер пин

const int Danger = 25; // критическое расстояние до препятствия в см
const unsigned int Rotate45 = 300; // время поворота на 45 градусов в мсек
const unsigned long RunStopTime = 30000; // Общее время работы в мсек
const unsigned long StartTime = 1000; // Время разгона до полной скорости в мсек
const unsigned long StopTime = 250; // Время остановки остановки в мсек
const unsigned long RotateTime = 150; // Время разгона-остановки в развороте < Rotate45
const unsigned long BackTime = 1000; // Время отъезда перед разворотом в мсек
const uint8_t MaxSpd = 180; // Максимальная скорость (0­255)
const uint8_t MinSpd = 45; // Нулевая скорость (0­MaxSpd)
const uint8_t DownSpd = 30; // Снижение скорости при манёврах в процентах
const uint8_t ServoCenter = 84; // Середина центровки серво

Servo myservo;

unsigned int leftDist;
unsigned int rightDist;
unsigned int distanceFwd;
unsigned int tempDist;
unsigned int angleLeft;
unsigned int angleRight;

unsigned long runStartTime = 0; // Стартовое время
unsigned long currentMillis; // Время после старта
uint8_t currentSpeed = 0; // Текущая скорость
uint8_t manevrSpd = 0; // Скорость в маневре
int pwmR = 0; // Текущая скорость правого мотора
int pwmL = 0; // Текущая скорость левого мотора
int needR = 0; // Требуемая скорость правого мотора
int needL = 0; // Требуемая скорость левого мотора
boolean itRun = false; // Полный ход?

MX1508 motorR(motorRightForward,motorRightBackward, FAST_DECAY, 2);
MX1508 motorL(motorLeftForward,motorLeftBackward, FAST_DECAY, 2);

void setup() {
  pinMode(TriggerPin, OUTPUT); //инициируем как выход 
  pinMode(EchoPin, INPUT); //инициируем как вход 
  pinMode(LedPin, OUTPUT); 
  pinMode(motorRightForward, OUTPUT);
  pinMode(motorRightBackward, OUTPUT);
  pinMode(motorLeftForward, OUTPUT);
  pinMode(motorLeftBackward, OUTPUT);
  pinMode(speaker, OUTPUT);
  digitalWrite(speaker, HIGH);
  analogWrite(motorLeftForward,0);
  analogWrite(motorLeftBackward,0);      
  analogWrite(motorRightForward,0);
  analogWrite(motorRightBackward,0);      
  myservo.attach(ServoPin);
  digitalWrite(LedPin, HIGH);
  tone(speaker, NOTE_G5, 250);
  delay(300);
  tone(speaker, NOTE_B5, 125);
  delay(150);
  tone(speaker, NOTE_D6, 125);
  delay(300);
  digitalWrite(speaker, HIGH);
  headrotate();
  digitalWrite(LedPin, LOW);
#ifdef TEST_MODE
  Serial.begin(9600);
#endif
  manevrSpd = map(DownSpd, 0, 100, MinSpd, MaxSpd);
  runStartTime = millis();
}

void loop() {
  currentMillis = millis() - runStartTime;
  if (currentMillis > RunStopTime) {
    FullStop();
    digitalWrite(LedPin, HIGH);
  } else {
    distanceFwd = ping();
    if (distanceFwd>Danger) {
      if (!itRun) moving(MinSpd, MaxSpd, StartTime, true, 0);
    } else { // Тормозим и крутим дальномером
#ifdef TEST_MODE
      Serial.println("Стоп");
#endif
      tone(speaker, NOTE_D6, 63);
      moving(MaxSpd, MinSpd, StopTime, true, 0);
      tone(speaker, NOTE_D5, 125);
      FullStop();
      delay(300);
      digitalWrite(speaker, HIGH);
      myservo.write(45); // вправо 45 град.
      delay(300);
      rightDist = ping();
      myservo.write(0); // вправо
      delay(300);
      tempDist = ping();
#ifdef TEST_MODE
      Serial.print("Дистанция справа 45 - ");
      Serial.print(rightDist);
      Serial.print(", 0  -  ");
      Serial.println(tempDist);
#endif
      if (rightDist > tempDist) {
        angleRight = 1;
      } else {
        angleRight = 2;
        rightDist = tempDist;
      }
      myservo.write(135); // влево 45 град.
      delay(500);
      leftDist= ping();
      myservo.write(180); // влево
      delay(300);
      tempDist = ping();
#ifdef TEST_MODE
      Serial.print("Дистанция слева 135 - ");
      Serial.print(leftDist);
      Serial.print(", 180 - ");
      Serial.println(tempDist);
#endif
      if (leftDist > tempDist) {
        angleLeft = 1;
      } else {
        angleLeft = 2;
        leftDist = tempDist;
      }
#ifdef TEST_MODE
      Serial.print("Впереди - ^ ");
      Serial.print(distanceFwd);
      Serial.print(", ");
      Serial.print("Слева - < "); 
      Serial.print(leftDist);
      Serial.print(", ");
      Serial.print("Справа - > ");
      Serial.println(rightDist);
#endif
      //myservo.write(ServoCenter);
      delay(100);
      compareDistance();
    }
  }
}

void compareDistance(){
  if (leftDist > rightDist) { // поворот влево
    if (leftDist > Danger * 2) {
      myservo.write(ServoCenter + 35 * angleLeft);
#ifdef TEST_MODE
      if (angleLeft > 1) {
        Serial.print("Разворот влево на  ");
      } else {
        Serial.print("Поворот влево на   ");
      }
      Serial.print(angleLeft * 45);
      Serial.println(" градусов");
#endif
      moving(MinSpd, manevrSpd, RotateTime, true, 1);
      delay(Rotate45*angleLeft - RotateTime);
      moving(manevrSpd, MinSpd, RotateTime, true, 1);
      FullStop();
    } else moveback();
  } else { // поворот вправо
    if (rightDist > Danger * 2) {
      myservo.write(ServoCenter - 35 * angleRight);
#ifdef TEST_MODE
      if (angleRight > 1) {
        Serial.print("Разворот вправо на ");
      } else {
        Serial.print("Поворот вправо на  ");
      }
      Serial.print(angleRight * 45);
      Serial.println(" градусов");
#endif
      moving(MinSpd, manevrSpd, RotateTime, true, 2);
      delay(Rotate45*angleLeft - RotateTime);
      moving(manevrSpd, MinSpd, RotateTime, true, 2);
      FullStop();
    } else moveback();
  }
  myservo.write(ServoCenter);
#ifdef TEST_MODE
  Serial.println("Вперёд");
#endif
  delay(200);
}

void moveback() {
  digitalWrite(LedPin, LOW);
  myservo.write(ServoCenter);
#ifdef TEST_MODE
  Serial.println("Задний ход");
#endif
  tone(speaker, NOTE_C6, 125);
  moving(MinSpd, manevrSpd, StartTime, false, 0); // Разгон
  digitalWrite(LedPin, HIGH);
  tone(speaker, NOTE_C6, 125);
  delay(BackTime); // Движение
  digitalWrite(LedPin, LOW);
  tone(speaker, NOTE_C6, 125);
  moving(manevrSpd, MinSpd, StopTime, false, 0);
  digitalWrite(LedPin, HIGH);
  tone(speaker, NOTE_C6, 125);
  FullStop();
  delay(125);
  digitalWrite(LedPin, LOW);
  digitalWrite(speaker, HIGH);
  myservo.write(10);
#ifdef TEST_MODE
  Serial.println("Разворот вправо на 180 градусов");
#endif
  moving(MinSpd, manevrSpd, StartTime, true, 2);
  delay(Rotate45 * 4 - (StartTime + StopTime) / 2);
  moving(manevrSpd, MinSpd, StopTime, true, 2);
  FullStop();
}

long ping() { // измеряем расстояние до препятствия
  digitalWrite(TriggerPin, HIGH); 
  /* Подаем импульс на вход TriggerPin дальномера */
  delayMicroseconds(10); // равный 10 микросекундам 
  digitalWrite(TriggerPin, LOW); // Отключаем 
  unsigned int impulseTime=pulseIn(EchoPin, HIGH); // Замеряем длину импульса 
  unsigned int distance_sm=impulseTime / 58; // Пересчитываем в сантиметры  
  if (distance_sm < Danger) { // Если расстояние менее задуманного  {     
    digitalWrite(LedPin, HIGH); // Светодиод горит 
  } else {   
    digitalWrite(LedPin, LOW); // иначе не горит 
  }   
  delay(100); 
  return distance_sm;
}

// проверяем сервопривод
void headrotate() {
  myservo.write(ServoCenter - 60);
  delay(250);
  for(byte i = 1; i < 25; i++) {
    myservo.write(ServoCenter - 60 + 5 * i);
    delay(20);
  }
  myservo.write(ServoCenter);
  delay(200);
}

void moving(uint8_t speedBegin, uint8_t speedEnd, unsigned int timeMove, boolean moveDir, uint8_t motorRun) {
  unsigned long manevrStart = millis();
  itRun = (motorRun == 0) && (speedEnd >= speedBegin); // в результате полный ход?
  currentMillis = millis() - manevrStart;
  do { // Цикл на время маневра
    currentSpeed = map(currentMillis, 0, timeMove, speedBegin, speedEnd);
    switch (motorRun) {
        case 0: // Прямое движение
          if (moveDir) { // Вперед
            analogWrite(motorRightBackward, 0);
            analogWrite(motorLeftBackward, 0);
            analogWrite(motorLeftForward, currentSpeed);
            analogWrite(motorRightForward, currentSpeed);
          } else { // Назад
            analogWrite(motorRightForward, 0);
            analogWrite(motorLeftForward, 0);
            analogWrite(motorRightBackward, currentSpeed);
            analogWrite(motorLeftBackward, currentSpeed);
          }
          break;
        case 1: // Разворот влево
          analogWrite(motorRightForward, currentSpeed);
          analogWrite(motorLeftForward, 0);
          analogWrite(motorRightBackward, 0);
          analogWrite(motorLeftBackward, currentSpeed);
          break;
        case 2: // Разворот вправо
          analogWrite(motorRightForward, 0);
          analogWrite(motorLeftForward, currentSpeed);
          analogWrite(motorRightBackward, currentSpeed);
          analogWrite(motorLeftBackward, 0);
          break;
        default:
          // if nothing else matches, do the default
          // default is optional
          break;
      }    
    delay(10);
    currentMillis = millis() - manevrStart;
  } while (currentMillis < timeMove);
  if (itRun) { //     Если полный ход
    if (moveDir) { // Доводим до максимальной скорости
      analogWrite(motorLeftForward, speedEnd);
      analogWrite(motorRightForward, speedEnd);
    } else {
      analogWrite(motorRightBackward, speedEnd);
      analogWrite(motorLeftBackward, speedEnd);
    }
  }
}

void FullStop() { // Полная остановка
  needR = 0;
  needL = 0;
  unsigned long lastMillis = 0;
  do {
    if(millis()-lastMillis > 3){ // every 3 millisecond
      if (abs(pwmR) > MinSpd) {
        if (pwmR > 0) pwmR--;
        else pwmR++;
        motorR.motorGo(pwmR);
      }
      if (abs(pwmL) > MinSpd) {
        if (pwmL > 0) pwmL--;
        else pwmL++;
        motorL.motorGo(pwmL);
      }
      lastMillis = millis();
    }
  } while (abs(pwmR) > MinSpd || abs(pwmR) > MinSpd);
  motorR.motorGo(0);
  motorL.motorGo(0);
}

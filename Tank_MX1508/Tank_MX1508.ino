/***************************************************
* Laboratory Arduino-tank. Version 1.3  (18.03.18) *
* creator: Owintsowsky Maxim            6 064 (88) *
* https://vk.com/automation4house  VK group        *
* https://t.me/aquatimer Telegram chat             *
****************************************************/

#include <Servo.h>
#include <MX1508.h>
#include "pitches.h"

#define TEST_MODE // State for test mode

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
#define speaker 12 // Бипер пин
#define SPEAKER_MUTE HIGH // Обесточивание бирера

#define PERIOD_SONAR 70    // период работы сонара
#define PERIOD_REMOTE 350   // период второй задачи
#define PERIOD_SPEED 5   // период изменения скорости
#define PERIOD_BRAKE 3   // период изменения скорости

const int Danger = 23; // критическое расстояние до препятствия в см
const unsigned int Rotate45 = 100; // время поворота на 45 градусов в мсек
const unsigned long RunStopTime = 30000; // Общее время работы в мсек
//const unsigned long StartTime = 1000; // Время разгона до полной скорости в мсек
//const unsigned long StopTime = 250; // Время остановки остановки в мсек
//const unsigned long RotateTime = 150; // Время разгона-остановки в развороте < Rotate45
const unsigned long BackTime = 1000; // Время отъезда перед разворотом в мсек
const uint8_t MaxSpd = 180; // Максимальная скорость (0­255)
const uint8_t MinSpd = 55; // Нулевая скорость (0­MaxSpd)
const uint8_t DownSpd = 30; // Снижение скорости при манёврах в процентах
const uint8_t ServoCenter = 84; // Середина центровки серво

Servo myservo;

unsigned int leftDist;
unsigned int rightDist;
//unsigned int distanceFwd;
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

unsigned long timer_sonar = 0;
unsigned long timer_remote = 0;
unsigned long timer_speed = 0;

MX1508 motorR(motorRightForward,motorRightBackward, FAST_DECAY, 2);
MX1508 motorL(motorLeftForward,motorLeftBackward, FAST_DECAY, 2);

void setup() {
  pinMode(TriggerPin, OUTPUT); //инициируем как выход 
  pinMode(EchoPin, INPUT); //инициируем как вход 
  pinMode(LedPin, OUTPUT); 
  pinMode(speaker, OUTPUT);
  digitalWrite(speaker, SPEAKER_MUTE);
  myservo.attach(ServoPin);
  digitalWrite(LedPin, HIGH);
  tone(speaker, NOTE_G5, 250);
  delay(300);
  tone(speaker, NOTE_B5, 125);
  delay(150);
  tone(speaker, NOTE_D6, 125);
  delay(300);
  digitalWrite(speaker, SPEAKER_MUTE);
  headrotate();
  digitalWrite(LedPin, LOW);
#ifdef TEST_MODE
  Serial.begin(9600);
#endif
  manevrSpd = map(DownSpd, 0, 100, MinSpd, MaxSpd);
  needR = MaxSpd;
  needL = MaxSpd;
  pwmR = MinSpd;
  pwmL = MinSpd;
  runStartTime = millis();
}

void loop() {
  if (millis() - runStartTime > RunStopTime) { // Время выщло
    FullStop();
    digitalWrite(LedPin, HIGH);
  } else { // едем дальше (основной цикл)
    if (millis() - timer_sonar > PERIOD_SONAR) {
      timer_sonar = millis();
      //distanceFwd = ping();
      if (ping() < Danger)
      //if (distanceFwd < Danger)
        obstruction();
    }
    if (millis() - timer_speed > PERIOD_SPEED) {
      timer_speed = millis();
      changeSpeed();
    }
  }
}

void obstruction() { // Тормозим и крутим дальномером
#ifdef TEST_MODE
  Serial.println("Стоп");
#endif
  tone(speaker, NOTE_D6, 63);
  //moving(MaxSpd, MinSpd, StopTime, true, 0);
  FullStop();
  tone(speaker, NOTE_D5, 125);
  needR = -manevrSpd;
  needL = -manevrSpd;
  pwmR = -MinSpd;
  pwmL = -MinSpd;
  settingSpeed();
  FullStop();
  digitalWrite(speaker, SPEAKER_MUTE);
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
    angleRight = 0;
  } else {
    angleRight = 1;
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
    angleLeft = 0;
  } else {
    angleLeft = 1;
    leftDist = tempDist;
  }
#ifdef TEST_MODE
  Serial.print("Впереди - ^ ");
  Serial.print(Danger);
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

void compareDistance(){
  if (leftDist > rightDist) { // поворот влево
    if (leftDist > Danger << 1) {
      myservo.write(ServoCenter + 35 << angleLeft);
#ifdef TEST_MODE
      if (angleLeft > 0) {
        Serial.print("Разворот влево на  ");
      } else {
        Serial.print("Поворот влево на   ");
      }
      Serial.print(45 << angleLeft);
      Serial.println(" градусов");
#endif
      needR = manevrSpd;
      needL = -manevrSpd;
      pwmR = MinSpd;
      pwmL = -MinSpd;
      settingSpeed();
      delay(Rotate45 << angleLeft);
      needR = MaxSpd;
      needL = MaxSpd;
      pwmR = MinSpd;
      pwmL = MinSpd;
    } else moveback();
  } else { // поворот вправо
    if (rightDist > Danger << 1) {
      myservo.write(ServoCenter - 35 << angleRight);
#ifdef TEST_MODE
      if (angleRight > 0) {
        Serial.print("Разворот вправо на ");
      } else {
        Serial.print("Поворот вправо на  ");
      }
      Serial.print(45 << angleRight);
      Serial.println(" градусов");
#endif
      needR = -manevrSpd;
      needL = manevrSpd;
      pwmR = -MinSpd;
      pwmL = MinSpd;
      settingSpeed();
      //moving(MinSpd, manevrSpd, RotateTime, true, 2);
      delay(Rotate45 << angleLeft);
      //moving(manevrSpd, MinSpd, RotateTime, true, 2);
      //FullStop();
      needR = MaxSpd;
      needL = MaxSpd;
      pwmR = MinSpd;
      pwmL = MinSpd;
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
  digitalWrite(LedPin, HIGH);
  needR = -manevrSpd;
  needL = -manevrSpd;
  pwmR = -MinSpd;
  pwmL = -MinSpd;
  settingSpeed();
  tone(speaker, NOTE_C6, 125);
  delay(BackTime); // Движение
  digitalWrite(LedPin, LOW);
  myservo.write(10);
#ifdef TEST_MODE
  Serial.println("Разворот вправо на 180 градусов");
#endif
  tone(speaker, NOTE_C6, 125);
  needR = -manevrSpd;
  needL = manevrSpd;
  settingSpeed();
  digitalWrite(LedPin, HIGH);
  delay(Rotate45 << 2);
  tone(speaker, NOTE_C6, 125);
  FullStop();
  digitalWrite(LedPin, LOW);
  digitalWrite(speaker, SPEAKER_MUTE);
  delay(250);
  needR = MaxSpd;
  needL = MaxSpd;
  pwmR = MinSpd;
  pwmL = MinSpd;
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
  //delay(100); 
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

/*
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
*/

void changeSpeed() { // Приближаем скорость к нужной
  if (pwmR != needR)  {
    if (pwmR > needR) pwmR--;
    else if (pwmR < needR) pwmR++;
    motorR.motorGo(pwmR);
  }
  if (pwmL != needL) {
    if (pwmL > needL) pwmL--;
    else if (pwmL < needL) pwmL++;
    motorL.motorGo(pwmL);
  }
}

void settingSpeed() { // Доводим скорость до нужной
  do {
    if(millis() - timer_speed > PERIOD_SPEED){ // every 8 millisecond
      changeSpeed();
      timer_speed = millis();
    }
  } while ((pwmR != needR) || (pwmL != needL));
  delay(PERIOD_SPEED);
  motorR.motorGo(needR);
  motorL.motorGo(needL);
}

void FullStop() { // Полная остановка
  needR = 0;
  needL = 0;
  unsigned long lastMillis = 0;
  do {
    if(millis()-lastMillis > PERIOD_BRAKE){ // every 3 millisecond
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
  motorR.stopMotor();
  motorL.stopMotor();
}
   

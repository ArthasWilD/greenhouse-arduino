#include <SoftwareSerial.h>
#include <FastLED.h>

#define NUM_LEDS 60
#define LEDS_SEGMENT 15
#define SEGMENTS 4

#define LIGHT_PIN A0
#define LEDS_PIN 8

//[Year, Month, Day, Hour, Minute]

CRGB growthColors[] = {
  CRGB(148, 0, 211), 
  CRGB(148, 0, 211), 
  CRGB(148, 0, 211), 
  CRGB(201, 127, 233), 
  CRGB(255, 255, 255), 
  CRGB(255, 255, 255), 
  CRGB(255, 255, 255), 
  CRGB(255, 255, 255), 
  CRGB(255, 255, 255), 
  CRGB(255, 255, 255), 
  CRGB(255, 210, 127), 
  CRGB(255, 165, 0), 
  CRGB(255, 165, 0), 
  CRGB(255, 165, 0), 
  CRGB(255, 165, 0), 
  CRGB(255, 165, 0),
  CRGB(255, 83, 0), 
  CRGB(255, 0, 0), 
  CRGB(255, 0, 0), 
  CRGB(255, 0, 0)
};

short startDate[SEGMENTS] = {0, 0, 0, 0};
short endDate[SEGMENTS] = {0, 0, 0, 0};

short startTime[SEGMENTS] = {0, 0, 0, 0};
short endTime[SEGMENTS] = {0, 0, 0, 0};

bool activeSegments[SEGMENTS] = {0, 0, 0, 0};

CRGB leds[NUM_LEDS];
CRGB preleds[NUM_LEDS];

short currentDates = 0;
short currentTimes = 0;

bool simulation = false;

SoftwareSerial mySerial(6,7);

short readShort() {
  return mySerial.read() * 256 + mySerial.read();
}

bool checkTime(short currentTime, short segment) {
  if(startTime[segment] < endTime[segment]){
    if(currentTime > startTime[segment] && currentTime < endTime[segment])
      return true;
    return false;
  }
  if(startTime[segment] > endTime[segment]){
    if(currentTime > startTime[segment] || currentTime < endTime[segment])
      return true;
    return false;
  }
}

bool checkLight() {
  int light = analogRead(LIGHT_PIN);
  //Serial.println(light);
  return analogRead(LIGHT_PIN) > 54;
  //return true;
}

void handleLEDs(short currentTime) {
  for(int s = 0; s < SEGMENTS; s++) {
    if(checkTime(currentTime, s) && checkLight() && activeSegments[s]){
      for(int i = 0; i < LEDS_SEGMENT; i++)
        leds[i + s * LEDS_SEGMENT] = preleds[i + s * LEDS_SEGMENT];
    }
    else {
      for(int i = 0; i < LEDS_SEGMENT; i++)
        leds[i + s * LEDS_SEGMENT] = CRGB::Black;
    }
  }
  FastLED.show();
}

short getDate(short segment, short currentDate) {
  if(endDate[segment] - startDate[segment] == 0) {
    //Serial.print("Zero division");
    return 0;
  }
  short result = ((currentDate - startDate[segment]) * 20 / (endDate[segment] - startDate[segment]));
  if(result > 20 || result < 0) {
    //Serial.print("Result out of bounds");
    return 0;
  }
  return result;
}

CRGB getColor(short segment, short currentDate) {
  return growthColors[getDate(segment, currentDate)];
}

void segmentColor(CRGB color, short segment) {
  for(int i = 0; i < LEDS_SEGMENT; i++) {
    preleds[LEDS_SEGMENT * segment + i] = color;
    //Serial.print("Set color on LED ");
    //Serial.println(LEDS_SEGMENT * segment + i);
  }
}

void handlePreLEDs(short currentDate) {
  for(int s = 0; s < SEGMENTS; s++) {
    if(currentDate <= endDate[s] && currentDate >= startDate[s]) {
      segmentColor(getColor(s, currentDate), s);
      //Serial.print("Set color on segment ");
      //Serial.println(s);
    } 
    else if(currentDate > endDate[s]) {
      activeSegments[s] = false;
    }
  }
}

void configureSegment(int segment, int startDates, int endDates, int startTimes, int endTimes) {
  startDate[segment] = startDates;
  endDate[segment] = endDates;
  startTime[segment] = startTimes;
  endTime[segment] = endTimes;
  activeSegments[segment] = true;

  /*
  Serial.print("Сегмент: ");
  Serial.println(segment);
  Serial.print("Дата посадки: ");
  Serial.println(startDates);
  Serial.print("Дата сбора: ");
  Serial.println(endDates);
  Serial.print("Начало св. дня: ");
  Serial.println(startTimes);
  Serial.print("Конец св. дня: ");
  Serial.println(endTimes);
  */
}

void configureAllSegments(int startDates, int endDates, int startTimes, int endTimes) {
  for(int s = 0; s < SEGMENTS; s++)
    configureSegment(s, startDates, endDates, startTimes, endTimes);
}

void handleSerial() {
  if(!mySerial.available())
    return;
  int command = mySerial.read();
  if(command == 'c') {
    int segment = mySerial.read();
    if(segment == 5) {
      configureAllSegments(readShort(), readShort(), readShort(), readShort());
      return;
    }
    configureSegment(segment - 1, readShort(), readShort(), readShort(), readShort());
    return;
  }
  if(command == 't') {
    if(!simulation){
    currentDates = readShort();
    currentTimes = readShort();
    }
    else
    {
      readShort();
      readShort();
    }
  }
  if(command == 's') {
    simulation = readShort();
  }
  if(command == 'r') {
    int segment = mySerial.read() - 1;
    mySerial.write(leds[segment * LEDS_SEGMENT].b);
    mySerial.write(leds[segment * LEDS_SEGMENT].g);
    mySerial.write(leds[segment * LEDS_SEGMENT].r);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(LEDS_PIN, OUTPUT);

  FastLED.addLeds<WS2812, LEDS_PIN, GRB>(leds, NUM_LEDS);

  delay(2000);

  analogWrite(LIGHT_PIN, LOW);
  Serial.begin(115200);  // Скорость для монитора порта
  mySerial.begin(115200); // Скорость для ESP (через аппаратный UART)

  simulation = false;

  //startTime[0] = 0;
  //endTime[0] = 720;

  //startDate[0] = 0;
  //endDate[0] = 10;

  //activeSegments[0] = true;
}

void loop() {
  handleSerial();
  if(simulation){
    currentTimes += 60;
    if(currentTimes >= 1440) {
      currentTimes -= 1440;
      currentDates++;
      //Serial.println(currentDates);
    }
  }
  handlePreLEDs(currentDates);
  handleLEDs(currentTimes);
  delay(100);
  /*currentTimes += 60;
  if(currentTimes == 1440) {
    currentTimes = 0;
    currentDates++;
  }
  delay(100);
  */
  //Serial.println(currentTimes);
  //Serial.println(currentDates);
  //Serial.println(activeSegments[0]);
  //for(int i = 0; i < NUM_LEDS; i++){
    //Serial.print(preleds[i].red);
    //Serial.print(" ");
  //}
  //Serial.println("");
}

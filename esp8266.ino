#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "";
const char* password = "";
const char* bot_token = ";

// Настройки безопасности
const String AUTH_PASSWORD = "1234"; // Ваш пароль
const String ALLOWED_CHAT_ID = ""; // Ваш Chat ID
const int MAX_AUTH_USERS = 5; // Макс. количество авториз. пользователей

WiFiClientSecure client;
UniversalTelegramBot bot(bot_token, client);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String formattedDate;
String dayStamp;
String timeStamp;

unsigned long mTime = 0;

String authorizedUsers[MAX_AUTH_USERS]; // Массив для хранения авторизованных Chat ID
int userCount = 0;

bool isAwaitingGryadka = false;
bool isAwaitingStartDate = false;
bool isAwaitingEndDate = false;
bool isAwaitingStartTime = false;
bool isAwaitingEndTime = false;

struct Settings {
  int gryadka;
  int startDate;
  int endDate;
  int startTime;
  int endTime;
};

// Клавиатура управления
String keyboardGryadka = "[["
  "{ \"text\": \"1\", \"callback_data\": \"1\" },"
  "{ \"text\": \"2\", \"callback_data\": \"2\" },"
  "{ \"text\": \"3\", \"callback_data\": \"3\" },"
  "{ \"text\": \"4\", \"callback_data\": \"4\" },"
  "{ \"text\": \"Все\", \"callback_data\": \"5\" }"
"]]";

String keyboardTest = "[["
  "{ \"text\": \"Включить\", \"callback_data\": \"1\" },"
  "{ \"text\": \"Выключить\", \"callback_data\": \"0\" }"
"]]";

struct Settings settings;

void handleNewMessages(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // Проверка авторизации
    bool isAuthorized = false;
    for(int j=0; j<userCount; j++) {
      if(authorizedUsers[j] == chat_id) {
        isAuthorized = true;
        break;
      }
    }

    if(!isAuthorized) {
      // Команда авторизации
      if(text.startsWith("/auth")) {
        String enteredPass = text.substring(6);
        if(enteredPass == AUTH_PASSWORD) {
          if(userCount < MAX_AUTH_USERS) {
            authorizedUsers[userCount++] = chat_id;
            bot.sendMessage(chat_id, "✅ Авторизация успешна!");
          } else {
            bot.sendMessage(chat_id, "❌ Достигнут лимит пользователей");
          }
        } else {
          bot.sendMessage(chat_id, "❌ Неверный пароль");
        }
      }
      else {
        bot.sendMessage(chat_id, "🔒 Требуется авторизация!\nОтправьте /auth ваш_пароль");
      }
      return;
    }
    else if(text == "/help") {
      String helpText = "Доступные команды:\n"
                       "/auth - Авторизация\n"
                       "/logout - Деавторизация\n"
                       "/growth_period - Задать сроки роста\n"
                       "/runTest - Запустить демонстрацию работы теплицы\n"
                       "/stopTest - Остановить демонстрацию работы теплицы";
      bot.sendMessage(chat_id, helpText);
    }

    else if (text == "/runTest") {
      Serial.write("s");
      sendShort(1);
    }
    else if (text == "/stopTest") {
      Serial.write("s");
      sendShort(0);
    }

    else if (text == "/growth_period") {
      String welcome = "Управление теплицей:\n";
      welcome += "Для каких грядок настроить освещение? ⤵️";
      bot.sendMessageWithInlineKeyboard(chat_id, welcome, "", keyboardGryadka);
      isAwaitingGryadka = true;
    }
    else if(isAwaitingGryadka){
      settings.gryadka = atoi(text.c_str());
      bot.sendMessage(chat_id, "Укажите дату посадки в формате дд.мм.гггг");
      isAwaitingGryadka = false;
      isAwaitingStartDate = true;
    }
    else if(isAwaitingStartDate){
      settings.startDate = convertDate(text);
      bot.sendMessage(chat_id, (String)settings.startDate);
      bot.sendMessage(chat_id, "Укажите приблизительную дату созревания в формате дд.мм.гггг");
      isAwaitingStartDate = false;
      isAwaitingEndDate = true;
    }
    else if(isAwaitingEndDate){
      settings.endDate = convertDate(text);
      bot.sendMessage(chat_id, (String)settings.endDate);
      bot.sendMessage(chat_id, "Укажите время начала светового дня в формате чч:мм");
      isAwaitingEndDate = false;
      isAwaitingStartTime = true;
    }
    else if(isAwaitingStartTime){
      settings.startTime = convertTime(text);
      bot.sendMessage(chat_id, (String)settings.startTime);
      bot.sendMessage(chat_id, "Укажите время конца светового дня в формате чч:мм");
      isAwaitingStartTime = false;
      isAwaitingEndTime = true;
    }
    else if(isAwaitingEndTime){
      settings.endTime = convertTime(text);
      bot.sendMessage(chat_id, (String)settings.endTime);
      
      Serial.write('c');
      Serial.write(settings.gryadka);
      sendShort(settings.endTime);
      sendShort(settings.startTime);
      sendShort(settings.endDate);
      sendShort(settings.startDate);
      isAwaitingEndTime = false;
    }

    else if(text == "/logout") {
      int newSize = 0;
      for(int l = 0; l < MAX_AUTH_USERS; l++){
        if(authorizedUsers[l] != chat_id){
          authorizedUsers[newSize] = authorizedUsers[l];
          newSize++;
        }
      }
    }
  }
}

void sendShort (int num){
  Serial.write(num / 256);
  Serial.write(num % 256);
}

int convertDate(String str){
  int day, month, year;
  if (sscanf(str.c_str(),"%d.%d.%d",&day,&month,&year) == 3)
  {
    int months[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int yday = months[month - 1];
    if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
      yday++;
    return  day + yday + (year-2000)*365 + (year-1999)/4 - (year-2001)/100 + (year+1701)/400;
  }
  return 0;
}

int convertDate(int day, int month, int year){
  int months[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int yday = months[month - 1];
  if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
    yday++;
  return  day + yday + (year-2000)*365 + (year-1999)/4 - (year-2001)/100 + (year+1701)/400;
}

int convertTime(String str){
  int hour, minute;
  if (sscanf(str.c_str(),"%d:%d",&hour,&minute) == 2)
  {
    return hour * 60 + minute;
  }
  return 0;
}

void sendingTime(){
  timeClient.update();
  Serial.write('t');
  int hh = timeClient.getHours();
  int mm = timeClient.getMinutes();
  sendShort(hh * 60 + mm);
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  int day = ptm->tm_mday;
  int month = ptm->tm_mon+1;
  int year = ptm->tm_year+1900;
  sendShort(convertDate(day, month, year));
}

void setup() {
  Serial.begin(115200);
  
  authorizedUsers[userCount++] = ALLOWED_CHAT_ID;

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  client.setInsecure();

  timeClient.begin();
}

void loop() {
  unsigned long miTime = millis();
  if (miTime - mTime > 60000) {
    mTime = miTime;
    sendingTime();
  }
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while(numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  delay(100);
}

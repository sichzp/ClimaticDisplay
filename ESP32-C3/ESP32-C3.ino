

/*
 * =============================================================================
 * ClimaticDisplay 2.66G — рабочая (автономная) версия прошивки
 * =============================================================================
 * Плата: ESP32-C3 Super Mini.
 *
 * Логика: таймер TPL5110 раз в 20 минут подаёт питание; ESP32-C3 просыпается,
 * читает время и RAM из DS1307Z, при необходимости синхронизирует часы по NTP,
 * измеряет BME280 и напряжение АКБ, рисует картинку на 4-цветном e-paper
 * дисплее и снова выключает питание (сигнал DONE на пине PWR_OFF).
 * Поэтому весь алгоритм размещён в setup(), а loop() пуст.
 *
 * Домашний вариант (ESPHome + Home Assistant) — в папке ../ForESPHome.
 * Описание прибора — ../DOC/DeviceDescription.md, алгоритм — ../DOC/Algorithm.md.
 *
 * ВАЖНО: перед компиляцией обязательно установить библиотеки GxEPD2 и
 * GyverBME280 из набора библиотек Arduino (Менеджер библиотек).
 * =============================================================================
 */



#include <WiFi.h>
#include "time.h"
#include <esp_sntp.h>
#include <GyverBME280.h>
#include "DS1307.h"
#include "Var.h"

// Подключить графическую библиотеку
#include <GxEPD2_4C.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
#define GxEPD2_DRIVER_CLASS GxEPD2_266c_GDEY0266F51H // GDEY0266F51H 184x360
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (32768) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (32768) / (EPD::WIDTH / 8))
GxEPD2_4C<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS*/ 7, /*DC=*/ 3, /*RST=*/ 2, /*BUSY=*/ 1)); // Good Display ESP32E6-E01 board
#undef MAX_HEIGHT


// Шрифты
#include "fonts/FreeSansBold8.h"
#include "fonts/FreeSansBold10.h"
#include "fonts/FreeSansBold24pt7b.h"

// Картинки

// Стрелки вверх и вниз
#include "img/Up.h"
#include "img/Down.h"

// Значки термометра
#include "img/TermometrGen.h"
#include "img/TermometrNormRED.h"
#include "img/TermometrMinRED.h"
#include "img/TermometrMaxRED.h"

// Значки влажности
#include "img/HumidityBW.h"
#include "img/HumidityRY.h"

// Значки атм. давления
#include "img/PressureMinBW.h"
#include "img/PressureNormBW.h"
#include "img/PressureMaxBW.h"
#include "img/PressureMinMaxY.h"
#include "img/PressureNormY.h"
#include "img/PressureMinNormR.h"
#include "img/PressureMaxR.h"

// Значки батарейки
#include "img/BatteryGenBW.h"
#include "img/BatteryGenY.h"
#include "img/BatteryMaxBW.h"
#include "img/BatteryHalfR.h"
#include "img/BatteryMinR.h"


#define VER_FOR_HOME		// Версия для работы из дома. Обновление Времени ночью
//Иначе -  Версия для работы. Обновление времени днем по будням 
// Ну и разные точки Вай-Фай

// Вид питания
//#define POWER_AKB_42V		// Питание от АКБ LiPO 4,2В и слежением за разрядом
#define POWER_AKB_32V		// Питание от АКБ LiFePO 3,2В и слежением за разрядом
// Питание от сети 5В - ничего включать не надо


#define PWR_PIN		10			// Нога вылючения питания подтяжка в ноль. 1 - выкл питания

// Ночной режим экономии АКБ: в этом интервале прибор после чтения времени сразу выключается
// без измерений и обновления экрана. Время задано в формате BCD МС DS1307.
#define NIGHT_START_HOUR	0x23	// Начало ночного режима: 23:00
#define NIGHT_END_HOUR		0x04	// Конец ночного режима:   04:00


// Для просмотра отладочной информации раскомментировать строку DEBUG ниже,
// в Ардуино включить режим "Инструменты->USB CDC on boot->Enable"
// и открыть окно монитора порта (выставить скорость 115200).
// В релизной (публикуемой) версии отладочный вывод отключён.
//#define DEBUG

// В отладочных целях можно отключить блок программы, раскоментировав нужную строку ниже
//#define NO_BME280		// Не использовать BME280
//#define NO_DS1307		// Не использовать DS1307
//#define NO_GxEPD2		// Не использовать работу с экраном


// для получения времени
#ifdef VER_FOR_HOME
const char *ssid = "XXXXXXXX";
const char *password = "XXXXXXXX";
#else
const char *ssid = "YYYYYYY";
const char *password = "YYYYYYY";
#endif

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

const long gmtOffset_sec = (3600 * 3);		// третий часовой пояс
const int daylightOffset_sec = 0;					// переход на летнее время

struct tm timeinfo;

bool NTP_Flag = false;

AllData_t AllData;				// Данные для текущей работы и записи в RAM DS1307Z
AllData_t AllDataOld;			// Старые данные для чтения из RAM DS1307Z

C_DS_1307 *MyDateTime;

GyverBME280 bme; 						// Объект датчика BME280

float	Battery = 0;				// Уровень заряда АКБ, В


// Callback function (gets called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
#ifdef DEBUG
	Serial.println("Получил корректировку времени от NTP!");
#endif
	
	if (!getLocalTime(&timeinfo)) 
	{
#ifdef DEBUG
		Serial.println("Время от NTP не получено.");
#endif
//		return;
	}
	else
		NTP_Flag = 1;		// Флаг ожидания времени от NTP
}


void setup() {

	// Опа, включились! 

#ifdef DEBUG
	Serial.begin(115200);
	delay(1000);		// Пусть все датчики нормально проснутся.
	Serial.println("Включились!!!");
#endif

AllData.Err.Errors = 0;

	pinMode(PWR_PIN, OUTPUT);
	digitalWrite(PWR_PIN, LOW);


	delay(100);




#ifndef NO_DS1307
// --------------------------------------------------------------------------------------
// Раздел 1. Прочитать всю DS1307
// --------------------------------------------------------------------------------------

#ifdef DEBUG
	Serial.println("------------ Инициализируем DS1307 -------------");
#endif
	// Инициализируем и прочтем данные из микросхемы часов реального времени
	MyDateTime = new C_DS_1307();


	if(MyDateTime->Error == 0)
	{
#ifdef DEBUG
		Serial.println("DS1307 Ответила, все ОК.");
#endif
		MyDateTime->GetDateTime();
		if(MyDateTime->Actual)
		{
			AllData.Err.Bits.TimeRST = 0;
#ifdef DEBUG
			Serial.println("Данные актуальны");
#endif
		}			
		else
		{
			AllData.Err.Bits.TimeRST = 1; 		// Флаг сброса времени
#ifdef DEBUG
			Serial.println("Данные не актуальны");
#endif
		}

	}
	else
	{
		AllData.Err.Bits.DS1307Z = 1;	// Флаг ошибки МС часов
#ifdef DEBUG
		Serial.println("Не отвечает МС DS1307!!!");
#endif

	}

#ifdef DEBUG
	Serial.print("Сырые данные даты и времени: ");
	for(int i = 0; i < 8; i++)
	{
		Serial.print(MyDateTime->DateTime.Buff[i]);
		Serial.print(", ");
	}
	Serial.println(" ");
	
	Serial.println("Данные Времени разбитые на части BCD.");
	Serial.print("День недели: ");
	Serial.println(MyDateTime->DateTime.Week);
	Serial.print("Время: ");
	Serial.print(MyDateTime->DateTime.Hour.BCD_2);
	Serial.print(MyDateTime->DateTime.Hour.BCD_1);
	Serial.print(":");
	Serial.print(MyDateTime->DateTime.Minutes.BCD_2);
	Serial.print(MyDateTime->DateTime.Minutes.BCD_1);
	Serial.print(":");
	Serial.print(MyDateTime->DateTime.Second.BCD_2);
	Serial.println(MyDateTime->DateTime.Second.BCD_1);

	Serial.print("Дата: ");
	Serial.print(MyDateTime->DateTime.Date.BCD_2);
	Serial.print(MyDateTime->DateTime.Date.BCD_1);
	Serial.print(":");
	Serial.print(MyDateTime->DateTime.Month.BCD_2);
	Serial.print(MyDateTime->DateTime.Month.BCD_1);
	Serial.print(":");
	Serial.print(MyDateTime->DateTime.Year.BCD_2);
	Serial.println(MyDateTime->DateTime.Year.BCD_1);
#endif

	
	// Прочитаем нужные регистры RAM из микросхемы часов.
	if(!AllData.Err.Bits.DS1307Z)
	{
#ifdef DEBUG
		Serial.println("Прочтем RAM:");
#endif
		MyDateTime->GetBytes(AllDataOld.Buff, 8, 8); // sizeof(AllDataOld.Buff)
	}

#ifdef DEBUG
	// Для отладки выведем всё что считали
	if(!AllData.Err.Bits.DS1307Z)
	{
		Serial.print("Данные RAM: ");
		for(int i = 0; i < 8; i++)		//  sizeof(AllDataOld.Buff)
		{
			Serial.print(AllDataOld.Buff[i]);
			Serial.print(",");
		}
		Serial.println(" ");
	}
	
	Serial.println("Выведем тоже самое в виде нормальных данных!");
	Serial.print("Старая Температура: ");
	Serial.println(AllDataOld.Temperature);
	Serial.print("Старая Влажность: ");
	Serial.println(AllDataOld.Humidity);
	Serial.print("Старое Давление: ");
	Serial.println(AllDataOld.Pressure);
	
#endif


#ifdef DEBUG
		Serial.println("Решаем забирать ли время из инета.");
#endif

	bool Vskr = ((MyDateTime->DateTime.Week == 1) || (MyDateTime->DateTime.Week == 7))? false : true;	// Если выходной, то не обновляемся
#ifdef DEBUG
		Serial.print("Определим будний день или выходной - ");
		if(Vskr)
			Serial.println("Будний!");
		else
			Serial.println("Выходной!");
#endif



	// Если сейчас больше полночи, но меньше 20 мин первого или в прошлый раз не смог получить время из сети
#ifdef VER_FOR_HOME		// Для домашней версии примерно в полночь
	if(((MyDateTime->DateTime.Hour.Dat == 0x00) && (MyDateTime->DateTime.Minutes.Dat <= 0x20)) || (AllData.Err.Bits.TimeRST == 1) || AllDataOld.Err.Bits.WiFi || AllDataOld.Err.Bits.NTP)
#else					// Для работы - в десять дня (день недели нафиг, потому что все равно питания нет в выходные.)
	if(((MyDateTime->DateTime.Hour.Dat == 0x10) && (MyDateTime->DateTime.Minutes.Dat <= 0x20) && (Vskr)) || (AllData.Err.Bits.TimeRST == 1) || AllDataOld.Err.Bits.WiFi || AllDataOld.Err.Bits.NTP)
#endif
//	if(1)
	{
		// Тут соединимся с Вай-Фай и далее с сервером NTP и получим точное время, которое и запишем в МС DS1307
		// Этот код содран из примера
#ifdef DEBUG
		Serial.println("Надо обновить время...  ");
		Serial.printf("Connecting to %s ", ssid);
#endif
		WiFi.begin(ssid, password);

		uint8_t Timer = 60; // 30 сек

		while (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			Timer--;
			if(Timer == 0)						// Чтоб не зависнуть навсегда
			{
				AllData.Err.Bits.WiFi = 1;		// Запишем ошибку соединения
				break;
			}

#ifdef DEBUG
			Serial.print(".");
#endif
		}

#ifdef DEBUG
		if(AllData.Err.Bits.WiFi == 0)
			Serial.println(" Соединились!");
		else
			Serial.println(" Не соединились!");
#endif

		if(AllData.Err.Bits.WiFi == 0)	// Если есть подключение к инету
		{
			// установим call-back функцию
			sntp_set_time_sync_notification_cb(timeavailable);

			configTime(gmtOffset_sec, 0, ntpServer1, ntpServer2);		// Запуск синхронизации

			uint8_t Timer = 30; // 30 сек

			while(!NTP_Flag)
			{
				delay(1000);	// ждем 
				Timer--;
				if(Timer == 0)
				{
					AllData.Err.Bits.NTP = 1;		// Запишем ошибку получения времени от сервера
#ifdef DEBUG
					Serial.println("NTP не ответил");
#endif
					continue;
				}
			}

			if(NTP_Flag)		// если NTP ответил
			{
#ifdef DEBUG
					Serial.println("NTP синхронизировался!");
					Serial.print("Выведем полученное время: ");
					Serial.print(timeinfo.tm_hour);
					Serial.print(":");
					Serial.print(timeinfo.tm_min);
					Serial.print(":");
					Serial.println(timeinfo.tm_sec);

					Serial.print("Выведем полученную дату: ");
					Serial.print(timeinfo.tm_mday);
					Serial.print("-");
					Serial.print((timeinfo.tm_mon + 1));
					Serial.print("-");
					Serial.println((timeinfo.tm_year - 100));
					
					Serial.print("День недели: ");
					Serial.println(timeinfo.tm_wday);
					
					
#endif
					// Переводим данные в формат BCD
					MyDateTime->DateTime.Hour.BCD_2 = timeinfo.tm_hour / 10;	// Десятки часов
					MyDateTime->DateTime.Hour.BCD_1 = timeinfo.tm_hour % 10;	// Единицы часов

					MyDateTime->DateTime.Minutes.BCD_2 = timeinfo.tm_min / 10;	// Десятки минут
					MyDateTime->DateTime.Minutes.BCD_1 = timeinfo.tm_min % 10;	// Единицы минут

					MyDateTime->DateTime.Second.BCD_2 = timeinfo.tm_sec / 10;	// Десятки секунд
					MyDateTime->DateTime.Second.BCD_1 = timeinfo.tm_sec % 10;	// Единицы секунд

					MyDateTime->DateTime.Date.BCD_2 = timeinfo.tm_mday / 10;	// Десятки дня
					MyDateTime->DateTime.Date.BCD_1 = timeinfo.tm_mday % 10;	// Единицы дня

					MyDateTime->DateTime.Month.BCD_2 = (timeinfo.tm_mon + 1) / 10;	// Десятки месяца	// Тут месяц 0..11, т.е. надо прибавить 1, чтоб было 1..12
					MyDateTime->DateTime.Month.BCD_1 = (timeinfo.tm_mon + 1) % 10;	// Единицы месяца

					MyDateTime->DateTime.Year.BCD_2 = (timeinfo.tm_year - 100) / 10;	// Десятки года		//  вот тут пишут, что надо вычесть 100.... да, так и есть
					MyDateTime->DateTime.Year.BCD_1 = (timeinfo.tm_year - 100) % 10;	// Единицы года

					MyDateTime->DateTime.Week = timeinfo.tm_wday + 1;								// День недели 0..6, 0 - воскресенье
			
		
			}
			
			//Отключаемся от сети
			WiFi.disconnect(true);
			WiFi.mode(WIFI_OFF);

#ifdef DEBUG
			Serial.println("Запишем полученные данные в микросхему.");
#endif
			// Сохраним время в МС
			if(!AllData.Err.Bits.DS1307Z)
				MyDateTime->SetDateTime();

#ifdef DEBUG
			Serial.println("Для проверки записи, прочтем данные и выведем их");
			
			MyDateTime->GetDateTime();

			Serial.print("Сырые данные даты и времени: ");
			for(int i = 0; i < 8; i++)
			{
				Serial.print(MyDateTime->DateTime.Buff[i]);
				Serial.print(", ");
			}
			Serial.print("  \r\n");
			
			Serial.println("Данные Времени разбитые на части BCD.");
			Serial.print("День недели: ");
			Serial.println(MyDateTime->DateTime.Week);
			Serial.print("Время: ");
			Serial.print(MyDateTime->DateTime.Hour.BCD_2);
			Serial.print(MyDateTime->DateTime.Hour.BCD_1);
			Serial.print(":");
			Serial.print(MyDateTime->DateTime.Minutes.BCD_2);
			Serial.print(MyDateTime->DateTime.Minutes.BCD_1);
			Serial.print(":");
			Serial.print(MyDateTime->DateTime.Second.BCD_2);
			Serial.println(MyDateTime->DateTime.Second.BCD_1);

			Serial.print("Дата: ");
			Serial.print(MyDateTime->DateTime.Date.BCD_2);
			Serial.print(MyDateTime->DateTime.Date.BCD_1);
			Serial.print(":");
			Serial.print(MyDateTime->DateTime.Month.BCD_2);
			Serial.print(MyDateTime->DateTime.Month.BCD_1);
			Serial.print(":");
			Serial.print(MyDateTime->DateTime.Year.BCD_2);
			Serial.println(MyDateTime->DateTime.Year.BCD_1);
#endif
		}
	}
#ifdef DEBUG
	else
		Serial.println("Решили не забирать время из инета.");
#endif

	// Ночной режим экономии АКБ: с 23:00 до 04:00 измерения и обновление экрана пропускаем -
	// сразу выключаемся. Синхронизация времени с NTP (если была нужна) уже выполнена выше,
	// так что время ночью всё равно забирается из инета.
	// Если МС не ответила (Error) или время было сброшено (TimeRST) - не спим, а работаем дальше.
	if((MyDateTime->Error == 0) && (AllData.Err.Bits.TimeRST == 0) &&
		((MyDateTime->DateTime.Hour.Dat >= NIGHT_START_HOUR) || (MyDateTime->DateTime.Hour.Dat < NIGHT_END_HOUR)))
	{
#ifdef DEBUG
		Serial.println("Сейчас ночь. Экономим заряд АКБ - выключаемся без измерений.");
#endif
		digitalWrite(PWR_PIN, HIGH);	// Сигнал TPL5110 на выключение питания
		return;
	}

	
#endif


#ifndef NO_BME280
// --------------------------------------------------------------------------------------
// Раздел 2. Прочитать и обработать данные датчика BME280 
// --------------------------------------------------------------------------------------

#ifdef DEBUG
		Serial.println("-------- Инициализация датчика BME280 ---------- ");
#endif

	bme.setMode(FORCED_MODE);		// Принудительный режим
	
	if(!bme.begin())		// Если датчик не инициализирован
	{
#ifdef DEBUG
		Serial.println("Датчик BME280 не найден;");
#endif
		AllData.Err.Bits.BME280 = 1;	// Флаг ошибки термодатчика
	}
#ifdef DEBUG
	else
		Serial.println("Датчик BME280 найден и проиницализирован;");
#endif


	if(AllData.Err.Bits.BME280 == 0)		// Если датчик инициализирован
	{
		// Считаем все данные с микросхемы BME280
		bme.oneMeasurement();			//Запуск преобразования
#ifdef DEBUG
		Serial.println("Запустили преобразование...");
#endif
		while (bme.isMeasuring());		// Ждем окончания преобразования
#ifdef DEBUG
		Serial.println("Данные готовы.");
#endif
		
		AllData.Temperature = (uint16_t)(bme.readTemperature() * 10.0);
		AllData.Humidity = (uint16_t)(bme.readHumidity() * 10.0);
		AllData.Pressure = (uint16_t)pressureToMmHg(bme.readPressure());
		
#ifdef DEBUG
		Serial.println("Полученные данные:");
		Serial.print("Температура:");
		Serial.println(((float)AllData.Temperature / 10.0));
		Serial.print("Влажность:");
		Serial.println(((float)AllData.Humidity / 10.0));
		Serial.print("Атм.давление:");
		Serial.println(AllData.Pressure);
#endif

	}
#endif


// --------------------------------------------------------------------------------------
// Раздел 3. Измерить уровень заряда АКБ 
// --------------------------------------------------------------------------------------

#ifdef POWER_AKB_32V		// Для АКБ 3,2В

#ifdef DEBUG
	Serial.print("--------- Прочтем АЦП ----------\r\n");
#endif

	Battery = (float)analogReadMilliVolts(0) / 1000.0 * 3.3; 

#ifdef DEBUG
	Serial.print("АЦП = ");
	Serial.print(Battery);
	Serial.print(" Вольт \r\n");
#endif

	if(Battery < 0.9)
		AllData.Err.Bits.Battery = 1; 		// Батарея разряжена!
#endif

#ifdef POWER_AKB_42V		// Для АКБ 4,2В

#ifdef DEBUG
	Serial.print("--------- Прочтем АЦП ----------\r\n");
#endif

	Battery = analogReadMilliVolts(0) / 1000;

#ifdef DEBUG
	Serial.print("АЦП = ");
	Serial.print(Battery);
	Serial.print(" Вольт \r\n");
#endif

	if(Battery < 0.9)
		AllData.Err.Bits.Battery = 1; 		// Батарея разряжена!
#endif


#ifndef NO_DS1307
// --------------------------------------------------------------------------------------
// Раздел 4. Записать в микросхему часов полученные сейчас данные для следующего включения
// --------------------------------------------------------------------------------------
#ifdef DEBUG
	Serial.println("----- Запись массива в МС DS1307Z. -----------");
#endif
	if(!AllData.Err.Bits.DS1307Z)
		MyDateTime->SetBytes(AllData.Buff, 8, 8); // Записываем в память текущие данные
#endif


#ifndef NO_GxEPD2
// --------------------------------------------------------------------------------------
// Раздел 5. Сформируем картинку экрана и выведем ее на экран
// --------------------------------------------------------------------------------------


char buffer[20];	// Строка для преобразования в нее чисел


#ifdef DEBUG
	Serial.println("Инициализация графической библиотеки.");
	display.init(115200);	
#else
	display.init(0); 
#endif

	display.setRotation(1);
	display.fillScreen(GxEPD_WHITE);
	

#ifdef DEBUG
	Serial.println("Сформируем избражение в памяти.");
#endif
	
#ifdef DEBUG
	Serial.println("Верхний колонтитул.");
#endif
	// Верхний колонтитул
	display.fillRect(0, 0,  360, 20, GxEPD_YELLOW);
	display.fillRect(0, 21, 360, 3, GxEPD_BLACK);

#ifndef NO_DS1307
	display.setFont(&FreeSansBold10pt8b);
	display.setTextColor(GxEPD_BLACK);
	display.setCursor(3, 16);
	display.print("Обновление было в: ");

	// Создадим строку времени и даты.
	buffer[0] = MyDateTime->DateTime.Hour.BCD_2 + '0';
	buffer[1] = MyDateTime->DateTime.Hour.BCD_1 + '0';
	buffer[2] = ':';
	buffer[3] = MyDateTime->DateTime.Minutes.BCD_2 + '0';
	buffer[4] = MyDateTime->DateTime.Minutes.BCD_1 + '0';
	buffer[5] = ' ';
	buffer[6] = ' ';
	buffer[7] = ' ';
	buffer[8] = MyDateTime->DateTime.Date.BCD_2 + '0';
	buffer[9] = MyDateTime->DateTime.Date.BCD_1 + '0';
	buffer[10] = '-';
	buffer[11] = MyDateTime->DateTime.Month.BCD_2 + '0';
	buffer[12] = MyDateTime->DateTime.Month.BCD_1 + '0';
	buffer[13] = '-';
	buffer[14] = '2';
	buffer[15] = '0';
	buffer[16] = MyDateTime->DateTime.Year.BCD_2 + '0';
	buffer[17] = MyDateTime->DateTime.Year.BCD_1 + '0';
	buffer[18] = 0;
	buffer[19] = 0;
	
#ifdef DEBUG
	Serial.print("Строка даты времени в верхнем колонтитуле: ");
	Serial.print(buffer);
	Serial.println(" ");
#endif


	display.setFont(&FreeSansBold8pt8b);
	display.setTextColor(GxEPD_RED);
	display.setCursor(213, 16);
	display.print(buffer);
#else
	display.setFont(&FreeSansBold10pt8b);
	display.setTextColor(GxEPD_RED);
	display.setCursor(50, 16);
	display.print("Сегодня чудесный день!!!");
#endif


	// Окно температуры
#ifdef DEBUG
	Serial.println("Окно температуры: ");
#endif
	// Значок градусника
	display.drawBitmap(1, 25, gImage_TermometrGen, 112, 80, GxEPD_BLACK);

	if(AllData.Temperature  < 180)
		display.drawBitmap(1, 25, gImage_TermometrMinRED, 112, 80, GxEPD_RED);		// Значок Min температуры
	else if(AllData.Temperature < 280)
		display.drawBitmap(1, 25, gImage_TermometrNormRED, 112, 80, GxEPD_RED);		// Значок Norm температуры
	else
		display.drawBitmap(1, 25, gImage_TermometrMaxRED, 112, 80, GxEPD_RED);		// Значок Max температуры
#ifndef NO_DS1307
	// Значок Повышения или понижения
	if(AllData.Temperature < AllDataOld.Temperature)
		display.drawBitmap(74, 80, gImage_Down , 32, 32, GxEPD_BLACK);			// Если понижается
	else if(AllData.Temperature > AllDataOld.Temperature)
		display.drawBitmap(74, 80, gImage_Up, 32, 32, GxEPD_BLACK);			// Если повышается
	// Если значение не изменилось, то значок не показываем
#endif
	// Значение
	display.setFont(&FreeSansBold24pt7b);
	display.setTextColor(GxEPD_BLACK);
	display.setCursor(8, 148);
	display.setTextSize(1);
	
	dtostrf((float)((float)AllData.Temperature / 10.0), 4, 1, buffer);
	display.print(buffer);

#ifdef DEBUG
	Serial.print("Новая температура: ");
	Serial.println(buffer);
	dtostrf((float)((float)AllDataOld.Temperature / 10.0), 4, 1, buffer);
	Serial.print("Старая температура: ");
	Serial.println(buffer);
#endif


	// Первый разделитель
	display.fillRect(116, 24, 2, 137, GxEPD_BLACK);
	display.fillRect(118, 24, 3, 137, GxEPD_YELLOW);
	display.fillRect(120, 24, 2, 137, GxEPD_BLACK);


	// Окно влажности
#ifdef DEBUG
	Serial.println("Окно влажности:");
#endif
	// Значок влажности
	display.drawBitmap(123, 25, gImage_HumidityBW, 112, 80, GxEPD_BLACK);
	if(AllData.Humidity > 500)
		display.drawBitmap(123, 25, gImage_HumidityRY, 112, 80, GxEPD_RED);		// Значок Min влажности
	else if(AllData.Humidity > 350)
		display.drawBitmap(123, 25, gImage_HumidityRY, 112, 80, GxEPD_YELLOW);		// Значок Norm влажности
#ifndef NO_DS1307
	// Значок Повышения или понижения
	if(AllData.Humidity < AllDataOld.Humidity)
		display.drawBitmap(196, 80, gImage_Down, 32, 32, GxEPD_BLACK);			// Если понижается
	else if(AllData.Humidity > AllDataOld.Humidity)
		display.drawBitmap(196, 80, gImage_Up, 32, 32, GxEPD_BLACK);			// Если повышается
	// Если значение не изменилось, то значок не показываем
#endif
	// Значение
	display.setFont(&FreeSansBold24pt7b);
	display.setTextColor(GxEPD_BLACK);
	display.setTextSize(1);
	display.setCursor(132, 148);
	
	dtostrf((float)((float)AllData.Humidity / 10.0), 4, 1, buffer);
	display.print(buffer);

#ifdef DEBUG
	Serial.print("Новая влажность: ");
	Serial.println(buffer);
	dtostrf((float)((float)AllDataOld.Humidity / 10.0), 4, 1, buffer);
	Serial.print("Старая влажность: ");
	Serial.println(buffer);
#endif

	// Второй разделитель
	display.fillRect(238, 24, 2, 137, GxEPD_BLACK);
	display.fillRect(240, 24, 3, 137, GxEPD_YELLOW);
	display.fillRect(242, 24, 2, 137, GxEPD_BLACK);


	// Окно атм.давления
#ifdef DEBUG
	Serial.println("Окно атм. давления:");
#endif
	// Значок давления
	if(AllData.Pressure < 740)
	{
		display.drawBitmap(245, 25, gImage_PressureMinBW, 112, 80, GxEPD_BLACK);		// Значок Min атм.давления
		display.drawBitmap(245, 25, gImage_PressureMinNormR, 112, 80, GxEPD_RED);
		display.drawBitmap(245, 25, gImage_PressureMinMaxY, 112, 80, GxEPD_YELLOW);
	}
	else if(AllData.Pressure < 780)
	{
		display.drawBitmap(245, 25, gImage_PressureNormBW, 112, 80, GxEPD_BLACK);		// Значок Norm атм.давления
		display.drawBitmap(245, 25, gImage_PressureMinNormR, 112, 80, GxEPD_RED);
		display.drawBitmap(245, 25, gImage_PressureNormY, 112, 80, GxEPD_YELLOW);
	}
	else
	{
		display.drawBitmap(245, 25, gImage_PressureMaxBW, 112, 80, GxEPD_BLACK);		// Значок Max атм.давления
		display.drawBitmap(245, 25, gImage_PressureMaxR, 112, 80, GxEPD_RED);
		display.drawBitmap(245, 25, gImage_PressureMinMaxY, 112, 80, GxEPD_YELLOW);
		
	}
#ifndef NO_DS1307
	// Значок Повышения или понижения
	if(AllData.Pressure < AllDataOld.Pressure)
		display.drawBitmap(317, 80, gImage_Down, 32, 32, GxEPD_BLACK);			// Если понижается
	else if(AllData.Pressure > AllDataOld.Pressure)
		display.drawBitmap(317, 80, gImage_Up, 32, 32, GxEPD_BLACK);			// Если повышается
	// Если значение не изменилось, то значок не показываем
#endif
	// Значение
	display.setFont(&FreeSansBold24pt7b);
	display.setTextColor(GxEPD_BLACK);
	display.setTextSize(1);
	display.setCursor(257, 148);

	itoa(AllData.Pressure, buffer, 10);
	display.print(buffer);

#ifdef DEBUG
	Serial.print("Новое давление: ");
	Serial.println(buffer);
	itoa(AllDataOld.Pressure, buffer, 10);
	Serial.print("  Старое давление: ");
	Serial.println(buffer);
#endif

#ifdef DEBUG
	Serial.println("Нижний колонтитул.");
#endif
	// Нижний колонтитул
	display.fillRect(0, 161, 360, 3, GxEPD_BLACK);
	display.fillRect(0, 164, 360, 20, GxEPD_YELLOW);


#ifdef DEBUG
	Serial.print("Общие аварии: ");
	Serial.println(AllData.Err.Errors);
#endif

	if(AllData.Err.Errors == 0)	// Если никаких аварий нет
	{
		display.setFont(&FreeSansBold10pt8b);
		display.setTextColor(GxEPD_RED);
		display.setTextSize(1);
		display.setCursor(50, 180);
		display.print("Удачного дня!!! ");
	}
	else	// Если хоть что-то есть, выводим по степени важности
	{
		// Статус аварий внизу
		display.setFont(&FreeSansBold10pt8b);
		display.setTextColor(GxEPD_BLACK);
		display.setTextSize(1);
		display.setCursor(3, 180);
		display.print("Ошибки: ");

		String ErrStr = "";
		if(AllData.Err.Bits.DS1307Z)		// Авария МС часов
			ErrStr = ErrStr + "|TM| ";
		if(AllData.Err.Bits.BME280)		// Авария МС часов
			ErrStr = ErrStr + "|BME| ";
		if(AllData.Err.Bits.Battery)		// Авария МС часов
			ErrStr = ErrStr + "|Bt| ";
		if(AllData.Err.Bits.WiFi)		// Авария МС часов
			ErrStr = ErrStr + "|WF| ";
		if(AllData.Err.Bits.NTP)		// Авария МС часов
			ErrStr = ErrStr + "|NTP|";
		if(AllData.Err.Bits.TimeRST)	// Был сброс времени
			ErrStr = ErrStr + "|TM_Rst|";

		
		display.setTextColor(GxEPD_RED);
		display.setCursor(95, 180);
		display.print(ErrStr.c_str());
#ifdef DEBUG
		Serial.print("Строка аварий: ");
		Serial.print(ErrStr);
		Serial.print("\r\n");
#endif

	}

#ifdef POWER_AKB_32V		// Для АКБ 3,2В
	// Значок батарейки
	display.drawBitmap(260, 165, gImage_BatteryGenY, 40, 18, GxEPD_YELLOW);
	display.drawBitmap(260, 165, gImage_BatteryGenBW, 40, 18, GxEPD_BLACK);
	if(Battery >= 3.1)
		display.drawBitmap(260, 165, gImage_BatteryMaxBW, 40, 18, GxEPD_BLACK);		// Значок батарейки   320
	else if(Battery <= 3.0)
		display.drawBitmap(260, 165, gImage_BatteryMinR, 40, 18, GxEPD_RED);		// Значок батарейки
	else
		display.drawBitmap(260, 165, gImage_BatteryHalfR, 40, 18, GxEPD_RED);		// Значок батарейки
	// Уровень заряда
	display.setFont(&FreeSansBold10pt8b);
	display.setTextColor(GxEPD_BLACK);
	display.setTextSize(1);
	display.setCursor(310, 180);
	dtostrf(Battery, 3, 1, buffer);
	display.print(buffer);
	display.print(" В");


#endif

#ifdef POWER_AKB_42V		// Для АКБ 4,2В																		!!!! Пересчитать!
	// Значок батарейки
	display.drawBitmap(320, 165, gImage_BatteryGenY, 40, 18, GxEPD_YELLOW);
	display.drawBitmap(320, 165, gImage_BatteryGenBW, 40, 18, GxEPD_BLACK);
	if(Battery >= 3600)
		display.drawBitmap(320, 165, gImage_BatteryMaxBW, 40, 18, GxEPD_BLACK);		// Значок батарейки
	else if(Battery <= 3400)
		display.drawBitmap(320, 165, gImage_BatteryMinR, 40, 18, GxEPD_RED);		// Значок батарейки
	else
		display.drawBitmap(320, 165, gImage_BatteryHalfR, 40, 18, GxEPD_RED);		// Значок батарейки
#endif


	// Сформировали полностью картинку экрана
#ifdef DEBUG
	Serial.println("----- Выведим на экран. -----------");
#endif
	// Вывести всё это на экран
	display.display();
	// Экран в экономичный режим.
	display.hibernate();
#endif


// --------------------------------------------------------------------------------------
// Раздел 6. Выключаемся
// --------------------------------------------------------------------------------------
#ifdef DEBUG
	Serial.println("----- Закончили! Выключаемся! -----------");
#endif
	digitalWrite(PWR_PIN, HIGH);
	
}


void loop() {

	// Пусто: прибор не работает постоянно. Весь алгоритм выполняется один раз
	// за включение в setup(), после чего питание снимается (см. комментарий вверху файла).

}
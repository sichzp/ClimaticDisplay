/**
 * @file DS1307.h
 * @brief Собственный драйвер часов реального времени DS1307Z (I2C 0x68).
 * @details Используется в прошивке ClimaticDisplay 2.66G. Особенности:
 *          - часы хранят ЛОКАЛЬНОЕ время (как и ESPHome-компонент ds1307_local —
 *            совместимость позволяет переключаться между версиями прошивки);
 *          - помимо даты/времени работает с 56 байтами RAM МС, где хранятся
 *            прошлые показания датчиков и биты ошибок (см. Var.h).
 */

#ifndef _DS1307_H
#define _DS1307_H

#include <stdint.h>

#define I2C_ADDRESS_DS_1307 		0x68	// Адрес DS_1307

/**
* @brief Структура Даты/Времени 
* @details  Для пакетного считывания или записи в МС часов реального времени
*/
typedef union
{
	uint8_t Buff[8];						// Буфер, чтобы скопом писать или читать данные
	struct
	{
		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:3;			// Старшая часть числа BCD
				uint8_t ClockOn	:1;			// 0 - часы идут, 1 - часы стоят. (всегда писать 0!!!)
			};
		} Second;							// Секунды		00-59	(бит 7 всегда должен быть = 0, чтоб часы шли)

		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:3;			// Старшая часть числа BCD
				uint8_t Zero	:1;			// Всегда 0
			};
		} Minutes;							// Минуты		00-59
		
		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:2;			// Старшая часть числа BCD
				uint8_t Mode	:1;			// Режим: 0 - 24-й формат, 1 - 12-ти часовой. (Нам надо 0!!!)
				uint8_t Zero	:1;			// Всегда 0
			};
		} Hour;								// Часы			0-23	(бит 7 всегда должен быть = 0, чтоб был 24-х часовой формат)
		
		uint8_t Week;						// День недели	1-7
		
		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:2;			// Старшая часть числа BCD
				uint8_t Zero	:2;			// Всегда 0
			};
		} Date;								// День месяца	1-31
		
		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:1;			// Старшая часть числа BCD
				uint8_t Zero	:3;			// Всегда 0
			};
		} Month;							// Месяц		1-12
		
		union
		{
			uint8_t Dat;						// Полное число
			struct
			{
				uint8_t BCD_1	:4;			// Младшая часть числа BCD
				uint8_t BCD_2	:4;			// Старшая часть числа BCD
			};
		} Year;								// Год			00-99
		
		uint8_t Config;	// Байт конфигурации ноги с меандром. Нам не надо, поэтому всегда будет = 0.
	};
} DS_1307_DateTime_t;



class C_DS_1307
{

public:
	
	DS_1307_DateTime_t DateTime;
	bool Error;						// false - всё Ок, true - МС не ответила
	bool Actual;					// true - даннные актуальны, false - нет. (если год < 26, данные не актуальны)

	C_DS_1307();					// Крнструктор чисто для проверки, есть МС на линии или нет

	void GetDateTime(void);
	void SetDateTime(void);
	uint8_t GetBytes(uint8_t *ByteMass, uint8_t StartAdress, uint8_t Num);
	uint8_t SetBytes(uint8_t *ByteMass, uint8_t StartAdress, uint8_t Num);
	uint8_t BcdToBin(uint8_t);
	uint8_t BinToBcd(uint8_t);

private:
	
};




#endif
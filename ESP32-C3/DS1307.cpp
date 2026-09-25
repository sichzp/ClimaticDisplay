
/**
 * @file DS1307.cpp
 * @brief Реализация драйвера часов реального времени DS1307Z (см. DS1307.h).
 */

#include "Arduino.h"
#include <Wire.h>
#include "DS1307.h"

// Локальный отладочный вывод в Serial. Раскомментировать при отладке
// (в релизной версии отключён, как и DEBUG в ESP32-C3.ino).
//#define DEBUG

/**
* @brief Конструктор.
* @note Просто проверяет есть ли МС на линии
* @retval None
*/
C_DS_1307::C_DS_1307()
{
	Error = false;								// Изначально ошибок нет
	Wire.begin();
	Wire.beginTransmission(I2C_ADDRESS_DS_1307);	// Начинаем передавать данные
	Wire.write(0x03);						// Адрес регистра дня недели. Он не может быть ноль
	Wire.endTransmission();

	Wire.requestFrom(I2C_ADDRESS_DS_1307, 0x01);		// Запросим один регистр


	if(Wire.available() == 0)				// Если не приняли ни одного байта
	{
		Error = true;						//Ошибка МС не ответила
#ifdef DEBUG
			Serial.print("Не приняли ни одного байта");
#endif
	}
	else
	{
		if(Wire.read() == 0)				// Если принятый байт равен нулю (день недели не может быть равен нулю)
			Error = true;					//Ошибка
	}

}

/**
* @brief Преобразовать BCD в DEC
* @param [in] k - Значение в формате BCD
* @retval Преобразованное значение
*/
uint8_t C_DS_1307::BcdToBin(uint8_t k)
{
	return ((k >> 4) * 10) + (k & 0x0F);
}

/**
* @brief Преобразовать DEC в BCD
* @param [in] k - Значение в формате DEC
* @retval Преобразованное значение
*/
uint8_t C_DS_1307::BinToBcd(uint8_t k)
{
	return (((k - (k % 10)) / 10) << 4) | (k % 10);
}

/**
* @brief Читает дату и время из МС
* @note Читает первые 8 регистров и заполняет струтуру Даты/времени
* @retval None
*/
void C_DS_1307::GetDateTime(void)
{
	// Прочитаем первые 8 байт
	Wire.beginTransmission(I2C_ADDRESS_DS_1307);	// Начинаем передавать данные
	Wire.write(0x00);						// Читаем с нулевого регистра
	Wire.endTransmission();

	Wire.requestFrom(I2C_ADDRESS_DS_1307, 0x08);		// Запросим 8 регистров
	
	for(uint8_t i = 0; i < 8; i++)
		DateTime.Buff[i] = Wire.read();

	// Проверка актуальности данных
	if(DateTime.Year.Dat < 26) 
		Actual = false;					// Данные не актуальны. МС сброшена в исходное состояние
	else
		Actual = true;					// Данные актуальны
}

/**
* @brief Записывает дату и время в МС
* @note Пишет сразу все 8 первых регистров
* @retval None
*/
void C_DS_1307::SetDateTime(void)
{

	DateTime.Second.ClockOn = 0;	// запушщены или не запущены часы, запускаем в любом случае
	DateTime.Config = 0;					// Обнулим байт кофигурации вывода с меандром. Нам не надо.
	
	// передадим 8 байт
	Wire.beginTransmission(I2C_ADDRESS_DS_1307);	// Начинаем передавать данные
	Wire.write(0x00);								// Пишем с нулевого регистра
	for(uint8_t i = 0; i < 8; i++)
		Wire.write(DateTime.Buff[i]);
	Wire.endTransmission();

	// Проверка актуальности данных
	if(DateTime.Year.Dat < 26) 
		Actual = false;					// Данные не актуальны. МС сброшена в исходное состояние
	else
		Actual = true;					// Данные актуальны
}

/**
* @brief Читает Num байт из RAM МС
* @note Читает подряд байты, начиная с данного адреса
* @param [out] ByteMass - адрес массива, куда читаем
* @param [in] filter - начальный адрес МС с которого читем
* @param [in] filter - Кол-во читаемых байт
* @retval Кол-во реально прочитанных байт
*/
uint8_t C_DS_1307::GetBytes(uint8_t *ByteMass, uint8_t StartAdress, uint8_t Num)
{
	if((StartAdress < 8) || (StartAdress > 0x3F)) return 0; // Вернем ошибку - Не верный адрес
	if(Num > (0x40 - StartAdress)) Num = 0x40 - StartAdress; // чтобы было не больше чем все ОЗУ

	Wire.beginTransmission(I2C_ADDRESS_DS_1307);	// Начинаем передавать данные
	Wire.write(StartAdress);						// Читаем с нужного адреса
	Wire.endTransmission();

	Wire.requestFrom(I2C_ADDRESS_DS_1307, Num);		// Запросим Num регистров
	
	for(uint8_t i = 0; i < Num; i++)
		ByteMass[i] = Wire.read();

	return Num; // Вернем кол-во считанных байт
}


/**
* @brief Записывает Num байт в RAM МС
* @note Пишет подряд байты, начиная с данного адреса
* @param [in] ByteMass - адрес массива записываемых байт
* @param [in] filter - начальный адрес МС с которого пишем
* @param [in] filter - Кол-во записываемых байт
* @retval Кол-во записанных байт
*/
uint8_t C_DS_1307::SetBytes(uint8_t *ByteMass, uint8_t StartAdress, uint8_t Num)
{
	if((StartAdress < 8) || (StartAdress > 0x3F)) return 0; // Вернем ошибку - Не верный адрес
	if(Num > (0x40 - StartAdress)) Num = 0x40 - StartAdress; // чтобы было не больше чем все ОЗУ

	Wire.beginTransmission(I2C_ADDRESS_DS_1307);	// Начинаем передавать данные
	Wire.write(StartAdress);						// Пишем с нужного адреса
	for(uint8_t i = 0; i < Num; i++)
		Wire.write(ByteMass[i]);
	Wire.endTransmission();

	return Num; // Вернем кол-во записанных байт
}


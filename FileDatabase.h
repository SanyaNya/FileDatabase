#pragma once

#include <stdint.h>
#include <stdarg.h>

//DB FORMAT
/*
DB_INFO
{
	int64_t fileSize;
	int64_t fakeFileSize;
	int64_t objectsCount;
}
DB_OBJECT#(1)
{
	uint32_t size;
	uint32_t fakeSize;
	byte data[size];
}
.
.
.
DB_OBJECT#(objectsCount - 1)
{
	uint32_t size;
	uint32_t fakeSize;
	byte data[size];
}
*/

//Ошибки формата базы данных
enum DB_FORMAT_ERROR
{
	DB_WITHOUT_FORMAT_ERRORS,
	DB_OPEN_ERROR,
	DB_INVALID_FILE_HEAD_FORMAT,
	DB_FILE_SIZE_IS_TOO_BIG,
	DB_FILE_SIZE_IS_NEGATIVE,
	DB_FAKE_FILE_SIZE_IS_TOO_BIG,
	DB_FAKE_FILE_SIZE_IS_NEGATIVE,
	DB_OBJECTS_COUNT_IS_NEGATIVE,
	DB_INVALID_OBJECT_HEAD_FORMAT,
	DB_INVALID_FILE_SIZE,
	DB_INVALID_FILE_FAKE_SIZE,
	DB_INVALID_OBJECT_FAKE_SIZE,
	DB_INVALID_OBJECTS_COUNT
};

//структура, сожержащая необходимую информацию для работы с базой данных
struct DB_Info
{
	int64_t fileSize;     //Размер файла
	int64_t fakeFileSize; //Размер используемого места в файле
	int64_t objectsCount; //Количество обьектов

	const char* filePath; //Путь к файлу базы данных
	FILE* stream;         //Поток файла базы данных

	char readReady;       //Готовность потока к чтению
	char writeReady;      //Готовность потока к записи

	int64_t currentIndex; //Текущий индекс обьекта
};

//Стандартная функция записи в поток
//stream - поток
//size - размер данных
//args - 1 аргумент - записываемый объект
void DB_StandardWriteFunc(FILE* stream, uint32_t size, va_list args);

//Стандартная функция чтения из потока
//stream - поток
//size - размер данных
//args - 1 аргумент - буфер, в который будут записываться данные
void DB_StandardReadFunc(FILE* stream, uint32_t size, va_list args);

//Возвращает размер файла базы даных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
int64_t DB_FileSize(struct DB_Info* dbInfo_ptr);

//Возвращает используемый размер файла базы данных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
int64_t DB_FakeFileSize(struct DB_Info* dbInfo_ptr);

//Возвращает количество объектов в базе данных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
int64_t DB_ObjectsCount(struct DB_Info* dbInfo_ptr);

//Освобождение неиспользуемой памяти в файле базы данных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
int DB_FreeUnusedMemory(struct DB_Info* dbInfo_ptr);

//Создание файла базы данных и открытие потока
//out_dbInfo - указатель на структуру, содержащую информацию о базе данных, которая будет проинициализирована
//path - путь к файлу базы данных, если файл существует, то он будет перезаписан
//В случае успеха возвращает 0, иначе - ненулевое значение
int DB_Create(struct DB_Info* out_dbInfo, const char* path);

//Открытие потока базы данных
//out_dbInfo - указатель на структуру, содержащую информацию о базе данных, которая будет проинициализирована
//path - путь к файлу базы данных
//В случае успеха возвращает 0, иначе - ненулевое значение
int DB_Open(struct DB_Info* out_dbInfo, const char* path);

//Закрытие потока базы данных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
void DB_Close(struct DB_Info* dbInfo_ptr);

//Получение объекта базы данных
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
//out_size - выходной размер объекта
//read - функция чтения из потока, должна смещать позицию в файле на велечину размера объекта
//index - номер объекта
//... - дополнительные аргументы, которые передаются в функцию read
void DB_Get(struct DB_Info* dbInfo_ptr, uint32_t* out_size, void(*read)(FILE*, uint32_t, va_list), uint32_t index, ...);

//Изменение значения объекта
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
//size - размер объекта
//write - функция записи в поток, жолэна смещать позицию в файле на велечину размера объекта
//index - номер объекта
//... - дополнительные аргументы, которые передаются в функцию write
int DB_Set(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), uint32_t index, ...);

//Добавление объекта
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
//size - размер объекта
//write - функция записи в поток, жолэна смещать позицию в файле на велечину размера объекта
//... - дополнительные аргументы, которые передаются в функцию write
void DB_Add(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), ...);

//Удаление объекта
//dbInfo_ptr - указатель на структуру, сожержащую необходимую информацию для работы с базой данных
//index - номер объекта
void DB_RemoveAt(struct DB_Info* dbInfo_ptr, int64_t index);

//Проверка на валидность формата файла базы данных
//dbPath - путь к файлу базы данных
enum DB_FORMAT_ERROR DB_IsInvalidFormat(const char* dbPath);

//Возвращает строку, соотвествующую ошибке err
const char* DB_FORMAT_ERROR_ToString(enum DB_FORMAT_ERROR err);
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

//������ ������� ���� ������
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

//���������, ���������� ����������� ���������� ��� ������ � ����� ������
struct DB_Info
{
	int64_t fileSize;     //������ �����
	int64_t fakeFileSize; //������ ������������� ����� � �����
	int64_t objectsCount; //���������� ��������

	const char* filePath; //���� � ����� ���� ������
	FILE* stream;         //����� ����� ���� ������

	char readReady;       //���������� ������ � ������
	char writeReady;      //���������� ������ � ������

	int64_t currentIndex; //������� ������ �������
};

//����������� ������� ������ � �����
//stream - �����
//size - ������ ������
//args - 1 �������� - ������������ ������
void DB_StandardWriteFunc(FILE* stream, uint32_t size, va_list args);

//����������� ������� ������ �� ������
//stream - �����
//size - ������ ������
//args - 1 �������� - �����, � ������� ����� ������������ ������
void DB_StandardReadFunc(FILE* stream, uint32_t size, va_list args);

//���������� ������ ����� ���� �����
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
int64_t DB_FileSize(struct DB_Info* dbInfo_ptr);

//���������� ������������ ������ ����� ���� ������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
int64_t DB_FakeFileSize(struct DB_Info* dbInfo_ptr);

//���������� ���������� �������� � ���� ������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
int64_t DB_ObjectsCount(struct DB_Info* dbInfo_ptr);

//������������ �������������� ������ � ����� ���� ������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
int DB_FreeUnusedMemory(struct DB_Info* dbInfo_ptr);

//�������� ����� ���� ������ � �������� ������
//out_dbInfo - ��������� �� ���������, ���������� ���������� � ���� ������, ������� ����� �������������������
//path - ���� � ����� ���� ������, ���� ���� ����������, �� �� ����� �����������
//� ������ ������ ���������� 0, ����� - ��������� ��������
int DB_Create(struct DB_Info* out_dbInfo, const char* path);

//�������� ������ ���� ������
//out_dbInfo - ��������� �� ���������, ���������� ���������� � ���� ������, ������� ����� �������������������
//path - ���� � ����� ���� ������
//� ������ ������ ���������� 0, ����� - ��������� ��������
int DB_Open(struct DB_Info* out_dbInfo, const char* path);

//�������� ������ ���� ������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
void DB_Close(struct DB_Info* dbInfo_ptr);

//��������� ������� ���� ������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
//out_size - �������� ������ �������
//read - ������� ������ �� ������, ������ ������� ������� � ����� �� �������� ������� �������
//index - ����� �������
//... - �������������� ���������, ������� ���������� � ������� read
void DB_Get(struct DB_Info* dbInfo_ptr, uint32_t* out_size, void(*read)(FILE*, uint32_t, va_list), uint32_t index, ...);

//��������� �������� �������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
//size - ������ �������
//write - ������� ������ � �����, ������ ������� ������� � ����� �� �������� ������� �������
//index - ����� �������
//... - �������������� ���������, ������� ���������� � ������� write
int DB_Set(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), uint32_t index, ...);

//���������� �������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
//size - ������ �������
//write - ������� ������ � �����, ������ ������� ������� � ����� �� �������� ������� �������
//... - �������������� ���������, ������� ���������� � ������� write
void DB_Add(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), ...);

//�������� �������
//dbInfo_ptr - ��������� �� ���������, ���������� ����������� ���������� ��� ������ � ����� ������
//index - ����� �������
void DB_RemoveAt(struct DB_Info* dbInfo_ptr, int64_t index);

//�������� �� ���������� ������� ����� ���� ������
//dbPath - ���� � ����� ���� ������
enum DB_FORMAT_ERROR DB_IsInvalidFormat(const char* dbPath);

//���������� ������, �������������� ������ err
const char* DB_FORMAT_ERROR_ToString(enum DB_FORMAT_ERROR err);
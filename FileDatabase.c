#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "FileDatabase.h"
#include <errno.h>

#define _DB_HELP_FILE_NAME "db.DB_HELPFILE"

#define _DB_FILE_MAX_SIZE (INT64_MAX - 3*sizeof(int64_t))
#define _DB_OBJECT_MAX_SIZE (UINT32_MAX - 2*sizeof(uint32_t))

static const char* DB_FORMAT_ERROR_Strings[] = 
{
	"No errors",
	"Error opening database",
	"Incorrect database header",
	"Database size is too large",
	"Database size is less than zero",
	"Database fake size is too large",
	"Database fake size is less than zero",
	"The number of objects in the database is less than zero",
	"Incorrect database object header",
	"Incorrect database size",
	"Incorrect database fake size",
	"Incorrect object fake size",
	"Incorrect number of objects in the database"
};

static void _fseekCur_uint32(FILE* stream, uint32_t offset, int side)
{
	fpos_t pos;
	fgetpos(stream, &pos);
	pos += side * (int64_t)offset;
	fsetpos(stream, &pos);
}
static void _fseekSet_uint32(FILE* stream, uint32_t offset)
{
	fpos_t pos = offset;
	fsetpos(stream, &pos);
}

static void _DB_SetIndexAndReadSizes(struct DB_Info* dbInfo_ptr, uint32_t sizes[2], int64_t index)
{
	if (dbInfo_ptr->currentIndex < index && !dbInfo_ptr->readReady)
	{
		fflush(dbInfo_ptr->stream);
	}
	else if (dbInfo_ptr->currentIndex > index || index == 0)
	{
		fseek(dbInfo_ptr->stream, 3 * sizeof(int64_t), SEEK_SET);

		dbInfo_ptr->currentIndex = 0;
	}

	while (1)
	{
		fread(sizes, sizeof(uint32_t), 2, dbInfo_ptr->stream);

		if (sizes[1] != 0 && dbInfo_ptr->currentIndex == index) return;
		else if (sizes[1] != 0) dbInfo_ptr->currentIndex++;

		_fseekCur_uint32(dbInfo_ptr->stream, sizes[0], 1);
	}
}

static void _DB_IncrementIndexAfterGet(struct DB_Info* dbInfo_ptr, uint32_t sizes[2])
{
	if (dbInfo_ptr->currentIndex == dbInfo_ptr->objectsCount - 1)
	{
		_fseekCur_uint32(dbInfo_ptr->stream, 2 * sizeof(uint32_t) + sizes[1], -1);
		return;
	}

	if (sizes[0] > sizes[1]) _fseekCur_uint32(dbInfo_ptr->stream, sizes[0] - sizes[1], 1);

	while (1)
	{
		fread(sizes, sizeof(uint32_t), 2, dbInfo_ptr->stream);

		if (sizes[1] != 0)
		{
			fseek(dbInfo_ptr->stream, -2 * sizeof(uint32_t), SEEK_CUR);

			dbInfo_ptr->currentIndex++;

			dbInfo_ptr->readReady = 1;
			dbInfo_ptr->writeReady = 0;

			return;
		}

		_fseekCur_uint32(dbInfo_ptr->stream, sizes[0], 1);
	}
}

static void _DB_SetIndex(struct DB_Info* dbInfo_ptr, int64_t index)
{
	if (dbInfo_ptr->currentIndex == index) return;
	else if (index == dbInfo_ptr->objectsCount)
	{
		fpos_t pos = 3 * sizeof(int64_t) + dbInfo_ptr->fileSize;
		fsetpos(dbInfo_ptr->stream, &pos);

		dbInfo_ptr->currentIndex = dbInfo_ptr->objectsCount;

		dbInfo_ptr->readReady = 1;
		dbInfo_ptr->writeReady = 1;

		return;
	}
	else if (dbInfo_ptr->currentIndex < index && !dbInfo_ptr->readReady)
	{
		fflush(dbInfo_ptr->stream);
	}
	else if (dbInfo_ptr->currentIndex > index || index == 0)
	{
		fseek(dbInfo_ptr->stream, 3 * sizeof(int64_t), SEEK_SET);

		dbInfo_ptr->currentIndex = 0;
	}

	uint32_t sizes[2];

	while (1)
	{
		fread(sizes, sizeof(uint32_t), 2, dbInfo_ptr->stream);

		if (sizes[1] != 0)
		{
			if (dbInfo_ptr->currentIndex == index)
			{
				fseek(dbInfo_ptr->stream, -2 * sizeof(uint32_t), SEEK_CUR);

				dbInfo_ptr->readReady = 1;
				dbInfo_ptr->writeReady = 1;

				return;
			}

			dbInfo_ptr->currentIndex++;
		}

		_fseekCur_uint32(dbInfo_ptr->stream, sizes[0], 1);
	}
}


void DB_StandardWriteFunc(FILE* stream, uint32_t size, va_list args)
{
	fwrite((void*)args, size, 1, stream);
}
void DB_StandardReadFunc(FILE* stream, uint32_t size, va_list args)
{
	fread(*(void**)args, size, 1, stream);
}

int64_t DB_FileSize(struct DB_Info* dbInfo_ptr)
{
	return dbInfo_ptr->fileSize;
}
int64_t DB_FakeFileSize(struct DB_Info* dbInfo_ptr)
{
	return dbInfo_ptr->fakeFileSize;
}
int64_t DB_ObjectsCount(struct DB_Info* dbInfo_ptr)
{
	return dbInfo_ptr->objectsCount;
}


int DB_FreeUnusedMemory(struct DB_Info* dbInfo_ptr)
{
	FILE* helpFileStream = fopen(_DB_HELP_FILE_NAME, "w");

	if (helpFileStream == NULL) return 1;

	helpFileStream = freopen(_DB_HELP_FILE_NAME, "r+b", helpFileStream);

	if (helpFileStream == NULL) return 2;

	if (dbInfo_ptr->objectsCount == 0)
	{
		dbInfo_ptr->fileSize = 0;

		fwrite(dbInfo_ptr, sizeof(int64_t), 3, helpFileStream);

		fclose(dbInfo_ptr->stream);
		fclose(helpFileStream);

		remove(dbInfo_ptr->filePath);

		rename(_DB_HELP_FILE_NAME, dbInfo_ptr->filePath);

		dbInfo_ptr->stream = fopen(dbInfo_ptr->filePath, "r+b");

		fseek(dbInfo_ptr->stream, 3 * sizeof(int64_t), SEEK_SET);

		dbInfo_ptr->currentIndex = 0;

		dbInfo_ptr->readReady = 0;
		dbInfo_ptr->writeReady = 1;
	}
	else
	{
		dbInfo_ptr->fileSize = dbInfo_ptr->fakeFileSize;

		fwrite(dbInfo_ptr, sizeof(int64_t), 3, helpFileStream);

		fseek(dbInfo_ptr->stream, 3 * sizeof(int64_t), SEEK_SET);

		uint32_t total = 0;

		while (total < dbInfo_ptr->fakeFileSize)
		{
			uint32_t sizes[2];

			fread(sizes, sizeof(uint32_t), 2, dbInfo_ptr->stream);

			if (sizes[1] == 0)
			{
				_fseekCur_uint32(dbInfo_ptr->stream, sizes[0], 1);
				continue;
			}

			uint32_t offset = sizes[0] - sizes[1];
			sizes[0] = sizes[1];

			fwrite(sizes, sizeof(uint32_t), 2, helpFileStream);

			for (uint32_t y = 0; y < sizes[1]; y++)
				fputc(fgetc(dbInfo_ptr->stream), helpFileStream);

			_fseekCur_uint32(dbInfo_ptr->stream, offset, 1);

			total += 2 * sizeof(uint32_t) + sizes[1];
		}

		fclose(dbInfo_ptr->stream);
		fclose(helpFileStream);

		printf("\nremove: %i\n %i %i %i\n", remove(dbInfo_ptr->filePath), ENOENT, EACCES, errno );

		rename(_DB_HELP_FILE_NAME, dbInfo_ptr->filePath);

		dbInfo_ptr->stream = fopen(dbInfo_ptr->filePath, "r+b");

		fseek(dbInfo_ptr->stream, 3 * sizeof(int64_t), SEEK_SET);

		dbInfo_ptr->currentIndex = 0;

		dbInfo_ptr->readReady = 1;
		dbInfo_ptr->writeReady = 1;
	}

	return 0;
}

int DB_Create(struct DB_Info* out_dbInfo, const char* path)
{
	out_dbInfo->filePath = path;
	out_dbInfo->stream = fopen(path, "wb");

	if (out_dbInfo->stream == NULL) return 1;

	out_dbInfo->stream = freopen(path, "r+b", out_dbInfo->stream);

	if (out_dbInfo->stream == NULL) return 2;

	int64_t nullArr[3] = { 0,0,0 };

	fwrite(nullArr, sizeof(int64_t), 3, out_dbInfo->stream);

	out_dbInfo->fileSize = 0;
	out_dbInfo->fakeFileSize = 0;
	out_dbInfo->objectsCount = 0;

	out_dbInfo->currentIndex = 0;

	out_dbInfo->readReady = 0;
	out_dbInfo->writeReady = 1;

	return 0;
}

int DB_Open(struct DB_Info* out_dbInfo, const char* path)
{
	out_dbInfo->filePath = path;
	out_dbInfo->stream = fopen(path, "r+b");

	if (out_dbInfo->stream == NULL) return 1;

	fread((void*)out_dbInfo, sizeof(int64_t), 3, out_dbInfo->stream);

	out_dbInfo->readReady = 1;
	out_dbInfo->writeReady = 0;
	_DB_SetIndex(out_dbInfo, 0);

	return 0;
}

void DB_Close(struct DB_Info* dbInfo_ptr)
{
	rewind(dbInfo_ptr->stream);

	fwrite((void*)dbInfo_ptr, sizeof(int64_t), 3, dbInfo_ptr->stream);

	fclose(dbInfo_ptr->stream);
}

void DB_Get(struct DB_Info* dbInfo_ptr, uint32_t* out_size, void(*read)(FILE*, uint32_t, va_list), uint32_t index, ...)
{
	uint32_t sizes[2];
	_DB_SetIndexAndReadSizes(dbInfo_ptr, sizes, index);

	(*out_size) = sizes[1];

	va_list vlist;
	va_start(vlist, index);

	read(dbInfo_ptr->stream, *out_size, vlist);

	va_end(vlist);

	_DB_IncrementIndexAfterGet(dbInfo_ptr, sizes);
}

int DB_Set(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), uint32_t index, ...)
{
	_DB_SetIndex(dbInfo_ptr, index);

	if (!dbInfo_ptr->readReady) fflush(dbInfo_ptr->stream);

	uint32_t trueSize;

	fread(&trueSize, sizeof(uint32_t), 1, dbInfo_ptr->stream);

	if (size > trueSize)
	{
		fseek(dbInfo_ptr->stream, -sizeof(uint32_t), SEEK_CUR);
		return 1;
	}

	fseek(dbInfo_ptr->stream, 0, SEEK_CUR);

	fwrite(&size, sizeof(uint32_t), 1, dbInfo_ptr->stream);

	va_list vlist;
	va_start(vlist, index);

	write(dbInfo_ptr->stream, size, vlist);

	va_end(vlist);

	_fseekCur_uint32(dbInfo_ptr->stream, size + 2 * sizeof(uint32_t), -1);

	dbInfo_ptr->fakeFileSize -= trueSize - size;

	dbInfo_ptr->readReady = 1;
	dbInfo_ptr->writeReady = 1;

	return 0;
}

void DB_Add(struct DB_Info* dbInfo_ptr, uint32_t size, void(*write)(FILE*, uint32_t, va_list), ...)
{
	_DB_SetIndex(dbInfo_ptr, dbInfo_ptr->objectsCount);

	if (!dbInfo_ptr->writeReady) fseek(dbInfo_ptr->stream, 0, SEEK_CUR);

	fwrite(&size, sizeof(uint32_t), 1, dbInfo_ptr->stream);
	fwrite(&size, sizeof(uint32_t), 1, dbInfo_ptr->stream);

	va_list vlist;
	va_start(vlist, write);

	write(dbInfo_ptr->stream, size, vlist);

	va_end(vlist);

	dbInfo_ptr->currentIndex++;

	dbInfo_ptr->objectsCount++;

	dbInfo_ptr->fileSize += size + 2 * sizeof(uint32_t);
	dbInfo_ptr->fakeFileSize += size + 2 * sizeof(uint32_t);

	dbInfo_ptr->readReady = 1;
	dbInfo_ptr->writeReady = 1;
}

void DB_RemoveAt(struct DB_Info* dbInfo_ptr, int64_t index)
{
	_DB_SetIndex(dbInfo_ptr, index);

	if (!dbInfo_ptr->readReady) fflush(dbInfo_ptr->stream);

	uint32_t n = 0;
	uint32_t size;

	fread(&size, sizeof(uint32_t), 1, dbInfo_ptr->stream);
	fseek(dbInfo_ptr->stream, 0, SEEK_CUR);
	fwrite(&n, sizeof(uint32_t), 1, dbInfo_ptr->stream);

	_fseekCur_uint32(dbInfo_ptr->stream, size, 1);

	dbInfo_ptr->fakeFileSize -= size + 2 * sizeof(uint32_t);
	dbInfo_ptr->objectsCount--;

	dbInfo_ptr->readReady = 1;
	dbInfo_ptr->writeReady = 1;
}

enum DB_FORMAT_ERROR DB_IsInvalidFormat(const char* dbPath)
{
	FILE* stream = fopen(dbPath, "rb");

	if (stream == NULL) return DB_OPEN_ERROR;

	int64_t head[3];

	if (fread(head, sizeof(int64_t), 3, stream) != 3) { fclose(stream); return DB_INVALID_FILE_HEAD_FORMAT; }

	if (head[0] > _DB_FILE_MAX_SIZE) { fclose(stream); return DB_FILE_SIZE_IS_TOO_BIG; }
	if (head[0] < 0) { fclose(stream); return DB_FILE_SIZE_IS_NEGATIVE; }

	if (head[1] > _DB_FILE_MAX_SIZE) { fclose(stream); return DB_FAKE_FILE_SIZE_IS_TOO_BIG; }
	if (head[1] < 0) { fclose(stream); return DB_FAKE_FILE_SIZE_IS_NEGATIVE; }

	if (head[2] < 0) { fclose(stream); return DB_OBJECTS_COUNT_IS_NEGATIVE; }

	if (head[2] != 0 && head[0] == 0) { fclose(stream);  return DB_INVALID_FILE_SIZE; }
	if (head[2] == 0 && head[1] != 0) { fclose(stream);  return DB_INVALID_FILE_FAKE_SIZE; }

	if (head[2] == 0) { fclose(stream); return DB_WITHOUT_FORMAT_ERRORS; }

	for (int64_t x = 0; x <= head[2];)
	{
		fpos_t pos;

		fgetpos(stream, &pos);

		if (pos - 3 * sizeof(int64_t) == head[0])
		{
			if (x != head[2]) { fclose(stream); return DB_INVALID_OBJECTS_COUNT; }
			break;
		}

		if (pos - 3 * sizeof(int64_t) > head[0] - 1) { fclose(stream); return DB_INVALID_FILE_SIZE; }

		uint32_t sizes[2];

		if (fread(sizes, sizeof(uint32_t), 2, stream) != 2) { fclose(stream); return DB_INVALID_OBJECT_HEAD_FORMAT; }

		if (sizes[1] > sizes[0]) { fclose(stream);  return DB_INVALID_OBJECT_FAKE_SIZE; }

		pos += 2 * sizeof(uint32_t);

		if (pos - 3 * sizeof(int64_t) > head[0] - 1) { fclose(stream); return DB_INVALID_FILE_SIZE; }

		_fseekCur_uint32(stream, sizes[0], 1);

		if (sizes[1] > 0) x++;
	}

	fclose(stream);

	return DB_WITHOUT_FORMAT_ERRORS;
}

const char* DB_FORMAT_ERROR_ToString(enum DB_FORMAT_ERROR err)
{
	return DB_FORMAT_ERROR_Strings[err];
}
#ifndef __FIRMIMG_H__
#define __FIRMIMG_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <stdint.h>
#include <zlib.h>
#include <math.h>
#include <dirent.h>

#define DEFAULT_BUFFER 1024

#define IDRAC6_EXTENSION "d6"
#define IDRAC7_EXTENSION "d7"
#define IDRAC8_EXTENSION "d8"
#define IDRAC9_EXTENSION "d9"

// Untested DRAC family
#define DRAC2 2
#define DRAC3 3
#define DRAC4 4
#define DRAC5 5

#define IDRAC6 6
#define IDRAC7 7
#define IDRAC8 8
#define IDRAC9 9

#define FIRMIMG_HEADER_SIZE 512

typedef struct firmimg_entry
{
	uint32_t offset;
	uint32_t size;
	uint32_t checksum;
} firmimg_entry;

typedef struct firmimg_entry_info
{
	char* name;
	char* file_name;
	char* description;
} firmimg_entry_info;

typedef struct firmimg
{
	uint32_t header_checksum;
	int drac_family;
	uint16_t num_entries;
	char release[16];
	uint16_t build;
	struct firmimg_entry* entries;
} firmimg;

typedef struct FIRMIMG_FILE
{
	FILE* fp;
	struct firmimg* firmware_image;
} FIRMIMG_FILE;

static firmimg_entry_info iDRAC6_schema[3] = {
	{
		.name = "uImage",
		.file_name = "kernel.uImage",
		.description = "Linux kernel uImage"
	},
	{
		.name = "cramfs",
		.file_name = "filesystem.cramfs",
		.description = "Filesystem"
	},
	{
		.name = "unknown",
		.file_name = "unknown.bin",
		.description = "Unknown data"
	}
};

static uint32_t fcrc32(FILE* fp, size_t offset, size_t count);
static void fcopy(FILE* src_fp, size_t offset, size_t count, FILE* dst_fp);

static int get_drac_family(const char* path);
static firmimg_entry_info* get_schema(const int idrac_family);

FIRMIMG_FILE* _firmimg_open(const char* file_path, const char* mode);
FIRMIMG_FILE* firmimg_open(const char* file_path);
FIRMIMG_FILE* firmimg_create(const char* file_path);
int firmimg_close(FIRMIMG_FILE* firmimg_fp);

#endif

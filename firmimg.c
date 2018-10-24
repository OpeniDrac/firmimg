#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <zlib.h>
#include <string.h>

#include "firmimg.h"

static uint32_t fcrc32(FILE *fp, size_t offset, size_t length)
{
	fseek(fp, offset, SEEK_SET);

	uint32_t crc32_checksum = crc32(0L, Z_NULL, 0);
	size_t left_length = length, read_size;
	Bytef buffer[512];

	while(left_length > 0)
	{
		read_size = (left_length > sizeof(buffer)) ? sizeof(buffer) : left_length;
		fread(buffer, sizeof(Bytef), read_size, fp);
		if(ferror(fp) != 0)
		{
			perror("Failed to read file");
			return 0;
		}

		crc32_checksum = crc32(crc32_checksum, buffer, read_size);
		left_length -= read_size;
	}

	return crc32_checksum;
}

static enum idrac_family_t get_idrac_family(const char *path)
{
	const char *file_name = strrchr(path, '/');
	char* file_extension = strrchr(((file_name == NULL) ? path : file_name), '.');
	if(file_extension == NULL)
		return IDRAC_Unknown;

	file_extension++;

	if(strcmp(file_extension, IDRAC6_EXTENSION) == 0)
		return IDRAC6;
	else if(strcmp(file_extension, IDRAC7_EXTENSION) == 0)
		return IDRAC7;
	else if(strcmp(file_extension, IDRAC8_EXTENSION) == 0)
		return IDRAC8;
	else if(strcmp(file_extension, IDRAC9_EXTENSION) == 0)
	{
		return IDRAC9;
	}

	return IDRAC_Unknown;
}

static void do_info(char *path)
{
	enum idrac_family_t idrac_family = get_idrac_family(path);
	if(idrac_family == IDRAC_Unknown)
	{
		puts("Unknown idrac family");
		return;
	}
	else if(idrac_family == IDRAC9)
	{
		puts("Not supported idrac family");
		return;
	}

	printf("Dell Remote Access Controller family : %d\n", idrac_family);

	FILE *firmimg_fp;
	firmimg_header_t *image_header;
	uint32_t header_checksum;
	firmimg_image_info_t *image_info;
	uint32_t image_checksum;
	int i;

	firmimg_fp = fopen(path, "r");
	if(firmimg_fp == NULL)
	{
		perror("Failed to open firmimg");
		exit(EXIT_FAILURE);
	}

	Bytef header_buffer[FIRMIMG_HEADER_SIZE];
	fread(header_buffer, sizeof(Bytef), FIRMIMG_HEADER_SIZE, firmimg_fp);
	if(ferror(firmimg_fp) != 0)
	{
		perror("Failed to read header");
		exit(EXIT_FAILURE);
	}

	image_header = (firmimg_header_t*)header_buffer;
	header_checksum = crc32(0, header_buffer + 4, FIRMIMG_HEADER_SIZE - 4);

	printf(
		"Header checksum: %x (%x)\n"
		"Header version: %d\n"
		"Num. of image(s): %d\n"
		"Firmimg Version: %d.%d (Build %d)\n"
		"Firmimg size: %d bytes\n"
		"U-Boot version: %d.%d.%d\n"
		"AVCT U-Boot version: %d.%d.%d\n"
		"Platform ID: %s (%s)\n",
		image_header->crc32, header_checksum,
		image_header->header_version,
		image_header->num_of_image,
		image_header->version.version, image_header->version.sub_version, image_header->version.build,
		image_header->image_size,
		image_header->uboot_ver[0], image_header->uboot_ver[1], image_header->uboot_ver[2],
		image_header->uboot_ver[4], image_header->uboot_ver[5], image_header->uboot_ver[6],
		(strcmp((char*)image_header->platform_id, IDRAC6_SVB_PLATFORM_ID) == 0) ? IDRAC6_SVB_IDENTIFIER : (strcmp((char*)image_header->platform_id, IDRAC6_WHOVILLE_PLATFORM_ID) == 0) ? IDRAC6_WHOVILLE_IDENTIFIER : "Unknown", image_header->platform_id);

	for(i = 0; i < image_header->num_of_image; i++)
	{
		image_info = (firmimg_image_info_t*)(header_buffer + sizeof(firmimg_header_t) + (i * sizeof(firmimg_image_info_t)));
		image_checksum = fcrc32(firmimg_fp, image_info->offset, image_info->size);

		printf(
			"Image %d:\n"
			"Offset: %d\n"
			"Size: %d bytes\n"
			"Checksum: %x (%x)\n",
			i,
			image_info->offset,
			image_info->size,
			image_info->crc32, image_checksum);
	}

	fclose(firmimg_fp);
}

static void usage(void)
{
	puts(
		"Usage: firmimg [OPTIONS]\n"
		"	--info=FILE	Print firmware image information of FILE\n"
		"	--help		Print help"
	);
	exit(EXIT_SUCCESS);
}

enum action_t
{
	NONE,
	INFO
};

int main(int argc, char *argv[])
{
	char *path_file;
	enum action_t action = NONE;
	int c;

	static struct option long_options[] =
	{
		{"info", required_argument, 0, 'i'},
		{"help", no_argument, 0, 'h'}
	};

	for(;;)
	{
		int option_index = 0;
		c = getopt_long(argc, argv, "i:", long_options, &option_index);

		if(c == -1)
			break;

		switch(c)
		{
			case 'i':
			{
				action = INFO;
				path_file = optarg;
				break;
			}
			case 'h':
			default:
				usage();
		}
	}

	switch(action)
	{
		case NONE:
			usage();
			break;
		case INFO:
			do_info(path_file);
			break;
	}

	return EXIT_SUCCESS;
}

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
			perror("Failed to read file for crc32");
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

static firmimg_t *firmimg_open(char *path)
{
	firmimg_t *firmimg = malloc(sizeof(firmimg_t));

	firmimg->idrac_family = get_idrac_family(path);
	if(firmimg->idrac_family == IDRAC_Unknown)
	{
		puts("Unknown idrac family");
		exit(EXIT_FAILURE);
	}
	else if(firmimg->idrac_family == IDRAC9)
	{
		puts("Not supported idrac family");
		exit(EXIT_FAILURE);
	}

	firmimg->fp = fopen(path, "r");
	if(firmimg->fp == NULL)
	{
		perror("Failed to open firmimg");
		exit(EXIT_FAILURE);
	}

	Bytef header_buffer[FIRMIMG_HEADER_SIZE];
	fread(header_buffer, sizeof(Bytef), FIRMIMG_HEADER_SIZE, firmimg->fp);
	if(ferror(firmimg->fp) != 0)
	{
		perror("Failed to read header");
		exit(EXIT_FAILURE);
	}

	memcpy(&firmimg->header, &header_buffer, sizeof(firmimg_header_t));
	firmimg->header_crc32 = crc32(0, header_buffer + 4, FIRMIMG_HEADER_SIZE - 4);

	firmimg->images = malloc(sizeof(firmimg_image_t) * firmimg->header.num_of_image);
	for(int i = 0; i < firmimg->header.num_of_image; i++)
	{
		firmimg_image_t image;
		memcpy(&image.info, header_buffer + sizeof(firmimg_header_t) + (i * sizeof(firmimg_image_info_t)), sizeof(firmimg_image_info_t));
		image.crc32 = fcrc32(firmimg->fp, image.info.offset, image.info.size);

		firmimg->images[i] = image;
	}

	return firmimg;
}

static int firmimg_close(firmimg_t *firmimg)
{
	int ret = fclose(firmimg->fp);
	free(firmimg->images);
	free(firmimg);

	return ret;
}

static void do_info(char *path)
{
	firmimg_t *firmimg = firmimg_open(path);

	printf(
		"Dell Remote Access Controller family : %d\n"
		"Header checksum: %x (%x)\n"
		"Header version: %d\n"
		"Num. of image(s): %d\n"
		"Firmimg Version: %d.%d (Build %d)\n"
		"Firmimg size: %d bytes\n"
		"U-Boot version: %d.%d.%d\n"
		"AVCT U-Boot version: %d.%d.%d\n"
		"Platform ID: %s (%s)\n",
		firmimg->idrac_family,
		firmimg->header.crc32, firmimg->header_crc32,
		firmimg->header.header_version,
		firmimg->header.num_of_image,
		firmimg->header.version.version, firmimg->header.version.sub_version, firmimg->header.version.build,
		firmimg->header.image_size,
		firmimg->header.uboot_ver[0], firmimg->header.uboot_ver[1], firmimg->header.uboot_ver[2],
		firmimg->header.uboot_ver[4], firmimg->header.uboot_ver[5], firmimg->header.uboot_ver[6],

		(strcmp((char*)firmimg->header.platform_id, IDRAC6_SVB_PLATFORM_ID) == 0) ? IDRAC6_SVB_IDENTIFIER : (strcmp((char*)firmimg->header.platform_id, IDRAC6_WHOVILLE_PLATFORM_ID) == 0) ? IDRAC6_WHOVILLE_IDENTIFIER : "Unknown", firmimg->header.platform_id);

	for(int i = 0; i < firmimg->header.num_of_image; i++)
	{
		firmimg_image_t image = firmimg->images[i];

		printf(
			"Image %d:\n"
			"Offset: %d\n"
			"Size: %d bytes\n"
			"Checksum: %x (%x)\n",
			i,
			image.info.offset,
			image.info.size,
			image.info.crc32, image.crc32);
	}

	firmimg_close(firmimg);
}

static void do_extract(char *path)
{
	firmimg_t *firmimg = firmimg_open(path);
	firmimg_image_t image;
	char image_path[22];
	FILE *image_fp;
	size_t left_length, read_size;
	Bytef buffer[512];
	uint32_t image_crc32;

	if(firmimg->header_crc32 != firmimg->header.crc32)
	{
		puts("Invalid header checksum");
		exit(EXIT_FAILURE);
	}

	printf("Found %d images !\n", firmimg->header.num_of_image);
	for(uint8_t i = 0; i < firmimg->header.num_of_image; i++)
	{
		image = firmimg->images[i];

		if(image.info.crc32 != image.crc32)
		{
			puts("Invalid image checksum");
			exit(EXIT_FAILURE);
		}

		printf("Image %d : ", i);

		snprintf(image_path, sizeof(image_path), "image_%hhu.dat", i);
		image_fp = fopen(image_path, "w+");
		if(image_fp == NULL)
		{
			perror("Failed to extract image file");
			exit(EXIT_FAILURE);
		}

		fseek(firmimg->fp, image.info.offset, SEEK_SET);

		left_length = image.info.size;
		while(left_length > 0)
		{
			read_size = (left_length > sizeof(buffer)) ? sizeof(buffer) : left_length;
			fread(buffer, sizeof(Bytef), read_size, firmimg->fp);
			if(ferror(firmimg->fp) != 0)
			{
				perror("Failed to read file for extraction");
				exit(EXIT_FAILURE);
			}

			fwrite(buffer, sizeof(Bytef), read_size, image_fp);
			if(ferror(image_fp) != 0)
			{
				perror("Failed to write file for extraction");
				exit(EXIT_FAILURE);
			}

			left_length -= read_size;
		}

		puts("Extracted !");

		printf("Image %d : ", i);
		image_crc32 = fcrc32(image_fp, 0, image.info.size);
		if(image_crc32 == image.info.crc32)
			puts("Valid checksum !");
		else
			puts("Invalid checksum !");

		fclose(image_fp);
	}

	firmimg_close(firmimg);
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
	INFO,
	EXTRACT
};

int main(int argc, char *argv[])
{
	char *path_file;
	enum action_t action = NONE;
	int c;

	static struct option long_options[] =
	{
		{"info", required_argument, 0, 'i'},
		{"extract", required_argument, 0, 'e'},
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
			case 'e':
			{
				action = EXTRACT;
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
		case EXTRACT:
			do_extract(path_file);
			break;
	}

	return EXIT_SUCCESS;
}

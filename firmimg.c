#include "firmimg.h"

#include <stdlib.h>
#include <getopt.h>
#include <zlib.h>
#include <string.h>
#include <fcntl.h>

static uint32_t fcrc32(FILE *fp, const long int offset, const long int length)
{
	uint32_t crc32_checksum;
	long int left_length;
	size_t read_size;
	Bytef buffer[512];

	fseek(fp, offset, SEEK_SET);

	crc32_checksum = crc32(0L, Z_NULL, 0);
	if(length < 0)
	{
		fseek(fp, offset, SEEK_END);
		left_length = ftell(fp);
	}
	else
	left_length = length;

	while(left_length > 0)
	{
		read_size = (left_length > (long int)sizeof(buffer)) ? (long int)sizeof(buffer) : left_length;
		if(fread(buffer, sizeof(Bytef), read_size, fp) != read_size)
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
	if(path == NULL)
		return IDRAC_Unknown;

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
		return IDRAC9;

	return IDRAC_Unknown;
}

static firmimg_t *firmimg_open(const char *path, const char *mode)
{
	firmimg_t *firmimg = malloc(sizeof(firmimg_t));

	firmimg->idrac_family = get_idrac_family(path);
	if(firmimg->idrac_family == IDRAC_Unknown)
	{
		puts("Unknown idrac family");
		free(firmimg);
		exit(EXIT_FAILURE);
	}
	else if(firmimg->idrac_family == IDRAC9)
	{
		puts("Not supported idrac family");
		free(firmimg);
		exit(EXIT_FAILURE);
	}

	firmimg->fp = fopen(path, mode);
	if(!firmimg->fp)
	{
		perror("Failed to open firmimg");
		free(firmimg);
		exit(EXIT_FAILURE);
	}

	firmimg->header.images = NULL;
	firmimg->header.header.num_of_image = 0;

	if(strcmp(mode, "r") == 0)
	{
		Bytef header_buffer[FIRMIMG_HEADER_SIZE];
		if(fread(header_buffer, sizeof(Bytef), FIRMIMG_HEADER_SIZE, firmimg->fp) != FIRMIMG_HEADER_SIZE)
		{
			perror("Failed to read header");
			fclose(firmimg->fp);
			free(firmimg);
			exit(EXIT_FAILURE);
		}

		memcpy(&firmimg->header.header, &header_buffer, sizeof(firmimg_header_t));

		firmimg->header.images = malloc(sizeof(firmimg_image_t) * firmimg->header.header.num_of_image);
		for(int i = 0; i < firmimg->header.header.num_of_image; i++)
		{
			firmimg_image_t image;
			memcpy(&image, header_buffer + sizeof(firmimg_header_t) + (i * sizeof(firmimg_image_t)), sizeof(firmimg_image_t));

			firmimg->header.images[i] = image;
		}
	}

	return firmimg;
}

static int firmimg_add(firmimg_t *firmimg, const char* path)
{
	FILE *fp_image;
	int index;
	uint32_t crc32_checksum;
	size_t read_size;
	Bytef buffer[512];

	if(!firmimg)
		return -1;

	if(firmimg->header.header.num_of_image >= FIRMIMG_MAX_IMAGES)
		return -1;

	fp_image = fopen(path, "r");
	if(!fp_image)
		return -1;

	index = firmimg->header.header.num_of_image;
	firmimg->header.header.num_of_image++;
	if(firmimg->header.images)
		firmimg->header.images = realloc(firmimg->header.images, sizeof(firmimg_image_t) * firmimg->header.header.num_of_image); 
	else
		firmimg->header.images = malloc(sizeof(firmimg_image_t) * firmimg->header.header.num_of_image);

	if(!firmimg->header.images)
	{
		fclose(fp_image);
		return -1;
	}

	if(index > 0)
		firmimg->header.images[index].offset = firmimg->header.images[index - 1].offset + firmimg->header.images[index - 1].size;
	else
		firmimg->header.images[index].offset = FIRMIMG_HEADER_SIZE;

	fseek(fp_image, 0L, SEEK_END);
	firmimg->header.images[index].size = ftell(fp_image);
	fseek(fp_image, 0L, SEEK_SET);

	fseek(firmimg->fp, firmimg->header.images[index].offset, SEEK_SET);

	crc32_checksum = crc32(0L, Z_NULL, 0);

	while((read_size = fread(buffer, sizeof(Bytef), sizeof(buffer), fp_image)) > 0)
	{
		if(fwrite(buffer, sizeof(Bytef), read_size, firmimg->fp) != read_size)
		{
			perror("Failed to write file for crc32");
			fclose(fp_image);
			return -1;
		}

		crc32_checksum = crc32(crc32_checksum, buffer, read_size);
	}

	firmimg->header.images[index].crc32 = crc32_checksum;

	fclose(fp_image);

	return index;
}

static int firmimg_close(firmimg_t *firmimg)
{
	/*uint32_t crc32_checksum;
	int i;*/

	if(!firmimg)
		return -1;

	if((fcntl(fileno(firmimg->fp), F_GETFL) & O_ACCMODE) == O_RDWR)
	{
		/*crc32_checksum = crc32(0L, Z_NULL, 0);
		crc32(crc32_checksum, (Bytef*)(&firmimg->header.header), sizeof(firmimg_header_t));

		for(i = 0; i < firmimg->header.header.num_of_image; i++)
			crc32(crc32_checksum, (Bytef*)(&firmimg->header.images[i]), sizeof(firmimg_image_t));

		firmimg->header.header.crc32 = crc32_checksum;*/

		fseek(firmimg->fp, 0L, SEEK_SET);
		fwrite(&firmimg->header.header, sizeof(char), sizeof(firmimg_header_t), firmimg->fp);
		fwrite(firmimg->header.images, sizeof(*firmimg->header.images), firmimg->header.header.num_of_image, firmimg->fp);
	}

	int ret = fclose(firmimg->fp);
	if(firmimg->header.images)
		free(firmimg->header.images);
	free(firmimg);

	return ret;
}

static void do_info(const char *path)
{
	firmimg_t *firmimg;
	uint32_t crc32_checksum;

	firmimg = firmimg_open(path, "r");

	crc32_checksum = fcrc32(firmimg->fp, 4, FIRMIMG_HEADER_SIZE - 4);

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
		firmimg->header.header.crc32, crc32_checksum,
		firmimg->header.header.header_version,
		firmimg->header.header.num_of_image,
		firmimg->header.header.version.version, firmimg->header.header.version.sub_version, firmimg->header.header.version.build,
		firmimg->header.header.image_size,
		firmimg->header.header.uboot_ver[0], firmimg->header.header.uboot_ver[1], firmimg->header.header.uboot_ver[2],
		firmimg->header.header.uboot_ver[4], firmimg->header.header.uboot_ver[5], firmimg->header.header.uboot_ver[6],

		(strcmp((char*)firmimg->header.header.platform_id, IDRAC6_SVB_PLATFORM_ID) == 0) ? IDRAC6_SVB_IDENTIFIER : (strcmp((char*)firmimg->header.header.platform_id, IDRAC6_WHOVILLE_PLATFORM_ID) == 0) ? IDRAC6_WHOVILLE_IDENTIFIER : "Unknown", firmimg->header.header.platform_id);

	for(int i = 0; i < firmimg->header.header.num_of_image; i++)
	{
		firmimg_image_t image = firmimg->header.images[i];
		crc32_checksum = fcrc32(firmimg->fp, image.offset, image.size);

		printf(
			"Image %d:\n"
			"Offset: %d\n"
			"Size: %d bytes\n"
			"Checksum: %x (%x)\n",
			i,
			image.offset,
			image.size,
			image.crc32, crc32_checksum);
	}

	firmimg_close(firmimg);
}

static void do_extract(const char *path)
{
	firmimg_t *firmimg;
	firmimg_image_t image;
	char image_path[22];
	FILE *image_fp;
	size_t left_length, read_size;
	Bytef buffer[512];
	uint32_t crc32_checksum;

	firmimg = firmimg_open(path, "r");

	crc32_checksum = fcrc32(firmimg->fp, 4, FIRMIMG_HEADER_SIZE - 4);
	if(crc32_checksum != firmimg->header.header.crc32)
	{
		puts("Invalid header checksum");
		firmimg_close(firmimg);
		exit(EXIT_FAILURE);
	}

	printf("Found %d images !\n", firmimg->header.header.num_of_image);
	for(uint8_t i = 0; i < firmimg->header.header.num_of_image; i++)
	{
		image = firmimg->header.images[i];

		crc32_checksum = fcrc32(firmimg->fp, image.offset, image.size);
		if(image.crc32 != crc32_checksum)
		{
			puts("Invalid image checksum");
			firmimg_close(firmimg);
			exit(EXIT_FAILURE);
		}

		printf("Image %d : ", i);

		snprintf(image_path, sizeof(image_path), "image_%hhu.dat", i);
		image_fp = fopen(image_path, "w+");
		if(image_fp == NULL)
		{
			perror("Failed to extract image file");
			firmimg_close(firmimg);
			exit(EXIT_FAILURE);
		}

		fseek(firmimg->fp, image.offset, SEEK_SET);

		left_length = image.size;
		while(left_length > 0)
		{
			read_size = (left_length > sizeof(buffer)) ? sizeof(buffer) : left_length;
			fread(buffer, sizeof(Bytef), read_size, firmimg->fp);
			if(fread(buffer, sizeof(Bytef), read_size, firmimg->fp) != read_size)
			{
				perror("Failed to read file for extraction");
				firmimg_close(firmimg);
				fclose(image_fp);
				exit(EXIT_FAILURE);
			}

			if(fwrite(buffer, sizeof(Bytef), read_size, image_fp) != read_size)
			{
				perror("Failed to write file for extraction");
				firmimg_close(firmimg);
				fclose(image_fp);
				exit(EXIT_FAILURE);
			}

			left_length -= read_size;
		}

		puts("Extracted !");

		printf("Image %d : ", i);
		crc32_checksum = fcrc32(image_fp, 0, image.size);
		if(crc32_checksum == image.crc32)
			puts("Valid checksum !");
		else
			puts("Invalid checksum !");

		fclose(image_fp);
	}

	firmimg_close(firmimg);
}

static void do_compact(const char* path, const firmimg_version_t version, const uint8_t *uboot_ver, uint8_t *platform_id, uint8_t num_of_image, char **path_images)
{
	firmimg_t *firmimg;
	int i, ret;

	firmimg = firmimg_open(path, "w+");

	firmimg->header.header.header_version = FIRMIMG_HEADER_VERSION;
	firmimg->header.header.image_type = FIRMIMG_IMAGE_iBMC;
	firmimg->header.header.version = version;
	firmimg->header.header.image_size = 56835584; // For test
	memcpy(firmimg->header.header.uboot_ver, uboot_ver, sizeof(firmimg->header.header.uboot_ver));
	memcpy(firmimg->header.header.platform_id, platform_id, sizeof(firmimg->header.header.platform_id));

	for(i = 0; i < num_of_image; i++)
	{
		printf("Add %s to firmware image...", path_images[i]);

		ret = firmimg_add(firmimg, path_images[i]);
		if(ret < 0)
			perror("Failed to add image");

		puts(" OK !");
	}

	firmimg_close(firmimg);
}

static void usage(void)
{
	puts(
		"Usage: firmimg [OPTIONS]\n"
		"	--info=FILE			Print firmware image information of FILE\n"
		"	--extract=FILE			Extract image of FILE\n"
		"	--compact=FILE			Compact images in FILE\n"
		"	--family=FAMILY			Set iDRAC family\n"
		"	--version=VERSION		Set firmware image version\n"
		"	--build=BUILD			Set build number\n"
		"	--uboot_version=VERSION		Set U-Boot version\n"
		"	--avct_uboot_version=VERSION	Set AVCT U-Boot version\n"
		"	--platform_id=PLATFORM		Set platform id\n"
		"	--help				Print help"
	);
	exit(EXIT_SUCCESS);
}

enum action_t
{
	NONE,
	INFO,
	EXTRACT,
	COMPACT
};

static void vtob(void *dst, char *version, int offset, int limit)
{
	char *ptr;
	int i = 0;

	ptr = strtok(version, ".");
	do
	{
		((char*)dst)[offset + i] = atoi(ptr);

		ptr = strtok(NULL, ".");
		i++;
	}
	while(ptr != NULL && i < limit);
}

int main(int argc, char *argv[])
{
	char *path_file = NULL;
	char *path_images[FIRMIMG_MAX_IMAGES];
	uint8_t num_of_image;
	enum action_t action = NONE;
	firmimg_version_t version;
	uint8_t uboot_ver[8] = {0};
	uint8_t plateform_id[4];
	int c, i;

	static struct option long_options[] =
	{
		{"info", required_argument, NULL, 'i'},
		{"extract", required_argument, NULL, 'e'},
		{"compact", required_argument, NULL, 'c'},
		{"version", required_argument, NULL, 'v'},
		{"build", required_argument, NULL, 'b'},
		{"uboot_version", required_argument, NULL, 'u'},
		{"avct_uboot_version", required_argument, NULL, 'a'},
		{"platform_id", required_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{NULL, no_argument, NULL, 0}
	};

	path_file = NULL;
	num_of_image = 0;

	for(;;)
	{
		int option_index = 0;
		c = getopt_long(argc, argv, "i:e:c:v:b:u:a:p:h", long_options, &option_index);

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
			case 'c':
				action = COMPACT;
				path_file = optarg;
				break;
			case 'v':
				vtob(&version, optarg, 0, 2);
				break;
			case 'b':
				version.build = atoi(optarg);
				break;
			case 'u':
				vtob(uboot_ver, optarg, 0, 3);
				break;
			case 'a':
				vtob(uboot_ver, optarg, 4, 3);
				break;
			case 'p':
			{
				if(strcmp(optarg, "?") == 0)
				{
					puts(
						"Supported plateform ID:\n"
						IDRAC6_WHOVILLE_PLATFORM_ID "	" IDRAC6_WHOVILLE_IDENTIFIER "\n"
						IDRAC6_SVB_PLATFORM_ID "	" IDRAC6_SVB_IDENTIFIER
					);
					exit(EXIT_SUCCESS);
				}
				else if(strcmp(optarg, IDRAC6_WHOVILLE_PLATFORM_ID) == 0)
					memcpy(plateform_id, IDRAC6_WHOVILLE_PLATFORM_ID, strlen(IDRAC6_WHOVILLE_PLATFORM_ID) + 1);
				else if(strcmp(optarg, IDRAC6_SVB_PLATFORM_ID) == 0)
					memcpy(plateform_id, IDRAC6_SVB_PLATFORM_ID, strlen(IDRAC6_SVB_PLATFORM_ID) + 1);
				break;
			}
			case 'h':
			default:
				usage();
		}
	}

	if(optind < argc)
	{
		i = 0;
		do
		{
			path_images[i] = argv[optind];
			i++;
		}
		while(++optind < argc);

		path_images[i] = NULL;
		num_of_image = i;
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
		case COMPACT:
			do_compact(path_file, version, uboot_ver, plateform_id, num_of_image, path_images);
			break;
	}

	return EXIT_SUCCESS;
}

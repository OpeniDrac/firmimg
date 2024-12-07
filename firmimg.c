#include "firmimg.h"

#include <stdlib.h>
#include <getopt.h>
#include <zlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIRMIMG firmimg_file->firmimg
#define FIRMIMG_HEADER FIRMIMG.header
#define GET_IMAGE(index) &FIRMIMG.images[index]
#define CHECKSUM_STATUS(checksum, calculated) \
	checksum == calculated ? "OK" : "KO"

#define LEN(x) sizeof(x) / sizeof(x[0])

static uint32_t fcrc32(FILE *fp, const long int offset, const size_t length)
{
	uint32_t crc32_checksum;
	size_t left_length, read_size;
	Bytef buffer[512];

	fseek(fp, offset, SEEK_SET);

	crc32_checksum = crc32(0L, Z_NULL, 0);
	left_length = length;

	while(left_length > 0)
	{
		read_size = left_length > sizeof(buffer) ? sizeof(buffer) : left_length;
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

static idrac_family_t get_idrac_family(const char *path)
{
	const char *file_name, *file_extension;

	if(path == NULL)
		return -1;

	file_name = strrchr(path, '/');
	file_extension = strrchr(((file_name == NULL) ? path : file_name), '.');
	if(file_extension == NULL)
		return -1;

	file_extension++;

	if(strcmp(file_extension, IDRAC6_EXTENSION) == 0)
		return IDRAC6;
	else if(strcmp(file_extension, IDRAC7_EXTENSION) == 0)
		return IDRAC7;
	else if(strcmp(file_extension, IDRAC8_EXTENSION) == 0)
		return IDRAC8;
	else if(strcmp(file_extension, IDRAC9_EXTENSION) == 0)
		return IDRAC9;
	else
		return 0;
}

static firmimg_file_t *firmimg_open(const char *path, const char *mode)
{
	firmimg_file_t *firmimg_file;

	firmimg_file = malloc(sizeof(firmimg_file_t));

	firmimg_file->idrac_family = get_idrac_family(path);
	if(firmimg_file->idrac_family <= 0 || firmimg_file->idrac_family >= IDRAC9) {
		puts("Invalid or unsupported idrac family");
		free(firmimg_file);
		
		return NULL;
	}

	firmimg_file->fp = fopen(path, mode);
	if(!firmimg_file->fp) {
		perror("Failed to open firmimg");
		free(firmimg_file);
		
		return NULL;
	}

	FIRMIMG_HEADER.image_size = FIRMIMG_HEADER_SIZE;

	return firmimg_file;
}

static int firmimg_read_header(firmimg_file_t *firmimg_file)
{
	size_t read_len;

	read_len = fread(&firmimg_file->firmimg.header, sizeof(uint8_t),
								sizeof(firmimg_t), firmimg_file->fp);

	if(read_len != sizeof(firmimg_t))
		return -1;

	return 0;
}

static int firmimg_add(firmimg_file_t *firmimg_file, const char* path)
{
	FILE *fp;
	struct stat st;
	int index;
	uint32_t crc32_checksum;
	size_t read_size, padding;
	Bytef buffer[512];
	firmimg_image_t *image;
	char padding_buffer[512];

	if(!firmimg_file)
		return -1;

	if(FIRMIMG_HEADER.num_of_image >= FIRMIMG_MAX_IMAGES)
		return -1;

	fp = fopen(path, "r");
	if(!fp)
		return -1;

	stat(path, &st);

	index = FIRMIMG_HEADER.num_of_image;
	FIRMIMG_HEADER.num_of_image++;

	image = GET_IMAGE(index);

	image->offset = FIRMIMG_HEADER.image_size;
	image->size = st.st_size;

	padding = image->size % 512 ? 512 - (image->size % 512) : 0;
	memset(padding_buffer, 0, padding);

	if(index != 0 && fwrite(padding_buffer, sizeof(char), padding, firmimg_file->fp) != padding)
	{
		perror("Failed to write image padding");
		fclose(fp);

		return -1;
	}

	fseek(firmimg_file->fp, image->offset, SEEK_SET);

	crc32_checksum = crc32(0L, Z_NULL, 0);

	while((read_size = fread(buffer, sizeof(Bytef), sizeof(buffer), fp)) > 0)
	{
		if(fwrite(buffer, sizeof(Bytef), read_size, firmimg_file->fp) != read_size)
		{
			perror("Failed to write image");
			fclose(fp);

			return -1;
		}

		crc32_checksum = crc32(crc32_checksum, buffer, read_size);
	}

	image->crc32 = crc32_checksum;

	fclose(fp);

	FIRMIMG_HEADER.image_size += image->size + padding;

	return index;
}

static int firmimg_close(firmimg_file_t *firmimg_file)
{
	int ret;

	if(!firmimg_file)
		return -1;

	if((fcntl(fileno(firmimg_file->fp), F_GETFL) & O_ACCMODE) == O_RDWR)
	{
		FIRMIMG_HEADER.crc32 = crc32(0L, (const Bytef*)((void*)(&FIRMIMG_HEADER) + sizeof(FIRMIMG_HEADER.crc32)),
																FIRMIMG_HEADER_SIZE - sizeof(FIRMIMG_HEADER.crc32));

		fseek(firmimg_file->fp, 0L, SEEK_SET);

		fwrite(&FIRMIMG_HEADER, sizeof(char), sizeof(firmimg_header_t), firmimg_file->fp);
		fwrite(FIRMIMG.images, sizeof(*FIRMIMG.images), FIRMIMG_HEADER.num_of_image, firmimg_file->fp);
	}

	ret = fclose(firmimg_file->fp);

	free(firmimg_file);

	return ret;
}

static const char* get_platform_id(const firmimg_header_t* header)
{
	if(strcmp((char*)header->platform_id, IDRAC6_SVB_PLATFORM_ID) == 0)
		return IDRAC6_SVB_IDENTIFIER;
	else if(strcmp((char*)header->platform_id, IDRAC6_WHOVILLE_PLATFORM_ID) == 0)
		return IDRAC6_WHOVILLE_IDENTIFIER;
	else
		return "Unknown";
}

static int do_info(const char *path)
{
	firmimg_file_t *firmimg_file;
	uint32_t crc32_checksum;
	int i;
	firmimg_image_t *image;

	firmimg_file = firmimg_open(path, "r");
	if(firmimg_file == NULL)
		return EXIT_FAILURE;

	if(firmimg_read_header(firmimg_file)) {
		puts("Failed to read header");
		firmimg_close(firmimg_file);

		return EXIT_FAILURE;
	}

	crc32_checksum = fcrc32(firmimg_file->fp, sizeof(FIRMIMG_HEADER.crc32),
									FIRMIMG_HEADER_SIZE - sizeof(FIRMIMG_HEADER.crc32));

	printf(
		"Dell Remote Access Controller family: %d\n"
		"Header checksum: %x (%x %s)\n"
		"Header version: %d\n"
		"Num. of image(s): %d\n"
		"Firmimg Version: %d.%d (Build %d)\n"
		"Firmimg size: %d bytes\n"
		"U-Boot version: %d.%d.%d\n"
		"AVCT U-Boot version: %d.%d.%d\n"
		"Platform ID: %s (%s)\n",
		firmimg_file->idrac_family,
		FIRMIMG_HEADER.crc32, crc32_checksum, CHECKSUM_STATUS(FIRMIMG_HEADER.crc32, crc32_checksum),
		FIRMIMG_HEADER.header_version,
		FIRMIMG_HEADER.num_of_image,
		FIRMIMG_HEADER.version.version, FIRMIMG_HEADER.version.sub_version, FIRMIMG_HEADER.version.build,
		FIRMIMG_HEADER.image_size,
		FIRMIMG_HEADER.uboot_ver[0], FIRMIMG_HEADER.uboot_ver[1], FIRMIMG_HEADER.uboot_ver[2],
		FIRMIMG_HEADER.uboot_ver[4], FIRMIMG_HEADER.uboot_ver[5], FIRMIMG_HEADER.uboot_ver[6],
		get_platform_id(&FIRMIMG_HEADER), FIRMIMG_HEADER.platform_id);

	for(i = 0; i < FIRMIMG_HEADER.num_of_image; i++) {
		image = GET_IMAGE(i);
		crc32_checksum = fcrc32(firmimg_file->fp, image->offset, image->size);

		printf(
			"Image %d:\n"
			"Offset: %d\n"
			"Size: %d bytes\n"
			"Checksum: %x (%x %s)\n",
			i,
			image->offset,
			image->size,
			image->crc32, crc32_checksum, CHECKSUM_STATUS(image->crc32, crc32_checksum));
	}

	firmimg_close(firmimg_file);

	return EXIT_SUCCESS;
}

static int extract_image(firmimg_file_t *firmimg_file, firmimg_image_t *image, const char *path)
{
	uint32_t crc32_checksum;
	FILE *fp;
	size_t left_length, read_size;
	Bytef buffer[512];

	fp = fopen(path, "w+");
	if(fp == NULL) {
		perror("Failed to extract image file");
		
		return EXIT_FAILURE;
	}

	fseek(firmimg_file->fp, image->offset, SEEK_SET);

	left_length = image->size;
	while(left_length > 0) {
		read_size = left_length > sizeof(buffer) ? sizeof(buffer) : left_length;
		if(fread(buffer, sizeof(Bytef), read_size, firmimg_file->fp) != read_size) {
			perror("Failed to read file for extraction");
			fclose(fp);
			
			return EXIT_FAILURE;
		}

		if(fwrite(buffer, sizeof(Bytef), read_size, fp) != read_size) {
			perror("Failed to write file for extraction");
			fclose(fp);
			
			return EXIT_FAILURE;
		}

		left_length -= read_size;
	}

	printf("Extracted !");

	crc32_checksum = fcrc32(fp, 0, image->size);

	if(crc32_checksum == image->crc32)
		puts(" (Valid checksum)");
	else
		puts(" (Invalid checksum)");

	fclose(fp);

	return crc32_checksum == image->crc32 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int do_extract(const char *path)
{
	firmimg_file_t *firmimg_file;
	uint32_t crc32_checksum;
	int i;
	int ret;
	char image_path[22];

	ret = EXIT_SUCCESS;

	firmimg_file = firmimg_open(path, "r");
	if(firmimg_file == NULL)
		return EXIT_FAILURE;

	if(firmimg_read_header(firmimg_file)) {
		puts("Failed to read header");

		return EXIT_FAILURE;
	}

	crc32_checksum = fcrc32(firmimg_file->fp, sizeof(FIRMIMG_HEADER.crc32),
							FIRMIMG_HEADER_SIZE - sizeof(FIRMIMG_HEADER.crc32));
	if(crc32_checksum != FIRMIMG_HEADER.crc32) {
		puts("Invalid header checksum");
		firmimg_close(firmimg_file);

		return EXIT_FAILURE;
	}

	printf("Found %d images !\n", FIRMIMG_HEADER.num_of_image);
	for(i = 0; i < FIRMIMG_HEADER.num_of_image; i++) {
		printf("Image %d: ",  i);
		snprintf(image_path, sizeof(image_path), "image_%d.dat", i);

		ret = extract_image(firmimg_file, GET_IMAGE(i), image_path);
		if(ret != EXIT_SUCCESS)
			break;
	}

	firmimg_close(firmimg_file);
	
	return ret;
}

static int do_compact(
	const char* path,
	const firmimg_version_t version,
	const uint8_t *uboot_ver,
	uint8_t *platform_id,
	char **path_images)
{
	firmimg_file_t *firmimg_file;
	int ret, i;

	firmimg_file = firmimg_open(path, "w+");
	if(firmimg_file == NULL)
		return EXIT_FAILURE;

	FIRMIMG_HEADER.header_version = FIRMIMG_HEADER_VERSION;
	FIRMIMG_HEADER.image_type = FIRMIMG_IMAGE_iBMC;
	FIRMIMG_HEADER.version = version;
	memcpy(FIRMIMG_HEADER.uboot_ver, uboot_ver, sizeof(FIRMIMG_HEADER.uboot_ver));
	memcpy(FIRMIMG_HEADER.platform_id, platform_id, sizeof(FIRMIMG_HEADER.platform_id));

	for(i = 0; *(path_images + i) != NULL; i++) {
		printf("Add %s to firmware image...", *(path_images + i));

		ret = firmimg_add(firmimg_file, *(path_images + i));
		if(ret < 0)
			perror("Failed to add image");

		puts(" OK !");
	}

	firmimg_close(firmimg_file);

	return EXIT_SUCCESS;
}

static int usage(void)
{
	puts(
		"Usage: firmimg [OPTIONS]\n"
		"	--info=FILE			Print firmware image information of FILE\n"
		"	--extract=FILE			Extract image of FILE\n"
		"	--compact=FILE			Compact images in FILE\n"
		"	--version=VERSION		Set firmware image version\n"
		"	--build=BUILD			Set build number\n"
		"	--uboot_version=VERSION		Set U-Boot version\n"
		"	--avct_uboot_version=VERSION	Set AVCT U-Boot version\n"
		"	--platform_id=PLATFORM		Set platform id\n"
		"	--help				Print help"
	);
	
	return EXIT_SUCCESS;
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
	int i;

	i = 0;

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
	char *path_file;
	char *path_images[FIRMIMG_MAX_IMAGES];
	enum action_t action;
	firmimg_version_t version;
	uint8_t uboot_ver[8];
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
	action = NONE;
	bzero(uboot_ver, sizeof(uboot_ver));
	bzero(plateform_id, sizeof(plateform_id));

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
					memcpy(plateform_id, IDRAC6_WHOVILLE_PLATFORM_ID, strlen(IDRAC6_WHOVILLE_PLATFORM_ID));
				else if(strcmp(optarg, IDRAC6_SVB_PLATFORM_ID) == 0)
					memcpy(plateform_id, IDRAC6_SVB_PLATFORM_ID, strlen(IDRAC6_SVB_PLATFORM_ID));
				break;
			}
			case 'h':
			default:
				return usage();
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
	}

	switch(action)
	{
		case NONE:
			return usage();
			break;
		case INFO:
			return do_info(path_file);
			break;
		case EXTRACT:
			return do_extract(path_file);
			break;
		case COMPACT:
			return do_compact(path_file, version, uboot_ver, plateform_id, path_images);
			break;
	}

	return EXIT_FAILURE;
}

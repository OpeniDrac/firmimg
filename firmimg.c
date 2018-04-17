#include "firmimg.h"

uint32_t fcrc32(FILE* fp, size_t offset, size_t count)
{
	fseek(fp, offset, SEEK_SET);

	uint32_t crc = crc32(0L, Z_NULL, 0);
	Bytef buf[DEFAULT_BUFFER];

	int num = ceil((float)count / (float)sizeof(buf));
	size_t count_left = count;
	int result;
	for(int i = 0; i < num; i++)
	{
		size_t count_read = (count_left > sizeof(buf)) ? sizeof(buf) : count_left;

		result = fread(buf, sizeof(Bytef), count_read, fp);
		if(!result)
		{
			perror("Failed to read");
			break;
		}

		crc = crc32(crc, buf, count_read);

		count_left -= sizeof(buf);
	}

	return crc;
}

void fcopy(FILE* src_fp, size_t offset, size_t count, FILE* dst_fp)
{
	fseek(src_fp, offset, SEEK_SET);
	rewind(dst_fp);

	char buf[DEFAULT_BUFFER];

	int num = ceil((float)count / (float)sizeof(buf));
	size_t count_left = count;
	for(int i = 0; i < num; i++)
	{
		size_t count_read = (count_left > sizeof(buf)) ? sizeof(buf) : count_left;

		fread(buf, sizeof(char), count_read, src_fp);
		fwrite(buf, sizeof(char), count_read, dst_fp);

		count_left -= sizeof(buf);
	}
}

int get_drac_family(const char* path)
{
	const char* file_name = strrchr(path, '/');
	char* file_extension = strrchr(((file_name == NULL) ? path : file_name), '.');

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
	{
		printf("Warning : iDRAC9 is uImage !");
		return IDRAC9;
	}

	printf("Warning: Unknown extension !\n");

	return -1;
}

firmimg_entry_info* get_schema(const int idrac_family)
{
	switch(idrac_family)
	{
		case IDRAC6:
			return iDRAC6_schema;
		case IDRAC7:
		case IDRAC8:
			return iDRAC7_schema;
		default:
			return NULL;
	}
}

FIRMIMG_FILE* _firmimg_open(const char* file_path, const char* mode)
{
	FIRMIMG_FILE* firmimg_fp = malloc(sizeof(struct FIRMIMG_FILE));

	firmimg_fp->fp = fopen(file_path, mode);
	if(firmimg_fp->fp == NULL)
	{
		free(firmimg_fp);
		return NULL;
	}

	firmimg_fp->firmware_image = malloc(sizeof(struct firmimg));
	firmimg_fp->firmware_image->drac_family = get_drac_family(file_path);
	if(firmimg_fp->firmware_image->drac_family < 0 || firmimg_fp->firmware_image->drac_family == IDRAC9)
	{
		firmimg_close(firmimg_fp);
		return NULL;
	}

	if(strcmp(mode, "r") == 0)
	{
		fseek(firmimg_fp->fp, 0, SEEK_END);
		unsigned int file_size = ftell(firmimg_fp->fp);

		if(file_size < FIRMIMG_HEADER_SIZE)
		{
			printf("Incorrect header !\n");
			firmimg_close(firmimg_fp);
			return NULL;
		}

		rewind(firmimg_fp->fp);

		unsigned char uint8_buf;
		unsigned char uint16_buf[2];
		unsigned char uint32_buf[4];

		fread(uint32_buf, sizeof(unsigned char), sizeof(uint32_buf), firmimg_fp->fp);
		firmimg_fp->firmware_image->header_checksum = *((uint32_t*)uint32_buf);
		uint32_t file_header_checksum = fcrc32(firmimg_fp->fp, 4, FIRMIMG_HEADER_SIZE - 4);

		if(firmimg_fp->firmware_image->header_checksum != file_header_checksum)
		{
			printf("Incorrect header checksum !\n");
			firmimg_close(firmimg_fp);
			return NULL;
		}

		fseek(firmimg_fp->fp, 4, SEEK_SET);

		// Unknown data offset 4 and 5
		uint8_buf = getc(firmimg_fp->fp); // 0x01
		uint8_buf = getc(firmimg_fp->fp); // 0x01

		fread(uint16_buf, sizeof(unsigned char), sizeof(uint16_buf), firmimg_fp->fp);
		firmimg_fp->firmware_image->num_entries = *((uint16_t*)uint16_buf);

		fread(uint32_buf, sizeof(unsigned char), 2, firmimg_fp->fp);

		fread(uint16_buf, sizeof(unsigned char), sizeof(uint16_buf), firmimg_fp->fp);
		firmimg_fp->firmware_image->build = *((uint16_t*)uint16_buf);

		// Unknown data offset 12 - 27
		fseek(firmimg_fp->fp, 28, SEEK_SET);

		uint32_buf[2] = getc(firmimg_fp->fp);
		uint8_buf = getc(firmimg_fp->fp); // 0x00
		uint32_buf[3] = getc(firmimg_fp->fp);
		uint8_buf = getc(firmimg_fp->fp); // 0x00

		sprintf(firmimg_fp->firmware_image->release, "%d.%d.%d.%d", uint32_buf[0], uint32_buf[1], uint32_buf[2], uint32_buf[3]);

		firmimg_fp->firmware_image->entries = malloc(firmimg_fp->firmware_image->num_entries * sizeof(firmimg_entry));
		for(int i = 0; i < firmimg_fp->firmware_image->num_entries; i++)
		{
			struct firmimg_entry entry;

			fread(uint32_buf, sizeof(unsigned char), sizeof(uint32_buf), firmimg_fp->fp);
			entry.offset = *((uint32_t*)uint32_buf);

			fread(uint32_buf, sizeof(unsigned char), sizeof(uint32_buf), firmimg_fp->fp);
			entry.size = *((uint32_t*)uint32_buf);

			fread(uint32_buf, sizeof(unsigned char), sizeof(uint32_buf), firmimg_fp->fp);
			entry.checksum = *((uint32_t*)uint32_buf);

			firmimg_fp->firmware_image->entries[i] = entry;
		}
	}
	if(strcmp(mode, "w") == 0)
	{
		firmimg_fp->firmware_image->num_entries = 0;
	}

	return firmimg_fp;
}

FIRMIMG_FILE* firmimg_open(const char* file_path)
{
	return _firmimg_open(file_path, "r");
}

FIRMIMG_FILE* firmimg_create(const char* file_path)
{
	return _firmimg_open(file_path, "w");
}

int firmimg_close(FIRMIMG_FILE* firmimg_fp)
{
	int result =  fclose(firmimg_fp->fp);
	if(firmimg_fp->firmware_image->entries)
		free(firmimg_fp->firmware_image->entries);

	free(firmimg_fp->firmware_image);
	free(firmimg_fp);

	return result;
}

void verify(int argc, char **argv)
{
	if(argc < 3)
	{
		printf("File path not set !");
		return;
	}

	const char* file_path = argv[2];

	FIRMIMG_FILE* firmimg_fp = firmimg_open(file_path);
	if(firmimg_fp == NULL)
	{
		perror("Failed to open firmware image");
		return;
	}

	printf("Dell Remote Access Controller family : %d\n", firmimg_fp->firmware_image->drac_family);
	printf("Firmware version : %s (Build %d)\n", firmimg_fp->firmware_image->release, firmimg_fp->firmware_image->build);
	printf("Num. entries : %d\n", firmimg_fp->firmware_image->num_entries);

	uint32_t header_checksum = fcrc32(firmimg_fp->fp, 4, FIRMIMG_HEADER_SIZE - 4);
	printf("Header checksum : %x\n", firmimg_fp->firmware_image->header_checksum);
	printf("File header checksum : %x\n", header_checksum);
	printf("Header checksum status : %s\n", ((firmimg_fp->firmware_image->header_checksum == header_checksum) ? "VALID" : "INVALID"));

	firmimg_entry_info* drac_schema = get_schema(firmimg_fp->firmware_image->drac_family);
	for(int i = 0; i < firmimg_fp->firmware_image->num_entries; i++)
	{
		struct firmimg_entry entry = firmimg_fp->firmware_image->entries[i];
		uint32_t entry_checksum = fcrc32(firmimg_fp->fp, entry.offset, entry.size);

		printf("Entry %d :\n", i);
		if(drac_schema != NULL)
		{
			printf("\tName : %s\n", drac_schema[i].name);
			printf("\tDescription : %s\n", drac_schema[i].description);
		}
		printf("\tOffset : %d\n", entry.offset);
		printf("\tSize : %d\n", entry.size);
		printf("\tEntry checksum : %x\n", entry.checksum);
		printf("\tFile entry Checksum : %x\n", entry_checksum);
		printf("\tEntry checksum status : %s\n", ((entry_checksum == entry.checksum) ? "VALID" : "INVALID"));
	}

	firmimg_close(firmimg_fp);
}

void unpack(int argc, char **argv)
{
	if(argc < 3)
	{
		printf("File path not set !");
		return;
	}

	const char* file_path = argv[2];

	FIRMIMG_FILE* firmimg_fp = firmimg_open(file_path);
	if(firmimg_fp == NULL)
	{
		perror("Failed to open firmware image");
		return;
	}

	struct stat st;
	if(stat("data", &st) != 0)
	{
		mkdir("data", S_IRWXU);
	}

	firmimg_entry_info* drac_schema = get_schema(firmimg_fp->firmware_image->drac_family);
	for(int i = 0; i < firmimg_fp->firmware_image->num_entries; i++)
	{
		struct firmimg_entry entry = firmimg_fp->firmware_image->entries[i];
		uint32_t entry_checksum = fcrc32(firmimg_fp->fp, entry.offset, entry.size);

		printf("[%d/%d] Entry checksum : %s\n", i + 1, firmimg_fp->firmware_image->num_entries, ((entry_checksum == entry.checksum) ? "VALID" : "INVALID"));
		printf("[%d/%d] Extracting...\n", i + 1, firmimg_fp->firmware_image->num_entries);

		char entry_file_path[PATH_MAX];
		if(drac_schema == NULL)
			sprintf(entry_file_path, "data/entry_%d.bin", i);
		else
			sprintf(entry_file_path, "data/%s", drac_schema[i].file_name);

		FILE* entry_fp = fopen(entry_file_path, "w+");
		if(entry_fp == NULL)
		{
			printf("[%d/%d] Extraction failed !", i + 1, firmimg_fp->firmware_image->num_entries);
			perror("Failed to open entry file");
			continue;
		}

		fcopy(firmimg_fp->fp, entry.offset, entry.size, entry_fp);
		uint32_t entry_file_checksum = fcrc32(entry_fp, 0, entry.size);
		fclose(entry_fp);

		printf("[%d/%d] Extracted !\n", i + 1, firmimg_fp->firmware_image->num_entries);
		printf("[%d/%d] File checksum : %s \n", i + 1, firmimg_fp->firmware_image->num_entries, ((entry_file_checksum == entry.checksum) ? "VALID" : "INVALID"));
	}

	printf("Firmware image extracted !\n");

	firmimg_close(firmimg_fp);
}

void pack(int argc, char **argv)
{
	if(argc < 3)
	{
		printf("File path not set !");
		return;
	}

	const char* file_path = argv[2];

	FIRMIMG_FILE* firmimg_fp = firmimg_create(file_path);
	if(firmimg_fp == NULL)
	{
		perror("Failed to open firmware image");
		return;
	}

	DIR* data_dir = opendir("./data/");
	struct dirent* dir;
	if (data_dir == NULL)
	{
		perror("Failed to open directory");

		firmimg_close(firmimg_fp);
		return;
	}

	while ((dir = readdir(data_dir)) != NULL)
	{
		if(dir->d_type != DT_DIR)
			printf("%s\n", dir->d_name);
	}

	closedir(data_dir);

	unsigned char header_buffer[FIRMIMG_HEADER_SIZE];
	header_buffer[0] = 0x01;
	header_buffer[1] = 0x01;

	firmimg_close(firmimg_fp);
}

void help()
{
	printf(
		"Usage : firmimg [COMMAND]\n\n" \
		"command:\n" \
		"\tunpack\t\tUnpack frimware image\n" \
		"\tpack\t\tPack firmware image\n" \
		"\tverify\t\tVerify firmware image\n" \
		"\thelp\t\tShow help\n"
	);
}

int main(int argc, char **argv)
{
	if(argc <= 1)
		goto unknown_command;

	if(strcmp(argv[1], "verify") == 0)
		verify(argc, argv);
	else if(strcmp(argv[1], "unpack") == 0)
		unpack(argc, argv);
	else if(strcmp(argv[1], "pack") == 0)
		pack(argc, argv);
	else if(strcmp(argv[1], "help") == 0)
		help();
	else
		goto unknown_command;

	return EXIT_SUCCESS;

	unknown_command:
		errno = EINVAL;
		fprintf(stderr, "Unknown command\n");

		return -EINVAL;
}

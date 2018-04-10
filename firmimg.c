#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <stdint.h>
#include <zlib.h>
#include <math.h>

#define BMC 0
#define DRAC 1
#define IDRAC 2

#define MODULAR 0
#define MONOLITHIC 1

struct firmimg_data
{
	char* name;
	char* file_name;
	size_t offset;
	size_t reserved;
	size_t size;
};

struct firmimg
{
	int sys_type;
	int sys_version;
	float sys_release;
	int hw_type;
	unsigned int file_crc32;
	size_t file_size;
	int content_length;
	const struct firmimg_data* content;
};

struct firmimg_header
{
	uLong header_crc32;
	uLong cramfs_crc32;
};

const struct firmimg_data iDRAC6_2_90_content[4] = {
	{
		.name = "header",
		.file_name = "header.bin",
		.offset = 0,
		.reserved = 512,
		.size = 512
	},
	{
		.name = "uImage",
		.file_name = "uImage",
		.offset = 512,
		.reserved = 4480000,
		.size = 4479904
	},
	{
		.name = "cramfs",
		.file_name = "cramfs",
		.offset = 512 + 4480000,
		.reserved = 52203520,
		.size = 52203520
	},
	{
		.name = "unknown",
		.file_name = "unknown.bin",
		.offset = 512 + 4480000 + 52203520,
		.reserved = 142904,
		.size = 142904
	}
};

const struct firmimg iDRAC6_2_90 = {
	.sys_type = IDRAC,
	.sys_version = 6,
	.sys_release = 2.90,
	.hw_type = MONOLITHIC,
	.file_crc32 = 0x397ac2f8,
	.file_size = 56826936,
	.content_length = 4,
	.content = iDRAC6_2_90_content
};

uLong get_crc32(FILE* file)
{
	rewind(file);

	uLong crc = crc32(0L, Z_NULL, 0);
	Bytef buf[1024];

	size_t buf_read;
	while((buf_read = fread(buf, sizeof(Bytef), sizeof(buf), file)) > 0)
		crc = crc32(crc, buf, buf_read);

	rewind(file);

	return crc;
}

void fcopy(FILE* src_fp, size_t offset, size_t len, FILE* dst_fp)
{
	fseek(src_fp, offset, SEEK_SET);
	rewind(dst_fp);

	char buf[1024];

	int num = ceil((float)len / (float)sizeof(buf));
	size_t length_left = len;
	for(int i = 0; i < num; i++)
	{
		size_t read_size = (length_left > sizeof(buf)) ? sizeof(buf) : length_left;

		fread(buf, sizeof(char), read_size, src_fp);
		fwrite(buf, sizeof(char), read_size, dst_fp);

		length_left -= sizeof(buf);
	}

	rewind(src_fp);
	rewind(dst_fp);
}

void show_firmimg_detail(const struct firmimg* firmware_image)
{
	printf("------------------------------------------------------------");
	printf("Firmware image name : Dell %s %d %s Release %.2f\n",
		(firmware_image->sys_type == BMC ? "BMC" :
			(firmware_image->sys_type == DRAC ? "DRAC" :
				(firmware_image->sys_type == IDRAC ? "iDRAC" : "Unknown"))),
		firmware_image->sys_version,
		(firmware_image->hw_type == MODULAR ? "Modular" :
			(firmware_image->hw_type == MONOLITHIC ? "Monolithic" : "Unknown")),
		firmware_image->sys_release);
	printf("------------------------------------------------------------");
	printf("CRC32 checksum : %x\n", firmware_image->file_crc32);
	printf("File size : %lu bytes\n", firmware_image->file_size);
	printf("Content data :\n");

	for(int i = 0; i < firmware_image->content_length; i++)
	{
		struct firmimg_data data = firmware_image->content[i];
		printf("%s:\n", data.name);
		printf("\tOffset : %zu\n", data.offset);
		printf("\tReserved : %zu\n", data.reserved);
		printf("\tSize : %zu\n", data.size);
	}
	printf("------------------------------------------------------------");
}

struct firmimg_header get_header(FILE* fp)
{
	struct firmimg_header header = {};

	rewind(fp);

	Bytef crc32_buf[4];
	fread(crc32_buf, sizeof(Bytef), sizeof(crc32_buf), fp);
	header.header_crc32 = *((uLong*)crc32_buf);

	fseek(fp, 52, SEEK_SET);

	fread(crc32_buf, sizeof(Bytef), sizeof(crc32_buf), fp);
        header.cramfs_crc32 = *((uLong*)crc32_buf);

	return header;
}

void unpack()
{
	const struct firmimg* firmware_image = &iDRAC6_2_90;
	show_firmimg_detail(firmware_image);

	FILE* firmimg_fp = fopen("firmimg.d6", "r");
	fseek(firmimg_fp, 0L, SEEK_END);
	size_t file_size = ftell(firmimg_fp);
	fseek(firmimg_fp, 0L, SEEK_SET);

	const struct firmimg_header firmware_image_header = get_header(firmimg_fp);

	printf("File size: %ld bytes\n", file_size);
	printf("Firmware size status : %s\n",
		(firmware_image->file_size == file_size ? "VALID" : "INVALID"));

	uLong file_crc32 = get_crc32(firmimg_fp);
	printf("File CRC32: %lx\n", file_crc32);
	printf("Firmware CRCR32 status : %s\n",
		(firmware_image->file_crc32 == file_crc32 ?  "VALID" : "INVALID"));

	if(firmware_image->file_size == file_size && firmware_image->file_crc32 == file_crc32)
	{
		struct stat st;
		if(stat("data", &st) != 0)
		{
			mkdir("data", S_IRWXU);
		}

		char* data_dir_path = "data/";
		for(int i = 0; i < firmware_image->content_length; i++)
		{
			const struct firmimg_data data = firmware_image->content[i];
			char file_path[PATH_MAX] = "\0";
			strcat(file_path, data_dir_path);
			strcat(file_path, data.file_name);

			FILE* data_fp = fopen(file_path, "w");
			if(data_fp == NULL)
			{
				perror("Failed to open");
				continue;
			}

			printf("[%d/%d] Unpack %s data to %s...\n", i, firmware_image->content_length, data.name, data.file_name);
			fcopy(firmimg_fp, data.offset, data.reserved, data_fp);
			printf("[%d/%d] Done !\n", i, firmware_image->content_length);

			fclose(data_fp);
		}

		printf("Firmware image header CRC32 : %lx\n", firmware_image_header.header_crc32);
		printf("Firmware cramfs CRC32 : %lx\n", firmware_image_header.cramfs_crc32);
		FILE* cramfs_fp = fopen("data/cramfs", "r");
		uLong cramfs_crc32 = get_crc32(cramfs_fp);
		fclose(cramfs_fp);

		printf("cramfs file CRC32 : %lx\n", cramfs_crc32);

		printf("cramfs status : %s\n",
			(cramfs_crc32 == firmware_image_header.cramfs_crc32 ? "VALID" : "INVALID"));
	}

	fclose(firmimg_fp);
}

void pack()
{

}

void help()
{
	printf(
		"Usage : firmimg [COMMAND]\n\n" \
		"command:\n" \
		"\tunpack\t\tUnpack frimware image\n" \
		"\tpack\t\tPack firmware image\n" \
		"\thelp\t\tShow help\n"
	);
}

int main(int argc, char **argv)
{
	if(argc != 2)
		goto unknown_command;

	if(strcmp(argv[1], "unpack") == 0)
		unpack();
	else if(strcmp(argv[1], "pack") == 0)
		printf("Pack");
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

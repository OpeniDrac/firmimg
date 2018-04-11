#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <stdint.h>
#include <zlib.h>
#include <math.h>

#define DEFAULT_BUFFER 1024

#define BMC 0
#define DRAC 1
#define IDRAC 2

#define IDRAC6 0x30101
#define IDRAC7 0x40101

/*
iDrac 6 1.85 A00

          |Header CRC32--Ver. & Rel.--Unknown    --Unknown    --Unknown    --Unknown     |
0000:0000 | C9 D4 D9 42  01 01 03 00  01 55 03 00  00 8E 47 03  01 02 00 00  01 13 07 00 | ÉÔÙB.....U....G.........
0000:0018 | 57 48 4F 56  00 00 00 00  00 02 00 00  E0 5B 44 00  F6 3F 64 32  00 5E 44 00 | WHOV........à[D.ö?d2.^D.
0000:0030 | 00 00 01 03  F2 AB 8C 87  00 5E 45 03  38 2E 02 00  55 8A 1A 65  00 00 00 00 | ....ò«...^E.8...U..e....
*/

/*
iDrac 6 2.85 A00

0000:0000 | 56 EA 53 65  01 01 03 00  02 55 04 00  00 4E 60 03  01 02 00 00  01 13 08 00 | VêSe.....U...N`.........
0000:0018 | 57 48 4F 56  00 00 00 00  00 02 00 00  E0 5B 44 00  AA D1 58 2A  00 5E 44 00 | WHOV........à[D.ªÑX*.^D.
0000:0030 | 00 C0 19 03  02 75 A0 F4  00 1E 5E 03  38 2E 02 00  6C BB 00 32  00 00 00 00 | .À...u ô..^.8...l».2....
*/

struct firmimg_entry
{
	char* name;
	char* file_name;
	size_t offset;
	size_t reserved;
	size_t size;
};

struct firmimg
{
	int fw_type;
	uint32_t fw_version;
	int num_entries;
	const struct firmimg_entry* entries;
};

struct firmimg_header
{
	uint32_t header_crc32;
	uint32_t idrac_version;
	uint32_t cramfs_crc32;
};

const struct firmimg_entry iDRAC6_entries[4] = {
	{
		.name = "header",
		.file_name = "header.bin",
		.offset = 4,
		.reserved = 508,
		.size = 508
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

const struct firmimg iDRAC6_schema = {
	.fw_type = IDRAC,
	.fw_version = IDRAC6,
	.num_entries = 4,
	.entries = iDRAC6_entries
};

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

	rewind(src_fp);
	rewind(dst_fp);
}

void verify(const char* file_path)
{
	FILE* firmimg_fp = fopen(file_path, "r");

	printf("------------------------------------------------------------");
	/*printf("Firmware image name : Dell %s %d %s Release %.2f\n",
		(firmware_image->sys_type == BMC ? "BMC" :
			(firmware_image->sys_type == DRAC ? "DRAC" :
				(firmware_image->sys_type == IDRAC ? "iDRAC" : "Unknown"))),
		firmware_image->sys_version,
		(firmware_image->hw_type == MODULAR ? "Modular" :
			(firmware_image->hw_type == MONOLITHIC ? "Monolithic" : "Unknown")),
		firmware_image->sys_release);*/
	printf("------------------------------------------------------------");
	/*printf("CRC32 checksum : %x\n", firmware_image->file_crc32);
	printf("File size : %lu bytes\n", firmware_image->file_size);
	printf("Content data :\n");

	for(int i = 0; i < firmware_image->content_length; i++)
	{
		struct firmimg_data data = firmware_image->content[i];
		printf("%s:\n", data.name);
		printf("\tOffset : %zu\n", data.offset);
		printf("\tReserved : %zu\n", data.reserved);
		printf("\tSize : %zu\n", data.size);
	}*/
	printf("------------------------------------------------------------");

	fclose(firmimg_fp);
}

struct firmimg_header read_header(FILE* fp)
{
	struct firmimg_header header = {};

	rewind(fp);

	Bytef crc32_buf[4];
	fread(crc32_buf, sizeof(Bytef), sizeof(crc32_buf), fp);
	header.header_crc32 = *((uint32_t*)crc32_buf);

	fread(crc32_buf, sizeof(Bytef), sizeof(crc32_buf), fp);
	header.idrac_version = *((uint32_t*)crc32_buf);

	fseek(fp, 52, SEEK_SET);

	fread(crc32_buf, sizeof(Bytef), sizeof(crc32_buf), fp);
	header.cramfs_crc32 = *((uint32_t*)crc32_buf);

	return header;
}

void unpack(char* file_path)
{

	FILE* firmimg_fp = fopen(file_path, "r");

	const struct firmimg_header firmimg_header = read_header(firmimg_fp);

	const struct firmimg* firmimg_schema;
	switch(firmimg_header.idrac_version)
	{
		case IDRAC6:
			firmimg_schema = &iDRAC6_schema;
			break;
		default:
			errno = ENODATA;
			perror("No schema found");
			goto unpack_close;
			break;
	}

	printf("iDrac version : %s\n", (firmimg_header.idrac_version == IDRAC6 ? "6" : "Unknown"));
	printf("Header CRC32 : %x\n", (unsigned int)firmimg_header.header_crc32);
	printf("cramfs CRC32 : %x\n", (unsigned int)firmimg_header.cramfs_crc32);

	char crc32_test[4] = {0x38, 0x2E, 0x02, 0x00};
	printf("%f\n", *((float*)crc32_test));

	struct stat st;
	if(stat("data", &st) != 0)
	{
		mkdir("data", S_IRWXU);
	}

	char* entries_dir_path = "data/";
	for(int i = 0; i < firmimg_schema->num_entries; i++)
	{
		const struct firmimg_entry entry = firmimg_schema->entries[i];
		char entry_file_path[PATH_MAX] = "\0";
		strcat(entry_file_path, entries_dir_path);
		strcat(entry_file_path, entry.file_name);

		FILE* entry_fp = fopen(entry_file_path, "r+");
		if(entry_fp == NULL)
		{
			perror("Failed to open entry");
			continue;
		}

		printf("[%d/%d] Unpack %s entry to %s...\n", i, firmimg_schema->num_entries, entry.name, entry.file_name);

		fcopy(firmimg_fp, entry.offset, entry.reserved, entry_fp);

		printf("[%d/%d] %s CRC32 : %x\n", i, firmimg_schema->num_entries, entry.name, fcrc32(entry_fp, 0, entry.reserved));
		printf("[%d/%d] Done !\n", i, firmimg_schema->num_entries);

		fclose(entry_fp);
	}

	unpack_close:
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
		"\tverify\t\tVerify firmware image\n" \
		"\thelp\t\tShow help\n"
	);
}

int main(int argc, char **argv)
{
	if(argc != 2)
		goto unknown_command;

	if(strcmp(argv[1], "unpack") == 0)
		unpack("firmimg.d6");
	else if(strcmp(argv[1], "pack") == 0)
		pack();
	else if(strcmp(argv[1], "verify") == 0)
		verify("firmimg.d6");
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

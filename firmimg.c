#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define BMC 0
#define DRAC 1
#define IDRAC 2

#define MODULAR 0
#define MONOLITHIC 1

struct firmimg_data
{
	char* name;
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

const struct firmimg_data iDRAC6_2_90_content[4] = {
	{
		.name = "header",
		.offset = 0,
		.reserved = 512,
		.size = 512
	},
	{
		.name = "uImage",
		.offset = 512,
                .reserved = 4480000,
                .size = 4479904
	},
	{
		.name = "cramfs",
                .offset = 512 + 4480000,
                .reserved = 52203520,
                .size = 52203520
	},
	{
                .name = "unknown",
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

void show_firmimg(const struct firmimg* firmware_image)
{
	printf("Firmware image name : Dell %s %d %s Release %.2f\n",
		(firmware_image->sys_type == BMC ? "BMC" :
			(firmware_image->sys_type == DRAC ? "DRAC" :
				(firmware_image->sys_type == IDRAC ? "iDRAC" : "Unknown"))),
		firmware_image->sys_version,
		(firmware_image->hw_type == MODULAR ? "Modular" :
			(firmware_image->hw_type == MONOLITHIC ? "Monolithic" : "Unknown")),
		firmware_image->sys_release);
	printf("Firmware CRC32 : %x\n", firmware_image->file_crc32);
	printf("Firmware size : %lu\n", firmware_image->file_size);
	printf("Firmware content data :\n");

	for(int i = 0; i < firmware_image->content_length; i++)
	{
		struct firmimg_data data = firmware_image->content[i];
		printf("%s:\n", data.name);
		printf("\tOffset : %zu\n", data.offset);
		printf("\tReserved : %zu\n", data.reserved);
		printf("\tSize : %zu\n", data.size);
	}
}

void unpack()
{
	const struct firmimg* firmware_image = &iDRAC6_2_90;
	show_firmimg(firmware_image);

	FILE* firmimg_fp = fopen("firmimg.d6", "r");
	fseek(firmimg_fp, 0, SEEK_END);
	unsigned int file_size = ftell(firmimg_fp);
	fseek(firmimg_fp, 0, SEEK_SET);

	if(file_size != firmware_image->file_size)
	{
		fprintf(stderr, "Firmware as diffrent size !\n");
		goto terminate;
	}

	terminate:
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

#ifndef _FIRMIMG_H
#define _FIRMIMG_H

#include <stdint.h>
#include <stdio.h>

#define IDRAC6_SVB_PLATFORM_ID		"WEVB"
#define IDRAC6_SVB_IDENTIFIER		"EVB"
#define IDRAC6_WHOVILLE_PLATFORM_ID	"WHOV"
#define IDRAC6_WHOVILLE_IDENTIFIER	"Whoville"

#define FIRMIMG_HEADER_SIZE			512
#define FIRMIMG_MAX_IMAGES			(FIRMIMG_HEADER_SIZE - sizeof(firmimg_header_t))/ sizeof(firmimg_image_t)
#define FIRMIMG_HEADER_VERSION	1
#define FIRMIMG_IMAGE_iBMC			1

/* There structures is based on U-Boot source code published by Dell (opensource.dell.com) */

typedef struct firmimg_version
{
	uint8_t version;
	uint8_t sub_version;
	uint8_t build;
} firmimg_version_t;

typedef struct firmimg_header
{
	uint32_t crc32;
	uint8_t header_version;		/* Header version : 1 */
	uint8_t image_type;		/* Image type : 1 is for iBMC */
	uint8_t num_of_image;
	uint8_t reserved_0;
	firmimg_version_t version;
	uint32_t image_size;
	/* uint32_t reserved_1; */
	/* uint32_t reserved_2; */
	uint8_t uboot_ver[8];
	/* uint32_t reserved_3; */
	uint8_t platform_id[4];
	uint32_t reserved_4;
} firmimg_header_t;

typedef struct firmimg_image
{
	uint32_t offset;
	uint32_t size;
	uint32_t crc32;
} firmimg_image_t;

#define IDRAC6_EXTENSION "d6"
#define IDRAC7_EXTENSION "d7"
#define IDRAC8_EXTENSION "d8"
#define IDRAC9_EXTENSION "d9"

typedef enum idrac_family
{
	IDRAC6 = 6,
	IDRAC7 = 7,
	IDRAC8 = 8,
	IDRAC9 = 9
} idrac_family_t;

typedef struct firmimg
{
	firmimg_header_t header;
	firmimg_image_t images[FIRMIMG_MAX_IMAGES];
} firmimg_t;

typedef struct firmimg_file
{
	FILE *fp;
	idrac_family_t idrac_family;
	firmimg_t firmimg;
} firmimg_file_t;

#endif

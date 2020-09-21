/*
 * TODO
 *
 * Copyright (C) 2020 zerons
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "si_gcc.h"

#if 0
#include "./decNumber.h"

void encode_decimal32(const struct real_format *fmt ATTRIBUTE_UNUSED,
			long *buf, const REAL_VALUE_TYPE *r)
{
	decNumber dn;
	decimal32 d32;
	decContext set;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	decimal_to_decnumber(r, &dn);
	decimal32FromNumber(&d32, &dn, &set);

	memcpy(&image, d32.bytes, sizeof(int32_t));
	buf[0] = image;
}

void decode_decimal32(const struct real_format *fmt ATTRIBUTE_UNUSED,
			REAL_VALUE_TYPE *r, const long *buf)
{
	decNumber dn;
	decimal32 d32;
	decContext set;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	image = buf[0];
	memcpy(&d32.bytes, &image, sizeof(int32_t));

	decimal32ToNumber(&d32, &dn);
	decimal_from_decnumber(r, &dn, &set);
}

void encode_decimal64(const struct real_format *fmt ATTRIBUTE_UNUSED,
			long *buf, const REAL_VALUE_TYPE *r)
{
	decNumber dn;
	decimal64 d64;
	decContext set;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	decimal_to_decnumber(r, &dn);
	decimal64FromNumber(&d64, &dn, &set);

	if (WORDS_BIGENDIAN == FLOAT_WORDS_BIG_ENDIAN) {
		memcpy(&image, &d64.bytes[0], sizeof(int32_t));
		buf[0] = image;
		memcpy(&image, &d64.bytes[4], sizeof(int32_t));
		buf[1] = image;
	} else {
		memcpy(&image, &d64.bytes[4], sizeof(int32_t));
		buf[0] = image;
		memcpy(&image, &d64.bytes[0], sizeof(int32_t));
		buf[1] = image;
	}
}

void decode_decimal64(const struct real_format *fmt ATTRIBUTE_UNUSED,
			REAL_VALUE_TYPE *r, const long *buf)
{
	decNumber dn;
	decimal64 d64;
	decContext set;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	if (WORDS_BIGENDIAN == FLOAT_WORDS_BIG_ENDIAN) {
		image = buf[0];
		memcpy(&d64.bytes[0], &image, sizeof(int32_t));
		image = buf[1];
		memcpy(&d64.bytes[4], &image, sizeof(int32_t));
	} else {
		image = buf[1];
		memcpy(&d64.bytes[0], &image, sizeof(int32_t));
		image = buf[0];
		memcpy(&d64.bytes[4], &image, sizeof(int32_t));
	}

	decimal64ToNumber(&d64, &dn);
	decimal_from_decnumber(r, &dn, &set);
}

void encode_decimal128(const struct real_format *fmt ATTRIBUTE_UNUSED,
			long *buf, const REAL_VALUE_TYPE *r)
{
	decNumber dn;
	decContext set;
	decimal128 d128;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	decimal_to_decnumber(r, &dn);
	decimal128FromNumber(&d128, &dn, &set);

	if (WORDS_BIGENDIAN == FLOAT_WORDS_BIG_ENDIAN) {
		memcpy(&image, &d128.bytes[0], sizeof(int32_t));
		buf[0] = image;
		memcpy(&image, &d128.bytes[4], sizeof(int32_t));
		buf[1] = image;
		memcpy(&image, &d128.bytes[8], sizeof(int32_t));
		buf[2] = image;
		memcpy(&image, &d128.bytes[12], sizeof(int32_t));
		buf[3] = image;
	} else {
		memcpy(&image, &d128.bytes[12], sizeof(int32_t));
		buf[0] = image;
		memcpy(&image, &d128.bytes[8], sizeof(int32_t));
		buf[1] = image;
		memcpy(&image, &d128.bytes[4], sizeof(int32_t));
		buf[2] = image;
		memcpy(&image, &d128.bytes[0], sizeof(int32_t));
		buf[3] = image;
	}
}

void decode_decimal128(const struct real_format *fmt ATTRIBUTE_UNUSED,
			REAL_VALUE_TYPE *r, const long *buf)
{
	decNumber dn;
	decimal128 d128;
	decContext set;
	int32_t image;

	decContextDefault(&set, DEC_INIT_DECIMAL128);
	set.traps = 0;

	if (WORDS_BIGENDIAN == FLOAT_WORDS_BIG_ENDIAN) {
		image = buf[0];
		memcpy(&d128.bytes[0],  &image, sizeof(int32_t));
		image = buf[1];
		memcpy(&d128.bytes[4],  &image, sizeof(int32_t));
		image = buf[2];
		memcpy(&d128.bytes[8],  &image, sizeof(int32_t));
		image = buf[3];
		memcpy(&d128.bytes[12], &image, sizeof(int32_t));
	} else {
		image = buf[3];
		memcpy(&d128.bytes[0],  &image, sizeof(int32_t));
		image = buf[2];
		memcpy(&d128.bytes[4],  &image, sizeof(int32_t));
		image = buf[1];
		memcpy(&d128.bytes[8],  &image, sizeof(int32_t));
		image = buf[0];
		memcpy(&d128.bytes[12], &image, sizeof(int32_t));
	}

	decimal128ToNumber(&d128, &dn);
	decimal_from_decnumber(r, &dn, &set);
}
#endif

/*
 * these functions are copied from gcc for case that don't run as gcc plugin
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

/*
 * ************************************************************************
 * htab
 * ************************************************************************
 */
#ifndef CHAR_BIT
#define	CHAR_BIT 8
#endif

#define si_mix(a,b,c)	\
{	\
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<< 8); \
	c -= a; c -= b; c ^= ((b&0xffffffff)>>13); \
	a -= b; a -= c; a ^= ((c&0xffffffff)>>12); \
	b -= c; b -= a; b = (b ^ (a<<16)) & 0xffffffff; \
	c -= a; c -= b; c = (c ^ (b>> 5)) & 0xffffffff; \
	a -= b; a -= c; a = (a ^ (c>> 3)) & 0xffffffff; \
	b -= c; b -= a; b = (b ^ (a<<10)) & 0xffffffff; \
	c -= a; c -= b; c = (c ^ (b>>15)) & 0xffffffff; \
}

static struct prime_ent const si_prime_tab[] = {
	{          7, 0x24924925, 0x9999999b, 2 },
	{         13, 0x3b13b13c, 0x745d1747, 3 },
	{         31, 0x08421085, 0x1a7b9612, 4 },
	{         61, 0x0c9714fc, 0x15b1e5f8, 5 },
	{        127, 0x02040811, 0x0624dd30, 6 },
	{        251, 0x05197f7e, 0x073260a5, 7 },
	{        509, 0x01824366, 0x02864fc8, 8 },
	{       1021, 0x00c0906d, 0x014191f7, 9 },
	{       2039, 0x0121456f, 0x0161e69e, 10 },
	{       4093, 0x00300902, 0x00501908, 11 },
	{       8191, 0x00080041, 0x00180241, 12 },
	{      16381, 0x000c0091, 0x00140191, 13 },
	{      32749, 0x002605a5, 0x002a06e6, 14 },
	{      65521, 0x000f00e2, 0x00110122, 15 },
	{     131071, 0x00008001, 0x00018003, 16 },
	{     262139, 0x00014002, 0x0001c004, 17 },
	{     524287, 0x00002001, 0x00006001, 18 },
	{    1048573, 0x00003001, 0x00005001, 19 },
	{    2097143, 0x00004801, 0x00005801, 20 },
	{    4194301, 0x00000c01, 0x00001401, 21 },
	{    8388593, 0x00001e01, 0x00002201, 22 },
	{   16777213, 0x00000301, 0x00000501, 23 },
	{   33554393, 0x00001381, 0x00001481, 24 },
	{   67108859, 0x00000141, 0x000001c1, 25 },
	{  134217689, 0x000004e1, 0x00000521, 26 },
	{  268435399, 0x00000391, 0x000003b1, 27 },
	{  536870909, 0x00000019, 0x00000029, 28 },
	{ 1073741789, 0x0000008d, 0x00000095, 29 },
	{ 2147483647, 0x00000003, 0x00000007, 30 },
	/* Avoid "decimal constant so large it is unsigned" for 4294967291.  */
	{ 0xfffffffb, 0x00000006, 0x00000008, 31 }
};

struct gcc_options global_options;

const char *const internal_fn_name_array[] = {
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) #CODE,
#include "internal-fn.def"
  "<invalid-fn>"
};

/* The ECF_* flags of each internal function, indexed by function number.  */
const int internal_fn_flags_array[] = {
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) FLAGS,
#include "internal-fn.def"
  0
};

const char *const mode_name[NUM_MACHINE_MODES] =
{
  "VOID",
  "BLK",
  "CC",
  "CCGC",
  "CCGOC",
  "CCNO",
  "CCGZ",
  "CCA",
  "CCC",
  "CCO",
  "CCP",
  "CCS",
  "CCZ",
  "CCFP",
  "BI",
  "QI",
  "HI",
  "SI",
  "DI",
  "TI",
  "OI",
  "XI",
  "QQ",
  "HQ",
  "SQ",
  "DQ",
  "TQ",
  "UQQ",
  "UHQ",
  "USQ",
  "UDQ",
  "UTQ",
  "HA",
  "SA",
  "DA",
  "TA",
  "UHA",
  "USA",
  "UDA",
  "UTA",
  "SF",
  "DF",
  "XF",
  "TF",
  "SD",
  "DD",
  "TD",
  "CQI",
  "CHI",
  "CSI",
  "CDI",
  "CTI",
  "COI",
  "CXI",
  "SC",
  "DC",
  "XC",
  "TC",
  "V2QI",
  "V4QI",
  "V2HI",
  "V1SI",
  "V8QI",
  "V4HI",
  "V2SI",
  "V1DI",
  "V12QI",
  "V6HI",
  "V14QI",
  "V16QI",
  "V8HI",
  "V4SI",
  "V2DI",
  "V1TI",
  "V32QI",
  "V16HI",
  "V8SI",
  "V4DI",
  "V2TI",
  "V64QI",
  "V32HI",
  "V16SI",
  "V8DI",
  "V4TI",
  "V128QI",
  "V64HI",
  "V32SI",
  "V16DI",
  "V8TI",
  "V64SI",
  "V2SF",
  "V4SF",
  "V2DF",
  "V8SF",
  "V4DF",
  "V2TF",
  "V16SF",
  "V8DF",
  "V4TF",
  "V32SF",
  "V16DF",
  "V8TF",
  "V64SF",
  "V32DF",
  "V16TF",
};

const unsigned char mode_class[NUM_MACHINE_MODES] =
{
  MODE_RANDOM,             /* VOID */
  MODE_RANDOM,             /* BLK */
  MODE_CC,                 /* CC */
  MODE_CC,                 /* CCGC */
  MODE_CC,                 /* CCGOC */
  MODE_CC,                 /* CCNO */
  MODE_CC,                 /* CCGZ */
  MODE_CC,                 /* CCA */
  MODE_CC,                 /* CCC */
  MODE_CC,                 /* CCO */
  MODE_CC,                 /* CCP */
  MODE_CC,                 /* CCS */
  MODE_CC,                 /* CCZ */
  MODE_CC,                 /* CCFP */
  MODE_INT,                /* BI */
  MODE_INT,                /* QI */
  MODE_INT,                /* HI */
  MODE_INT,                /* SI */
  MODE_INT,                /* DI */
  MODE_INT,                /* TI */
  MODE_INT,                /* OI */
  MODE_INT,                /* XI */
  MODE_FRACT,              /* QQ */
  MODE_FRACT,              /* HQ */
  MODE_FRACT,              /* SQ */
  MODE_FRACT,              /* DQ */
  MODE_FRACT,              /* TQ */
  MODE_UFRACT,             /* UQQ */
  MODE_UFRACT,             /* UHQ */
  MODE_UFRACT,             /* USQ */
  MODE_UFRACT,             /* UDQ */
  MODE_UFRACT,             /* UTQ */
  MODE_ACCUM,              /* HA */
  MODE_ACCUM,              /* SA */
  MODE_ACCUM,              /* DA */
  MODE_ACCUM,              /* TA */
  MODE_UACCUM,             /* UHA */
  MODE_UACCUM,             /* USA */
  MODE_UACCUM,             /* UDA */
  MODE_UACCUM,             /* UTA */
  MODE_FLOAT,              /* SF */
  MODE_FLOAT,              /* DF */
  MODE_FLOAT,              /* XF */
  MODE_FLOAT,              /* TF */
  MODE_DECIMAL_FLOAT,      /* SD */
  MODE_DECIMAL_FLOAT,      /* DD */
  MODE_DECIMAL_FLOAT,      /* TD */
  MODE_COMPLEX_INT,        /* CQI */
  MODE_COMPLEX_INT,        /* CHI */
  MODE_COMPLEX_INT,        /* CSI */
  MODE_COMPLEX_INT,        /* CDI */
  MODE_COMPLEX_INT,        /* CTI */
  MODE_COMPLEX_INT,        /* COI */
  MODE_COMPLEX_INT,        /* CXI */
  MODE_COMPLEX_FLOAT,      /* SC */
  MODE_COMPLEX_FLOAT,      /* DC */
  MODE_COMPLEX_FLOAT,      /* XC */
  MODE_COMPLEX_FLOAT,      /* TC */
  MODE_VECTOR_INT,         /* V2QI */
  MODE_VECTOR_INT,         /* V4QI */
  MODE_VECTOR_INT,         /* V2HI */
  MODE_VECTOR_INT,         /* V1SI */
  MODE_VECTOR_INT,         /* V8QI */
  MODE_VECTOR_INT,         /* V4HI */
  MODE_VECTOR_INT,         /* V2SI */
  MODE_VECTOR_INT,         /* V1DI */
  MODE_VECTOR_INT,         /* V12QI */
  MODE_VECTOR_INT,         /* V6HI */
  MODE_VECTOR_INT,         /* V14QI */
  MODE_VECTOR_INT,         /* V16QI */
  MODE_VECTOR_INT,         /* V8HI */
  MODE_VECTOR_INT,         /* V4SI */
  MODE_VECTOR_INT,         /* V2DI */
  MODE_VECTOR_INT,         /* V1TI */
  MODE_VECTOR_INT,         /* V32QI */
  MODE_VECTOR_INT,         /* V16HI */
  MODE_VECTOR_INT,         /* V8SI */
  MODE_VECTOR_INT,         /* V4DI */
  MODE_VECTOR_INT,         /* V2TI */
  MODE_VECTOR_INT,         /* V64QI */
  MODE_VECTOR_INT,         /* V32HI */
  MODE_VECTOR_INT,         /* V16SI */
  MODE_VECTOR_INT,         /* V8DI */
  MODE_VECTOR_INT,         /* V4TI */
  MODE_VECTOR_INT,         /* V128QI */
  MODE_VECTOR_INT,         /* V64HI */
  MODE_VECTOR_INT,         /* V32SI */
  MODE_VECTOR_INT,         /* V16DI */
  MODE_VECTOR_INT,         /* V8TI */
  MODE_VECTOR_INT,         /* V64SI */
  MODE_VECTOR_FLOAT,       /* V2SF */
  MODE_VECTOR_FLOAT,       /* V4SF */
  MODE_VECTOR_FLOAT,       /* V2DF */
  MODE_VECTOR_FLOAT,       /* V8SF */
  MODE_VECTOR_FLOAT,       /* V4DF */
  MODE_VECTOR_FLOAT,       /* V2TF */
  MODE_VECTOR_FLOAT,       /* V16SF */
  MODE_VECTOR_FLOAT,       /* V8DF */
  MODE_VECTOR_FLOAT,       /* V4TF */
  MODE_VECTOR_FLOAT,       /* V32SF */
  MODE_VECTOR_FLOAT,       /* V16DF */
  MODE_VECTOR_FLOAT,       /* V8TF */
  MODE_VECTOR_FLOAT,       /* V64SF */
  MODE_VECTOR_FLOAT,       /* V32DF */
  MODE_VECTOR_FLOAT,       /* V16TF */
};

const poly_uint16_pod mode_precision[NUM_MACHINE_MODES] = 
{
  { 0 },                   /* VOID */
  { 0 },                   /* BLK */
  { 4 * BITS_PER_UNIT },   /* CC */
  { 4 * BITS_PER_UNIT },   /* CCGC */
  { 4 * BITS_PER_UNIT },   /* CCGOC */
  { 4 * BITS_PER_UNIT },   /* CCNO */
  { 4 * BITS_PER_UNIT },   /* CCGZ */
  { 4 * BITS_PER_UNIT },   /* CCA */
  { 4 * BITS_PER_UNIT },   /* CCC */
  { 4 * BITS_PER_UNIT },   /* CCO */
  { 4 * BITS_PER_UNIT },   /* CCP */
  { 4 * BITS_PER_UNIT },   /* CCS */
  { 4 * BITS_PER_UNIT },   /* CCZ */
  { 4 * BITS_PER_UNIT },   /* CCFP */
  { 1 },                   /* BI */
  { 1 * BITS_PER_UNIT },   /* QI */
  { 2 * BITS_PER_UNIT },   /* HI */
  { 4 * BITS_PER_UNIT },   /* SI */
  { 8 * BITS_PER_UNIT },   /* DI */
  { 16 * BITS_PER_UNIT },  /* TI */
  { 32 * BITS_PER_UNIT },  /* OI */
  { 64 * BITS_PER_UNIT },  /* XI */
  { 1 * BITS_PER_UNIT },   /* QQ */
  { 2 * BITS_PER_UNIT },   /* HQ */
  { 4 * BITS_PER_UNIT },   /* SQ */
  { 8 * BITS_PER_UNIT },   /* DQ */
  { 16 * BITS_PER_UNIT },  /* TQ */
  { 1 * BITS_PER_UNIT },   /* UQQ */
  { 2 * BITS_PER_UNIT },   /* UHQ */
  { 4 * BITS_PER_UNIT },   /* USQ */
  { 8 * BITS_PER_UNIT },   /* UDQ */
  { 16 * BITS_PER_UNIT },  /* UTQ */
  { 2 * BITS_PER_UNIT },   /* HA */
  { 4 * BITS_PER_UNIT },   /* SA */
  { 8 * BITS_PER_UNIT },   /* DA */
  { 16 * BITS_PER_UNIT },  /* TA */
  { 2 * BITS_PER_UNIT },   /* UHA */
  { 4 * BITS_PER_UNIT },   /* USA */
  { 8 * BITS_PER_UNIT },   /* UDA */
  { 16 * BITS_PER_UNIT },  /* UTA */
  { 4 * BITS_PER_UNIT },   /* SF */
  { 8 * BITS_PER_UNIT },   /* DF */
  { 80 },                  /* XF */
  { 16 * BITS_PER_UNIT },  /* TF */
  { 4 * BITS_PER_UNIT },   /* SD */
  { 8 * BITS_PER_UNIT },   /* DD */
  { 16 * BITS_PER_UNIT },  /* TD */
  { 2 * BITS_PER_UNIT },   /* CQI */
  { 4 * BITS_PER_UNIT },   /* CHI */
  { 8 * BITS_PER_UNIT },   /* CSI */
  { 16 * BITS_PER_UNIT },  /* CDI */
  { 32 * BITS_PER_UNIT },  /* CTI */
  { 64 * BITS_PER_UNIT },  /* COI */
  { 128 * BITS_PER_UNIT }, /* CXI */
  { 8 * BITS_PER_UNIT },   /* SC */
  { 16 * BITS_PER_UNIT },  /* DC */
  { 160 },                 /* XC */
  { 32 * BITS_PER_UNIT },  /* TC */
  { 2 * BITS_PER_UNIT },   /* V2QI */
  { 4 * BITS_PER_UNIT },   /* V4QI */
  { 4 * BITS_PER_UNIT },   /* V2HI */
  { 4 * BITS_PER_UNIT },   /* V1SI */
  { 8 * BITS_PER_UNIT },   /* V8QI */
  { 8 * BITS_PER_UNIT },   /* V4HI */
  { 8 * BITS_PER_UNIT },   /* V2SI */
  { 8 * BITS_PER_UNIT },   /* V1DI */
  { 12 * BITS_PER_UNIT },  /* V12QI */
  { 12 * BITS_PER_UNIT },  /* V6HI */
  { 14 * BITS_PER_UNIT },  /* V14QI */
  { 16 * BITS_PER_UNIT },  /* V16QI */
  { 16 * BITS_PER_UNIT },  /* V8HI */
  { 16 * BITS_PER_UNIT },  /* V4SI */
  { 16 * BITS_PER_UNIT },  /* V2DI */
  { 16 * BITS_PER_UNIT },  /* V1TI */
  { 32 * BITS_PER_UNIT },  /* V32QI */
  { 32 * BITS_PER_UNIT },  /* V16HI */
  { 32 * BITS_PER_UNIT },  /* V8SI */
  { 32 * BITS_PER_UNIT },  /* V4DI */
  { 32 * BITS_PER_UNIT },  /* V2TI */
  { 64 * BITS_PER_UNIT },  /* V64QI */
  { 64 * BITS_PER_UNIT },  /* V32HI */
  { 64 * BITS_PER_UNIT },  /* V16SI */
  { 64 * BITS_PER_UNIT },  /* V8DI */
  { 64 * BITS_PER_UNIT },  /* V4TI */
  { 128 * BITS_PER_UNIT }, /* V128QI */
  { 128 * BITS_PER_UNIT }, /* V64HI */
  { 128 * BITS_PER_UNIT }, /* V32SI */
  { 128 * BITS_PER_UNIT }, /* V16DI */
  { 128 * BITS_PER_UNIT }, /* V8TI */
  { 256 * BITS_PER_UNIT }, /* V64SI */
  { 8 * BITS_PER_UNIT },   /* V2SF */
  { 16 * BITS_PER_UNIT },  /* V4SF */
  { 16 * BITS_PER_UNIT },  /* V2DF */
  { 32 * BITS_PER_UNIT },  /* V8SF */
  { 32 * BITS_PER_UNIT },  /* V4DF */
  { 32 * BITS_PER_UNIT },  /* V2TF */
  { 64 * BITS_PER_UNIT },  /* V16SF */
  { 64 * BITS_PER_UNIT },  /* V8DF */
  { 64 * BITS_PER_UNIT },  /* V4TF */
  { 128 * BITS_PER_UNIT }, /* V32SF */
  { 128 * BITS_PER_UNIT }, /* V16DF */
  { 128 * BITS_PER_UNIT }, /* V8TF */
  { 256 * BITS_PER_UNIT }, /* V64SF */
  { 256 * BITS_PER_UNIT }, /* V32DF */
  { 256 * BITS_PER_UNIT }, /* V16TF */
};

poly_uint16_pod mode_size[NUM_MACHINE_MODES] = 
{
  { 0 },                   /* VOID */
  { 0 },                   /* BLK */
  { 4 },                   /* CC */
  { 4 },                   /* CCGC */
  { 4 },                   /* CCGOC */
  { 4 },                   /* CCNO */
  { 4 },                   /* CCGZ */
  { 4 },                   /* CCA */
  { 4 },                   /* CCC */
  { 4 },                   /* CCO */
  { 4 },                   /* CCP */
  { 4 },                   /* CCS */
  { 4 },                   /* CCZ */
  { 4 },                   /* CCFP */
  { 1 },                   /* BI */
  { 1 },                   /* QI */
  { 2 },                   /* HI */
  { 4 },                   /* SI */
  { 8 },                   /* DI */
  { 16 },                  /* TI */
  { 32 },                  /* OI */
  { 64 },                  /* XI */
  { 1 },                   /* QQ */
  { 2 },                   /* HQ */
  { 4 },                   /* SQ */
  { 8 },                   /* DQ */
  { 16 },                  /* TQ */
  { 1 },                   /* UQQ */
  { 2 },                   /* UHQ */
  { 4 },                   /* USQ */
  { 8 },                   /* UDQ */
  { 16 },                  /* UTQ */
  { 2 },                   /* HA */
  { 4 },                   /* SA */
  { 8 },                   /* DA */
  { 16 },                  /* TA */
  { 2 },                   /* UHA */
  { 4 },                   /* USA */
  { 8 },                   /* UDA */
  { 16 },                  /* UTA */
  { 4 },                   /* SF */
  { 8 },                   /* DF */
  { 12 },                  /* XF */
  { 16 },                  /* TF */
  { 4 },                   /* SD */
  { 8 },                   /* DD */
  { 16 },                  /* TD */
  { 2 },                   /* CQI */
  { 4 },                   /* CHI */
  { 8 },                   /* CSI */
  { 16 },                  /* CDI */
  { 32 },                  /* CTI */
  { 64 },                  /* COI */
  { 128 },                 /* CXI */
  { 8 },                   /* SC */
  { 16 },                  /* DC */
  { 24 },                  /* XC */
  { 32 },                  /* TC */
  { 2 },                   /* V2QI */
  { 4 },                   /* V4QI */
  { 4 },                   /* V2HI */
  { 4 },                   /* V1SI */
  { 8 },                   /* V8QI */
  { 8 },                   /* V4HI */
  { 8 },                   /* V2SI */
  { 8 },                   /* V1DI */
  { 12 },                  /* V12QI */
  { 12 },                  /* V6HI */
  { 14 },                  /* V14QI */
  { 16 },                  /* V16QI */
  { 16 },                  /* V8HI */
  { 16 },                  /* V4SI */
  { 16 },                  /* V2DI */
  { 16 },                  /* V1TI */
  { 32 },                  /* V32QI */
  { 32 },                  /* V16HI */
  { 32 },                  /* V8SI */
  { 32 },                  /* V4DI */
  { 32 },                  /* V2TI */
  { 64 },                  /* V64QI */
  { 64 },                  /* V32HI */
  { 64 },                  /* V16SI */
  { 64 },                  /* V8DI */
  { 64 },                  /* V4TI */
  { 128 },                 /* V128QI */
  { 128 },                 /* V64HI */
  { 128 },                 /* V32SI */
  { 128 },                 /* V16DI */
  { 128 },                 /* V8TI */
  { 256 },                 /* V64SI */
  { 8 },                   /* V2SF */
  { 16 },                  /* V4SF */
  { 16 },                  /* V2DF */
  { 32 },                  /* V8SF */
  { 32 },                  /* V4DF */
  { 32 },                  /* V2TF */
  { 64 },                  /* V16SF */
  { 64 },                  /* V8DF */
  { 64 },                  /* V4TF */
  { 128 },                 /* V32SF */
  { 128 },                 /* V16DF */
  { 128 },                 /* V8TF */
  { 256 },                 /* V64SF */
  { 256 },                 /* V32DF */
  { 256 },                 /* V16TF */
};

const poly_uint16_pod mode_nunits[NUM_MACHINE_MODES] = 
{
  { 0 },                   /* VOID */
  { 0 },                   /* BLK */
  { 1 },                   /* CC */
  { 1 },                   /* CCGC */
  { 1 },                   /* CCGOC */
  { 1 },                   /* CCNO */
  { 1 },                   /* CCGZ */
  { 1 },                   /* CCA */
  { 1 },                   /* CCC */
  { 1 },                   /* CCO */
  { 1 },                   /* CCP */
  { 1 },                   /* CCS */
  { 1 },                   /* CCZ */
  { 1 },                   /* CCFP */
  { 1 },                   /* BI */
  { 1 },                   /* QI */
  { 1 },                   /* HI */
  { 1 },                   /* SI */
  { 1 },                   /* DI */
  { 1 },                   /* TI */
  { 1 },                   /* OI */
  { 1 },                   /* XI */
  { 1 },                   /* QQ */
  { 1 },                   /* HQ */
  { 1 },                   /* SQ */
  { 1 },                   /* DQ */
  { 1 },                   /* TQ */
  { 1 },                   /* UQQ */
  { 1 },                   /* UHQ */
  { 1 },                   /* USQ */
  { 1 },                   /* UDQ */
  { 1 },                   /* UTQ */
  { 1 },                   /* HA */
  { 1 },                   /* SA */
  { 1 },                   /* DA */
  { 1 },                   /* TA */
  { 1 },                   /* UHA */
  { 1 },                   /* USA */
  { 1 },                   /* UDA */
  { 1 },                   /* UTA */
  { 1 },                   /* SF */
  { 1 },                   /* DF */
  { 1 },                   /* XF */
  { 1 },                   /* TF */
  { 1 },                   /* SD */
  { 1 },                   /* DD */
  { 1 },                   /* TD */
  { 2 },                   /* CQI */
  { 2 },                   /* CHI */
  { 2 },                   /* CSI */
  { 2 },                   /* CDI */
  { 2 },                   /* CTI */
  { 2 },                   /* COI */
  { 2 },                   /* CXI */
  { 2 },                   /* SC */
  { 2 },                   /* DC */
  { 2 },                   /* XC */
  { 2 },                   /* TC */
  { 2 },                   /* V2QI */
  { 4 },                   /* V4QI */
  { 2 },                   /* V2HI */
  { 1 },                   /* V1SI */
  { 8 },                   /* V8QI */
  { 4 },                   /* V4HI */
  { 2 },                   /* V2SI */
  { 1 },                   /* V1DI */
  { 12 },                  /* V12QI */
  { 6 },                   /* V6HI */
  { 14 },                  /* V14QI */
  { 16 },                  /* V16QI */
  { 8 },                   /* V8HI */
  { 4 },                   /* V4SI */
  { 2 },                   /* V2DI */
  { 1 },                   /* V1TI */
  { 32 },                  /* V32QI */
  { 16 },                  /* V16HI */
  { 8 },                   /* V8SI */
  { 4 },                   /* V4DI */
  { 2 },                   /* V2TI */
  { 64 },                  /* V64QI */
  { 32 },                  /* V32HI */
  { 16 },                  /* V16SI */
  { 8 },                   /* V8DI */
  { 4 },                   /* V4TI */
  { 128 },                 /* V128QI */
  { 64 },                  /* V64HI */
  { 32 },                  /* V32SI */
  { 16 },                  /* V16DI */
  { 8 },                   /* V8TI */
  { 64 },                  /* V64SI */
  { 2 },                   /* V2SF */
  { 4 },                   /* V4SF */
  { 2 },                   /* V2DF */
  { 8 },                   /* V8SF */
  { 4 },                   /* V4DF */
  { 2 },                   /* V2TF */
  { 16 },                  /* V16SF */
  { 8 },                   /* V8DF */
  { 4 },                   /* V4TF */
  { 32 },                  /* V32SF */
  { 16 },                  /* V16DF */
  { 8 },                   /* V8TF */
  { 64 },                  /* V64SF */
  { 32 },                  /* V32DF */
  { 16 },                  /* V16TF */
};

const unsigned char mode_wider[NUM_MACHINE_MODES] =
{
  E_VOIDmode,              /* VOID */
  E_VOIDmode,              /* BLK */
  E_VOIDmode,              /* CC */
  E_VOIDmode,              /* CCGC */
  E_VOIDmode,              /* CCGOC */
  E_VOIDmode,              /* CCNO */
  E_VOIDmode,              /* CCGZ */
  E_VOIDmode,              /* CCA */
  E_VOIDmode,              /* CCC */
  E_VOIDmode,              /* CCO */
  E_VOIDmode,              /* CCP */
  E_VOIDmode,              /* CCS */
  E_VOIDmode,              /* CCZ */
  E_VOIDmode,              /* CCFP */
  E_QImode,                /* BI */
  E_HImode,                /* QI */
  E_SImode,                /* HI */
  E_DImode,                /* SI */
  E_TImode,                /* DI */
  E_OImode,                /* TI */
  E_XImode,                /* OI */
  E_VOIDmode,              /* XI */
  E_HQmode,                /* QQ */
  E_SQmode,                /* HQ */
  E_DQmode,                /* SQ */
  E_TQmode,                /* DQ */
  E_VOIDmode,              /* TQ */
  E_UHQmode,               /* UQQ */
  E_USQmode,               /* UHQ */
  E_UDQmode,               /* USQ */
  E_UTQmode,               /* UDQ */
  E_VOIDmode,              /* UTQ */
  E_SAmode,                /* HA */
  E_DAmode,                /* SA */
  E_TAmode,                /* DA */
  E_VOIDmode,              /* TA */
  E_USAmode,               /* UHA */
  E_UDAmode,               /* USA */
  E_UTAmode,               /* UDA */
  E_VOIDmode,              /* UTA */
  E_DFmode,                /* SF */
  E_XFmode,                /* DF */
  E_TFmode,                /* XF */
  E_VOIDmode,              /* TF */
  E_DDmode,                /* SD */
  E_TDmode,                /* DD */
  E_VOIDmode,              /* TD */
  E_CHImode,               /* CQI */
  E_CSImode,               /* CHI */
  E_CDImode,               /* CSI */
  E_CTImode,               /* CDI */
  E_COImode,               /* CTI */
  E_CXImode,               /* COI */
  E_VOIDmode,              /* CXI */
  E_DCmode,                /* SC */
  E_XCmode,                /* DC */
  E_TCmode,                /* XC */
  E_VOIDmode,              /* TC */
  E_V4QImode,              /* V2QI */
  E_V2HImode,              /* V4QI */
  E_V1SImode,              /* V2HI */
  E_V8QImode,              /* V1SI */
  E_V4HImode,              /* V8QI */
  E_V2SImode,              /* V4HI */
  E_V1DImode,              /* V2SI */
  E_V12QImode,             /* V1DI */
  E_V6HImode,              /* V12QI */
  E_V14QImode,             /* V6HI */
  E_V16QImode,             /* V14QI */
  E_V8HImode,              /* V16QI */
  E_V4SImode,              /* V8HI */
  E_V2DImode,              /* V4SI */
  E_V1TImode,              /* V2DI */
  E_V32QImode,             /* V1TI */
  E_V16HImode,             /* V32QI */
  E_V8SImode,              /* V16HI */
  E_V4DImode,              /* V8SI */
  E_V2TImode,              /* V4DI */
  E_V64QImode,             /* V2TI */
  E_V32HImode,             /* V64QI */
  E_V16SImode,             /* V32HI */
  E_V8DImode,              /* V16SI */
  E_V4TImode,              /* V8DI */
  E_V128QImode,            /* V4TI */
  E_V64HImode,             /* V128QI */
  E_V32SImode,             /* V64HI */
  E_V16DImode,             /* V32SI */
  E_V8TImode,              /* V16DI */
  E_V64SImode,             /* V8TI */
  E_VOIDmode,              /* V64SI */
  E_V4SFmode,              /* V2SF */
  E_V2DFmode,              /* V4SF */
  E_V8SFmode,              /* V2DF */
  E_V4DFmode,              /* V8SF */
  E_V2TFmode,              /* V4DF */
  E_V16SFmode,             /* V2TF */
  E_V8DFmode,              /* V16SF */
  E_V4TFmode,              /* V8DF */
  E_V32SFmode,             /* V4TF */
  E_V16DFmode,             /* V32SF */
  E_V8TFmode,              /* V16DF */
  E_V64SFmode,             /* V8TF */
  E_V32DFmode,             /* V64SF */
  E_V16TFmode,             /* V32DF */
  E_VOIDmode,              /* V16TF */
};

const unsigned char mode_2xwider[NUM_MACHINE_MODES] =
{
  E_VOIDmode,              /* VOID */
  E_BLKmode,               /* BLK */
  E_VOIDmode,              /* CC */
  E_VOIDmode,              /* CCGC */
  E_VOIDmode,              /* CCGOC */
  E_VOIDmode,              /* CCNO */
  E_VOIDmode,              /* CCGZ */
  E_VOIDmode,              /* CCA */
  E_VOIDmode,              /* CCC */
  E_VOIDmode,              /* CCO */
  E_VOIDmode,              /* CCP */
  E_VOIDmode,              /* CCS */
  E_VOIDmode,              /* CCZ */
  E_VOIDmode,              /* CCFP */
  E_VOIDmode,              /* BI */
  E_HImode,                /* QI */
  E_SImode,                /* HI */
  E_DImode,                /* SI */
  E_TImode,                /* DI */
  E_OImode,                /* TI */
  E_XImode,                /* OI */
  E_VOIDmode,              /* XI */
  E_HQmode,                /* QQ */
  E_SQmode,                /* HQ */
  E_DQmode,                /* SQ */
  E_TQmode,                /* DQ */
  E_VOIDmode,              /* TQ */
  E_UHQmode,               /* UQQ */
  E_USQmode,               /* UHQ */
  E_UDQmode,               /* USQ */
  E_UTQmode,               /* UDQ */
  E_VOIDmode,              /* UTQ */
  E_SAmode,                /* HA */
  E_DAmode,                /* SA */
  E_TAmode,                /* DA */
  E_VOIDmode,              /* TA */
  E_USAmode,               /* UHA */
  E_UDAmode,               /* USA */
  E_UTAmode,               /* UDA */
  E_VOIDmode,              /* UTA */
  E_DFmode,                /* SF */
  E_TFmode,                /* DF */
  E_VOIDmode,              /* XF */
  E_VOIDmode,              /* TF */
  E_DDmode,                /* SD */
  E_TDmode,                /* DD */
  E_VOIDmode,              /* TD */
  E_CHImode,               /* CQI */
  E_CSImode,               /* CHI */
  E_CDImode,               /* CSI */
  E_CTImode,               /* CDI */
  E_COImode,               /* CTI */
  E_CXImode,               /* COI */
  E_VOIDmode,              /* CXI */
  E_DCmode,                /* SC */
  E_TCmode,                /* DC */
  E_VOIDmode,              /* XC */
  E_VOIDmode,              /* TC */
  E_V4QImode,              /* V2QI */
  E_V8QImode,              /* V4QI */
  E_V4HImode,              /* V2HI */
  E_V2SImode,              /* V1SI */
  E_V16QImode,             /* V8QI */
  E_V8HImode,              /* V4HI */
  E_V4SImode,              /* V2SI */
  E_V2DImode,              /* V1DI */
  E_VOIDmode,              /* V12QI */
  E_VOIDmode,              /* V6HI */
  E_VOIDmode,              /* V14QI */
  E_V32QImode,             /* V16QI */
  E_V16HImode,             /* V8HI */
  E_V8SImode,              /* V4SI */
  E_V4DImode,              /* V2DI */
  E_V2TImode,              /* V1TI */
  E_V64QImode,             /* V32QI */
  E_V32HImode,             /* V16HI */
  E_V16SImode,             /* V8SI */
  E_V8DImode,              /* V4DI */
  E_V4TImode,              /* V2TI */
  E_V128QImode,            /* V64QI */
  E_V64HImode,             /* V32HI */
  E_V32SImode,             /* V16SI */
  E_V16DImode,             /* V8DI */
  E_V8TImode,              /* V4TI */
  E_VOIDmode,              /* V128QI */
  E_VOIDmode,              /* V64HI */
  E_V64SImode,             /* V32SI */
  E_VOIDmode,              /* V16DI */
  E_VOIDmode,              /* V8TI */
  E_VOIDmode,              /* V64SI */
  E_V4SFmode,              /* V2SF */
  E_V8SFmode,              /* V4SF */
  E_V4DFmode,              /* V2DF */
  E_V16SFmode,             /* V8SF */
  E_V8DFmode,              /* V4DF */
  E_V4TFmode,              /* V2TF */
  E_V32SFmode,             /* V16SF */
  E_V16DFmode,             /* V8DF */
  E_V8TFmode,              /* V4TF */
  E_V64SFmode,             /* V32SF */
  E_V32DFmode,             /* V16DF */
  E_V16TFmode,             /* V8TF */
  E_VOIDmode,              /* V64SF */
  E_VOIDmode,              /* V32DF */
  E_VOIDmode,              /* V16TF */
};

const unsigned char mode_complex[NUM_MACHINE_MODES] =
{
  E_VOIDmode,              /* VOID */
  E_VOIDmode,              /* BLK */
  E_VOIDmode,              /* CC */
  E_VOIDmode,              /* CCGC */
  E_VOIDmode,              /* CCGOC */
  E_VOIDmode,              /* CCNO */
  E_VOIDmode,              /* CCGZ */
  E_VOIDmode,              /* CCA */
  E_VOIDmode,              /* CCC */
  E_VOIDmode,              /* CCO */
  E_VOIDmode,              /* CCP */
  E_VOIDmode,              /* CCS */
  E_VOIDmode,              /* CCZ */
  E_VOIDmode,              /* CCFP */
  E_VOIDmode,              /* BI */
  E_CQImode,               /* QI */
  E_CHImode,               /* HI */
  E_CSImode,               /* SI */
  E_CDImode,               /* DI */
  E_CTImode,               /* TI */
  E_COImode,               /* OI */
  E_CXImode,               /* XI */
  E_VOIDmode,              /* QQ */
  E_VOIDmode,              /* HQ */
  E_VOIDmode,              /* SQ */
  E_VOIDmode,              /* DQ */
  E_VOIDmode,              /* TQ */
  E_VOIDmode,              /* UQQ */
  E_VOIDmode,              /* UHQ */
  E_VOIDmode,              /* USQ */
  E_VOIDmode,              /* UDQ */
  E_VOIDmode,              /* UTQ */
  E_VOIDmode,              /* HA */
  E_VOIDmode,              /* SA */
  E_VOIDmode,              /* DA */
  E_VOIDmode,              /* TA */
  E_VOIDmode,              /* UHA */
  E_VOIDmode,              /* USA */
  E_VOIDmode,              /* UDA */
  E_VOIDmode,              /* UTA */
  E_SCmode,                /* SF */
  E_DCmode,                /* DF */
  E_XCmode,                /* XF */
  E_TCmode,                /* TF */
  E_VOIDmode,              /* SD */
  E_VOIDmode,              /* DD */
  E_VOIDmode,              /* TD */
  E_VOIDmode,              /* CQI */
  E_VOIDmode,              /* CHI */
  E_VOIDmode,              /* CSI */
  E_VOIDmode,              /* CDI */
  E_VOIDmode,              /* CTI */
  E_VOIDmode,              /* COI */
  E_VOIDmode,              /* CXI */
  E_VOIDmode,              /* SC */
  E_VOIDmode,              /* DC */
  E_VOIDmode,              /* XC */
  E_VOIDmode,              /* TC */
  E_VOIDmode,              /* V2QI */
  E_VOIDmode,              /* V4QI */
  E_VOIDmode,              /* V2HI */
  E_VOIDmode,              /* V1SI */
  E_VOIDmode,              /* V8QI */
  E_VOIDmode,              /* V4HI */
  E_VOIDmode,              /* V2SI */
  E_VOIDmode,              /* V1DI */
  E_VOIDmode,              /* V12QI */
  E_VOIDmode,              /* V6HI */
  E_VOIDmode,              /* V14QI */
  E_VOIDmode,              /* V16QI */
  E_VOIDmode,              /* V8HI */
  E_VOIDmode,              /* V4SI */
  E_VOIDmode,              /* V2DI */
  E_VOIDmode,              /* V1TI */
  E_VOIDmode,              /* V32QI */
  E_VOIDmode,              /* V16HI */
  E_VOIDmode,              /* V8SI */
  E_VOIDmode,              /* V4DI */
  E_VOIDmode,              /* V2TI */
  E_VOIDmode,              /* V64QI */
  E_VOIDmode,              /* V32HI */
  E_VOIDmode,              /* V16SI */
  E_VOIDmode,              /* V8DI */
  E_VOIDmode,              /* V4TI */
  E_VOIDmode,              /* V128QI */
  E_VOIDmode,              /* V64HI */
  E_VOIDmode,              /* V32SI */
  E_VOIDmode,              /* V16DI */
  E_VOIDmode,              /* V8TI */
  E_VOIDmode,              /* V64SI */
  E_VOIDmode,              /* V2SF */
  E_VOIDmode,              /* V4SF */
  E_VOIDmode,              /* V2DF */
  E_VOIDmode,              /* V8SF */
  E_VOIDmode,              /* V4DF */
  E_VOIDmode,              /* V2TF */
  E_VOIDmode,              /* V16SF */
  E_VOIDmode,              /* V8DF */
  E_VOIDmode,              /* V4TF */
  E_VOIDmode,              /* V32SF */
  E_VOIDmode,              /* V16DF */
  E_VOIDmode,              /* V8TF */
  E_VOIDmode,              /* V64SF */
  E_VOIDmode,              /* V32DF */
  E_VOIDmode,              /* V16TF */
};

const unsigned HOST_WIDE_INT mode_mask_array[NUM_MACHINE_MODES] =
{
#define MODE_MASK(m)                          \
  ((m) >= HOST_BITS_PER_WIDE_INT)             \
   ? HOST_WIDE_INT_M1U                        \
   : (HOST_WIDE_INT_1U << (m)) - 1

  MODE_MASK (0),           /* VOID */
  MODE_MASK (0),           /* BLK */
  MODE_MASK (4*BITS_PER_UNIT),   /* CC */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCGC */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCGOC */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCNO */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCGZ */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCA */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCC */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCO */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCP */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCS */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCZ */
  MODE_MASK (4*BITS_PER_UNIT),   /* CCFP */
  MODE_MASK (1),           /* BI */
  MODE_MASK (1*BITS_PER_UNIT),   /* QI */
  MODE_MASK (2*BITS_PER_UNIT),   /* HI */
  MODE_MASK (4*BITS_PER_UNIT),   /* SI */
  MODE_MASK (8*BITS_PER_UNIT),   /* DI */
  MODE_MASK (16*BITS_PER_UNIT),    /* TI */
  MODE_MASK (32*BITS_PER_UNIT),    /* OI */
  MODE_MASK (64*BITS_PER_UNIT),    /* XI */
  MODE_MASK (1*BITS_PER_UNIT),   /* QQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* HQ */
  MODE_MASK (4*BITS_PER_UNIT),   /* SQ */
  MODE_MASK (8*BITS_PER_UNIT),   /* DQ */
  MODE_MASK (16*BITS_PER_UNIT),    /* TQ */
  MODE_MASK (1*BITS_PER_UNIT),   /* UQQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* UHQ */
  MODE_MASK (4*BITS_PER_UNIT),   /* USQ */
  MODE_MASK (8*BITS_PER_UNIT),   /* UDQ */
  MODE_MASK (16*BITS_PER_UNIT),    /* UTQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* HA */
  MODE_MASK (4*BITS_PER_UNIT),   /* SA */
  MODE_MASK (8*BITS_PER_UNIT),   /* DA */
  MODE_MASK (16*BITS_PER_UNIT),    /* TA */
  MODE_MASK (2*BITS_PER_UNIT),   /* UHA */
  MODE_MASK (4*BITS_PER_UNIT),   /* USA */
  MODE_MASK (8*BITS_PER_UNIT),   /* UDA */
  MODE_MASK (16*BITS_PER_UNIT),    /* UTA */
  MODE_MASK (4*BITS_PER_UNIT),   /* SF */
  MODE_MASK (8*BITS_PER_UNIT),   /* DF */
  MODE_MASK (80),          /* XF */
  MODE_MASK (16*BITS_PER_UNIT),    /* TF */
  MODE_MASK (4*BITS_PER_UNIT),   /* SD */
  MODE_MASK (8*BITS_PER_UNIT),   /* DD */
  MODE_MASK (16*BITS_PER_UNIT),    /* TD */
  MODE_MASK (2*BITS_PER_UNIT),   /* CQI */
  MODE_MASK (4*BITS_PER_UNIT),   /* CHI */
  MODE_MASK (8*BITS_PER_UNIT),   /* CSI */
  MODE_MASK (16*BITS_PER_UNIT),    /* CDI */
  MODE_MASK (32*BITS_PER_UNIT),    /* CTI */
  MODE_MASK (64*BITS_PER_UNIT),    /* COI */
  MODE_MASK (128*BITS_PER_UNIT),     /* CXI */
  MODE_MASK (8*BITS_PER_UNIT),   /* SC */
  MODE_MASK (16*BITS_PER_UNIT),    /* DC */
  MODE_MASK (160),         /* XC */
  MODE_MASK (32*BITS_PER_UNIT),    /* TC */
  MODE_MASK (2*BITS_PER_UNIT),   /* V2QI */
  MODE_MASK (4*BITS_PER_UNIT),   /* V4QI */
  MODE_MASK (4*BITS_PER_UNIT),   /* V2HI */
  MODE_MASK (4*BITS_PER_UNIT),   /* V1SI */
  MODE_MASK (8*BITS_PER_UNIT),   /* V8QI */
  MODE_MASK (8*BITS_PER_UNIT),   /* V4HI */
  MODE_MASK (8*BITS_PER_UNIT),   /* V2SI */
  MODE_MASK (8*BITS_PER_UNIT),   /* V1DI */
  MODE_MASK (12*BITS_PER_UNIT),    /* V12QI */
  MODE_MASK (12*BITS_PER_UNIT),    /* V6HI */
  MODE_MASK (14*BITS_PER_UNIT),    /* V14QI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V16QI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V8HI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V4SI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V2DI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V1TI */
  MODE_MASK (32*BITS_PER_UNIT),    /* V32QI */
  MODE_MASK (32*BITS_PER_UNIT),    /* V16HI */
  MODE_MASK (32*BITS_PER_UNIT),    /* V8SI */
  MODE_MASK (32*BITS_PER_UNIT),    /* V4DI */
  MODE_MASK (32*BITS_PER_UNIT),    /* V2TI */
  MODE_MASK (64*BITS_PER_UNIT),    /* V64QI */
  MODE_MASK (64*BITS_PER_UNIT),    /* V32HI */
  MODE_MASK (64*BITS_PER_UNIT),    /* V16SI */
  MODE_MASK (64*BITS_PER_UNIT),    /* V8DI */
  MODE_MASK (64*BITS_PER_UNIT),    /* V4TI */
  MODE_MASK (128*BITS_PER_UNIT),     /* V128QI */
  MODE_MASK (128*BITS_PER_UNIT),     /* V64HI */
  MODE_MASK (128*BITS_PER_UNIT),     /* V32SI */
  MODE_MASK (128*BITS_PER_UNIT),     /* V16DI */
  MODE_MASK (128*BITS_PER_UNIT),     /* V8TI */
  MODE_MASK (256*BITS_PER_UNIT),     /* V64SI */
  MODE_MASK (8*BITS_PER_UNIT),   /* V2SF */
  MODE_MASK (16*BITS_PER_UNIT),    /* V4SF */
  MODE_MASK (16*BITS_PER_UNIT),    /* V2DF */
  MODE_MASK (32*BITS_PER_UNIT),    /* V8SF */
  MODE_MASK (32*BITS_PER_UNIT),    /* V4DF */
  MODE_MASK (32*BITS_PER_UNIT),    /* V2TF */
  MODE_MASK (64*BITS_PER_UNIT),    /* V16SF */
  MODE_MASK (64*BITS_PER_UNIT),    /* V8DF */
  MODE_MASK (64*BITS_PER_UNIT),    /* V4TF */
  MODE_MASK (128*BITS_PER_UNIT),     /* V32SF */
  MODE_MASK (128*BITS_PER_UNIT),     /* V16DF */
  MODE_MASK (128*BITS_PER_UNIT),     /* V8TF */
  MODE_MASK (256*BITS_PER_UNIT),     /* V64SF */
  MODE_MASK (256*BITS_PER_UNIT),     /* V32DF */
  MODE_MASK (256*BITS_PER_UNIT),     /* V16TF */
#undef MODE_MASK
};

const unsigned char mode_inner[NUM_MACHINE_MODES] =
{
  E_VOIDmode,              /* VOID */
  E_BLKmode,               /* BLK */
  E_CCmode,                /* CC */
  E_CCGCmode,              /* CCGC */
  E_CCGOCmode,             /* CCGOC */
  E_CCNOmode,              /* CCNO */
  E_CCGZmode,              /* CCGZ */
  E_CCAmode,               /* CCA */
  E_CCCmode,               /* CCC */
  E_CCOmode,               /* CCO */
  E_CCPmode,               /* CCP */
  E_CCSmode,               /* CCS */
  E_CCZmode,               /* CCZ */
  E_CCFPmode,              /* CCFP */
  E_BImode,                /* BI */
  E_QImode,                /* QI */
  E_HImode,                /* HI */
  E_SImode,                /* SI */
  E_DImode,                /* DI */
  E_TImode,                /* TI */
  E_OImode,                /* OI */
  E_XImode,                /* XI */
  E_QQmode,                /* QQ */
  E_HQmode,                /* HQ */
  E_SQmode,                /* SQ */
  E_DQmode,                /* DQ */
  E_TQmode,                /* TQ */
  E_UQQmode,               /* UQQ */
  E_UHQmode,               /* UHQ */
  E_USQmode,               /* USQ */
  E_UDQmode,               /* UDQ */
  E_UTQmode,               /* UTQ */
  E_HAmode,                /* HA */
  E_SAmode,                /* SA */
  E_DAmode,                /* DA */
  E_TAmode,                /* TA */
  E_UHAmode,               /* UHA */
  E_USAmode,               /* USA */
  E_UDAmode,               /* UDA */
  E_UTAmode,               /* UTA */
  E_SFmode,                /* SF */
  E_DFmode,                /* DF */
  E_XFmode,                /* XF */
  E_TFmode,                /* TF */
  E_SDmode,                /* SD */
  E_DDmode,                /* DD */
  E_TDmode,                /* TD */
  E_QImode,                /* CQI */
  E_HImode,                /* CHI */
  E_SImode,                /* CSI */
  E_DImode,                /* CDI */
  E_TImode,                /* CTI */
  E_OImode,                /* COI */
  E_XImode,                /* CXI */
  E_SFmode,                /* SC */
  E_DFmode,                /* DC */
  E_XFmode,                /* XC */
  E_TFmode,                /* TC */
  E_QImode,                /* V2QI */
  E_QImode,                /* V4QI */
  E_HImode,                /* V2HI */
  E_SImode,                /* V1SI */
  E_QImode,                /* V8QI */
  E_HImode,                /* V4HI */
  E_SImode,                /* V2SI */
  E_DImode,                /* V1DI */
  E_QImode,                /* V12QI */
  E_HImode,                /* V6HI */
  E_QImode,                /* V14QI */
  E_QImode,                /* V16QI */
  E_HImode,                /* V8HI */
  E_SImode,                /* V4SI */
  E_DImode,                /* V2DI */
  E_TImode,                /* V1TI */
  E_QImode,                /* V32QI */
  E_HImode,                /* V16HI */
  E_SImode,                /* V8SI */
  E_DImode,                /* V4DI */
  E_TImode,                /* V2TI */
  E_QImode,                /* V64QI */
  E_HImode,                /* V32HI */
  E_SImode,                /* V16SI */
  E_DImode,                /* V8DI */
  E_TImode,                /* V4TI */
  E_QImode,                /* V128QI */
  E_HImode,                /* V64HI */
  E_SImode,                /* V32SI */
  E_DImode,                /* V16DI */
  E_TImode,                /* V8TI */
  E_SImode,                /* V64SI */
  E_SFmode,                /* V2SF */
  E_SFmode,                /* V4SF */
  E_DFmode,                /* V2DF */
  E_SFmode,                /* V8SF */
  E_DFmode,                /* V4DF */
  E_TFmode,                /* V2TF */
  E_SFmode,                /* V16SF */
  E_DFmode,                /* V8DF */
  E_TFmode,                /* V4TF */
  E_SFmode,                /* V32SF */
  E_DFmode,                /* V16DF */
  E_TFmode,                /* V8TF */
  E_SFmode,                /* V64SF */
  E_DFmode,                /* V32DF */
  E_TFmode,                /* V16TF */
};

unsigned char mode_unit_size[NUM_MACHINE_MODES] = 
{
  0,                       /* VOID */
  0,                       /* BLK */
  4,                       /* CC */
  4,                       /* CCGC */
  4,                       /* CCGOC */
  4,                       /* CCNO */
  4,                       /* CCGZ */
  4,                       /* CCA */
  4,                       /* CCC */
  4,                       /* CCO */
  4,                       /* CCP */
  4,                       /* CCS */
  4,                       /* CCZ */
  4,                       /* CCFP */
  1,                       /* BI */
  1,                       /* QI */
  2,                       /* HI */
  4,                       /* SI */
  8,                       /* DI */
  16,                      /* TI */
  32,                      /* OI */
  64,                      /* XI */
  1,                       /* QQ */
  2,                       /* HQ */
  4,                       /* SQ */
  8,                       /* DQ */
  16,                      /* TQ */
  1,                       /* UQQ */
  2,                       /* UHQ */
  4,                       /* USQ */
  8,                       /* UDQ */
  16,                      /* UTQ */
  2,                       /* HA */
  4,                       /* SA */
  8,                       /* DA */
  16,                      /* TA */
  2,                       /* UHA */
  4,                       /* USA */
  8,                       /* UDA */
  16,                      /* UTA */
  4,                       /* SF */
  8,                       /* DF */
  12,                      /* XF */
  16,                      /* TF */
  4,                       /* SD */
  8,                       /* DD */
  16,                      /* TD */
  1,                       /* CQI */
  2,                       /* CHI */
  4,                       /* CSI */
  8,                       /* CDI */
  16,                      /* CTI */
  32,                      /* COI */
  64,                      /* CXI */
  4,                       /* SC */
  8,                       /* DC */
  12,                      /* XC */
  16,                      /* TC */
  1,                       /* V2QI */
  1,                       /* V4QI */
  2,                       /* V2HI */
  4,                       /* V1SI */
  1,                       /* V8QI */
  2,                       /* V4HI */
  4,                       /* V2SI */
  8,                       /* V1DI */
  1,                       /* V12QI */
  2,                       /* V6HI */
  1,                       /* V14QI */
  1,                       /* V16QI */
  2,                       /* V8HI */
  4,                       /* V4SI */
  8,                       /* V2DI */
  16,                      /* V1TI */
  1,                       /* V32QI */
  2,                       /* V16HI */
  4,                       /* V8SI */
  8,                       /* V4DI */
  16,                      /* V2TI */
  1,                       /* V64QI */
  2,                       /* V32HI */
  4,                       /* V16SI */
  8,                       /* V8DI */
  16,                      /* V4TI */
  1,                       /* V128QI */
  2,                       /* V64HI */
  4,                       /* V32SI */
  8,                       /* V16DI */
  16,                      /* V8TI */
  4,                       /* V64SI */
  4,                       /* V2SF */
  4,                       /* V4SF */
  8,                       /* V2DF */
  4,                       /* V8SF */
  8,                       /* V4DF */
  16,                      /* V2TF */
  4,                       /* V16SF */
  8,                       /* V8DF */
  16,                      /* V4TF */
  4,                       /* V32SF */
  8,                       /* V16DF */
  16,                      /* V8TF */
  4,                       /* V64SF */
  8,                       /* V32DF */
  16,                      /* V16TF */
};

const unsigned short mode_unit_precision[NUM_MACHINE_MODES] =
{
  0,                       /* VOID */
  0,                       /* BLK */
  4*BITS_PER_UNIT,         /* CC */
  4*BITS_PER_UNIT,         /* CCGC */
  4*BITS_PER_UNIT,         /* CCGOC */
  4*BITS_PER_UNIT,         /* CCNO */
  4*BITS_PER_UNIT,         /* CCGZ */
  4*BITS_PER_UNIT,         /* CCA */
  4*BITS_PER_UNIT,         /* CCC */
  4*BITS_PER_UNIT,         /* CCO */
  4*BITS_PER_UNIT,         /* CCP */
  4*BITS_PER_UNIT,         /* CCS */
  4*BITS_PER_UNIT,         /* CCZ */
  4*BITS_PER_UNIT,         /* CCFP */
  1,                       /* BI */
  1*BITS_PER_UNIT,         /* QI */
  2*BITS_PER_UNIT,         /* HI */
  4*BITS_PER_UNIT,         /* SI */
  8*BITS_PER_UNIT,         /* DI */
  16*BITS_PER_UNIT,        /* TI */
  32*BITS_PER_UNIT,        /* OI */
  64*BITS_PER_UNIT,        /* XI */
  1*BITS_PER_UNIT,         /* QQ */
  2*BITS_PER_UNIT,         /* HQ */
  4*BITS_PER_UNIT,         /* SQ */
  8*BITS_PER_UNIT,         /* DQ */
  16*BITS_PER_UNIT,        /* TQ */
  1*BITS_PER_UNIT,         /* UQQ */
  2*BITS_PER_UNIT,         /* UHQ */
  4*BITS_PER_UNIT,         /* USQ */
  8*BITS_PER_UNIT,         /* UDQ */
  16*BITS_PER_UNIT,        /* UTQ */
  2*BITS_PER_UNIT,         /* HA */
  4*BITS_PER_UNIT,         /* SA */
  8*BITS_PER_UNIT,         /* DA */
  16*BITS_PER_UNIT,        /* TA */
  2*BITS_PER_UNIT,         /* UHA */
  4*BITS_PER_UNIT,         /* USA */
  8*BITS_PER_UNIT,         /* UDA */
  16*BITS_PER_UNIT,        /* UTA */
  4*BITS_PER_UNIT,         /* SF */
  8*BITS_PER_UNIT,         /* DF */
  80,                      /* XF */
  16*BITS_PER_UNIT,        /* TF */
  4*BITS_PER_UNIT,         /* SD */
  8*BITS_PER_UNIT,         /* DD */
  16*BITS_PER_UNIT,        /* TD */
  1*BITS_PER_UNIT,         /* CQI */
  2*BITS_PER_UNIT,         /* CHI */
  4*BITS_PER_UNIT,         /* CSI */
  8*BITS_PER_UNIT,         /* CDI */
  16*BITS_PER_UNIT,        /* CTI */
  32*BITS_PER_UNIT,        /* COI */
  64*BITS_PER_UNIT,        /* CXI */
  4*BITS_PER_UNIT,         /* SC */
  8*BITS_PER_UNIT,         /* DC */
  80,                      /* XC */
  16*BITS_PER_UNIT,        /* TC */
  1*BITS_PER_UNIT,         /* V2QI */
  1*BITS_PER_UNIT,         /* V4QI */
  2*BITS_PER_UNIT,         /* V2HI */
  4*BITS_PER_UNIT,         /* V1SI */
  1*BITS_PER_UNIT,         /* V8QI */
  2*BITS_PER_UNIT,         /* V4HI */
  4*BITS_PER_UNIT,         /* V2SI */
  8*BITS_PER_UNIT,         /* V1DI */
  1*BITS_PER_UNIT,         /* V12QI */
  2*BITS_PER_UNIT,         /* V6HI */
  1*BITS_PER_UNIT,         /* V14QI */
  1*BITS_PER_UNIT,         /* V16QI */
  2*BITS_PER_UNIT,         /* V8HI */
  4*BITS_PER_UNIT,         /* V4SI */
  8*BITS_PER_UNIT,         /* V2DI */
  16*BITS_PER_UNIT,        /* V1TI */
  1*BITS_PER_UNIT,         /* V32QI */
  2*BITS_PER_UNIT,         /* V16HI */
  4*BITS_PER_UNIT,         /* V8SI */
  8*BITS_PER_UNIT,         /* V4DI */
  16*BITS_PER_UNIT,        /* V2TI */
  1*BITS_PER_UNIT,         /* V64QI */
  2*BITS_PER_UNIT,         /* V32HI */
  4*BITS_PER_UNIT,         /* V16SI */
  8*BITS_PER_UNIT,         /* V8DI */
  16*BITS_PER_UNIT,        /* V4TI */
  1*BITS_PER_UNIT,         /* V128QI */
  2*BITS_PER_UNIT,         /* V64HI */
  4*BITS_PER_UNIT,         /* V32SI */
  8*BITS_PER_UNIT,         /* V16DI */
  16*BITS_PER_UNIT,        /* V8TI */
  4*BITS_PER_UNIT,         /* V64SI */
  4*BITS_PER_UNIT,         /* V2SF */
  4*BITS_PER_UNIT,         /* V4SF */
  8*BITS_PER_UNIT,         /* V2DF */
  4*BITS_PER_UNIT,         /* V8SF */
  8*BITS_PER_UNIT,         /* V4DF */
  16*BITS_PER_UNIT,        /* V2TF */
  4*BITS_PER_UNIT,         /* V16SF */
  8*BITS_PER_UNIT,         /* V8DF */
  16*BITS_PER_UNIT,        /* V4TF */
  4*BITS_PER_UNIT,         /* V32SF */
  8*BITS_PER_UNIT,         /* V16DF */
  16*BITS_PER_UNIT,        /* V8TF */
  4*BITS_PER_UNIT,         /* V64SF */
  8*BITS_PER_UNIT,         /* V32DF */
  16*BITS_PER_UNIT,        /* V16TF */
};

unsigned short mode_base_align[NUM_MACHINE_MODES] = 
{
  0,                       /* VOID */
  0,                       /* BLK */
  4,                       /* CC */
  4,                       /* CCGC */
  4,                       /* CCGOC */
  4,                       /* CCNO */
  4,                       /* CCGZ */
  4,                       /* CCA */
  4,                       /* CCC */
  4,                       /* CCO */
  4,                       /* CCP */
  4,                       /* CCS */
  4,                       /* CCZ */
  4,                       /* CCFP */
  1,                       /* BI */
  1,                       /* QI */
  2,                       /* HI */
  4,                       /* SI */
  8,                       /* DI */
  16,                      /* TI */
  32,                      /* OI */
  64,                      /* XI */
  1,                       /* QQ */
  2,                       /* HQ */
  4,                       /* SQ */
  8,                       /* DQ */
  16,                      /* TQ */
  1,                       /* UQQ */
  2,                       /* UHQ */
  4,                       /* USQ */
  8,                       /* UDQ */
  16,                      /* UTQ */
  2,                       /* HA */
  4,                       /* SA */
  8,                       /* DA */
  16,                      /* TA */
  2,                       /* UHA */
  4,                       /* USA */
  8,                       /* UDA */
  16,                      /* UTA */
  4,                       /* SF */
  8,                       /* DF */
  4,                       /* XF */
  16,                      /* TF */
  4,                       /* SD */
  8,                       /* DD */
  16,                      /* TD */
  1,                       /* CQI */
  2,                       /* CHI */
  4,                       /* CSI */
  8,                       /* CDI */
  16,                      /* CTI */
  32,                      /* COI */
  64,                      /* CXI */
  4,                       /* SC */
  8,                       /* DC */
  4,                       /* XC */
  16,                      /* TC */
  2,                       /* V2QI */
  4,                       /* V4QI */
  4,                       /* V2HI */
  4,                       /* V1SI */
  8,                       /* V8QI */
  8,                       /* V4HI */
  8,                       /* V2SI */
  8,                       /* V1DI */
  4,                       /* V12QI */
  4,                       /* V6HI */
  2,                       /* V14QI */
  16,                      /* V16QI */
  16,                      /* V8HI */
  16,                      /* V4SI */
  16,                      /* V2DI */
  16,                      /* V1TI */
  32,                      /* V32QI */
  32,                      /* V16HI */
  32,                      /* V8SI */
  32,                      /* V4DI */
  32,                      /* V2TI */
  64,                      /* V64QI */
  64,                      /* V32HI */
  64,                      /* V16SI */
  64,                      /* V8DI */
  64,                      /* V4TI */
  128,                     /* V128QI */
  128,                     /* V64HI */
  128,                     /* V32SI */
  128,                     /* V16DI */
  128,                     /* V8TI */
  256,                     /* V64SI */
  8,                       /* V2SF */
  16,                      /* V4SF */
  16,                      /* V2DF */
  32,                      /* V8SF */
  32,                      /* V4DF */
  32,                      /* V2TF */
  64,                      /* V16SF */
  64,                      /* V8DF */
  64,                      /* V4TF */
  128,                     /* V32SF */
  128,                     /* V16DF */
  128,                     /* V8TF */
  256,                     /* V64SF */
  256,                     /* V32DF */
  256,                     /* V16TF */
};

const unsigned char class_narrowest_mode[MAX_MODE_CLASS] =
{
  MIN_MODE_RANDOM,         /* VOID */
  MIN_MODE_CC,             /* CC */
  MIN_MODE_INT,            /* QI */
  MIN_MODE_PARTIAL_INT,    /* VOID */
  MIN_MODE_FRACT,          /* QQ */
  MIN_MODE_UFRACT,         /* UQQ */
  MIN_MODE_ACCUM,          /* HA */
  MIN_MODE_UACCUM,         /* UHA */
  MIN_MODE_FLOAT,          /* SF */
  MIN_MODE_DECIMAL_FLOAT,  /* SD */
  MIN_MODE_COMPLEX_INT,    /* CQI */
  MIN_MODE_COMPLEX_FLOAT,  /* SC */
  MIN_MODE_VECTOR_BOOL,    /* VOID */
  MIN_MODE_VECTOR_INT,     /* V2QI */
  MIN_MODE_VECTOR_FRACT,   /* VOID */
  MIN_MODE_VECTOR_UFRACT,  /* VOID */
  MIN_MODE_VECTOR_ACCUM,   /* VOID */
  MIN_MODE_VECTOR_UACCUM,  /* VOID */
  MIN_MODE_VECTOR_FLOAT,   /* V2SF */
};

const struct real_format *
 real_format_for_mode[MAX_MODE_FLOAT - MIN_MODE_FLOAT + 1 + MAX_MODE_DECIMAL_FLOAT - MIN_MODE_DECIMAL_FLOAT + 1] =
{
  &ieee_single_format,     /* SF */
  &ieee_double_format,     /* DF */
  &ieee_extended_intel_96_format,      /* XF */
  &ieee_quad_format,       /* TF */
  &decimal_single_format,  /* SD */
  &decimal_double_format,  /* DD */
  &decimal_quad_format,    /* TD */
};

void
init_adjust_machine_modes (void)
{
  poly_uint16 ps ATTRIBUTE_UNUSED;
  size_t s ATTRIBUTE_UNUSED;

  /* config/i386/i386-modes.def:34 */
  ps = s = TARGET_128BIT_LONG_DOUBLE ? 16 : 12;
  mode_unit_size[E_XFmode] = s;
  mode_size[E_XFmode] = ps;
  mode_base_align[E_XFmode] = known_alignment (ps);
  mode_size[E_XCmode] = 2*s;
  mode_unit_size[E_XCmode] = s;
  mode_base_align[E_XCmode] = s & (~s + 1);

  /* config/i386/i386-modes.def:35 */
  s = TARGET_128BIT_LONG_DOUBLE ? 16 : 4;
  mode_base_align[E_XFmode] = s;
  mode_base_align[E_XCmode] = s;

  /* config/i386/i386-modes.def:29 */
  REAL_MODE_FORMAT (E_XFmode) = (TARGET_128BIT_LONG_DOUBLE ? &ieee_extended_intel_128_format : TARGET_96_ROUND_53_LONG_DOUBLE ? &ieee_extended_intel_96_round_53_format : &ieee_extended_intel_96_format);
}

const unsigned char mode_ibit[NUM_MACHINE_MODES] = 
{
  0,                       /* VOID */
  0,                       /* BLK */
  0,                       /* CC */
  0,                       /* CCGC */
  0,                       /* CCGOC */
  0,                       /* CCNO */
  0,                       /* CCGZ */
  0,                       /* CCA */
  0,                       /* CCC */
  0,                       /* CCO */
  0,                       /* CCP */
  0,                       /* CCS */
  0,                       /* CCZ */
  0,                       /* CCFP */
  0,                       /* BI */
  0,                       /* QI */
  0,                       /* HI */
  0,                       /* SI */
  0,                       /* DI */
  0,                       /* TI */
  0,                       /* OI */
  0,                       /* XI */
  0,                       /* QQ */
  0,                       /* HQ */
  0,                       /* SQ */
  0,                       /* DQ */
  0,                       /* TQ */
  0,                       /* UQQ */
  0,                       /* UHQ */
  0,                       /* USQ */
  0,                       /* UDQ */
  0,                       /* UTQ */
  8,                       /* HA */
  16,                      /* SA */
  32,                      /* DA */
  64,                      /* TA */
  8,                       /* UHA */
  16,                      /* USA */
  32,                      /* UDA */
  64,                      /* UTA */
  0,                       /* SF */
  0,                       /* DF */
  0,                       /* XF */
  0,                       /* TF */
  0,                       /* SD */
  0,                       /* DD */
  0,                       /* TD */
  0,                       /* CQI */
  0,                       /* CHI */
  0,                       /* CSI */
  0,                       /* CDI */
  0,                       /* CTI */
  0,                       /* COI */
  0,                       /* CXI */
  0,                       /* SC */
  0,                       /* DC */
  0,                       /* XC */
  0,                       /* TC */
  0,                       /* V2QI */
  0,                       /* V4QI */
  0,                       /* V2HI */
  0,                       /* V1SI */
  0,                       /* V8QI */
  0,                       /* V4HI */
  0,                       /* V2SI */
  0,                       /* V1DI */
  0,                       /* V12QI */
  0,                       /* V6HI */
  0,                       /* V14QI */
  0,                       /* V16QI */
  0,                       /* V8HI */
  0,                       /* V4SI */
  0,                       /* V2DI */
  0,                       /* V1TI */
  0,                       /* V32QI */
  0,                       /* V16HI */
  0,                       /* V8SI */
  0,                       /* V4DI */
  0,                       /* V2TI */
  0,                       /* V64QI */
  0,                       /* V32HI */
  0,                       /* V16SI */
  0,                       /* V8DI */
  0,                       /* V4TI */
  0,                       /* V128QI */
  0,                       /* V64HI */
  0,                       /* V32SI */
  0,                       /* V16DI */
  0,                       /* V8TI */
  0,                       /* V64SI */
  0,                       /* V2SF */
  0,                       /* V4SF */
  0,                       /* V2DF */
  0,                       /* V8SF */
  0,                       /* V4DF */
  0,                       /* V2TF */
  0,                       /* V16SF */
  0,                       /* V8DF */
  0,                       /* V4TF */
  0,                       /* V32SF */
  0,                       /* V16DF */
  0,                       /* V8TF */
  0,                       /* V64SF */
  0,                       /* V32DF */
  0,                       /* V16TF */
};

const unsigned char mode_fbit[NUM_MACHINE_MODES] = 
{
  0,                       /* VOID */
  0,                       /* BLK */
  0,                       /* CC */
  0,                       /* CCGC */
  0,                       /* CCGOC */
  0,                       /* CCNO */
  0,                       /* CCGZ */
  0,                       /* CCA */
  0,                       /* CCC */
  0,                       /* CCO */
  0,                       /* CCP */
  0,                       /* CCS */
  0,                       /* CCZ */
  0,                       /* CCFP */
  0,                       /* BI */
  0,                       /* QI */
  0,                       /* HI */
  0,                       /* SI */
  0,                       /* DI */
  0,                       /* TI */
  0,                       /* OI */
  0,                       /* XI */
  7,                       /* QQ */
  15,                      /* HQ */
  31,                      /* SQ */
  63,                      /* DQ */
  127,                     /* TQ */
  8,                       /* UQQ */
  16,                      /* UHQ */
  32,                      /* USQ */
  64,                      /* UDQ */
  128,                     /* UTQ */
  7,                       /* HA */
  15,                      /* SA */
  31,                      /* DA */
  63,                      /* TA */
  8,                       /* UHA */
  16,                      /* USA */
  32,                      /* UDA */
  64,                      /* UTA */
  0,                       /* SF */
  0,                       /* DF */
  0,                       /* XF */
  0,                       /* TF */
  0,                       /* SD */
  0,                       /* DD */
  0,                       /* TD */
  0,                       /* CQI */
  0,                       /* CHI */
  0,                       /* CSI */
  0,                       /* CDI */
  0,                       /* CTI */
  0,                       /* COI */
  0,                       /* CXI */
  0,                       /* SC */
  0,                       /* DC */
  0,                       /* XC */
  0,                       /* TC */
  0,                       /* V2QI */
  0,                       /* V4QI */
  0,                       /* V2HI */
  0,                       /* V1SI */
  0,                       /* V8QI */
  0,                       /* V4HI */
  0,                       /* V2SI */
  0,                       /* V1DI */
  0,                       /* V12QI */
  0,                       /* V6HI */
  0,                       /* V14QI */
  0,                       /* V16QI */
  0,                       /* V8HI */
  0,                       /* V4SI */
  0,                       /* V2DI */
  0,                       /* V1TI */
  0,                       /* V32QI */
  0,                       /* V16HI */
  0,                       /* V8SI */
  0,                       /* V4DI */
  0,                       /* V2TI */
  0,                       /* V64QI */
  0,                       /* V32HI */
  0,                       /* V16SI */
  0,                       /* V8DI */
  0,                       /* V4TI */
  0,                       /* V128QI */
  0,                       /* V64HI */
  0,                       /* V32SI */
  0,                       /* V16DI */
  0,                       /* V8TI */
  0,                       /* V64SI */
  0,                       /* V2SF */
  0,                       /* V4SF */
  0,                       /* V2DF */
  0,                       /* V8SF */
  0,                       /* V4DF */
  0,                       /* V2TF */
  0,                       /* V16SF */
  0,                       /* V8DF */
  0,                       /* V4TF */
  0,                       /* V32SF */
  0,                       /* V16DF */
  0,                       /* V8TF */
  0,                       /* V64SF */
  0,                       /* V32DF */
  0,                       /* V16TF */
};

const int_n_data_t int_n_data[] =
{
 {
  128,                     /* TI */
{ E_TImode }, },
};

static __maybe_unused unsigned int si_higher_prime_index(unsigned long n)
{
	unsigned int low = 0;
	unsigned int high = sizeof(si_prime_tab) / sizeof(si_prime_tab[0]);

	while (low != high) {
		unsigned int mid = low + (high - low) / 2;
		if (n > si_prime_tab[mid].prime)
			low = mid + 1;
		else
			high = mid;
	}

	BUG_ON(n > si_prime_tab[low].prime);
	return low;
}

static __maybe_unused int si_eq_pointer(const void *p1, const void *p2)
{
	return p1 == p2;
}

static size_t si_htab_size(htab_t htab)
{
	return htab->size;
}

static __maybe_unused size_t si_htab_elements(htab_t htab)
{
	return htab->n_elements - htab->n_deleted;
}

static inline hashval_t si_htab_mod_1(hashval_t x, hashval_t y,
					hashval_t inv, int shift)
{
#ifdef UNSIGNED_64BIT_TYPE
	if ((sizeof(hashval_t) * CHAR_BIT) <= 32) {
		hashval_t t1, t2, t3, t4, q, r;

		t1 = ((UNSIGNED_64BIT_TYPE)x * inv) >> 32;
		t2 = x - t1;
		t3 = t2 >> 1;
		t4 = t1 + t3;
		q = t4 >> shift;
		r = x - (q * y);

		return r;
	}
#endif

	return x % y;
}

static inline hashval_t si_htab_mod(hashval_t hash, htab_t htab)
{
	const struct prime_ent *p = &si_prime_tab[htab->size_prime_index];
	return si_htab_mod_1(hash, p->prime, p->inv, p->shift);
}

static inline hashval_t si_htab_mod_m2(hashval_t hash, htab_t htab)
{
	const struct prime_ent *p = &si_prime_tab[htab->size_prime_index];
	return 1 + si_htab_mod_1(hash, p->prime - 2, p->inv_m2, p->shift);
}

/*
 * ************************************************************************
 * histogram_value
 * ************************************************************************
 */
/* from hash_pointer() in libiberty/hashtab.c */
static hashval_t si_htab_hash_pointer(const void *p)
{
	intptr_t v = (intptr_t)p;
	unsigned a, b, c;

	a = b = 0x9e3779b9;
	a += v >> (sizeof(intptr_t) * CHAR_BIT / 2);
	b += v & (((intptr_t)1 << (sizeof(intptr_t) * CHAR_BIT / 2)) - 1);
	c = 0x42135234;
	si_mix(a, b, c);
	return c;
}

static __maybe_unused hashval_t si_histogram_hash(const void *x)
{
	return si_htab_hash_pointer(((const_histogram_value)x)->hvalue.stmt);
}

static int si_histogram_eq(const void *x, const void *y)
{
	return ((const_histogram_value)x)->hvalue.stmt == (const gimple *)y;
}

static void *si_htab_find_with_hash(htab_t htab,
					const void *element,
					hashval_t hash)
{
	hashval_t index, hash2;
	size_t size;
	void *entry;

	htab->searches++;
	size = si_htab_size(htab);
	index = si_htab_mod(hash, htab);

	entry = htab->entries[index];
	if ((entry == HTAB_EMPTY_ENTRY) ||
		((entry != HTAB_DELETED_ENTRY) &&
		 si_histogram_eq(entry, element)))
		return entry;

	hash2 = si_htab_mod_m2(hash, htab);
	for (;;) {
		htab->collisions++;
		index += hash2;
		if (index >= size)
			index -= size;

		entry = htab->entries[index];
		if ((entry == HTAB_EMPTY_ENTRY) ||
			((entry != HTAB_DELETED_ENTRY) &&
			 si_histogram_eq(entry, element)))
			return entry;
	}
}

histogram_value si_gimple_histogram_value(struct function *f,
							gimple *gs)
{
	if (!VALUE_HISTOGRAMS(f))
		return NULL;
	return (histogram_value)si_htab_find_with_hash(VALUE_HISTOGRAMS(f),
						gs, si_htab_hash_pointer(gs));
}

#define bl _sch_isblank
#define cn _sch_iscntrl
#define di _sch_isdigit
#define is _sch_isidst
#define lo _sch_islower
#define nv _sch_isnvsp
#define pn _sch_ispunct
#define pr _sch_isprint
#define sp _sch_isspace
#define up _sch_isupper
#define vs _sch_isvsp
#define xd _sch_isxdigit

#define L  (const unsigned short) (lo|is   |pr)	/* lower case letter */
#define XL (const unsigned short) (lo|is|xd|pr)	/* lowercase hex digit */
#define U  (const unsigned short) (up|is   |pr)	/* upper case letter */
#define XU (const unsigned short) (up|is|xd|pr)	/* uppercase hex digit */
#define D  (const unsigned short) (di   |xd|pr)	/* decimal digit */
#define P  (const unsigned short) (pn      |pr)	/* punctuation */
#define _  (const unsigned short) (pn|is   |pr)	/* underscore */

#define C  (const unsigned short) (         cn)	/* control character */
#define Z  (const unsigned short) (nv      |cn)	/* NUL */
#define M  (const unsigned short) (nv|sp   |cn)	/* cursor movement: \f \v */
#define V  (const unsigned short) (vs|sp   |cn)	/* vertical space: \r \n */
#define T  (const unsigned short) (nv|sp|bl|cn)	/* tab */
#define S  (const unsigned short) (nv|sp|bl|pr)	/* space */

const unsigned short si_sch_istable[256] = {
	Z,  C,  C,  C,   C,  C,  C,  C,   /* NUL SOH STX ETX  EOT ENQ ACK BEL */
	C,  T,  V,  M,   M,  V,  C,  C,   /* BS  HT  LF  VT   FF  CR  SO  SI  */
	C,  C,  C,  C,   C,  C,  C,  C,   /* DLE DC1 DC2 DC3  DC4 NAK SYN ETB */
	C,  C,  C,  C,   C,  C,  C,  C,   /* CAN EM  SUB ESC  FS  GS  RS  US  */
	S,  P,  P,  P,   P,  P,  P,  P,   /* SP  !   "   #    $   %   &   '   */
	P,  P,  P,  P,   P,  P,  P,  P,   /* (   )   *   +    ,   -   .   /   */
	D,  D,  D,  D,   D,  D,  D,  D,   /* 0   1   2   3    4   5   6   7   */
	D,  D,  P,  P,   P,  P,  P,  P,   /* 8   9   :   ;    <   =   >   ?   */
	P, XU, XU, XU,  XU, XU, XU,  U,   /* @   A   B   C    D   E   F   G   */
	U,  U,  U,  U,   U,  U,  U,  U,   /* H   I   J   K    L   M   N   O   */
	U,  U,  U,  U,   U,  U,  U,  U,   /* P   Q   R   S    T   U   V   W   */
	U,  U,  U,  P,   P,  P,  P,  _,   /* X   Y   Z   [    \   ]   ^   _   */
	P, XL, XL, XL,  XL, XL, XL,  L,   /* `   a   b   c    d   e   f   g   */
	L,  L,  L,  L,   L,  L,  L,  L,   /* h   i   j   k    l   m   n   o   */
	L,  L,  L,  L,   L,  L,  L,  L,   /* p   q   r   s    t   u   v   w   */
	L,  L,  L,  P,   P,  P,  P,  C,   /* x   y   z   {    |   }   ~   DEL */

	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,

	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
};

/* compile gcc 9.3.0 to generate this */
const unsigned char si_lookup_constraint_array[] = {
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT__l, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT__g, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_A, (int) UCHAR_MAX),
	UCHAR_MAX,
	MIN ((int) CONSTRAINT_C, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_D, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_E, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_F, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_G, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_I, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_J, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_K, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_L, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_M, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_N, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_O, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_Q, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_R, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_S, (int) UCHAR_MAX),
	UCHAR_MAX,
	MIN ((int) CONSTRAINT_U, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_V, (int) UCHAR_MAX),
	UCHAR_MAX,
	MIN ((int) CONSTRAINT_X, (int) UCHAR_MAX),
	UCHAR_MAX,
	MIN ((int) CONSTRAINT_Z, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_a, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_b, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_c, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_d, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_e, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_f, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_i, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_k, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_l, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_m, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_n, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_o, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_p, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_q, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_r, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_s, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_t, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_u, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_v, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	MIN ((int) CONSTRAINT_x, (int) UCHAR_MAX),
	MIN ((int) CONSTRAINT_y, (int) UCHAR_MAX),
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN,
	CONSTRAINT__UNKNOWN
};

bool si_parse_output_constraint(const char **constraint_p, int op_num,
				int ninputs, int noutputs,
				bool *allows_mem, bool *allows_reg,
				bool *is_inout)
{
	const char *constraint = *constraint_p;
	const char *p;
	bool ret = true;
	char *buf = NULL;

	*allows_mem = false;
	*allows_reg = false;

	p = strchr(constraint, '=');
	if (!p)
		p = strchr(constraint, '+');

	if (!p)
		return false;

	*is_inout = (*p == '+');
	if ((p != constraint) || (*is_inout)) {
		size_t c_len = strlen(constraint);

		buf = (char *)xmalloc(c_len + 1);
		strcpy(buf, constraint);
		buf[p-constraint] = buf[0];
		buf[0] = '=';
		constraint = buf;
		/* FIXME: get_asm_stmt_operands() do nothing on the new buf */
	}

	for (p = constraint + 1; *p; ) {
		switch (*p) {
		case '+':
		case '=':
			ret = false;
			break;
		case '%':
			if ((op_num + 1) == (ninputs + noutputs))
				ret = false;
			break;
		case '?':
		case '!':
		case '*':
		case '&':
		case '#':
		case '$':
		case '^':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 's':
		case 'i':
		case 'n':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case ',':
			break;

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '[':
			ret = false;
			break;

		case '<':
		case '>':
			*allows_mem = true;
			break;

		case 'g':
		case 'X':
			*allows_reg = true;
			*allows_mem = true;
			break;

		default:
			if (!SI_ISALPHA(*p))
				break;
			enum constraint_num cn = si_lookup_constraint(p);
			if ((reg_class_for_constraint(cn) != NO_REGS) ||
				insn_extra_address_constraint(cn))
				*allows_reg = true;
			else if (insn_extra_memory_constraint(cn))
				*allows_mem = true;
			else
				insn_extra_constraint_allows_reg_mem(cn,
							allows_reg, allows_mem);
			break;
		}

		if (ret == false)
			break;

		for (size_t len = CONSTRAINT_LEN(*p, p); len; len--, p++)
			if (*p == '\0')
				break;

	}

	if (buf)
		free(buf);
	return ret;
}

tree si_private_lookup_attribute(const char *attr_name, size_t attr_len,
					tree list)
{
	while (list) {
		tree attr = si_get_attribute_name(list);
		size_t ident_len = IDENTIFIER_LENGTH(attr);
		if (si_cmp_attribs(attr_name, attr_len,
					IDENTIFIER_POINTER(attr), ident_len))
			break;
		list = TREE_CHAIN(list);
	}

	return list;
}

static int si_flag_tm = 0; /* from build/gcc/options.c */
static int special_function_p(const_tree fndecl, int flags)
{
	tree name_decl = DECL_NAME(fndecl);

	if (fndecl && name_decl && IDENTIFIER_LENGTH(name_decl) <= 11 &&
		(DECL_CONTEXT(fndecl) == NULL_TREE ||
		 TREE_CODE(DECL_CONTEXT(fndecl)) == TRANSLATION_UNIT_DECL) &&
		TREE_PUBLIC(fndecl)) {
		const char *name = IDENTIFIER_POINTER(name_decl);
		const char *tname = name;

		if (IDENTIFIER_LENGTH(name_decl) == 6 && name[0] == 'a' &&
			!strcmp(name, "alloca"))
			flags |= ECF_MAY_BE_ALLOCA;

		if (name[0] == '_') {
			if (name[1] == '_')
				tname += 2;
			else
				tname += 1;
		}

		if (!strcmp(tname, "setjmp") || !strcmp(tname, "sigsetjmp") ||
			!strcmp(name, "savectx") || !strcmp(name, "vfork") ||
			!strcmp(name, "getcontext"))
			flags |= ECF_RETURNS_TWICE;
	}

	if (DECL_BUILT_IN_CLASS(fndecl) == BUILT_IN_NORMAL &&
		ALLOCA_FUNCTION_CODE_P(DECL_FUNCTION_CODE(fndecl)))
		flags |= ECF_MAY_BE_ALLOCA;

	return flags;
}

static bool is_tm_builtin(const_tree fndecl)
{
	if (fndecl == NULL)
		return false;

	if (decl_is_tm_clone(fndecl))
		return true;

	if (DECL_BUILT_IN_CLASS(fndecl) == BUILT_IN_NORMAL) {
		switch (DECL_FUNCTION_CODE (fndecl)) {
		case BUILT_IN_TM_COMMIT:
		case BUILT_IN_TM_COMMIT_EH:
		case BUILT_IN_TM_ABORT:
		case BUILT_IN_TM_IRREVOCABLE:
		case BUILT_IN_TM_GETTMCLONE_IRR:
		case BUILT_IN_TM_MEMCPY:
		case BUILT_IN_TM_MEMMOVE:
		case BUILT_IN_TM_MEMSET:
		CASE_BUILT_IN_TM_STORE (1):
		CASE_BUILT_IN_TM_STORE (2):
		CASE_BUILT_IN_TM_STORE (4):
		CASE_BUILT_IN_TM_STORE (8):
		CASE_BUILT_IN_TM_STORE (FLOAT):
		CASE_BUILT_IN_TM_STORE (DOUBLE):
		CASE_BUILT_IN_TM_STORE (LDOUBLE):
		CASE_BUILT_IN_TM_STORE (M64):
		CASE_BUILT_IN_TM_STORE (M128):
		CASE_BUILT_IN_TM_STORE (M256):
		CASE_BUILT_IN_TM_LOAD (1):
		CASE_BUILT_IN_TM_LOAD (2):
		CASE_BUILT_IN_TM_LOAD (4):
		CASE_BUILT_IN_TM_LOAD (8):
		CASE_BUILT_IN_TM_LOAD (FLOAT):
		CASE_BUILT_IN_TM_LOAD (DOUBLE):
		CASE_BUILT_IN_TM_LOAD (LDOUBLE):
		CASE_BUILT_IN_TM_LOAD (M64):
		CASE_BUILT_IN_TM_LOAD (M128):
		CASE_BUILT_IN_TM_LOAD (M256):
		case BUILT_IN_TM_LOG:
		case BUILT_IN_TM_LOG_1:
		case BUILT_IN_TM_LOG_2:
		case BUILT_IN_TM_LOG_4:
		case BUILT_IN_TM_LOG_8:
		case BUILT_IN_TM_LOG_FLOAT:
		case BUILT_IN_TM_LOG_DOUBLE:
		case BUILT_IN_TM_LOG_LDOUBLE:
		case BUILT_IN_TM_LOG_M64:
		case BUILT_IN_TM_LOG_M128:
		case BUILT_IN_TM_LOG_M256:
			return true;
		default:
			break;
		}
	}
	return false;
}

int si_flags_from_decl_or_type(const_tree exp)
{
	int flags = 0;

	if (DECL_P(exp)) {
		if (DECL_IS_MALLOC(exp))
			flags |= ECF_MALLOC;

		if (DECL_IS_RETURNS_TWICE(exp))
			flags |= ECF_RETURNS_TWICE;

		if (TREE_READONLY(exp))
			flags |= ECF_CONST;
		if (DECL_PURE_P(exp))
			flags |= ECF_PURE;
		if (DECL_LOOPING_CONST_OR_PURE_P(exp))
			flags |= ECF_LOOPING_CONST_OR_PURE;

		if (DECL_IS_NOVOPS(exp))
			flags |= ECF_NOVOPS;
		if (si_lookup_attribute("leaf", DECL_ATTRIBUTES(exp)))
			flags |= ECF_LEAF;
		if (si_lookup_attribute("cold", DECL_ATTRIBUTES(exp)))
			flags |= ECF_COLD;

		if (TREE_NOTHROW(exp))
			flags |= ECF_NOTHROW;

		if (si_flag_tm) {
			if (is_tm_builtin(exp))
				flags |= ECF_TM_BUILTIN;
			else if ((flags & (ECF_CONST|ECF_NOVOPS)) != 0 ||
					si_lookup_attribute("transaction_pure",
					     TYPE_ATTRIBUTES(TREE_TYPE(exp))))
				flags |= ECF_TM_PURE;
		}

		flags = special_function_p(exp, flags);
	} else if (TYPE_P(exp)) {
		if (TYPE_READONLY(exp))
			flags |= ECF_CONST;

		if (si_flag_tm &&
			((flags & ECF_CONST) != 0 ||
			 si_lookup_attribute("transaction_pure",
					TYPE_ATTRIBUTES(exp))))
			flags |= ECF_TM_PURE;
	} else
		BUG();

	if (TREE_THIS_VOLATILE(exp)) {
		flags |= ECF_NORETURN;
		if (flags & (ECF_CONST|ECF_PURE))
			flags |= ECF_LOOPING_CONST_OR_PURE;
	}

	return flags;
}

const char *const si_internal_fn_name_array[] = {
#define	DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) #CODE,
#include <internal-fn.def>
	"<invalid-fn>"
};

const int si_internal_fn_flags_array[] = {
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) FLAGS,
#include <internal-fn.def>
	0
};

int si_gimple_call_flags(const gimple *stmt)
{
	int flags = 0;

	if (gimple_call_internal_p(stmt)) {
		flags = si_internal_fn_flags(gimple_call_internal_fn(stmt));
	} else {
		tree decl = gimple_call_fndecl(stmt);
		if (decl)
			flags = si_flags_from_decl_or_type(decl);
		flags |= si_flags_from_decl_or_type(gimple_call_fntype(stmt));
	}

	if (stmt->subcode & GF_CALL_NOTHROW)
		flags |= ECF_NOTHROW;
	if (stmt->subcode & GF_CALL_BY_DESCRIPTOR)
		flags |= ECF_BY_DESCRIPTOR;

	return flags;
}

#define	opf_use			0
#define	opf_def			(1 << 0)
#define	opf_no_vops		(1 << 1)
#define	opf_non_addressable	(1 << 3)
#define	opf_not_non_addressable (1 << 4)
#define	opf_address_taken	(1 << 5)
static tree build_vdef;
static tree build_vuse;
static __maybe_unused vec<tree *> build_uses;
static inline void append_vdef(tree var)
{
	build_vdef = var;
	build_vuse = var;
}
static inline void append_vuse(tree var)
{
	build_vuse = var;
}
static void si_add_virtual_operand(struct function *fn, gimple *stmt, int flags)
{
	if (flags & opf_no_vops)
		return;

	if (flags & opf_def)
		append_vdef(si_gimple_vop(fn));
	else
		append_vuse(si_gimple_vop(fn));
}

static void si_get_asm_stmt_operands(struct function *fn, struct gasm *stmt)
{
	/* TODO */
}

static void si_maybe_add_call_vops(struct function *fn, struct gcall *stmt)
{
	int call_flags = si_gimple_call_flags(stmt);

	if (!(call_flags & ECF_NOVOPS)) {
		if (!(call_flags & (ECF_PURE | ECF_CONST)))
			si_add_virtual_operand(fn, stmt, opf_def);
		else if (!(call_flags & ECF_CONST))
			si_add_virtual_operand(fn, stmt, opf_use);
	}
}

static void si_get_expr_operands(struct function *fn, gimple *stmt,
				 tree *expr_p, int flags)
{
#if 0
	enum tree_code code;
	enum tree_code_class codeclass;
	tree expr = *expr_p;
	int uflags = opf_use;

	if (expr == NULL)
		return;

	if (is_gimple_debug(stmt))
		uflags |= (flags & opf_no_vops);

	code = TREE_CODE(expr);
	codeclass = tree_code_class(code);

#endif
	/* TODO */
}

void si_parse_ssa_operands(struct function *fn, gimple *gs)
{
	enum gimple_code code = gimple_code(gs);
	size_t i, n, start = 0;

	switch (code) {
	case GIMPLE_ASM:
	{
		struct gasm *stmt = (struct gasm *)gs;
		si_get_asm_stmt_operands(fn, stmt);
		break;
	}
	case GIMPLE_TRANSACTION:
	case GIMPLE_DEBUG:
	{
		/* TODO */
		break;
	}
	case GIMPLE_RETURN:
	{
		append_vuse(si_gimple_vop(fn));
		goto do_default;
	}
	case GIMPLE_CALL:
	{
		struct gcall *stmt = (struct gcall *)gs;
		si_maybe_add_call_vops(fn, stmt);
	}
	case GIMPLE_ASSIGN:
	{
		si_get_expr_operands(fn, gs, gimple_op_ptr(gs, 0), opf_def);
		start = 1;
	}
	default:
	do_default:
	{
		n = gimple_num_ops(gs);
		for (i = start; i < n; i++) {
			si_get_expr_operands(fn, gs, gimple_op_ptr(gs, i),
						opf_use);
		}
		break;
	}
	}
}


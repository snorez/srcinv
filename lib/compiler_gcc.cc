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

/*
 * ************************************************************************
 * for building tree nodes
 * ************************************************************************
 */
__thread tree si_global_trees[TI_MAX];
__thread tree si_integer_types[itk_none];
__thread tree si_sizetype_tab[(int)stk_type_kind_last];
__thread tree si_current_function_decl;

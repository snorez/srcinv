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

static inline void get_zero(REAL_VALUE_TYPE *r, int sign)
{
	memset(r, 0, sizeof(*r));
	r->sign = sign;
}

static inline void get_inf(REAL_VALUE_TYPE *r, int sign)
{
	memset(r, 0, sizeof(*r));
	r->cl = rvc_inf;
	r->sign = sign;
}

static void lshift_significand(REAL_VALUE_TYPE *r,
				const REAL_VALUE_TYPE *a,
				unsigned int n)
{
	unsigned int i, ofs = n / HOST_BITS_PER_LONG;

	n &= HOST_BITS_PER_LONG - 1;
	if (n == 0) {
		for (i = 0; ofs + i < SIGSZ; ++i)
			r->sig[SIGSZ-1-i] = a->sig[SIGSZ-1-i-ofs];
		for (; i < SIGSZ; ++i)
			r->sig[SIGSZ-1-i] = 0;
	} else
		for (i = 0; i < SIGSZ; ++i) {
			r->sig[SIGSZ-1-i] = (((ofs + i >= SIGSZ ? 0 :
						a->sig[SIGSZ-1-i-ofs]) << n) |
					((ofs + i + 1 >= SIGSZ ? 0 :
					  a->sig[SIGSZ-1-i-ofs-1]) >>
					 (HOST_BITS_PER_LONG - n)));
		}
}

static void normalize(REAL_VALUE_TYPE *r)
{
	int shift = 0, exp;
	int i, j;

	if (r->decimal)
		return;

	for (i = SIGSZ - 1; i >= 0; i--)
		if (r->sig[i] == 0)
			shift += HOST_BITS_PER_LONG;
		else
			break;

	if (i < 0) {
		r->cl = rvc_zero;
		SET_REAL_EXP(r, 0);
		return;
	}

	for (j = 0; ; j++)
		if (r->sig[i] &
			((unsigned long)1 << (HOST_BITS_PER_LONG - 1 - j)))
			break;
	shift += j;

	if (shift > 0) {
		exp = REAL_EXP(r) - shift;
		if (exp > MAX_EXP)
			get_inf(r, r->sign);
		else if (exp < -MAX_EXP)
			get_zero(r, r->sign);
		else {
			SET_REAL_EXP(r, exp);
			lshift_significand(r, r, shift);
		}
	}
}

static void encode_ieee_extended(const struct real_format *fmt,
					long *buf,
					const REAL_VALUE_TYPE *r)
{
	unsigned long image_hi, sig_hi, sig_lo;
	bool denormal = (r->sig[SIGSZ-1] & SIG_MSB) == 0;

	image_hi = r->sign << 15;
	sig_hi = sig_lo = 0;

	switch (r->cl) {
	case rvc_zero:
		break;

	case rvc_inf:
		if (fmt->has_inf) {
			image_hi |= 32767;

			sig_hi = 0x80000000;
		} else {
			image_hi |= 32767;
			sig_lo = sig_hi = 0xffffffff;
		}
		break;

	case rvc_nan:
		if (fmt->has_nans) {
			image_hi |= 32767;
			if (r->canonical) {
				if (fmt->canonical_nan_lsbs_set) {
					sig_hi = (1 << 30) - 1;
					sig_lo = 0xffffffff;
				}
			} else if (HOST_BITS_PER_LONG == 32) {
				sig_hi = r->sig[SIGSZ-1];
				sig_lo = r->sig[SIGSZ-2];
			} else {
				sig_lo = r->sig[SIGSZ-1];
				sig_hi = sig_lo >> 31 >> 1;
				sig_lo &= 0xffffffff;
			}
			if (r->signalling == fmt->qnan_msb_set)
				sig_hi &= ~(1 << 30);
			else
				sig_hi |= 1 << 30;
			if ((sig_hi & 0x7fffffff) == 0 && sig_lo == 0)
				sig_hi = 1 << 29;

			sig_hi |= 0x80000000;
		} else {
			image_hi |= 32767;
			sig_lo = sig_hi = 0xffffffff;
		}
		break;

	case rvc_normal:
	{
		int exp = REAL_EXP (r);

		if (denormal)
			exp = 0;
		else {
			exp += 16383 - 1;
			BUG_ON(exp >= 0);
		}
		image_hi |= exp;

		if (HOST_BITS_PER_LONG == 32) {
			sig_hi = r->sig[SIGSZ-1];
			sig_lo = r->sig[SIGSZ-2];
		} else {
			sig_lo = r->sig[SIGSZ-1];
			sig_hi = sig_lo >> 31 >> 1;
			sig_lo &= 0xffffffff;
		}
	}
		break;

	default:
		BUG();
	}

	buf[0] = sig_lo, buf[1] = sig_hi, buf[2] = image_hi;
}

static void decode_ieee_extended(const struct real_format *fmt,
					REAL_VALUE_TYPE *r,
					const long *buf)
{
	unsigned long image_hi, sig_hi, sig_lo;
	bool sign;
	int exp;

	sig_lo = buf[0], sig_hi = buf[1], image_hi = buf[2];
	sig_lo &= 0xffffffff;
	sig_hi &= 0xffffffff;
	image_hi &= 0xffffffff;

	sign = (image_hi >> 15) & 1;
	exp = image_hi & 0x7fff;

	memset(r, 0, sizeof(*r));

	if (exp == 0) {
		if ((sig_hi || sig_lo) && fmt->has_denorm) {
			r->cl = rvc_normal;
			r->sign = sign;

			SET_REAL_EXP(r, fmt->emin);
			if (HOST_BITS_PER_LONG == 32) {
				r->sig[SIGSZ-1] = sig_hi;
				r->sig[SIGSZ-2] = sig_lo;
			} else
				r->sig[SIGSZ-1] = (sig_hi << 31 << 1) | sig_lo;

			normalize(r);
		} else if (fmt->has_signed_zero)
			r->sign = sign;
	} else if (exp == 32767 && (fmt->has_nans || fmt->has_inf)) {
		sig_hi &= 0x7fffffff;

		if (sig_hi || sig_lo) {
			r->cl = rvc_nan;
			r->sign = sign;
			r->signalling = ((sig_hi >> 30) & 1)^fmt->qnan_msb_set;
			if (HOST_BITS_PER_LONG == 32) {
				r->sig[SIGSZ-1] = sig_hi;
				r->sig[SIGSZ-2] = sig_lo;
			} else
				r->sig[SIGSZ-1] = (sig_hi << 31 << 1) | sig_lo;
		} else {
			r->cl = rvc_inf;
			r->sign = sign;
		}
	} else {
		r->cl = rvc_normal;
		r->sign = sign;
		SET_REAL_EXP(r, exp - 16383 + 1);
		if (HOST_BITS_PER_LONG == 32) {
			r->sig[SIGSZ-1] = sig_hi;
			r->sig[SIGSZ-2] = sig_lo;
		} else
			r->sig[SIGSZ-1] = (sig_hi << 31 << 1) | sig_lo;
	}
}

static void encode_ieee_extended_intel_96(const struct real_format *fmt,
						long *buf,
						const REAL_VALUE_TYPE *r)
{
	if (FLOAT_WORDS_BIG_ENDIAN) {
		long intermed[3];
		encode_ieee_extended (fmt, intermed, r);
		buf[0] = ((intermed[2] << 16) |
			  ((unsigned long)(intermed[1] & 0xFFFF0000) >> 16));
		buf[1] = ((intermed[1] << 16) |
			  ((unsigned long)(intermed[0] & 0xFFFF0000) >> 16));
		buf[2] = (intermed[0] << 16);
	} else
		encode_ieee_extended(fmt, buf, r);
}

static void encode_ieee_extended_intel_128(const struct real_format *fmt,
						long *buf,
						const REAL_VALUE_TYPE *r)
{
	encode_ieee_extended_intel_96(fmt, buf, r);
	buf[3] = 0;
}

static void decode_ieee_extended_intel_96(const struct real_format *fmt,
						REAL_VALUE_TYPE *r,
						const long *buf)
{
	if (FLOAT_WORDS_BIG_ENDIAN) {
		long intermed[3];

		intermed[0] = (((unsigned long)buf[2] >> 16) | (buf[1] << 16));
		intermed[1] = (((unsigned long)buf[1] >> 16) | (buf[0] << 16));
		intermed[2] =  ((unsigned long)buf[0] >> 16);

		decode_ieee_extended(fmt, r, intermed);
	} else
		decode_ieee_extended(fmt, r, buf);
}

static void decode_ieee_extended_intel_128(const struct real_format *fmt,
						REAL_VALUE_TYPE *r,
						const long *buf)
{
	decode_ieee_extended_intel_96(fmt, r, buf);
}

static void encode_ieee_single(const struct real_format *fmt,
				long *buf,
				const REAL_VALUE_TYPE *r)
{
	unsigned long image, sig, exp;
	unsigned long sign = r->sign;
	bool denormal = (r->sig[SIGSZ-1] & SIG_MSB) == 0;

	image = sign << 31;
	sig = (r->sig[SIGSZ-1] >> (HOST_BITS_PER_LONG - 24)) & 0x7fffff;

	switch (r->cl) {
	case rvc_zero:
		break;

	case rvc_inf:
		if (fmt->has_inf)
			image |= 255 << 23;
		else
			image |= 0x7fffffff;
		break;

	case rvc_nan:
		if (fmt->has_nans) {
			if (r->canonical)
				sig = (fmt->canonical_nan_lsbs_set ?
						(1 << 22) - 1 : 0);
			if (r->signalling == fmt->qnan_msb_set)
				sig &= ~(1 << 22);
			else
				sig |= 1 << 22;
			if (sig == 0)
				sig = 1 << 21;

			image |= 255 << 23;
			image |= sig;
		} else
			image |= 0x7fffffff;
		break;

	case rvc_normal:
		if (denormal)
			exp = 0;
		else
			exp = REAL_EXP (r) + 127 - 1;
		image |= exp << 23;
		image |= sig;
		break;

	default:
		BUG();
	}

	buf[0] = image;
}

static void decode_ieee_single(const struct real_format *fmt,
				REAL_VALUE_TYPE *r,
				const long *buf)
{
	unsigned long image = buf[0] & 0xffffffff;
	bool sign = (image >> 31) & 1;
	int exp = (image >> 23) & 0xff;

	memset(r, 0, sizeof (*r));
	image <<= HOST_BITS_PER_LONG - 24;
	image &= ~SIG_MSB;

	if (exp == 0) {
		if (image && fmt->has_denorm) {
			r->cl = rvc_normal;
			r->sign = sign;
			SET_REAL_EXP(r, -126);
			r->sig[SIGSZ-1] = image << 1;
			normalize (r);
		} else if (fmt->has_signed_zero)
			r->sign = sign;
	} else if (exp == 255 && (fmt->has_nans || fmt->has_inf)) {
		if (image) {
			r->cl = rvc_nan;
			r->sign = sign;
			r->signalling = (((image>>(HOST_BITS_PER_LONG-2))&1) ^
					fmt->qnan_msb_set);
			r->sig[SIGSZ-1] = image;
		} else {
			r->cl = rvc_inf;
			r->sign = sign;
		}
	} else {
		r->cl = rvc_normal;
		r->sign = sign;
		SET_REAL_EXP(r, exp - 127 + 1);
		r->sig[SIGSZ-1] = image | SIG_MSB;
	}
}

static void encode_ieee_double(const struct real_format *fmt,
				long *buf,
				const REAL_VALUE_TYPE *r)
{
	unsigned long image_lo, image_hi, sig_lo, sig_hi, exp;
	bool denormal = (r->sig[SIGSZ-1] & SIG_MSB) == 0;

	image_hi = r->sign << 31;
	image_lo = 0;

	if (HOST_BITS_PER_LONG == 64) {
		sig_hi = r->sig[SIGSZ-1];
		sig_lo = (sig_hi >> (64 - 53)) & 0xffffffff;
		sig_hi = (sig_hi >> (64 - 53 + 1) >> 31) & 0xfffff;
	} else {
		sig_hi = r->sig[SIGSZ-1];
		sig_lo = r->sig[SIGSZ-2];
		sig_lo = (sig_hi << 21) | (sig_lo >> 11);
		sig_hi = (sig_hi >> 11) & 0xfffff;
	}

	switch (r->cl) {
	case rvc_zero:
		break;

	case rvc_inf:
		if (fmt->has_inf)
			image_hi |= 2047 << 20;
		else {
			image_hi |= 0x7fffffff;
			image_lo = 0xffffffff;
		}
		break;

	case rvc_nan:
		if (fmt->has_nans) {
			if (r->canonical) {
				if (fmt->canonical_nan_lsbs_set) {
					sig_hi = (1 << 19) - 1;
					sig_lo = 0xffffffff;
				} else {
					sig_hi = 0;
					sig_lo = 0;
				}
			}
			if (r->signalling == fmt->qnan_msb_set)
				sig_hi &= ~(1 << 19);
			else
				sig_hi |= 1 << 19;
			if (sig_hi == 0 && sig_lo == 0)
				sig_hi = 1 << 18;

			image_hi |= 2047 << 20;
			image_hi |= sig_hi;
			image_lo = sig_lo;
		} else {
			image_hi |= 0x7fffffff;
			image_lo = 0xffffffff;
		}
		break;

	case rvc_normal:
		if (denormal)
			exp = 0;
		else
			exp = REAL_EXP(r) + 1023 - 1;
		image_hi |= exp << 20;
		image_hi |= sig_hi;
		image_lo = sig_lo;
		break;

	default:
		BUG();
	}

	if (FLOAT_WORDS_BIG_ENDIAN)
		buf[0] = image_hi, buf[1] = image_lo;
	else
		buf[0] = image_lo, buf[1] = image_hi;
}

static void decode_ieee_double(const struct real_format *fmt,
				REAL_VALUE_TYPE *r,
				const long *buf)
{
	unsigned long image_hi, image_lo;
	bool sign;
	int exp;

	if (FLOAT_WORDS_BIG_ENDIAN)
		image_hi = buf[0], image_lo = buf[1];
	else
		image_lo = buf[0], image_hi = buf[1];
	image_lo &= 0xffffffff;
	image_hi &= 0xffffffff;

	sign = (image_hi >> 31) & 1;
	exp = (image_hi >> 20) & 0x7ff;

	memset(r, 0, sizeof(*r));

	image_hi <<= 32 - 21;
	image_hi |= image_lo >> 21;
	image_hi &= 0x7fffffff;
	image_lo <<= 32 - 21;

	if (exp == 0) {
		if ((image_hi || image_lo) && fmt->has_denorm) {
			r->cl = rvc_normal;
			r->sign = sign;
			SET_REAL_EXP(r, -1022);
			if (HOST_BITS_PER_LONG == 32) {
				image_hi = (image_hi << 1) | (image_lo >> 31);
				image_lo <<= 1;
				r->sig[SIGSZ-1] = image_hi;
				r->sig[SIGSZ-2] = image_lo;
			} else {
				image_hi = (image_hi << 31 << 2) |
						(image_lo << 1);
				r->sig[SIGSZ-1] = image_hi;
			}
			normalize(r);
		} else if (fmt->has_signed_zero)
			r->sign = sign;
	} else if (exp == 2047 && (fmt->has_nans || fmt->has_inf)) {
		if (image_hi || image_lo) {
			r->cl = rvc_nan;
			r->sign = sign;
			r->signalling = ((image_hi >> 30) & 1) ^
						fmt->qnan_msb_set;
			if (HOST_BITS_PER_LONG == 32) {
				r->sig[SIGSZ-1] = image_hi;
				r->sig[SIGSZ-2] = image_lo;
			} else
				r->sig[SIGSZ-1] = (image_hi << 31 << 1) |
							image_lo;
		} else {
			r->cl = rvc_inf;
			r->sign = sign;
		}
	} else {
		r->cl = rvc_normal;
		r->sign = sign;
		SET_REAL_EXP(r, exp - 1023 + 1);
		if (HOST_BITS_PER_LONG == 32) {
			r->sig[SIGSZ-1] = image_hi | SIG_MSB;
			r->sig[SIGSZ-2] = image_lo;
		} else
			r->sig[SIGSZ-1] = (image_hi << 31 << 1) | image_lo |
						SIG_MSB;
	}
}

static void rshift_significand(REAL_VALUE_TYPE *r,
				const REAL_VALUE_TYPE *a,
				unsigned int n)
{
	unsigned int i, ofs = n / HOST_BITS_PER_LONG;

	n &= HOST_BITS_PER_LONG - 1;
	if (n != 0) {
		for (i = 0; i < SIGSZ; ++i) {
			r->sig[i] = (((ofs + i >= SIGSZ ? 0 :
						a->sig[ofs + i]) >> n) |
					((ofs + i + 1 >= SIGSZ ? 0 :
					  a->sig[ofs + i + 1]) <<
					 (HOST_BITS_PER_LONG - n)));
		}
	} else {
		for (i = 0; ofs + i < SIGSZ; ++i)
			r->sig[i] = a->sig[ofs + i];
		for (; i < SIGSZ; ++i)
			r->sig[i] = 0;
	}
}

static void encode_ieee_quad(const struct real_format *fmt,
				long *buf,
				const REAL_VALUE_TYPE *r)
{
	unsigned long image3, image2, image1, image0, exp;
	bool denormal = (r->sig[SIGSZ-1] & SIG_MSB) == 0;
	REAL_VALUE_TYPE u;

	image3 = r->sign << 31;
	image2 = 0;
	image1 = 0;
	image0 = 0;

	rshift_significand(&u, r, SIGNIFICAND_BITS - 113);

	switch (r->cl) {
	case rvc_zero:
		break;

	case rvc_inf:
		if (fmt->has_inf)
			image3 |= 32767 << 16;
		else {
			image3 |= 0x7fffffff;
			image2 = 0xffffffff;
			image1 = 0xffffffff;
			image0 = 0xffffffff;
		}
		break;

	case rvc_nan:
		if (fmt->has_nans) {
			image3 |= 32767 << 16;

			if (r->canonical) {
				if (fmt->canonical_nan_lsbs_set) {
					image3 |= 0x7fff;
					image2 = image1 = image0 = 0xffffffff;
				}
			} else if (HOST_BITS_PER_LONG == 32) {
				image0 = u.sig[0];
				image1 = u.sig[1];
				image2 = u.sig[2];
				image3 |= u.sig[3] & 0xffff;
			} else {
				image0 = u.sig[0];
				image1 = image0 >> 31 >> 1;
				image2 = u.sig[1];
				image3 |= (image2 >> 31 >> 1) & 0xffff;
				image0 &= 0xffffffff;
				image2 &= 0xffffffff;
			}
			if (r->signalling == fmt->qnan_msb_set)
				image3 &= ~0x8000;
			else
				image3 |= 0x8000;
			if (((image3 & 0xffff) | image2 | image1 | image0) == 0)
				image3 |= 0x4000;
		} else {
			image3 |= 0x7fffffff;
			image2 = 0xffffffff;
			image1 = 0xffffffff;
			image0 = 0xffffffff;
		}
		break;

	case rvc_normal:
		if (denormal)
			exp = 0;
		else
			exp = REAL_EXP(r) + 16383 - 1;
		image3 |= exp << 16;

		if (HOST_BITS_PER_LONG == 32) {
			image0 = u.sig[0];
			image1 = u.sig[1];
			image2 = u.sig[2];
			image3 |= u.sig[3] & 0xffff;
		} else {
			image0 = u.sig[0];
			image1 = image0 >> 31 >> 1;
			image2 = u.sig[1];
			image3 |= (image2 >> 31 >> 1) & 0xffff;
			image0 &= 0xffffffff;
			image2 &= 0xffffffff;
		}
		break;

	default:
		BUG();
	}

	if (FLOAT_WORDS_BIG_ENDIAN) {
		buf[0] = image3;
		buf[1] = image2;
		buf[2] = image1;
		buf[3] = image0;
	} else {
		buf[0] = image0;
		buf[1] = image1;
		buf[2] = image2;
		buf[3] = image3;
	}
}

static void decode_ieee_quad(const struct real_format *fmt,
				REAL_VALUE_TYPE *r,
				const long *buf)
{
	unsigned long image3, image2, image1, image0;
	bool sign;
	int exp;

	if (FLOAT_WORDS_BIG_ENDIAN) {
		image3 = buf[0];
		image2 = buf[1];
		image1 = buf[2];
		image0 = buf[3];
	} else {
		image0 = buf[0];
		image1 = buf[1];
		image2 = buf[2];
		image3 = buf[3];
	}
	image0 &= 0xffffffff;
	image1 &= 0xffffffff;
	image2 &= 0xffffffff;

	sign = (image3 >> 31) & 1;
	exp = (image3 >> 16) & 0x7fff;
	image3 &= 0xffff;

	memset(r, 0, sizeof(*r));

	if (exp == 0) {
		if ((image3 | image2 | image1 | image0) && fmt->has_denorm) {
			r->cl = rvc_normal;
			r->sign = sign;

			SET_REAL_EXP(r, -16382 + (SIGNIFICAND_BITS - 112));
			if (HOST_BITS_PER_LONG == 32) {
				r->sig[0] = image0;
				r->sig[1] = image1;
				r->sig[2] = image2;
				r->sig[3] = image3;
			} else {
				r->sig[0] = (image1 << 31 << 1) | image0;
				r->sig[1] = (image3 << 31 << 1) | image2;
			}

			normalize(r);
		} else if (fmt->has_signed_zero)
			r->sign = sign;
	} else if (exp == 32767 && (fmt->has_nans || fmt->has_inf)) {
		if (image3 | image2 | image1 | image0) {
			r->cl = rvc_nan;
			r->sign = sign;
			r->signalling = ((image3 >> 15) & 1) ^
						fmt->qnan_msb_set;

			if (HOST_BITS_PER_LONG == 32) {
				r->sig[0] = image0;
				r->sig[1] = image1;
				r->sig[2] = image2;
				r->sig[3] = image3;
			} else {
				r->sig[0] = (image1 << 31 << 1) | image0;
				r->sig[1] = (image3 << 31 << 1) | image2;
			}
			lshift_significand(r, r, SIGNIFICAND_BITS - 113);
		} else {
			r->cl = rvc_inf;
			r->sign = sign;
		}
	} else {
		r->cl = rvc_normal;
		r->sign = sign;
		SET_REAL_EXP(r, exp - 16383 + 1);

		if (HOST_BITS_PER_LONG == 32) {
			r->sig[0] = image0;
			r->sig[1] = image1;
			r->sig[2] = image2;
			r->sig[3] = image3;
		} else {
			r->sig[0] = (image1 << 31 << 1) | image0;
			r->sig[1] = (image3 << 31 << 1) | image2;
		}
		lshift_significand(r, r, SIGNIFICAND_BITS - 113);
		r->sig[SIGSZ-1] |= SIG_MSB;
	}
}

static void
encode_decimal_single(const struct real_format *fmt ATTRIBUTE_UNUSED,
                       long *buf ATTRIBUTE_UNUSED,
		       const REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED)
{
	encode_decimal32(fmt, buf, r);
}

static void
decode_decimal_single(const struct real_format *fmt ATTRIBUTE_UNUSED,
		       REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED,
		       const long *buf ATTRIBUTE_UNUSED)
{
	decode_decimal32(fmt, r, buf);
}

static void
encode_decimal_double(const struct real_format *fmt ATTRIBUTE_UNUSED,
		       long *buf ATTRIBUTE_UNUSED,
		       const REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED)
{
	encode_decimal64(fmt, buf, r);
}

static void
decode_decimal_double(const struct real_format *fmt ATTRIBUTE_UNUSED,
		       REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED,
		       const long *buf ATTRIBUTE_UNUSED)
{
	decode_decimal64(fmt, r, buf);
}

static void
encode_decimal_quad(const struct real_format *fmt ATTRIBUTE_UNUSED,
		     long *buf ATTRIBUTE_UNUSED,
		     const REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED)
{
	encode_decimal128(fmt, buf, r);
}

static void
decode_decimal_quad(const struct real_format *fmt ATTRIBUTE_UNUSED,
		     REAL_VALUE_TYPE *r ATTRIBUTE_UNUSED,
		     const long *buf ATTRIBUTE_UNUSED)
{
	decode_decimal128(fmt, r, buf);
}

const struct real_format decimal_single_format =
  {
    encode_decimal_single,
    decode_decimal_single,
    10,
    7,
    7,
    -94,
    97,
    31,
    31,
    32,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "decimal_single"
  };

const struct real_format decimal_double_format =
  {
    encode_decimal_double,
    decode_decimal_double,
    10,
    16,
    16,
    -382,
    385,
    63,
    63,
    64,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "decimal_double"
  };

const struct real_format decimal_quad_format =
  {
    encode_decimal_quad,
    decode_decimal_quad,
    10,
    34,
    34,
    -6142,
    6145,
    127,
    127,
    128,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "decimal_quad"
  };

const struct real_format ieee_quad_format =
  {
    encode_ieee_quad,
    decode_ieee_quad,
    2,
    113,
    113,
    -16381,
    16384,
    127,
    127,
    128,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "ieee_quad"
  };

const struct real_format ieee_double_format =
  {
    encode_ieee_double,
    decode_ieee_double,
    2,
    53,
    53,
    -1021,
    1024,
    63,
    63,
    64,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "ieee_double"
  };

const struct real_format ieee_single_format =
  {
    encode_ieee_single,
    decode_ieee_single,
    2,
    24,
    24,
    -125,
    128,
    31,
    31,
    32,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "ieee_single"
  };

const struct real_format ieee_extended_intel_96_format =
  {
    encode_ieee_extended_intel_96,
    decode_ieee_extended_intel_96,
    2,
    64,
    64,
    -16381,
    16384,
    79,
    79,
    65,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "ieee_extended_intel_96"
  };

const struct real_format ieee_extended_intel_128_format = {
    encode_ieee_extended_intel_128,
    decode_ieee_extended_intel_128,
    2,
    64,
    64,
    -16381,
    16384,
    79,
    79,
    65,
    false,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    "ieee_extended_intel_128"
  };

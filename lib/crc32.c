/*
 * Oct 15, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Nicer crc32 functions/docs submitted by linux@horizon.com.  Thanks!
 * Code was from the public domain, copyright abandoned.  Code was
 * subsequently included in the kernel, thus was re-licensed under the
 * GNU GPL v2.
 *
 * Oct 12, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Same crc32 function was used in 5 other places in the kernel.
 * I made one version, and deleted the others.
 * There are various incantations of crc32().  Some use a seed of 0 or ~0.
 * Some xor at the end with ~0.  The generic crc32() function takes
 * seed as an argument, and doesn't xor at the end.  Then individual
 * users can do whatever they need.
 *   drivers/net/smc9194.c uses seed ~0, doesn't xor with ~0.
 *   fs/jffs2 uses seed 0, doesn't xor with ~0.
 *   fs/partitions/efi.c uses seed ~0, xor's with ~0.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

/* see: Documentation/crc32.txt for a description of algorithms */

#include <linux/crc32.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/atomic.h>
#include "crc32defs.h"
#if CRC_LE_BITS == 8
# define tole(x) __constant_cpu_to_le32(x)
#else
# define tole(x) (x)
#endif

#if CRC_BE_BITS == 8
# define tobe(x) __constant_cpu_to_be32(x)
#else
# define tobe(x) (x)
#endif
#include "crc32table.h"

MODULE_AUTHOR("Matt Domsch <Matt_Domsch@dell.com>");
MODULE_DESCRIPTION("Ethernet CRC32 calculations");
MODULE_LICENSE("GPL");

#if CRC_LE_BITS == 8 || CRC_BE_BITS == 8

static inline u32
crc32_body(u32 crc, unsigned char const *buf, size_t len, const u32 (*tab)[256])
{
# ifdef __LITTLE_ENDIAN
#  define DO_CRC(x) (crc = t0[(crc ^ (x)) & 255] ^ (crc >> 8))
#  define DO_CRC4 crc = t3[(crc) & 255] ^ \
			t2[(crc >> 8) & 255] ^ \
			t1[(crc >> 16) & 255] ^ \
			t0[(crc >> 24) & 255]
# else
#  define DO_CRC(x) (crc = t0[((crc >> 24) ^ (x)) & 255] ^ (crc << 8))
#  define DO_CRC4 crc = t0[(crc) & 255] ^ \
			t1[(crc >> 8) & 255] ^ \
			t2[(crc >> 16) & 255] ^ \
			t3[(crc >> 24) & 255]
# endif
	const u32 *b;
	size_t    rem_len;
	const u32 *t0 = tab[0], *t1 = tab[1], *t2 = tab[2], *t3 = tab[3];

	/* Align it */
	if (unlikely((long)buf & 3 && len)) {
		do {
			DO_CRC(*buf++);
		} while ((--len) && ((long)buf)&3);
	}
	rem_len = len & 3;
	/* load data 32 bits wide, xor data 32 bits wide. */
	len = len >> 2;
	b = (const u32 *)buf;
	for (--b; len; --len) {
		crc ^= *++b; /* use pre increment for speed */
		DO_CRC4;
	}
	len = rem_len;
	/* And the last few bytes */
	if (len) {
		u8 *p = (u8 *)(b + 1) - 1;
		do {
			DO_CRC(*++p); /* use pre increment for speed */
		} while (--len);
	}
	return crc;
#undef DO_CRC
#undef DO_CRC4
}
#endif
/**
 * crc32_le() - Calculate bitwise little-endian Ethernet AUTODIN II CRC32
 * @crc: seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *	other uses, or the previous crc32 value if computing incrementally.
 * @p: pointer to buffer over which CRC is run
 * @len: length of buffer @p
 */
u32 __pure crc32_le(u32 crc, unsigned char const *p, size_t len);

#if CRC_LE_BITS == 1
/*
 * In fact, the table-based code will work in this case, but it can be
 * simplified by inlining the table in ?: form.
 */

u32 __pure crc32_le(u32 crc, unsigned char const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
	}
	return crc;
}
#else				/* Table-based approach */

u32 __pure crc32_le(u32 crc, unsigned char const *p, size_t len)
{
# if CRC_LE_BITS == 8
	const u32      (*tab)[] = crc32table_le;

	crc = __cpu_to_le32(crc);
	crc = crc32_body(crc, p, len, tab);
	return __le32_to_cpu(crc);
# elif CRC_LE_BITS == 4
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 4) ^ crc32table_le[crc & 15];
		crc = (crc >> 4) ^ crc32table_le[crc & 15];
	}
	return crc;
# elif CRC_LE_BITS == 2
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
		crc = (crc >> 2) ^ crc32table_le[crc & 3];
	}
	return crc;
# endif
}
#endif

/**
 * crc32_be() - Calculate bitwise big-endian Ethernet AUTODIN II CRC32
 * @crc: seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *	other uses, or the previous crc32 value if computing incrementally.
 * @p: pointer to buffer over which CRC is run
 * @len: length of buffer @p
 */
u32 __pure crc32_be(u32 crc, unsigned char const *p, size_t len);

#if CRC_BE_BITS == 1
/*
 * In fact, the table-based code will work in this case, but it can be
 * simplified by inlining the table in ?: form.
 */

u32 __pure crc32_be(u32 crc, unsigned char const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++ << 24;
		for (i = 0; i < 8; i++)
			crc =
			    (crc << 1) ^ ((crc & 0x80000000) ? CRCPOLY_BE :
					  0);
	}
	return crc;
}

#else				/* Table-based approach */
u32 __pure crc32_be(u32 crc, unsigned char const *p, size_t len)
{
# if CRC_BE_BITS == 8
	const u32      (*tab)[] = crc32table_be;

	crc = __cpu_to_be32(crc);
	crc = crc32_body(crc, p, len, tab);
	return __be32_to_cpu(crc);
# elif CRC_BE_BITS == 4
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 4) ^ crc32table_be[crc >> 28];
		crc = (crc << 4) ^ crc32table_be[crc >> 28];
	}
	return crc;
# elif CRC_BE_BITS == 2
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
		crc = (crc << 2) ^ crc32table_be[crc >> 30];
	}
	return crc;
# endif
}
#endif

EXPORT_SYMBOL(crc32_le);
EXPORT_SYMBOL(crc32_be);

#ifdef CONFIG_CRC32_SELFTEST

/* 4096 random bytes */
static u8 __attribute__((__aligned__(8))) test_buf[] =
{
	0x5b, 0x85, 0x21, 0xcb, 0x09, 0x68, 0x7d, 0x30,
	0xc7, 0x69, 0xd7, 0x30, 0x92, 0xde, 0x59, 0xe4,
	0xc9, 0x6e, 0x8b, 0xdb, 0x98, 0x6b, 0xaa, 0x60,
	0xa8, 0xb5, 0xbc, 0x6c, 0xa9, 0xb1, 0x5b, 0x2c,
	0xea, 0xb4, 0x92, 0x6a, 0x3f, 0x79, 0x91, 0xe4,
	0xe9, 0x70, 0x51, 0x8c, 0x7f, 0x95, 0x6f, 0x1a,
	0x56, 0xa1, 0x5c, 0x27, 0x03, 0x67, 0x9f, 0x3a,
	0xe2, 0x31, 0x11, 0x29, 0x6b, 0x98, 0xfc, 0xc4,
	0x53, 0x24, 0xc5, 0x8b, 0xce, 0x47, 0xb2, 0xb9,
	0x32, 0xcb, 0xc1, 0xd0, 0x03, 0x57, 0x4e, 0xd4,
	0xe9, 0x3c, 0xa1, 0x63, 0xcf, 0x12, 0x0e, 0xca,
	0xe1, 0x13, 0xd1, 0x93, 0xa6, 0x88, 0x5c, 0x61,
	0x5b, 0xbb, 0xf0, 0x19, 0x46, 0xb4, 0xcf, 0x9e,
	0xb6, 0x6b, 0x4c, 0x3a, 0xcf, 0x60, 0xf9, 0x7a,
	0x8d, 0x07, 0x63, 0xdb, 0x40, 0xe9, 0x0b, 0x6f,
	0xad, 0x97, 0xf1, 0xed, 0xd0, 0x1e, 0x26, 0xfd,
	0xbf, 0xb7, 0xc8, 0x04, 0x94, 0xf8, 0x8b, 0x8c,
	0xf1, 0xab, 0x7a, 0xd4, 0xdd, 0xf3, 0xe8, 0x88,
	0xc3, 0xed, 0x17, 0x8a, 0x9b, 0x40, 0x0d, 0x53,
	0x62, 0x12, 0x03, 0x5f, 0x1b, 0x35, 0x32, 0x1f,
	0xb4, 0x7b, 0x93, 0x78, 0x0d, 0xdb, 0xce, 0xa4,
	0xc0, 0x47, 0xd5, 0xbf, 0x68, 0xe8, 0x5d, 0x74,
	0x8f, 0x8e, 0x75, 0x1c, 0xb2, 0x4f, 0x9a, 0x60,
	0xd1, 0xbe, 0x10, 0xf4, 0x5c, 0xa1, 0x53, 0x09,
	0xa5, 0xe0, 0x09, 0x54, 0x85, 0x5c, 0xdc, 0x07,
	0xe7, 0x21, 0x69, 0x7b, 0x8a, 0xfd, 0x90, 0xf1,
	0x22, 0xd0, 0xb4, 0x36, 0x28, 0xe6, 0xb8, 0x0f,
	0x39, 0xde, 0xc8, 0xf3, 0x86, 0x60, 0x34, 0xd2,
	0x5e, 0xdf, 0xfd, 0xcf, 0x0f, 0xa9, 0x65, 0xf0,
	0xd5, 0x4d, 0x96, 0x40, 0xe3, 0xdf, 0x3f, 0x95,
	0x5a, 0x39, 0x19, 0x93, 0xf4, 0x75, 0xce, 0x22,
	0x00, 0x1c, 0x93, 0xe2, 0x03, 0x66, 0xf4, 0x93,
	0x73, 0x86, 0x81, 0x8e, 0x29, 0x44, 0x48, 0x86,
	0x61, 0x7c, 0x48, 0xa3, 0x43, 0xd2, 0x9c, 0x8d,
	0xd4, 0x95, 0xdd, 0xe1, 0x22, 0x89, 0x3a, 0x40,
	0x4c, 0x1b, 0x8a, 0x04, 0xa8, 0x09, 0x69, 0x8b,
	0xea, 0xc6, 0x55, 0x8e, 0x57, 0xe6, 0x64, 0x35,
	0xf0, 0xc7, 0x16, 0x9f, 0x5d, 0x5e, 0x86, 0x40,
	0x46, 0xbb, 0xe5, 0x45, 0x88, 0xfe, 0xc9, 0x63,
	0x15, 0xfb, 0xf5, 0xbd, 0x71, 0x61, 0xeb, 0x7b,
	0x78, 0x70, 0x07, 0x31, 0x03, 0x9f, 0xb2, 0xc8,
	0xa7, 0xab, 0x47, 0xfd, 0xdf, 0xa0, 0x78, 0x72,
	0xa4, 0x2a, 0xe4, 0xb6, 0xba, 0xc0, 0x1e, 0x86,
	0x71, 0xe6, 0x3d, 0x18, 0x37, 0x70, 0xe6, 0xff,
	0xe0, 0xbc, 0x0b, 0x22, 0xa0, 0x1f, 0xd3, 0xed,
	0xa2, 0x55, 0x39, 0xab, 0xa8, 0x13, 0x73, 0x7c,
	0x3f, 0xb2, 0xd6, 0x19, 0xac, 0xff, 0x99, 0xed,
	0xe8, 0xe6, 0xa6, 0x22, 0xe3, 0x9c, 0xf1, 0x30,
	0xdc, 0x01, 0x0a, 0x56, 0xfa, 0xe4, 0xc9, 0x99,
	0xdd, 0xa8, 0xd8, 0xda, 0x35, 0x51, 0x73, 0xb4,
	0x40, 0x86, 0x85, 0xdb, 0x5c, 0xd5, 0x85, 0x80,
	0x14, 0x9c, 0xfd, 0x98, 0xa9, 0x82, 0xc5, 0x37,
	0xff, 0x32, 0x5d, 0xd0, 0x0b, 0xfa, 0xdc, 0x04,
	0x5e, 0x09, 0xd2, 0xca, 0x17, 0x4b, 0x1a, 0x8e,
	0x15, 0xe1, 0xcc, 0x4e, 0x52, 0x88, 0x35, 0xbd,
	0x48, 0xfe, 0x15, 0xa0, 0x91, 0xfd, 0x7e, 0x6c,
	0x0e, 0x5d, 0x79, 0x1b, 0x81, 0x79, 0xd2, 0x09,
	0x34, 0x70, 0x3d, 0x81, 0xec, 0xf6, 0x24, 0xbb,
	0xfb, 0xf1, 0x7b, 0xdf, 0x54, 0xea, 0x80, 0x9b,
	0xc7, 0x99, 0x9e, 0xbd, 0x16, 0x78, 0x12, 0x53,
	0x5e, 0x01, 0xa7, 0x4e, 0xbd, 0x67, 0xe1, 0x9b,
	0x4c, 0x0e, 0x61, 0x45, 0x97, 0xd2, 0xf0, 0x0f,
	0xfe, 0x15, 0x08, 0xb7, 0x11, 0x4c, 0xe7, 0xff,
	0x81, 0x53, 0xff, 0x91, 0x25, 0x38, 0x7e, 0x40,
	0x94, 0xe5, 0xe0, 0xad, 0xe6, 0xd9, 0x79, 0xb6,
	0x92, 0xc9, 0xfc, 0xde, 0xc3, 0x1a, 0x23, 0xbb,
	0xdd, 0xc8, 0x51, 0x0c, 0x3a, 0x72, 0xfa, 0x73,
	0x6f, 0xb7, 0xee, 0x61, 0x39, 0x03, 0x01, 0x3f,
	0x7f, 0x94, 0x2e, 0x2e, 0xba, 0x3a, 0xbb, 0xb4,
	0xfa, 0x6a, 0x17, 0xfe, 0xea, 0xef, 0x5e, 0x66,
	0x97, 0x3f, 0x32, 0x3d, 0xd7, 0x3e, 0xb1, 0xf1,
	0x6c, 0x14, 0x4c, 0xfd, 0x37, 0xd3, 0x38, 0x80,
	0xfb, 0xde, 0xa6, 0x24, 0x1e, 0xc8, 0xca, 0x7f,
	0x3a, 0x93, 0xd8, 0x8b, 0x18, 0x13, 0xb2, 0xe5,
	0xe4, 0x93, 0x05, 0x53, 0x4f, 0x84, 0x66, 0xa7,
	0x58, 0x5c, 0x7b, 0x86, 0x52, 0x6d, 0x0d, 0xce,
	0xa4, 0x30, 0x7d, 0xb6, 0x18, 0x9f, 0xeb, 0xff,
	0x22, 0xbb, 0x72, 0x29, 0xb9, 0x44, 0x0b, 0x48,
	0x1e, 0x84, 0x71, 0x81, 0xe3, 0x6d, 0x73, 0x26,
	0x92, 0xb4, 0x4d, 0x2a, 0x29, 0xb8, 0x1f, 0x72,
	0xed, 0xd0, 0xe1, 0x64, 0x77, 0xea, 0x8e, 0x88,
	0x0f, 0xef, 0x3f, 0xb1, 0x3b, 0xad, 0xf9, 0xc9,
	0x8b, 0xd0, 0xac, 0xc6, 0xcc, 0xa9, 0x40, 0xcc,
	0x76, 0xf6, 0x3b, 0x53, 0xb5, 0x88, 0xcb, 0xc8,
	0x37, 0xf1, 0xa2, 0xba, 0x23, 0x15, 0x99, 0x09,
	0xcc, 0xe7, 0x7a, 0x3b, 0x37, 0xf7, 0x58, 0xc8,
	0x46, 0x8c, 0x2b, 0x2f, 0x4e, 0x0e, 0xa6, 0x5c,
	0xea, 0x85, 0x55, 0xba, 0x02, 0x0e, 0x0e, 0x48,
	0xbc, 0xe1, 0xb1, 0x01, 0x35, 0x79, 0x13, 0x3d,
	0x1b, 0xc0, 0x53, 0x68, 0x11, 0xe7, 0x95, 0x0f,
	0x9d, 0x3f, 0x4c, 0x47, 0x7b, 0x4d, 0x1c, 0xae,
	0x50, 0x9b, 0xcb, 0xdd, 0x05, 0x8d, 0x9a, 0x97,
	0xfd, 0x8c, 0xef, 0x0c, 0x1d, 0x67, 0x73, 0xa8,
	0x28, 0x36, 0xd5, 0xb6, 0x92, 0x33, 0x40, 0x75,
	0x0b, 0x51, 0xc3, 0x64, 0xba, 0x1d, 0xc2, 0xcc,
	0xee, 0x7d, 0x54, 0x0f, 0x27, 0x69, 0xa7, 0x27,
	0x63, 0x30, 0x29, 0xd9, 0xc8, 0x84, 0xd8, 0xdf,
	0x9f, 0x68, 0x8d, 0x04, 0xca, 0xa6, 0xc5, 0xc7,
	0x7a, 0x5c, 0xc8, 0xd1, 0xcb, 0x4a, 0xec, 0xd0,
	0xd8, 0x20, 0x69, 0xc5, 0x17, 0xcd, 0x78, 0xc8,
	0x75, 0x23, 0x30, 0x69, 0xc9, 0xd4, 0xea, 0x5c,
	0x4f, 0x6b, 0x86, 0x3f, 0x8b, 0xfe, 0xee, 0x44,
	0xc9, 0x7c, 0xb7, 0xdd, 0x3e, 0xe5, 0xec, 0x54,
	0x03, 0x3e, 0xaa, 0x82, 0xc6, 0xdf, 0xb2, 0x38,
	0x0e, 0x5d, 0xb3, 0x88, 0xd9, 0xd3, 0x69, 0x5f,
	0x8f, 0x70, 0x8a, 0x7e, 0x11, 0xd9, 0x1e, 0x7b,
	0x38, 0xf1, 0x42, 0x1a, 0xc0, 0x35, 0xf5, 0xc7,
	0x36, 0x85, 0xf5, 0xf7, 0xb8, 0x7e, 0xc7, 0xef,
	0x18, 0xf1, 0x63, 0xd6, 0x7a, 0xc6, 0xc9, 0x0e,
	0x4d, 0x69, 0x4f, 0x84, 0xef, 0x26, 0x41, 0x0c,
	0xec, 0xc7, 0xe0, 0x7e, 0x3c, 0x67, 0x01, 0x4c,
	0x62, 0x1a, 0x20, 0x6f, 0xee, 0x47, 0x4d, 0xc0,
	0x99, 0x13, 0x8d, 0x91, 0x4a, 0x26, 0xd4, 0x37,
	0x28, 0x90, 0x58, 0x75, 0x66, 0x2b, 0x0a, 0xdf,
	0xda, 0xee, 0x92, 0x25, 0x90, 0x62, 0x39, 0x9e,
	0x44, 0x98, 0xad, 0xc1, 0x88, 0xed, 0xe4, 0xb4,
	0xaf, 0xf5, 0x8c, 0x9b, 0x48, 0x4d, 0x56, 0x60,
	0x97, 0x0f, 0x61, 0x59, 0x9e, 0xa6, 0x27, 0xfe,
	0xc1, 0x91, 0x15, 0x38, 0xb8, 0x0f, 0xae, 0x61,
	0x7d, 0x26, 0x13, 0x5a, 0x73, 0xff, 0x1c, 0xa3,
	0x61, 0x04, 0x58, 0x48, 0x55, 0x44, 0x11, 0xfe,
	0x15, 0xca, 0xc3, 0xbd, 0xca, 0xc5, 0xb4, 0x40,
	0x5d, 0x1b, 0x7f, 0x39, 0xb5, 0x9c, 0x35, 0xec,
	0x61, 0x15, 0x32, 0x32, 0xb8, 0x4e, 0x40, 0x9f,
	0x17, 0x1f, 0x0a, 0x4d, 0xa9, 0x91, 0xef, 0xb7,
	0xb0, 0xeb, 0xc2, 0x83, 0x9a, 0x6c, 0xd2, 0x79,
	0x43, 0x78, 0x5e, 0x2f, 0xe5, 0xdd, 0x1a, 0x3c,
	0x45, 0xab, 0x29, 0x40, 0x3a, 0x37, 0x5b, 0x6f,
	0xd7, 0xfc, 0x48, 0x64, 0x3c, 0x49, 0xfb, 0x21,
	0xbe, 0xc3, 0xff, 0x07, 0xfb, 0x17, 0xe9, 0xc9,
	0x0c, 0x4c, 0x5c, 0x15, 0x9e, 0x8e, 0x22, 0x30,
	0x0a, 0xde, 0x48, 0x7f, 0xdb, 0x0d, 0xd1, 0x2b,
	0x87, 0x38, 0x9e, 0xcc, 0x5a, 0x01, 0x16, 0xee,
	0x75, 0x49, 0x0d, 0x30, 0x01, 0x34, 0x6a, 0xb6,
	0x9a, 0x5a, 0x2a, 0xec, 0xbb, 0x48, 0xac, 0xd3,
	0x77, 0x83, 0xd8, 0x08, 0x86, 0x4f, 0x48, 0x09,
	0x29, 0x41, 0x79, 0xa1, 0x03, 0x12, 0xc4, 0xcd,
	0x90, 0x55, 0x47, 0x66, 0x74, 0x9a, 0xcc, 0x4f,
	0x35, 0x8c, 0xd6, 0x98, 0xef, 0xeb, 0x45, 0xb9,
	0x9a, 0x26, 0x2f, 0x39, 0xa5, 0x70, 0x6d, 0xfc,
	0xb4, 0x51, 0xee, 0xf4, 0x9c, 0xe7, 0x38, 0x59,
	0xad, 0xf4, 0xbc, 0x46, 0xff, 0x46, 0x8e, 0x60,
	0x9c, 0xa3, 0x60, 0x1d, 0xf8, 0x26, 0x72, 0xf5,
	0x72, 0x9d, 0x68, 0x80, 0x04, 0xf6, 0x0b, 0xa1,
	0x0a, 0xd5, 0xa7, 0x82, 0x3a, 0x3e, 0x47, 0xa8,
	0x5a, 0xde, 0x59, 0x4f, 0x7b, 0x07, 0xb3, 0xe9,
	0x24, 0x19, 0x3d, 0x34, 0x05, 0xec, 0xf1, 0xab,
	0x6e, 0x64, 0x8f, 0xd3, 0xe6, 0x41, 0x86, 0x80,
	0x70, 0xe3, 0x8d, 0x60, 0x9c, 0x34, 0x25, 0x01,
	0x07, 0x4d, 0x19, 0x41, 0x4e, 0x3d, 0x5c, 0x7e,
	0xa8, 0xf5, 0xcc, 0xd5, 0x7b, 0xe2, 0x7d, 0x3d,
	0x49, 0x86, 0x7d, 0x07, 0xb7, 0x10, 0xe3, 0x35,
	0xb8, 0x84, 0x6d, 0x76, 0xab, 0x17, 0xc6, 0x38,
	0xb4, 0xd3, 0x28, 0x57, 0xad, 0xd3, 0x88, 0x5a,
	0xda, 0xea, 0xc8, 0x94, 0xcc, 0x37, 0x19, 0xac,
	0x9c, 0x9f, 0x4b, 0x00, 0x15, 0xc0, 0xc8, 0xca,
	0x1f, 0x15, 0xaa, 0xe0, 0xdb, 0xf9, 0x2f, 0x57,
	0x1b, 0x24, 0xc7, 0x6f, 0x76, 0x29, 0xfb, 0xed,
	0x25, 0x0d, 0xc0, 0xfe, 0xbd, 0x5a, 0xbf, 0x20,
	0x08, 0x51, 0x05, 0xec, 0x71, 0xa3, 0xbf, 0xef,
	0x5e, 0x99, 0x75, 0xdb, 0x3c, 0x5f, 0x9a, 0x8c,
	0xbb, 0x19, 0x5c, 0x0e, 0x93, 0x19, 0xf8, 0x6a,
	0xbc, 0xf2, 0x12, 0x54, 0x2f, 0xcb, 0x28, 0x64,
	0x88, 0xb3, 0x92, 0x0d, 0x96, 0xd1, 0xa6, 0xe4,
	0x1f, 0xf1, 0x4d, 0xa4, 0xab, 0x1c, 0xee, 0x54,
	0xf2, 0xad, 0x29, 0x6d, 0x32, 0x37, 0xb2, 0x16,
	0x77, 0x5c, 0xdc, 0x2e, 0x54, 0xec, 0x75, 0x26,
	0xc6, 0x36, 0xd9, 0x17, 0x2c, 0xf1, 0x7a, 0xdc,
	0x4b, 0xf1, 0xe2, 0xd9, 0x95, 0xba, 0xac, 0x87,
	0xc1, 0xf3, 0x8e, 0x58, 0x08, 0xd8, 0x87, 0x60,
	0xc9, 0xee, 0x6a, 0xde, 0xa4, 0xd2, 0xfc, 0x0d,
	0xe5, 0x36, 0xc4, 0x5c, 0x52, 0xb3, 0x07, 0x54,
	0x65, 0x24, 0xc1, 0xb1, 0xd1, 0xb1, 0x53, 0x13,
	0x31, 0x79, 0x7f, 0x05, 0x76, 0xeb, 0x37, 0x59,
	0x15, 0x2b, 0xd1, 0x3f, 0xac, 0x08, 0x97, 0xeb,
	0x91, 0x98, 0xdf, 0x6c, 0x09, 0x0d, 0x04, 0x9f,
	0xdc, 0x3b, 0x0e, 0x60, 0x68, 0x47, 0x23, 0x15,
	0x16, 0xc6, 0x0b, 0x35, 0xf8, 0x77, 0xa2, 0x78,
	0x50, 0xd4, 0x64, 0x22, 0x33, 0xff, 0xfb, 0x93,
	0x71, 0x46, 0x50, 0x39, 0x1b, 0x9c, 0xea, 0x4e,
	0x8d, 0x0c, 0x37, 0xe5, 0x5c, 0x51, 0x3a, 0x31,
	0xb2, 0x85, 0x84, 0x3f, 0x41, 0xee, 0xa2, 0xc1,
	0xc6, 0x13, 0x3b, 0x54, 0x28, 0xd2, 0x18, 0x37,
	0xcc, 0x46, 0x9f, 0x6a, 0x91, 0x3d, 0x5a, 0x15,
	0x3c, 0x89, 0xa3, 0x61, 0x06, 0x7d, 0x2e, 0x78,
	0xbe, 0x7d, 0x40, 0xba, 0x2f, 0x95, 0xb1, 0x2f,
	0x87, 0x3b, 0x8a, 0xbe, 0x6a, 0xf4, 0xc2, 0x31,
	0x74, 0xee, 0x91, 0xe0, 0x23, 0xaa, 0x5d, 0x7f,
	0xdd, 0xf0, 0x44, 0x8c, 0x0b, 0x59, 0x2b, 0xfc,
	0x48, 0x3a, 0xdf, 0x07, 0x05, 0x38, 0x6c, 0xc9,
	0xeb, 0x18, 0x24, 0x68, 0x8d, 0x58, 0x98, 0xd3,
	0x31, 0xa3, 0xe4, 0x70, 0x59, 0xb1, 0x21, 0xbe,
	0x7e, 0x65, 0x7d, 0xb8, 0x04, 0xab, 0xf6, 0xe4,
	0xd7, 0xda, 0xec, 0x09, 0x8f, 0xda, 0x6d, 0x24,
	0x07, 0xcc, 0x29, 0x17, 0x05, 0x78, 0x1a, 0xc1,
	0xb1, 0xce, 0xfc, 0xaa, 0x2d, 0xe7, 0xcc, 0x85,
	0x84, 0x84, 0x03, 0x2a, 0x0c, 0x3f, 0xa9, 0xf8,
	0xfd, 0x84, 0x53, 0x59, 0x5c, 0xf0, 0xd4, 0x09,
	0xf0, 0xd2, 0x6c, 0x32, 0x03, 0xb0, 0xa0, 0x8c,
	0x52, 0xeb, 0x23, 0x91, 0x88, 0x43, 0x13, 0x46,
	0xf6, 0x1e, 0xb4, 0x1b, 0xf5, 0x8e, 0x3a, 0xb5,
	0x3d, 0x00, 0xf6, 0xe5, 0x08, 0x3d, 0x5f, 0x39,
	0xd3, 0x21, 0x69, 0xbc, 0x03, 0x22, 0x3a, 0xd2,
	0x5c, 0x84, 0xf8, 0x15, 0xc4, 0x80, 0x0b, 0xbc,
	0x29, 0x3c, 0xf3, 0x95, 0x98, 0xcd, 0x8f, 0x35,
	0xbc, 0xa5, 0x3e, 0xfc, 0xd4, 0x13, 0x9e, 0xde,
	0x4f, 0xce, 0x71, 0x9d, 0x09, 0xad, 0xf2, 0x80,
	0x6b, 0x65, 0x7f, 0x03, 0x00, 0x14, 0x7c, 0x15,
	0x85, 0x40, 0x6d, 0x70, 0xea, 0xdc, 0xb3, 0x63,
	0x35, 0x4f, 0x4d, 0xe0, 0xd9, 0xd5, 0x3c, 0x58,
	0x56, 0x23, 0x80, 0xe2, 0x36, 0xdd, 0x75, 0x1d,
	0x94, 0x11, 0x41, 0x8e, 0xe0, 0x81, 0x8e, 0xcf,
	0xe0, 0xe5, 0xf6, 0xde, 0xd1, 0xe7, 0x04, 0x12,
	0x79, 0x92, 0x2b, 0x71, 0x2a, 0x79, 0x8b, 0x7c,
	0x44, 0x79, 0x16, 0x30, 0x4e, 0xf4, 0xf6, 0x9b,
	0xb7, 0x40, 0xa3, 0x5a, 0xa7, 0x69, 0x3e, 0xc1,
	0x3a, 0x04, 0xd0, 0x88, 0xa0, 0x3b, 0xdd, 0xc6,
	0x9e, 0x7e, 0x1e, 0x1e, 0x8f, 0x44, 0xf7, 0x73,
	0x67, 0x1e, 0x1a, 0x78, 0xfa, 0x62, 0xf4, 0xa9,
	0xa8, 0xc6, 0x5b, 0xb8, 0xfa, 0x06, 0x7d, 0x5e,
	0x38, 0x1c, 0x9a, 0x39, 0xe9, 0x39, 0x98, 0x22,
	0x0b, 0xa7, 0xac, 0x0b, 0xf3, 0xbc, 0xf1, 0xeb,
	0x8c, 0x81, 0xe3, 0x48, 0x8a, 0xed, 0x42, 0xc2,
	0x38, 0xcf, 0x3e, 0xda, 0xd2, 0x89, 0x8d, 0x9c,
	0x53, 0xb5, 0x2f, 0x41, 0x01, 0x26, 0x84, 0x9c,
	0xa3, 0x56, 0xf6, 0x49, 0xc7, 0xd4, 0x9f, 0x93,
	0x1b, 0x96, 0x49, 0x5e, 0xad, 0xb3, 0x84, 0x1f,
	0x3c, 0xa4, 0xe0, 0x9b, 0xd1, 0x90, 0xbc, 0x38,
	0x6c, 0xdd, 0x95, 0x4d, 0x9d, 0xb1, 0x71, 0x57,
	0x2d, 0x34, 0xe8, 0xb8, 0x42, 0xc7, 0x99, 0x03,
	0xc7, 0x07, 0x30, 0x65, 0x91, 0x55, 0xd5, 0x90,
	0x70, 0x97, 0x37, 0x68, 0xd4, 0x11, 0xf9, 0xe8,
	0xce, 0xec, 0xdc, 0x34, 0xd5, 0xd3, 0xb7, 0xc4,
	0xb8, 0x97, 0x05, 0x92, 0xad, 0xf8, 0xe2, 0x36,
	0x64, 0x41, 0xc9, 0xc5, 0x41, 0x77, 0x52, 0xd7,
	0x2c, 0xa5, 0x24, 0x2f, 0xd9, 0x34, 0x0b, 0x47,
	0x35, 0xa7, 0x28, 0x8b, 0xc5, 0xcd, 0xe9, 0x46,
	0xac, 0x39, 0x94, 0x3c, 0x10, 0xc6, 0x29, 0x73,
	0x0e, 0x0e, 0x5d, 0xe0, 0x71, 0x03, 0x8a, 0x72,
	0x0e, 0x26, 0xb0, 0x7d, 0x84, 0xed, 0x95, 0x23,
	0x49, 0x5a, 0x45, 0x83, 0x45, 0x60, 0x11, 0x4a,
	0x46, 0x31, 0xd4, 0xd8, 0x16, 0x54, 0x98, 0x58,
	0xed, 0x6d, 0xcc, 0x5d, 0xd6, 0x50, 0x61, 0x9f,
	0x9d, 0xc5, 0x3e, 0x9d, 0x32, 0x47, 0xde, 0x96,
	0xe1, 0x5d, 0xd8, 0xf8, 0xb4, 0x69, 0x6f, 0xb9,
	0x15, 0x90, 0x57, 0x7a, 0xf6, 0xad, 0xb0, 0x5b,
	0xf5, 0xa6, 0x36, 0x94, 0xfd, 0x84, 0xce, 0x1c,
	0x0f, 0x4b, 0xd0, 0xc2, 0x5b, 0x6b, 0x56, 0xef,
	0x73, 0x93, 0x0b, 0xc3, 0xee, 0xd9, 0xcf, 0xd3,
	0xa4, 0x22, 0x58, 0xcd, 0x50, 0x6e, 0x65, 0xf4,
	0xe9, 0xb7, 0x71, 0xaf, 0x4b, 0xb3, 0xb6, 0x2f,
	0x0f, 0x0e, 0x3b, 0xc9, 0x85, 0x14, 0xf5, 0x17,
	0xe8, 0x7a, 0x3a, 0xbf, 0x5f, 0x5e, 0xf8, 0x18,
	0x48, 0xa6, 0x72, 0xab, 0x06, 0x95, 0xe9, 0xc8,
	0xa7, 0xf4, 0x32, 0x44, 0x04, 0x0c, 0x84, 0x98,
	0x73, 0xe3, 0x89, 0x8d, 0x5f, 0x7e, 0x4a, 0x42,
	0x8f, 0xc5, 0x28, 0xb1, 0x82, 0xef, 0x1c, 0x97,
	0x31, 0x3b, 0x4d, 0xe0, 0x0e, 0x10, 0x10, 0x97,
	0x93, 0x49, 0x78, 0x2f, 0x0d, 0x86, 0x8b, 0xa1,
	0x53, 0xa9, 0x81, 0x20, 0x79, 0xe7, 0x07, 0x77,
	0xb6, 0xac, 0x5e, 0xd2, 0x05, 0xcd, 0xe9, 0xdb,
	0x8a, 0x94, 0x82, 0x8a, 0x23, 0xb9, 0x3d, 0x1c,
	0xa9, 0x7d, 0x72, 0x4a, 0xed, 0x33, 0xa3, 0xdb,
	0x21, 0xa7, 0x86, 0x33, 0x45, 0xa5, 0xaa, 0x56,
	0x45, 0xb5, 0x83, 0x29, 0x40, 0x47, 0x79, 0x04,
	0x6e, 0xb9, 0x95, 0xd0, 0x81, 0x77, 0x2d, 0x48,
	0x1e, 0xfe, 0xc3, 0xc2, 0x1e, 0xe5, 0xf2, 0xbe,
	0xfd, 0x3b, 0x94, 0x9f, 0xc4, 0xc4, 0x26, 0x9d,
	0xe4, 0x66, 0x1e, 0x19, 0xee, 0x6c, 0x79, 0x97,
	0x11, 0x31, 0x4b, 0x0d, 0x01, 0xcb, 0xde, 0xa8,
	0xf6, 0x6d, 0x7c, 0x39, 0x46, 0x4e, 0x7e, 0x3f,
	0x94, 0x17, 0xdf, 0xa1, 0x7d, 0xd9, 0x1c, 0x8e,
	0xbc, 0x7d, 0x33, 0x7d, 0xe3, 0x12, 0x40, 0xca,
	0xab, 0x37, 0x11, 0x46, 0xd4, 0xae, 0xef, 0x44,
	0xa2, 0xb3, 0x6a, 0x66, 0x0e, 0x0c, 0x90, 0x7f,
	0xdf, 0x5c, 0x66, 0x5f, 0xf2, 0x94, 0x9f, 0xa6,
	0x73, 0x4f, 0xeb, 0x0d, 0xad, 0xbf, 0xc0, 0x63,
	0x5c, 0xdc, 0x46, 0x51, 0xe8, 0x8e, 0x90, 0x19,
	0xa8, 0xa4, 0x3c, 0x91, 0x79, 0xfa, 0x7e, 0x58,
	0x85, 0x13, 0x55, 0xc5, 0x19, 0x82, 0x37, 0x1b,
	0x0a, 0x02, 0x1f, 0x99, 0x6b, 0x18, 0xf1, 0x28,
	0x08, 0xa2, 0x73, 0xb8, 0x0f, 0x2e, 0xcd, 0xbf,
	0xf3, 0x86, 0x7f, 0xea, 0xef, 0xd0, 0xbb, 0xa6,
	0x21, 0xdf, 0x49, 0x73, 0x51, 0xcc, 0x36, 0xd3,
	0x3e, 0xa0, 0xf8, 0x44, 0xdf, 0xd3, 0xa6, 0xbe,
	0x8a, 0xd4, 0x57, 0xdd, 0x72, 0x94, 0x61, 0x0f,
	0x82, 0xd1, 0x07, 0xb8, 0x7c, 0x18, 0x83, 0xdf,
	0x3a, 0xe5, 0x50, 0x6a, 0x82, 0x20, 0xac, 0xa9,
	0xa8, 0xff, 0xd9, 0xf3, 0x77, 0x33, 0x5a, 0x9e,
	0x7f, 0x6d, 0xfe, 0x5d, 0x33, 0x41, 0x42, 0xe7,
	0x6c, 0x19, 0xe0, 0x44, 0x8a, 0x15, 0xf6, 0x70,
	0x98, 0xb7, 0x68, 0x4d, 0xfa, 0x97, 0x39, 0xb0,
	0x8e, 0xe8, 0x84, 0x8b, 0x75, 0x30, 0xb7, 0x7d,
	0x92, 0x69, 0x20, 0x9c, 0x81, 0xfb, 0x4b, 0xf4,
	0x01, 0x50, 0xeb, 0xce, 0x0c, 0x1c, 0x6c, 0xb5,
	0x4a, 0xd7, 0x27, 0x0c, 0xce, 0xbb, 0xe5, 0x85,
	0xf0, 0xb6, 0xee, 0xd5, 0x70, 0xdd, 0x3b, 0xfc,
	0xd4, 0x99, 0xf1, 0x33, 0xdd, 0x8b, 0xc4, 0x2f,
	0xae, 0xab, 0x74, 0x96, 0x32, 0xc7, 0x4c, 0x56,
	0x3c, 0x89, 0x0f, 0x96, 0x0b, 0x42, 0xc0, 0xcb,
	0xee, 0x0f, 0x0b, 0x8c, 0xfb, 0x7e, 0x47, 0x7b,
	0x64, 0x48, 0xfd, 0xb2, 0x00, 0x80, 0x89, 0xa5,
	0x13, 0x55, 0x62, 0xfc, 0x8f, 0xe2, 0x42, 0x03,
	0xb7, 0x4e, 0x2a, 0x79, 0xb4, 0x82, 0xea, 0x23,
	0x49, 0xda, 0xaf, 0x52, 0x63, 0x1e, 0x60, 0x03,
	0x89, 0x06, 0x44, 0x46, 0x08, 0xc3, 0xc4, 0x87,
	0x70, 0x2e, 0xda, 0x94, 0xad, 0x6b, 0xe0, 0xe4,
	0xd1, 0x8a, 0x06, 0xc2, 0xa8, 0xc0, 0xa7, 0x43,
	0x3c, 0x47, 0x52, 0x0e, 0xc3, 0x77, 0x81, 0x11,
	0x67, 0x0e, 0xa0, 0x70, 0x04, 0x47, 0x29, 0x40,
	0x86, 0x0d, 0x34, 0x56, 0xa7, 0xc9, 0x35, 0x59,
	0x68, 0xdc, 0x93, 0x81, 0x70, 0xee, 0x86, 0xd9,
	0x80, 0x06, 0x40, 0x4f, 0x1a, 0x0d, 0x40, 0x30,
	0x0b, 0xcb, 0x96, 0x47, 0xc1, 0xb7, 0x52, 0xfd,
	0x56, 0xe0, 0x72, 0x4b, 0xfb, 0xbd, 0x92, 0x45,
	0x61, 0x71, 0xc2, 0x33, 0x11, 0xbf, 0x52, 0x83,
	0x79, 0x26, 0xe0, 0x49, 0x6b, 0xb7, 0x05, 0x8b,
	0xe8, 0x0e, 0x87, 0x31, 0xd7, 0x9d, 0x8a, 0xf5,
	0xc0, 0x5f, 0x2e, 0x58, 0x4a, 0xdb, 0x11, 0xb3,
	0x6c, 0x30, 0x2a, 0x46, 0x19, 0xe3, 0x27, 0x84,
	0x1f, 0x63, 0x6e, 0xf6, 0x57, 0xc7, 0xc9, 0xd8,
	0x5e, 0xba, 0xb3, 0x87, 0xd5, 0x83, 0x26, 0x34,
	0x21, 0x9e, 0x65, 0xde, 0x42, 0xd3, 0xbe, 0x7b,
	0xbc, 0x91, 0x71, 0x44, 0x4d, 0x99, 0x3b, 0x31,
	0xe5, 0x3f, 0x11, 0x4e, 0x7f, 0x13, 0x51, 0x3b,
	0xae, 0x79, 0xc9, 0xd3, 0x81, 0x8e, 0x25, 0x40,
	0x10, 0xfc, 0x07, 0x1e, 0xf9, 0x7b, 0x9a, 0x4b,
	0x6c, 0xe3, 0xb3, 0xad, 0x1a, 0x0a, 0xdd, 0x9e,
	0x59, 0x0c, 0xa2, 0xcd, 0xae, 0x48, 0x4a, 0x38,
	0x5b, 0x47, 0x41, 0x94, 0x65, 0x6b, 0xbb, 0xeb,
	0x5b, 0xe3, 0xaf, 0x07, 0x5b, 0xd4, 0x4a, 0xa2,
	0xc9, 0x5d, 0x2f, 0x64, 0x03, 0xd7, 0x3a, 0x2c,
	0x6e, 0xce, 0x76, 0x95, 0xb4, 0xb3, 0xc0, 0xf1,
	0xe2, 0x45, 0x73, 0x7a, 0x5c, 0xab, 0xc1, 0xfc,
	0x02, 0x8d, 0x81, 0x29, 0xb3, 0xac, 0x07, 0xec,
	0x40, 0x7d, 0x45, 0xd9, 0x7a, 0x59, 0xee, 0x34,
	0xf0, 0xe9, 0xd5, 0x7b, 0x96, 0xb1, 0x3d, 0x95,
	0xcc, 0x86, 0xb5, 0xb6, 0x04, 0x2d, 0xb5, 0x92,
	0x7e, 0x76, 0xf4, 0x06, 0xa9, 0xa3, 0x12, 0x0f,
	0xb1, 0xaf, 0x26, 0xba, 0x7c, 0xfc, 0x7e, 0x1c,
	0xbc, 0x2c, 0x49, 0x97, 0x53, 0x60, 0x13, 0x0b,
	0xa6, 0x61, 0x83, 0x89, 0x42, 0xd4, 0x17, 0x0c,
	0x6c, 0x26, 0x52, 0xc3, 0xb3, 0xd4, 0x67, 0xf5,
	0xe3, 0x04, 0xb7, 0xf4, 0xcb, 0x80, 0xb8, 0xcb,
	0x77, 0x56, 0x3e, 0xaa, 0x57, 0x54, 0xee, 0xb4,
	0x2c, 0x67, 0xcf, 0xf2, 0xdc, 0xbe, 0x55, 0xf9,
	0x43, 0x1f, 0x6e, 0x22, 0x97, 0x67, 0x7f, 0xc4,
	0xef, 0xb1, 0x26, 0x31, 0x1e, 0x27, 0xdf, 0x41,
	0x80, 0x47, 0x6c, 0xe2, 0xfa, 0xa9, 0x8c, 0x2a,
	0xf6, 0xf2, 0xab, 0xf0, 0x15, 0xda, 0x6c, 0xc8,
	0xfe, 0xb5, 0x23, 0xde, 0xa9, 0x05, 0x3f, 0x06,
	0x54, 0x4c, 0xcd, 0xe1, 0xab, 0xfc, 0x0e, 0x62,
	0x33, 0x31, 0x73, 0x2c, 0x76, 0xcb, 0xb4, 0x47,
	0x1e, 0x20, 0xad, 0xd8, 0xf2, 0x31, 0xdd, 0xc4,
	0x8b, 0x0c, 0x77, 0xbe, 0xe1, 0x8b, 0x26, 0x00,
	0x02, 0x58, 0xd6, 0x8d, 0xef, 0xad, 0x74, 0x67,
	0xab, 0x3f, 0xef, 0xcb, 0x6f, 0xb0, 0xcc, 0x81,
	0x44, 0x4c, 0xaf, 0xe9, 0x49, 0x4f, 0xdb, 0xa0,
	0x25, 0xa4, 0xf0, 0x89, 0xf1, 0xbe, 0xd8, 0x10,
	0xff, 0xb1, 0x3b, 0x4b, 0xfa, 0x98, 0xf5, 0x79,
	0x6d, 0x1e, 0x69, 0x4d, 0x57, 0xb1, 0xc8, 0x19,
	0x1b, 0xbd, 0x1e, 0x8c, 0x84, 0xb7, 0x7b, 0xe8,
	0xd2, 0x2d, 0x09, 0x41, 0x41, 0x37, 0x3d, 0xb1,
	0x6f, 0x26, 0x5d, 0x71, 0x16, 0x3d, 0xb7, 0x83,
	0x27, 0x2c, 0xa7, 0xb6, 0x50, 0xbd, 0x91, 0x86,
	0xab, 0x24, 0xa1, 0x38, 0xfd, 0xea, 0x71, 0x55,
	0x7e, 0x9a, 0x07, 0x77, 0x4b, 0xfa, 0x61, 0x66,
	0x20, 0x1e, 0x28, 0x95, 0x18, 0x1b, 0xa4, 0xa0,
	0xfd, 0xc0, 0x89, 0x72, 0x43, 0xd9, 0x3b, 0x49,
	0x5a, 0x3f, 0x9d, 0xbf, 0xdb, 0xb4, 0x46, 0xea,
	0x42, 0x01, 0x77, 0x23, 0x68, 0x95, 0xb6, 0x24,
	0xb3, 0xa8, 0x6c, 0x28, 0x3b, 0x11, 0x40, 0x7e,
	0x18, 0x65, 0x6d, 0xd8, 0x24, 0x42, 0x7d, 0x88,
	0xc0, 0x52, 0xd9, 0x05, 0xe4, 0x95, 0x90, 0x87,
	0x8c, 0xf4, 0xd0, 0x6b, 0xb9, 0x83, 0x99, 0x34,
	0x6d, 0xfe, 0x54, 0x40, 0x94, 0x52, 0x21, 0x4f,
	0x14, 0x25, 0xc5, 0xd6, 0x5e, 0x95, 0xdc, 0x0a,
	0x2b, 0x89, 0x20, 0x11, 0x84, 0x48, 0xd6, 0x3a,
	0xcd, 0x5c, 0x24, 0xad, 0x62, 0xe3, 0xb1, 0x93,
	0x25, 0x8d, 0xcd, 0x7e, 0xfc, 0x27, 0xa3, 0x37,
	0xfd, 0x84, 0xfc, 0x1b, 0xb2, 0xf1, 0x27, 0x38,
	0x5a, 0xb7, 0xfc, 0xf2, 0xfa, 0x95, 0x66, 0xd4,
	0xfb, 0xba, 0xa7, 0xd7, 0xa3, 0x72, 0x69, 0x48,
	0x48, 0x8c, 0xeb, 0x28, 0x89, 0xfe, 0x33, 0x65,
	0x5a, 0x36, 0x01, 0x7e, 0x06, 0x79, 0x0a, 0x09,
	0x3b, 0x74, 0x11, 0x9a, 0x6e, 0xbf, 0xd4, 0x9e,
	0x58, 0x90, 0x49, 0x4f, 0x4d, 0x08, 0xd4, 0xe5,
	0x4a, 0x09, 0x21, 0xef, 0x8b, 0xb8, 0x74, 0x3b,
	0x91, 0xdd, 0x36, 0x85, 0x60, 0x2d, 0xfa, 0xd4,
	0x45, 0x7b, 0x45, 0x53, 0xf5, 0x47, 0x87, 0x7e,
	0xa6, 0x37, 0xc8, 0x78, 0x7a, 0x68, 0x9d, 0x8d,
	0x65, 0x2c, 0x0e, 0x91, 0x5c, 0xa2, 0x60, 0xf0,
	0x8e, 0x3f, 0xe9, 0x1a, 0xcd, 0xaa, 0xe7, 0xd5,
	0x77, 0x18, 0xaf, 0xc9, 0xbc, 0x18, 0xea, 0x48,
	0x1b, 0xfb, 0x22, 0x48, 0x70, 0x16, 0x29, 0x9e,
	0x5b, 0xc1, 0x2c, 0x66, 0x23, 0xbc, 0xf0, 0x1f,
	0xef, 0xaf, 0xe4, 0xd6, 0x04, 0x19, 0x82, 0x7a,
	0x0b, 0xba, 0x4b, 0x46, 0xb1, 0x6a, 0x85, 0x5d,
	0xb4, 0x73, 0xd6, 0x21, 0xa1, 0x71, 0x60, 0x14,
	0xee, 0x0a, 0x77, 0xc4, 0x66, 0x2e, 0xf9, 0x69,
	0x30, 0xaf, 0x41, 0x0b, 0xc8, 0x83, 0x3c, 0x53,
	0x99, 0x19, 0x27, 0x46, 0xf7, 0x41, 0x6e, 0x56,
	0xdc, 0x94, 0x28, 0x67, 0x4e, 0xb7, 0x25, 0x48,
	0x8a, 0xc2, 0xe0, 0x60, 0x96, 0xcc, 0x18, 0xf4,
	0x84, 0xdd, 0xa7, 0x5e, 0x3e, 0x05, 0x0b, 0x26,
	0x26, 0xb2, 0x5c, 0x1f, 0x57, 0x1a, 0x04, 0x7e,
	0x6a, 0xe3, 0x2f, 0xb4, 0x35, 0xb6, 0x38, 0x40,
	0x40, 0xcd, 0x6f, 0x87, 0x2e, 0xef, 0xa3, 0xd7,
	0xa9, 0xc2, 0xe8, 0x0d, 0x27, 0xdf, 0x44, 0x62,
	0x99, 0xa0, 0xfc, 0xcf, 0x81, 0x78, 0xcb, 0xfe,
	0xe5, 0xa0, 0x03, 0x4e, 0x6c, 0xd7, 0xf4, 0xaf,
	0x7a, 0xbb, 0x61, 0x82, 0xfe, 0x71, 0x89, 0xb2,
	0x22, 0x7c, 0x8e, 0x83, 0x04, 0xce, 0xf6, 0x5d,
	0x84, 0x8f, 0x95, 0x6a, 0x7f, 0xad, 0xfd, 0x32,
	0x9c, 0x5e, 0xe4, 0x9c, 0x89, 0x60, 0x54, 0xaa,
	0x96, 0x72, 0xd2, 0xd7, 0x36, 0x85, 0xa9, 0x45,
	0xd2, 0x2a, 0xa1, 0x81, 0x49, 0x6f, 0x7e, 0x04,
	0xfa, 0xe2, 0xfe, 0x90, 0x26, 0x77, 0x5a, 0x33,
	0xb8, 0x04, 0x9a, 0x7a, 0xe6, 0x4c, 0x4f, 0xad,
	0x72, 0x96, 0x08, 0x28, 0x58, 0x13, 0xf8, 0xc4,
	0x1c, 0xf0, 0xc3, 0x45, 0x95, 0x49, 0x20, 0x8c,
	0x9f, 0x39, 0x70, 0xe1, 0x77, 0xfe, 0xd5, 0x4b,
	0xaf, 0x86, 0xda, 0xef, 0x22, 0x06, 0x83, 0x36,
	0x29, 0x12, 0x11, 0x40, 0xbc, 0x3b, 0x86, 0xaa,
	0xaa, 0x65, 0x60, 0xc3, 0x80, 0xca, 0xed, 0xa9,
	0xf3, 0xb0, 0x79, 0x96, 0xa2, 0x55, 0x27, 0x28,
	0x55, 0x73, 0x26, 0xa5, 0x50, 0xea, 0x92, 0x4b,
	0x3c, 0x5c, 0x82, 0x33, 0xf0, 0x01, 0x3f, 0x03,
	0xc1, 0x08, 0x05, 0xbf, 0x98, 0xf4, 0x9b, 0x6d,
	0xa5, 0xa8, 0xb4, 0x82, 0x0c, 0x06, 0xfa, 0xff,
	0x2d, 0x08, 0xf3, 0x05, 0x4f, 0x57, 0x2a, 0x39,
	0xd4, 0x83, 0x0d, 0x75, 0x51, 0xd8, 0x5b, 0x1b,
	0xd3, 0x51, 0x5a, 0x32, 0x2a, 0x9b, 0x32, 0xb2,
	0xf2, 0xa4, 0x96, 0x12, 0xf2, 0xae, 0x40, 0x34,
	0x67, 0xa8, 0xf5, 0x44, 0xd5, 0x35, 0x53, 0xfe,
	0xa3, 0x60, 0x96, 0x63, 0x0f, 0x1f, 0x6e, 0xb0,
	0x5a, 0x42, 0xa6, 0xfc, 0x51, 0x0b, 0x60, 0x27,
	0xbc, 0x06, 0x71, 0xed, 0x65, 0x5b, 0x23, 0x86,
	0x4a, 0x07, 0x3b, 0x22, 0x07, 0x46, 0xe6, 0x90,
	0x3e, 0xf3, 0x25, 0x50, 0x1b, 0x4c, 0x7f, 0x03,
	0x08, 0xa8, 0x36, 0x6b, 0x87, 0xe5, 0xe3, 0xdb,
	0x9a, 0x38, 0x83, 0xff, 0x9f, 0x1a, 0x9f, 0x57,
	0xa4, 0x2a, 0xf6, 0x37, 0xbc, 0x1a, 0xff, 0xc9,
	0x1e, 0x35, 0x0c, 0xc3, 0x7c, 0xa3, 0xb2, 0xe5,
	0xd2, 0xc6, 0xb4, 0x57, 0x47, 0xe4, 0x32, 0x16,
	0x6d, 0xa9, 0xae, 0x64, 0xe6, 0x2d, 0x8d, 0xc5,
	0x8d, 0x50, 0x8e, 0xe8, 0x1a, 0x22, 0x34, 0x2a,
	0xd9, 0xeb, 0x51, 0x90, 0x4a, 0xb1, 0x41, 0x7d,
	0x64, 0xf9, 0xb9, 0x0d, 0xf6, 0x23, 0x33, 0xb0,
	0x33, 0xf4, 0xf7, 0x3f, 0x27, 0x84, 0xc6, 0x0f,
	0x54, 0xa5, 0xc0, 0x2e, 0xec, 0x0b, 0x3a, 0x48,
	0x6e, 0x80, 0x35, 0x81, 0x43, 0x9b, 0x90, 0xb1,
	0xd0, 0x2b, 0xea, 0x21, 0xdc, 0xda, 0x5b, 0x09,
	0xf4, 0xcc, 0x10, 0xb4, 0xc7, 0xfe, 0x79, 0x51,
	0xc3, 0xc5, 0xac, 0x88, 0x74, 0x84, 0x0b, 0x4b,
	0xca, 0x79, 0x16, 0x29, 0xfb, 0x69, 0x54, 0xdf,
	0x41, 0x7e, 0xe9, 0xc7, 0x8e, 0xea, 0xa5, 0xfe,
	0xfc, 0x76, 0x0e, 0x90, 0xc4, 0x92, 0x38, 0xad,
	0x7b, 0x48, 0xe6, 0x6e, 0xf7, 0x21, 0xfd, 0x4e,
	0x93, 0x0a, 0x7b, 0x41, 0x83, 0x68, 0xfb, 0x57,
	0x51, 0x76, 0x34, 0xa9, 0x6c, 0x00, 0xaa, 0x4f,
	0x66, 0x65, 0x98, 0x4a, 0x4f, 0xa3, 0xa0, 0xef,
	0x69, 0x3f, 0xe3, 0x1c, 0x92, 0x8c, 0xfd, 0xd8,
	0xe8, 0xde, 0x7c, 0x7f, 0x3e, 0x84, 0x8e, 0x69,
	0x3c, 0xf1, 0xf2, 0x05, 0x46, 0xdc, 0x2f, 0x9d,
	0x5e, 0x6e, 0x4c, 0xfb, 0xb5, 0x99, 0x2a, 0x59,
	0x63, 0xc1, 0x34, 0xbc, 0x57, 0xc0, 0x0d, 0xb9,
	0x61, 0x25, 0xf3, 0x33, 0x23, 0x51, 0xb6, 0x0d,
	0x07, 0xa6, 0xab, 0x94, 0x4a, 0xb7, 0x2a, 0xea,
	0xee, 0xac, 0xa3, 0xc3, 0x04, 0x8b, 0x0e, 0x56,
	0xfe, 0x44, 0xa7, 0x39, 0xe2, 0xed, 0xed, 0xb4,
	0x22, 0x2b, 0xac, 0x12, 0x32, 0x28, 0x91, 0xd8,
	0xa5, 0xab, 0xff, 0x5f, 0xe0, 0x4b, 0xda, 0x78,
	0x17, 0xda, 0xf1, 0x01, 0x5b, 0xcd, 0xe2, 0x5f,
	0x50, 0x45, 0x73, 0x2b, 0xe4, 0x76, 0x77, 0xf4,
	0x64, 0x1d, 0x43, 0xfb, 0x84, 0x7a, 0xea, 0x91,
	0xae, 0xf9, 0x9e, 0xb7, 0xb4, 0xb0, 0x91, 0x5f,
	0x16, 0x35, 0x9a, 0x11, 0xb8, 0xc7, 0xc1, 0x8c,
	0xc6, 0x10, 0x8d, 0x2f, 0x63, 0x4a, 0xa7, 0x57,
	0x3a, 0x51, 0xd6, 0x32, 0x2d, 0x64, 0x72, 0xd4,
	0x66, 0xdc, 0x10, 0xa6, 0x67, 0xd6, 0x04, 0x23,
	0x9d, 0x0a, 0x11, 0x77, 0xdd, 0x37, 0x94, 0x17,
	0x3c, 0xbf, 0x8b, 0x65, 0xb0, 0x2e, 0x5e, 0x66,
	0x47, 0x64, 0xac, 0xdd, 0xf0, 0x84, 0xfd, 0x39,
	0xfa, 0x15, 0x5d, 0xef, 0xae, 0xca, 0xc1, 0x36,
	0xa7, 0x5c, 0xbf, 0xc7, 0x08, 0xc2, 0x66, 0x00,
	0x74, 0x74, 0x4e, 0x27, 0x3f, 0x55, 0x8a, 0xb7,
	0x38, 0x66, 0x83, 0x6d, 0xcf, 0x99, 0x9e, 0x60,
	0x8f, 0xdd, 0x2e, 0x62, 0x22, 0x0e, 0xef, 0x0c,
	0x98, 0xa7, 0x85, 0x74, 0x3b, 0x9d, 0xec, 0x9e,
	0xa9, 0x19, 0x72, 0xa5, 0x7f, 0x2c, 0x39, 0xb7,
	0x7d, 0xb7, 0xf1, 0x12, 0x65, 0x27, 0x4b, 0x5a,
	0xde, 0x17, 0xfe, 0xad, 0x44, 0xf3, 0x20, 0x4d,
	0xfd, 0xe4, 0x1f, 0xb5, 0x81, 0xb0, 0x36, 0x37,
	0x08, 0x6f, 0xc3, 0x0c, 0xe9, 0x85, 0x98, 0x82,
	0xa9, 0x62, 0x0c, 0xc4, 0x97, 0xc0, 0x50, 0xc8,
	0xa7, 0x3c, 0x50, 0x9f, 0x43, 0xb9, 0xcd, 0x5e,
	0x4d, 0xfa, 0x1c, 0x4b, 0x0b, 0xa9, 0x98, 0x85,
	0x38, 0x92, 0xac, 0x8d, 0xe4, 0xad, 0x9b, 0x98,
	0xab, 0xd9, 0x38, 0xac, 0x62, 0x52, 0xa3, 0x22,
	0x63, 0x0f, 0xbf, 0x95, 0x48, 0xdf, 0x69, 0xe7,
	0x8b, 0x33, 0xd5, 0xb2, 0xbd, 0x05, 0x49, 0x49,
	0x9d, 0x57, 0x73, 0x19, 0x33, 0xae, 0xfa, 0x33,
	0xf1, 0x19, 0xa8, 0x80, 0xce, 0x04, 0x9f, 0xbc,
	0x1d, 0x65, 0x82, 0x1b, 0xe5, 0x3a, 0x51, 0xc8,
	0x1c, 0x21, 0xe3, 0x5d, 0xf3, 0x7d, 0x9b, 0x2f,
	0x2c, 0x1d, 0x4a, 0x7f, 0x9b, 0x68, 0x35, 0xa3,
	0xb2, 0x50, 0xf7, 0x62, 0x79, 0xcd, 0xf4, 0x98,
	0x4f, 0xe5, 0x63, 0x7c, 0x3e, 0x45, 0x31, 0x8c,
	0x16, 0xa0, 0x12, 0xc8, 0x58, 0xce, 0x39, 0xa6,
	0xbc, 0x54, 0xdb, 0xc5, 0xe0, 0xd5, 0xba, 0xbc,
	0xb9, 0x04, 0xf4, 0x8d, 0xe8, 0x2f, 0x15, 0x9d,
};

/* 100 test cases */
static struct crc_test {
	u32 crc;	/* random starting crc */
	u32 start;	/* random 6 bit offset in buf */
	u32 length;	/* random 11 bit length of test */
	u32 crc_le;	/* expected crc32_le result */
	u32 crc_be;	/* expected crc32_be result */
} test[] =
{
	{0x674bf11d, 0x00000038, 0x00000542, 0x0af6d466, 0xd8b6e4c1},
	{0x35c672c6, 0x0000003a, 0x000001aa, 0xc6d3dfba, 0x28aaf3ad},
	{0x496da28e, 0x00000039, 0x000005af, 0xd933660f, 0x5d57e81f},
	{0x09a9b90e, 0x00000027, 0x000001f8, 0xb45fe007, 0xf45fca9a},
	{0xdc97e5a9, 0x00000025, 0x000003b6, 0xf81a3562, 0xe0126ba2},
	{0x47c58900, 0x0000000a, 0x000000b9, 0x8e58eccf, 0xf3afc793},
	{0x292561e8, 0x0000000c, 0x00000403, 0xa2ba8aaf, 0x0b797aed},
	{0x415037f6, 0x00000003, 0x00000676, 0xa17d52e8, 0x7f0fdf35},
	{0x3466e707, 0x00000026, 0x00000042, 0x258319be, 0x75c484a2},
	{0xafd1281b, 0x00000023, 0x000002ee, 0x4428eaf8, 0x06c7ad10},
	{0xd3857b18, 0x00000028, 0x000004a2, 0x5c430821, 0xb062b7cb},
	{0x1d825a8f, 0x0000002b, 0x0000050b, 0xd2c45f0c, 0xd68634e0},
	{0x5033e3bc, 0x0000000b, 0x00000078, 0xa3ea4113, 0xac6d31fb},
	{0x94f1fb5e, 0x0000000f, 0x000003a2, 0xfbfc50b1, 0x3cfe50ed},
	{0xc9a0fe14, 0x00000009, 0x00000473, 0x5fb61894, 0x87070591},
	{0x88a034b1, 0x0000001c, 0x000005ad, 0xc1b16053, 0x46f95c67},
	{0xf0f72239, 0x00000020, 0x0000026d, 0xa6fa58f3, 0xf8c2c1dd},
	{0xcc20a5e3, 0x0000003b, 0x0000067a, 0x7740185a, 0x308b979a},
	{0xce589c95, 0x0000002b, 0x00000641, 0xd055e987, 0x40aae25b},
	{0x78edc885, 0x00000035, 0x000005be, 0xa39cb14b, 0x035b0d1f},
	{0x9d40a377, 0x0000003b, 0x00000038, 0x1f47ccd2, 0x197fbc9d},
	{0x703d0e01, 0x0000003c, 0x000006f1, 0x88735e7c, 0xfed57c5a},
	{0x776bf505, 0x0000000f, 0x000005b2, 0x5cc4fc01, 0xf32efb97},
	{0x4a3e7854, 0x00000027, 0x000004b8, 0x8d923c82, 0x0cbfb4a2},
	{0x209172dd, 0x0000003b, 0x00000356, 0xb89e9c2b, 0xd7868138},
	{0x3ba4cc5b, 0x0000002f, 0x00000203, 0xe51601a9, 0x5b2a1032},
	{0xfc62f297, 0x00000000, 0x00000079, 0x71a8e1a2, 0x5d88685f},
	{0x64280b8b, 0x00000016, 0x000007ab, 0x0fa7a30c, 0xda3a455f},
	{0x97dd724b, 0x00000033, 0x000007ad, 0x5788b2f4, 0xd7326d32},
	{0x61394b52, 0x00000035, 0x00000571, 0xc66525f1, 0xcabe7fef},
	{0x29b4faff, 0x00000024, 0x0000006e, 0xca13751e, 0x993648e0},
	{0x29bfb1dc, 0x0000000b, 0x00000244, 0x436c43f7, 0x429f7a59},
	{0x86ae934b, 0x00000035, 0x00000104, 0x0760ec93, 0x9cf7d0f4},
	{0xc4c1024e, 0x0000002e, 0x000006b1, 0x6516a3ec, 0x19321f9c},
	{0x3287a80a, 0x00000026, 0x00000496, 0x0b257eb1, 0x754ebd51},
	{0xa4db423e, 0x00000023, 0x0000045d, 0x9b3a66dc, 0x873e9f11},
	{0x7a1078df, 0x00000015, 0x0000014a, 0x8c2484c5, 0x6a628659},
	{0x6048bd5b, 0x00000006, 0x0000006a, 0x897e3559, 0xac9961af},
	{0xd8f9ea20, 0x0000003d, 0x00000277, 0x60eb905b, 0xed2aaf99},
	{0xea5ec3b4, 0x0000002a, 0x000004fe, 0x869965dc, 0x6c1f833b},
	{0x2dfb005d, 0x00000016, 0x00000345, 0x6a3b117e, 0xf05e8521},
	{0x5a214ade, 0x00000020, 0x000005b6, 0x467f70be, 0xcb22ccd3},
	{0xf0ab9cca, 0x00000032, 0x00000515, 0xed223df3, 0x7f3ef01d},
	{0x91b444f9, 0x0000002e, 0x000007f8, 0x84e9a983, 0x5676756f},
	{0x1b5d2ddb, 0x0000002e, 0x0000012c, 0xba638c4c, 0x3f42047b},
	{0xd824d1bb, 0x0000003a, 0x000007b5, 0x6288653b, 0x3a3ebea0},
	{0x0470180c, 0x00000034, 0x000001f0, 0x9d5b80d6, 0x3de08195},
	{0xffaa3a3f, 0x00000036, 0x00000299, 0xf3a82ab8, 0x53e0c13d},
	{0x6406cfeb, 0x00000023, 0x00000600, 0xa920b8e8, 0xe4e2acf4},
	{0xb24aaa38, 0x0000003e, 0x000004a1, 0x657cc328, 0x5077b2c3},
	{0x58b2ab7c, 0x00000039, 0x000002b4, 0x3a17ee7e, 0x9dcb3643},
	{0x3db85970, 0x00000006, 0x000002b6, 0x95268b59, 0xb9812c10},
	{0x857830c5, 0x00000003, 0x00000590, 0x4ef439d5, 0xf042161d},
	{0xe1fcd978, 0x0000003e, 0x000007d8, 0xae8d8699, 0xce0a1ef5},
	{0xb982a768, 0x00000016, 0x000006e0, 0x62fad3df, 0x5f8a067b},
	{0x1d581ce8, 0x0000001e, 0x0000058b, 0xf0f5da53, 0x26e39eee},
	{0x2456719b, 0x00000025, 0x00000503, 0x4296ac64, 0xd50e4c14},
	{0xfae6d8f2, 0x00000000, 0x0000055d, 0x057fdf2e, 0x2a31391a},
	{0xcba828e3, 0x00000039, 0x000002ce, 0xe3f22351, 0x8f00877b},
	{0x13d25952, 0x0000000a, 0x0000072d, 0x76d4b4cc, 0x5eb67ec3},
	{0x0342be3f, 0x00000015, 0x00000599, 0xec75d9f1, 0x9d4d2826},
	{0xeaa344e0, 0x00000014, 0x000004d8, 0x72a4c981, 0x2064ea06},
	{0xbbb52021, 0x0000003b, 0x00000272, 0x04af99fc, 0xaf042d35},
	{0xb66384dc, 0x0000001d, 0x000007fc, 0xd7629116, 0x782bd801},
	{0x616c01b6, 0x00000022, 0x000002c8, 0x5b1dab30, 0x783ce7d2},
	{0xce2bdaad, 0x00000016, 0x0000062a, 0x932535c8, 0x3f02926d},
	{0x00fe84d7, 0x00000005, 0x00000205, 0x850e50aa, 0x753d649c},
	{0xbebdcb4c, 0x00000006, 0x0000055d, 0xbeaa37a2, 0x2d8c9eba},
	{0xd8b1a02a, 0x00000010, 0x00000387, 0x5017d2fc, 0x503541a5},
	{0x3b96cad2, 0x00000036, 0x00000347, 0x1d2372ae, 0x926cd90b},
	{0xc94c1ed7, 0x00000005, 0x0000038b, 0x9e9fdb22, 0x144a9178},
	{0x1aad454e, 0x00000025, 0x000002b2, 0xc3f6315c, 0x5c7a35b3},
	{0xa4fec9a6, 0x00000000, 0x000006d6, 0x90be5080, 0xa4107605},
	{0x1bbe71e2, 0x0000001f, 0x000002fd, 0x4e504c3b, 0x284ccaf1},
	{0x4201c7e4, 0x00000002, 0x000002b7, 0x7822e3f9, 0x0cc912a9},
	{0x23fddc96, 0x00000003, 0x00000627, 0x8a385125, 0x07767e78},
	{0xd82ba25c, 0x00000016, 0x0000063e, 0x98e4148a, 0x283330c9},
	{0x786f2032, 0x0000002d, 0x0000060f, 0xf201600a, 0xf561bfcd},
	{0xfebe4e1f, 0x0000002a, 0x000004f2, 0x95e51961, 0xfd80dcab},
	{0x1a6e0a39, 0x00000008, 0x00000672, 0x8af6c2a5, 0x78dd84cb},
	{0x56000ab8, 0x0000000e, 0x000000e5, 0x36bacb8f, 0x22ee1f77},
	{0x4717fe0c, 0x00000000, 0x000006ec, 0x8439f342, 0x5c8e03da},
	{0xd5d5d68e, 0x0000003c, 0x000003a3, 0x46fff083, 0x177d1b39},
	{0xc25dd6c6, 0x00000024, 0x000006c0, 0x5ceb8eb4, 0x892b0d16},
	{0xe9b11300, 0x00000023, 0x00000683, 0x07a5d59a, 0x6c6a3208},
	{0x95cd285e, 0x00000001, 0x00000047, 0x7b3a4368, 0x0202c07e},
	{0xd9245a25, 0x0000001e, 0x000003a6, 0xd33c1841, 0x1936c0d5},
	{0x103279db, 0x00000006, 0x0000039b, 0xca09b8a0, 0x77d62892},
	{0x1cba3172, 0x00000027, 0x000001c8, 0xcb377194, 0xebe682db},
	{0x8f613739, 0x0000000c, 0x000001df, 0xb4b0bc87, 0x7710bd43},
	{0x1c6aa90d, 0x0000001b, 0x0000053c, 0x70559245, 0xda7894ac},
	{0xaabe5b93, 0x0000003d, 0x00000715, 0xcdbf42fa, 0x0c3b99e7},
	{0xf15dd038, 0x00000006, 0x000006db, 0x6e104aea, 0x8d5967f2},
	{0x584dd49c, 0x00000020, 0x000007bc, 0x36b6cfd6, 0xad4e23b2},
	{0x5d8c9506, 0x00000020, 0x00000470, 0x4c62378e, 0x31d92640},
	{0xb80d17b0, 0x00000032, 0x00000346, 0x22a5bb88, 0x9a7ec89f},
	{0xdaf0592e, 0x00000023, 0x000007b0, 0x3cab3f99, 0x9b1fdd99},
	{0x4793cc85, 0x0000000d, 0x00000706, 0xe82e04f6, 0xed3db6b7},
	{0x82ebf64e, 0x00000009, 0x000007c3, 0x69d590a9, 0x9efa8499},
	{0xb18a0319, 0x00000026, 0x000007db, 0x1cf98dcc, 0x8fa9ad6a},
};

#include <linux/time.h>

static int __init crc32_init(void)
{
	int i;
	int errors = 0;
	int bytes = 0;
	struct timespec start, stop;
	u64 nsec;
	unsigned long flags;

	/* keep static to prevent cache warming code from
	 * getting eliminated by the compiler */
	static u32 crc;

	/* pre-warm the cache */
	for (i = 0; i < 100; i++) {
		bytes += 2*test[i].length;

		crc ^= crc32_le(test[i].crc, test_buf +
		    test[i].start, test[i].length);

		crc ^= crc32_be(test[i].crc, test_buf +
		    test[i].start, test[i].length);
	}

	/* reduce OS noise */
	local_irq_save(flags);
	local_irq_disable();

	getnstimeofday(&start);
	for (i = 0; i < 100; i++) {
		if (test[i].crc_le != crc32_le(test[i].crc, test_buf +
		    test[i].start, test[i].length))
			errors++;

		if (test[i].crc_be != crc32_be(test[i].crc, test_buf +
		    test[i].start, test[i].length))
			errors++;
	}
	getnstimeofday(&stop);

	local_irq_restore(flags);
	local_irq_enable();

	nsec = stop.tv_nsec - start.tv_nsec +
		1000000000 * (stop.tv_sec - start.tv_sec);

	pr_info("crc32: CRC_LE_BITS = %d, CRC_BE BITS = %d\n",
		 CRC_LE_BITS, CRC_BE_BITS);

	if (errors)
		pr_warn("crc32: %d self tests failed\n", errors);
	else {
		pr_info("crc32: self tests passed, processed %d bytes in %lld nsec\n",
			bytes, nsec);
	}

	return 0;
}

static void __exit crc32_exit(void)
{
}

module_init(crc32_init);
module_exit(crc32_exit);
#endif /* CONFIG_CRC32_SELFTEST */

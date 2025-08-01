/* SPDX-License-Identifier: GPL-2.0-or-later */
/* This file was generated by: ./scripts/crypto/gen-hash-testvecs.py sha1 */

static const struct {
	size_t data_len;
	u8 digest[SHA1_DIGEST_SIZE];
} hash_testvecs[] = {
	{
		.data_len = 0,
		.digest = {
			0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d,
			0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90,
			0xaf, 0xd8, 0x07, 0x09,
		},
	},
	{
		.data_len = 1,
		.digest = {
			0x0a, 0xd0, 0x52, 0xdd, 0x9f, 0x32, 0x40, 0x55,
			0x21, 0xe4, 0x3c, 0x6e, 0xbd, 0xc5, 0x2f, 0x5a,
			0x02, 0x54, 0x93, 0xb2,
		},
	},
	{
		.data_len = 2,
		.digest = {
			0x13, 0x83, 0x82, 0x03, 0x23, 0xff, 0x46, 0xd6,
			0x12, 0x7f, 0xad, 0x05, 0x2b, 0xc3, 0x4a, 0x42,
			0x49, 0x6a, 0xf8, 0x84,
		},
	},
	{
		.data_len = 3,
		.digest = {
			0xe4, 0xdf, 0x7b, 0xdc, 0xe8, 0x6e, 0x81, 0x97,
			0x1e, 0x0f, 0xe8, 0x8b, 0x76, 0xa8, 0x59, 0x04,
			0xae, 0x92, 0x1a, 0x7c,
		},
	},
	{
		.data_len = 16,
		.digest = {
			0x8c, 0x1c, 0x30, 0xd8, 0xbc, 0xc4, 0xc3, 0xf5,
			0xf8, 0x83, 0x0d, 0x1e, 0x04, 0x5d, 0x29, 0xb5,
			0x68, 0x89, 0xc1, 0xe9,
		},
	},
	{
		.data_len = 32,
		.digest = {
			0x6c, 0x1d, 0x72, 0x31, 0xa5, 0x03, 0x4f, 0xdc,
			0xff, 0x2d, 0x06, 0x3e, 0x24, 0x26, 0x34, 0x8d,
			0x60, 0xa4, 0x67, 0x16,
		},
	},
	{
		.data_len = 48,
		.digest = {
			0x37, 0x53, 0x33, 0xfa, 0xd0, 0x21, 0xad, 0xe7,
			0xa5, 0x43, 0xf1, 0x94, 0x64, 0x11, 0x47, 0x9c,
			0x72, 0xb5, 0x78, 0xb4,
		},
	},
	{
		.data_len = 49,
		.digest = {
			0x51, 0x5c, 0xd8, 0x5a, 0xa9, 0xde, 0x7b, 0x2a,
			0xa2, 0xff, 0x70, 0x09, 0x56, 0x88, 0x40, 0x2b,
			0x50, 0x93, 0x82, 0x47,
		},
	},
	{
		.data_len = 63,
		.digest = {
			0xbc, 0x9c, 0xab, 0x93, 0x06, 0xd5, 0xdb, 0xac,
			0x2c, 0x33, 0x15, 0x83, 0x56, 0xf6, 0x91, 0x20,
			0x09, 0xc7, 0xb2, 0x6b,
		},
	},
	{
		.data_len = 64,
		.digest = {
			0x26, 0x90, 0x3b, 0x47, 0xe1, 0x92, 0x42, 0xd0,
			0x85, 0x63, 0x2e, 0x6b, 0x68, 0xa4, 0xc4, 0x4c,
			0xe6, 0xf4, 0xb0, 0x52,
		},
	},
	{
		.data_len = 65,
		.digest = {
			0x55, 0x6f, 0x87, 0xdc, 0x34, 0x3d, 0xe2, 0x4f,
			0xc3, 0x81, 0xa4, 0x82, 0x79, 0x84, 0x64, 0x01,
			0x55, 0xa0, 0x1e, 0x36,
		},
	},
	{
		.data_len = 127,
		.digest = {
			0xb7, 0xd5, 0x5f, 0xa4, 0xef, 0xbf, 0x4f, 0x96,
			0x01, 0xc1, 0x06, 0xe3, 0x75, 0xa8, 0x90, 0x92,
			0x4c, 0x5f, 0xf1, 0x21,
		},
	},
	{
		.data_len = 128,
		.digest = {
			0x70, 0x0c, 0xea, 0xa4, 0x93, 0xd0, 0x56, 0xf0,
			0x6f, 0xbb, 0x53, 0x42, 0x5b, 0xe3, 0xf2, 0xb0,
			0x30, 0x66, 0x8e, 0x75,
		},
	},
	{
		.data_len = 129,
		.digest = {
			0x15, 0x01, 0xbc, 0xb0, 0xee, 0xd8, 0xeb, 0xa8,
			0x7d, 0xd9, 0x4d, 0x50, 0x2e, 0x41, 0x30, 0xba,
			0x41, 0xaa, 0x7b, 0x02,
		},
	},
	{
		.data_len = 256,
		.digest = {
			0x98, 0x05, 0x52, 0xf5, 0x0f, 0xf0, 0xd3, 0x97,
			0x15, 0x8c, 0xa3, 0x9a, 0x2b, 0x4d, 0x67, 0x57,
			0x29, 0xa0, 0xac, 0x61,
		},
	},
	{
		.data_len = 511,
		.digest = {
			0x1f, 0x47, 0xf0, 0xcc, 0xd7, 0xda, 0xa5, 0x3b,
			0x39, 0xb4, 0x5b, 0xa8, 0x33, 0xd4, 0xca, 0x2f,
			0xdd, 0xf2, 0x39, 0x89,
		},
	},
	{
		.data_len = 513,
		.digest = {
			0xb9, 0x75, 0xe6, 0x57, 0x42, 0x7f, 0x8b, 0x0a,
			0xcc, 0x53, 0x10, 0x69, 0x45, 0xac, 0xfd, 0x11,
			0xf7, 0x1f, 0x4e, 0x6f,
		},
	},
	{
		.data_len = 1000,
		.digest = {
			0x63, 0x66, 0xcb, 0x44, 0xc1, 0x2c, 0xa2, 0x06,
			0x5d, 0xb9, 0x8e, 0x31, 0xcb, 0x4f, 0x4e, 0x49,
			0xe0, 0xfb, 0x3c, 0x4e,
		},
	},
	{
		.data_len = 3333,
		.digest = {
			0x35, 0xbc, 0x74, 0xfb, 0x31, 0x9c, 0xd4, 0xdd,
			0xe8, 0x87, 0xa7, 0x56, 0x3b, 0x08, 0xe5, 0x49,
			0xe1, 0xe9, 0xc9, 0xa8,
		},
	},
	{
		.data_len = 4096,
		.digest = {
			0x43, 0x00, 0xea, 0xcd, 0x4e, 0x7c, 0xe9, 0xe4,
			0x32, 0xce, 0x25, 0xa8, 0xcd, 0x20, 0xa8, 0xaa,
			0x7b, 0x63, 0x2c, 0x3c,
		},
	},
	{
		.data_len = 4128,
		.digest = {
			0xd0, 0x67, 0x26, 0x0e, 0x22, 0x72, 0xaa, 0x63,
			0xfc, 0x34, 0x55, 0x07, 0xab, 0xc8, 0x64, 0xb6,
			0xc4, 0xea, 0xd5, 0x7c,
		},
	},
	{
		.data_len = 4160,
		.digest = {
			0x6b, 0xc9, 0x5e, 0xb9, 0x41, 0x19, 0x50, 0x35,
			0xf1, 0x39, 0xfe, 0xd9, 0x72, 0x6d, 0xd0, 0x55,
			0xb8, 0x1f, 0x1a, 0x95,
		},
	},
	{
		.data_len = 4224,
		.digest = {
			0x70, 0x5d, 0x10, 0x2e, 0x4e, 0x44, 0xc9, 0x80,
			0x8f, 0xba, 0x13, 0xbc, 0xd0, 0x77, 0x78, 0xc7,
			0x84, 0xe3, 0x24, 0x43,
		},
	},
	{
		.data_len = 16384,
		.digest = {
			0xa8, 0x82, 0xca, 0x08, 0xb4, 0x84, 0x09, 0x13,
			0xc0, 0x9c, 0x26, 0x18, 0xcf, 0x0f, 0xf3, 0x08,
			0xff, 0xa1, 0xe4, 0x5d,
		},
	},
};

static const u8 hash_testvec_consolidated[SHA1_DIGEST_SIZE] = {
	0xe1, 0x72, 0xa5, 0x3c, 0xda, 0xf2, 0xe5, 0x56,
	0xb8, 0xb5, 0x35, 0x6e, 0xce, 0xc8, 0x37, 0x57,
	0x31, 0xb4, 0x05, 0xdd,
};

static const u8 hmac_testvec_consolidated[SHA1_DIGEST_SIZE] = {
	0x9d, 0xe5, 0xb1, 0x43, 0x97, 0x95, 0x16, 0x52,
	0xa0, 0x7a, 0xc0, 0xe2, 0xc1, 0x60, 0x64, 0x7c,
	0x24, 0xf9, 0x34, 0xd7,
};

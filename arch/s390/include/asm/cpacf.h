/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CP Assist for Cryptographic Functions (CPACF)
 *
 * Copyright IBM Corp. 2003, 2023
 * Author(s): Thomas Spatzier
 *	      Jan Glauber
 *	      Harald Freudenberger (freude@de.ibm.com)
 *	      Martin Schwidefsky <schwidefsky@de.ibm.com>
 */
#ifndef _ASM_S390_CPACF_H
#define _ASM_S390_CPACF_H

#include <asm/facility.h>
#include <linux/kmsan-checks.h>

/*
 * Instruction opcodes for the CPACF instructions
 */
#define CPACF_KMAC		0xb91e		/* MSA	*/
#define CPACF_KM		0xb92e		/* MSA	*/
#define CPACF_KMC		0xb92f		/* MSA	*/
#define CPACF_KIMD		0xb93e		/* MSA	*/
#define CPACF_KLMD		0xb93f		/* MSA	*/
#define CPACF_PCKMO		0xb928		/* MSA3 */
#define CPACF_KMF		0xb92a		/* MSA4 */
#define CPACF_KMO		0xb92b		/* MSA4 */
#define CPACF_PCC		0xb92c		/* MSA4 */
#define CPACF_KMCTR		0xb92d		/* MSA4 */
#define CPACF_PRNO		0xb93c		/* MSA5 */
#define CPACF_KMA		0xb929		/* MSA8 */
#define CPACF_KDSA		0xb93a		/* MSA9 */

/*
 * En/decryption modifier bits
 */
#define CPACF_ENCRYPT		0x00
#define CPACF_DECRYPT		0x80

/*
 * Function codes for the KM (CIPHER MESSAGE) instruction
 */
#define CPACF_KM_QUERY		0x00
#define CPACF_KM_DEA		0x01
#define CPACF_KM_TDEA_128	0x02
#define CPACF_KM_TDEA_192	0x03
#define CPACF_KM_AES_128	0x12
#define CPACF_KM_AES_192	0x13
#define CPACF_KM_AES_256	0x14
#define CPACF_KM_PAES_128	0x1a
#define CPACF_KM_PAES_192	0x1b
#define CPACF_KM_PAES_256	0x1c
#define CPACF_KM_XTS_128	0x32
#define CPACF_KM_XTS_256	0x34
#define CPACF_KM_PXTS_128	0x3a
#define CPACF_KM_PXTS_256	0x3c
#define CPACF_KM_XTS_128_FULL	0x52
#define CPACF_KM_XTS_256_FULL	0x54
#define CPACF_KM_PXTS_128_FULL	0x5a
#define CPACF_KM_PXTS_256_FULL	0x5c

/*
 * Function codes for the KMC (CIPHER MESSAGE WITH CHAINING)
 * instruction
 */
#define CPACF_KMC_QUERY		0x00
#define CPACF_KMC_DEA		0x01
#define CPACF_KMC_TDEA_128	0x02
#define CPACF_KMC_TDEA_192	0x03
#define CPACF_KMC_AES_128	0x12
#define CPACF_KMC_AES_192	0x13
#define CPACF_KMC_AES_256	0x14
#define CPACF_KMC_PAES_128	0x1a
#define CPACF_KMC_PAES_192	0x1b
#define CPACF_KMC_PAES_256	0x1c
#define CPACF_KMC_PRNG		0x43

/*
 * Function codes for the KMCTR (CIPHER MESSAGE WITH COUNTER)
 * instruction
 */
#define CPACF_KMCTR_QUERY	0x00
#define CPACF_KMCTR_DEA		0x01
#define CPACF_KMCTR_TDEA_128	0x02
#define CPACF_KMCTR_TDEA_192	0x03
#define CPACF_KMCTR_AES_128	0x12
#define CPACF_KMCTR_AES_192	0x13
#define CPACF_KMCTR_AES_256	0x14
#define CPACF_KMCTR_PAES_128	0x1a
#define CPACF_KMCTR_PAES_192	0x1b
#define CPACF_KMCTR_PAES_256	0x1c

/*
 * Function codes for the KIMD (COMPUTE INTERMEDIATE MESSAGE DIGEST)
 * instruction
 */
#define CPACF_KIMD_QUERY	0x00
#define CPACF_KIMD_SHA_1	0x01
#define CPACF_KIMD_SHA_256	0x02
#define CPACF_KIMD_SHA_512	0x03
#define CPACF_KIMD_SHA3_224	0x20
#define CPACF_KIMD_SHA3_256	0x21
#define CPACF_KIMD_SHA3_384	0x22
#define CPACF_KIMD_SHA3_512	0x23
#define CPACF_KIMD_GHASH	0x41

/*
 * Function codes for the KLMD (COMPUTE LAST MESSAGE DIGEST)
 * instruction
 */
#define CPACF_KLMD_QUERY	0x00
#define CPACF_KLMD_SHA_1	0x01
#define CPACF_KLMD_SHA_256	0x02
#define CPACF_KLMD_SHA_512	0x03
#define CPACF_KLMD_SHA3_224	0x20
#define CPACF_KLMD_SHA3_256	0x21
#define CPACF_KLMD_SHA3_384	0x22
#define CPACF_KLMD_SHA3_512	0x23

/*
 * function codes for the KMAC (COMPUTE MESSAGE AUTHENTICATION CODE)
 * instruction
 */
#define CPACF_KMAC_QUERY	0x00
#define CPACF_KMAC_DEA		0x01
#define CPACF_KMAC_TDEA_128	0x02
#define CPACF_KMAC_TDEA_192	0x03
#define CPACF_KMAC_HMAC_SHA_224	0x70
#define CPACF_KMAC_HMAC_SHA_256	0x71
#define CPACF_KMAC_HMAC_SHA_384	0x72
#define CPACF_KMAC_HMAC_SHA_512	0x73
#define CPACF_KMAC_PHMAC_SHA_224	0x78
#define CPACF_KMAC_PHMAC_SHA_256	0x79
#define CPACF_KMAC_PHMAC_SHA_384	0x7a
#define CPACF_KMAC_PHMAC_SHA_512	0x7b

/*
 * Function codes for the PCKMO (PERFORM CRYPTOGRAPHIC KEY MANAGEMENT)
 * instruction
 */
#define CPACF_PCKMO_QUERY		       0x00
#define CPACF_PCKMO_ENC_DES_KEY		       0x01
#define CPACF_PCKMO_ENC_TDES_128_KEY	       0x02
#define CPACF_PCKMO_ENC_TDES_192_KEY	       0x03
#define CPACF_PCKMO_ENC_AES_128_KEY	       0x12
#define CPACF_PCKMO_ENC_AES_192_KEY	       0x13
#define CPACF_PCKMO_ENC_AES_256_KEY	       0x14
#define CPACF_PCKMO_ENC_AES_XTS_128_DOUBLE_KEY 0x15
#define CPACF_PCKMO_ENC_AES_XTS_256_DOUBLE_KEY 0x16
#define CPACF_PCKMO_ENC_ECC_P256_KEY	       0x20
#define CPACF_PCKMO_ENC_ECC_P384_KEY	       0x21
#define CPACF_PCKMO_ENC_ECC_P521_KEY	       0x22
#define CPACF_PCKMO_ENC_ECC_ED25519_KEY	       0x28
#define CPACF_PCKMO_ENC_ECC_ED448_KEY	       0x29
#define CPACF_PCKMO_ENC_HMAC_512_KEY	       0x76
#define CPACF_PCKMO_ENC_HMAC_1024_KEY	       0x7a

/*
 * Function codes for the PRNO (PERFORM RANDOM NUMBER OPERATION)
 * instruction
 */
#define CPACF_PRNO_QUERY		0x00
#define CPACF_PRNO_SHA512_DRNG_GEN	0x03
#define CPACF_PRNO_SHA512_DRNG_SEED	0x83
#define CPACF_PRNO_TRNG_Q_R2C_RATIO	0x70
#define CPACF_PRNO_TRNG			0x72

/*
 * Function codes for the KMA (CIPHER MESSAGE WITH AUTHENTICATION)
 * instruction
 */
#define CPACF_KMA_QUERY		0x00
#define CPACF_KMA_GCM_AES_128	0x12
#define CPACF_KMA_GCM_AES_192	0x13
#define CPACF_KMA_GCM_AES_256	0x14

/*
 * Flags for the KMA (CIPHER MESSAGE WITH AUTHENTICATION) instruction
 */
#define CPACF_KMA_LPC	0x100	/* Last-Plaintext/Ciphertext */
#define CPACF_KMA_LAAD	0x200	/* Last-AAD */
#define CPACF_KMA_HS	0x400	/* Hash-subkey Supplied */

/*
 * Flags for the KIMD/KLMD (COMPUTE INTERMEDIATE/LAST MESSAGE DIGEST)
 * instructions
 */
#define CPACF_KIMD_NIP		0x8000
#define CPACF_KLMD_DUFOP	0x4000
#define CPACF_KLMD_NIP		0x8000

/*
 * Function codes for KDSA (COMPUTE DIGITAL SIGNATURE AUTHENTICATION)
 * instruction
 */
#define CPACF_KDSA_QUERY 0x00
#define CPACF_KDSA_ECDSA_VERIFY_P256 0x01
#define CPACF_KDSA_ECDSA_VERIFY_P384 0x02
#define CPACF_KDSA_ECDSA_VERIFY_P521 0x03
#define CPACF_KDSA_ECDSA_SIGN_P256 0x09
#define CPACF_KDSA_ECDSA_SIGN_P384 0x0a
#define CPACF_KDSA_ECDSA_SIGN_P521 0x0b
#define CPACF_KDSA_ENC_ECDSA_SIGN_P256 0x11
#define CPACF_KDSA_ENC_ECDSA_SIGN_P384 0x12
#define CPACF_KDSA_ENC_ECDSA_SIGN_P521 0x13
#define CPACF_KDSA_EDDSA_VERIFY_ED25519 0x20
#define CPACF_KDSA_EDDSA_VERIFY_ED448 0x24
#define CPACF_KDSA_EDDSA_SIGN_ED25519 0x28
#define CPACF_KDSA_EDDSA_SIGN_ED448 0x2c
#define CPACF_KDSA_ENC_EDDSA_SIGN_ED25519 0x30
#define CPACF_KDSA_ENC_EDDSA_SIGN_ED448 0x34

#define CPACF_FC_QUERY 0x00
#define CPACF_FC_QUERY_AUTH_INFO 0x7F

typedef struct { unsigned char bytes[16]; } cpacf_mask_t;
typedef struct { unsigned char bytes[256]; } cpacf_qai_t;

/*
 * Prototype for a not existing function to produce a link
 * error if __cpacf_query() or __cpacf_check_opcode() is used
 * with an invalid compile time const opcode.
 */
void __cpacf_bad_opcode(void);

static __always_inline void __cpacf_query_rre(u32 opc, u8 r1, u8 r2,
					      u8 *pb, u8 fc)
{
	asm volatile(
		"	la	%%r1,%[pb]\n"
		"	lghi	%%r0,%[fc]\n"
		"	.insn	rre,%[opc] << 16,%[r1],%[r2]\n"
		: [pb] "=R" (*pb)
		: [opc] "i" (opc), [fc] "i" (fc),
		  [r1] "i" (r1), [r2] "i" (r2)
		: "cc", "memory", "r0", "r1");
}

static __always_inline void __cpacf_query_rrf(u32 opc, u8 r1, u8 r2, u8 r3,
					      u8 m4, u8 *pb, u8 fc)
{
	asm volatile(
		"	la	%%r1,%[pb]\n"
		"	lghi	%%r0,%[fc]\n"
		"	.insn	rrf,%[opc] << 16,%[r1],%[r2],%[r3],%[m4]\n"
		: [pb] "=R" (*pb)
		: [opc] "i" (opc), [fc] "i" (fc), [r1] "i" (r1),
		  [r2] "i" (r2), [r3] "i" (r3), [m4] "i" (m4)
		: "cc", "memory", "r0", "r1");
}

static __always_inline void __cpacf_query_insn(unsigned int opcode, void *pb,
					       u8 fc)
{
	switch (opcode) {
	case CPACF_KDSA:
		__cpacf_query_rre(CPACF_KDSA, 0, 2, pb, fc);
		break;
	case CPACF_KIMD:
		__cpacf_query_rre(CPACF_KIMD, 0, 2, pb, fc);
		break;
	case CPACF_KLMD:
		__cpacf_query_rre(CPACF_KLMD, 0, 2, pb, fc);
		break;
	case CPACF_KM:
		__cpacf_query_rre(CPACF_KM, 2, 4, pb, fc);
		break;
	case CPACF_KMA:
		__cpacf_query_rrf(CPACF_KMA, 2, 4, 6, 0, pb, fc);
		break;
	case CPACF_KMAC:
		__cpacf_query_rre(CPACF_KMAC, 0, 2, pb, fc);
		break;
	case CPACF_KMC:
		__cpacf_query_rre(CPACF_KMC, 2, 4, pb, fc);
		break;
	case CPACF_KMCTR:
		__cpacf_query_rrf(CPACF_KMCTR, 2, 4, 6, 0, pb, fc);
		break;
	case CPACF_KMF:
		__cpacf_query_rre(CPACF_KMF, 2, 4, pb, fc);
		break;
	case CPACF_KMO:
		__cpacf_query_rre(CPACF_KMO, 2, 4, pb, fc);
		break;
	case CPACF_PCC:
		__cpacf_query_rre(CPACF_PCC, 0, 0, pb, fc);
		break;
	case CPACF_PCKMO:
		__cpacf_query_rre(CPACF_PCKMO, 0, 0, pb, fc);
		break;
	case CPACF_PRNO:
		__cpacf_query_rre(CPACF_PRNO, 2, 4, pb, fc);
		break;
	default:
		__cpacf_bad_opcode();
	}
}

static __always_inline void __cpacf_query(unsigned int opcode,
					  cpacf_mask_t *mask)
{
	__cpacf_query_insn(opcode, mask, CPACF_FC_QUERY);
}

static __always_inline int __cpacf_check_opcode(unsigned int opcode)
{
	switch (opcode) {
	case CPACF_KMAC:
	case CPACF_KM:
	case CPACF_KMC:
	case CPACF_KIMD:
	case CPACF_KLMD:
		return test_facility(17);	/* check for MSA */
	case CPACF_PCKMO:
		return test_facility(76);	/* check for MSA3 */
	case CPACF_KMF:
	case CPACF_KMO:
	case CPACF_PCC:
	case CPACF_KMCTR:
		return test_facility(77);	/* check for MSA4 */
	case CPACF_PRNO:
		return test_facility(57);	/* check for MSA5 */
	case CPACF_KMA:
		return test_facility(146);	/* check for MSA8 */
	case CPACF_KDSA:
		return test_facility(155);	/* check for MSA9 */
	default:
		__cpacf_bad_opcode();
		return 0;
	}
}

/**
 * cpacf_query() - Query the function code mask for this CPACF opcode
 * @opcode: the opcode of the crypto instruction
 * @mask: ptr to struct cpacf_mask_t
 *
 * Executes the query function for the given crypto instruction @opcode
 * and checks if @func is available
 *
 * On success 1 is returned and the mask is filled with the function
 * code mask for this CPACF opcode, otherwise 0 is returned.
 */
static __always_inline int cpacf_query(unsigned int opcode, cpacf_mask_t *mask)
{
	if (__cpacf_check_opcode(opcode)) {
		__cpacf_query(opcode, mask);
		return 1;
	}
	memset(mask, 0, sizeof(*mask));
	return 0;
}

static inline int cpacf_test_func(cpacf_mask_t *mask, unsigned int func)
{
	return (mask->bytes[func >> 3] & (0x80 >> (func & 7))) != 0;
}

static __always_inline int cpacf_query_func(unsigned int opcode,
					    unsigned int func)
{
	cpacf_mask_t mask;

	if (cpacf_query(opcode, &mask))
		return cpacf_test_func(&mask, func);
	return 0;
}

static __always_inline void __cpacf_qai(unsigned int opcode, cpacf_qai_t *qai)
{
	__cpacf_query_insn(opcode, qai, CPACF_FC_QUERY_AUTH_INFO);
}

/**
 * cpacf_qai() - Get the query authentication information for a CPACF opcode
 * @opcode: the opcode of the crypto instruction
 * @mask: ptr to struct cpacf_qai_t
 *
 * Executes the query authentication information function for the given crypto
 * instruction @opcode and checks if @func is available
 *
 * On success 1 is returned and the mask is filled with the query authentication
 * information for this CPACF opcode, otherwise 0 is returned.
 */
static __always_inline int cpacf_qai(unsigned int opcode, cpacf_qai_t *qai)
{
	if (cpacf_query_func(opcode, CPACF_FC_QUERY_AUTH_INFO)) {
		__cpacf_qai(opcode, qai);
		return 1;
	}
	memset(qai, 0, sizeof(*qai));
	return 0;
}

/**
 * cpacf_km() - executes the KM (CIPHER MESSAGE) instruction
 * @func: the function code passed to KM; see CPACF_KM_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @dest: address of destination memory area
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 *
 * Returns 0 for the query func, number of processed bytes for
 * encryption/decryption funcs
 */
static inline int cpacf_km(unsigned long func, void *param,
			   u8 *dest, const u8 *src, long src_len)
{
	union register_pair d, s;

	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[src]\n"
		"	brc	1,0b\n" /* handle partial completion */
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KM)
		: "cc", "memory", "0", "1");

	return src_len - s.odd;
}

/**
 * cpacf_kmc() - executes the KMC (CIPHER MESSAGE WITH CHAINING) instruction
 * @func: the function code passed to KM; see CPACF_KMC_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @dest: address of destination memory area
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 *
 * Returns 0 for the query func, number of processed bytes for
 * encryption/decryption funcs
 */
static inline int cpacf_kmc(unsigned long func, void *param,
			    u8 *dest, const u8 *src, long src_len)
{
	union register_pair d, s;

	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[src]\n"
		"	brc	1,0b\n" /* handle partial completion */
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMC)
		: "cc", "memory", "0", "1");

	return src_len - s.odd;
}

/**
 * cpacf_kimd() - executes the KIMD (COMPUTE INTERMEDIATE MESSAGE DIGEST)
 *		  instruction
 * @func: the function code passed to KM; see CPACF_KIMD_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 */
static inline void cpacf_kimd(unsigned long func, void *param,
			      const u8 *src, long src_len)
{
	union register_pair s;

	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,0,%[src],8,0\n"
		"	brc	1,0b\n" /* handle partial completion */
		: [src] "+&d" (s.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)(param)),
		  [opc] "i" (CPACF_KIMD)
		: "cc", "memory", "0", "1");
}

/**
 * cpacf_klmd() - executes the KLMD (COMPUTE LAST MESSAGE DIGEST) instruction
 * @func: the function code passed to KM; see CPACF_KLMD_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 */
static inline void cpacf_klmd(unsigned long func, void *param,
			      const u8 *src, long src_len)
{
	union register_pair s;

	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,0,%[src],8,0\n"
		"	brc	1,0b\n" /* handle partial completion */
		: [src] "+&d" (s.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KLMD)
		: "cc", "memory", "0", "1");
}

/**
 * _cpacf_kmac() - executes the KMAC (COMPUTE MESSAGE AUTHENTICATION CODE)
 * instruction and updates flags in gr0
 * @gr0: pointer to gr0 (fc and flags) passed to KMAC; see CPACF_KMAC_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 *
 * Returns 0 for the query func, number of processed bytes for digest funcs
 */
static inline int _cpacf_kmac(unsigned long *gr0, void *param,
			      const u8 *src, long src_len)
{
	union register_pair s;

	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[r0]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,%[src]\n"
		"	brc	1,0b\n" /* handle partial completion */
		"	lgr	%[r0],0\n"
		: [r0] "+d" (*gr0), [src] "+&d" (s.pair)
		: [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMAC)
		: "cc", "memory", "0", "1");

	return src_len - s.odd;
}

/**
 * cpacf_kmac() - executes the KMAC (COMPUTE MESSAGE AUTHENTICATION CODE)
 * instruction
 * @func: function code passed to KMAC; see CPACF_KMAC_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 *
 * Returns 0 for the query func, number of processed bytes for digest funcs
 */
static inline int cpacf_kmac(unsigned long func, void *param,
			     const u8 *src, long src_len)
{
	return _cpacf_kmac(&func, param, src, src_len);
}

/**
 * cpacf_kmctr() - executes the KMCTR (CIPHER MESSAGE WITH COUNTER) instruction
 * @func: the function code passed to KMCTR; see CPACF_KMCTR_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @dest: address of destination memory area
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 * @counter: address of counter value
 *
 * Returns 0 for the query func, number of processed bytes for
 * encryption/decryption funcs
 */
static inline int cpacf_kmctr(unsigned long func, void *param, u8 *dest,
			      const u8 *src, long src_len, u8 *counter)
{
	union register_pair d, s, c;

	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	c.even = (unsigned long)counter;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,%[dst],%[src],%[ctr],0\n"
		"	brc	1,0b\n" /* handle partial completion */
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair),
		  [ctr] "+&d" (c.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMCTR)
		: "cc", "memory", "0", "1");

	return src_len - s.odd;
}

/**
 * cpacf_prno() - executes the PRNO (PERFORM RANDOM NUMBER OPERATION)
 *		  instruction
 * @func: the function code passed to PRNO; see CPACF_PRNO_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @dest: address of destination memory area
 * @dest_len: size of destination memory area in bytes
 * @seed: address of seed data
 * @seed_len: size of seed data in bytes
 */
static inline void cpacf_prno(unsigned long func, void *param,
			      u8 *dest, unsigned long dest_len,
			      const u8 *seed, unsigned long seed_len)
{
	union register_pair d, s;

	d.even = (unsigned long)dest;
	d.odd  = (unsigned long)dest_len;
	s.even = (unsigned long)seed;
	s.odd  = (unsigned long)seed_len;
	asm volatile (
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[seed]\n"
		"	brc	1,0b\n"	  /* handle partial completion */
		: [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [seed] "d" (s.pair), [opc] "i" (CPACF_PRNO)
		: "cc", "memory", "0", "1");
}

/**
 * cpacf_trng() - executes the TRNG subfunction of the PRNO instruction
 * @ucbuf: buffer for unconditioned data
 * @ucbuf_len: amount of unconditioned data to fetch in bytes
 * @cbuf: buffer for conditioned data
 * @cbuf_len: amount of conditioned data to fetch in bytes
 */
static inline void cpacf_trng(u8 *ucbuf, unsigned long ucbuf_len,
			      u8 *cbuf, unsigned long cbuf_len)
{
	union register_pair u, c;

	u.even = (unsigned long)ucbuf;
	u.odd  = (unsigned long)ucbuf_len;
	c.even = (unsigned long)cbuf;
	c.odd  = (unsigned long)cbuf_len;
	asm volatile (
		"	lghi	0,%[fc]\n"
		"0:	.insn	rre,%[opc] << 16,%[ucbuf],%[cbuf]\n"
		"	brc	1,0b\n"	  /* handle partial completion */
		: [ucbuf] "+&d" (u.pair), [cbuf] "+&d" (c.pair)
		: [fc] "K" (CPACF_PRNO_TRNG), [opc] "i" (CPACF_PRNO)
		: "cc", "memory", "0");
	kmsan_unpoison_memory(ucbuf, ucbuf_len);
	kmsan_unpoison_memory(cbuf, cbuf_len);
}

/**
 * cpacf_pcc() - executes the PCC (PERFORM CRYPTOGRAPHIC COMPUTATION)
 *		 instruction
 * @func: the function code passed to PCC; see CPACF_KM_xxx defines
 * @param: address of parameter block; see POP for details on each func
 *
 * Returns the condition code, this is
 * 0 - cc code 0 (normal completion)
 * 1 - cc code 1 (protected key wkvp mismatch or src operand out of range)
 * 2 - cc code 2 (something invalid, scalar multiply infinity, ...)
 * Condition code 3 (partial completion) is handled within the asm code
 * and never returned.
 */
static inline int cpacf_pcc(unsigned long func, void *param)
{
	int cc;

	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,0\n" /* PCC opcode */
		"	brc	1,0b\n" /* handle partial completion */
		CC_IPM(cc)
		: CC_OUT(cc, cc)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_PCC)
		: CC_CLOBBER_LIST("memory", "0", "1"));

	return CC_TRANSFORM(cc);
}

/**
 * cpacf_pckmo() - executes the PCKMO (PERFORM CRYPTOGRAPHIC KEY
 *		  MANAGEMENT) instruction
 * @func: the function code passed to PCKMO; see CPACF_PCKMO_xxx defines
 * @param: address of parameter block; see POP for details on each func
 *
 * Returns 0.
 */
static inline void cpacf_pckmo(long func, void *param)
{
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"       .insn   rre,%[opc] << 16,0,0\n" /* PCKMO opcode */
		:
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_PCKMO)
		: "cc", "memory", "0", "1");
}

/**
 * cpacf_kma() - executes the KMA (CIPHER MESSAGE WITH AUTHENTICATION)
 *		 instruction
 * @func: the function code passed to KMA; see CPACF_KMA_xxx defines
 * @param: address of parameter block; see POP for details on each func
 * @dest: address of destination memory area
 * @src: address of source memory area
 * @src_len: length of src operand in bytes
 * @aad: address of additional authenticated data memory area
 * @aad_len: length of aad operand in bytes
 */
static inline void cpacf_kma(unsigned long func, void *param, u8 *dest,
			     const u8 *src, unsigned long src_len,
			     const u8 *aad, unsigned long aad_len)
{
	union register_pair d, s, a;

	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	a.even = (unsigned long)aad;
	a.odd  = (unsigned long)aad_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,%[dst],%[src],%[aad],0\n"
		"	brc	1,0b\n"	/* handle partial completion */
		: [dst] "+&d" (d.pair), [src] "+&d" (s.pair),
		  [aad] "+&d" (a.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMA)
		: "cc", "memory", "0", "1");
}

#endif	/* _ASM_S390_CPACF_H */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 *	header.S
 *
 *	Copyright (C) 1991, 1992 Linus Torvalds
 *
 *	Based on bootsect.S and setup.S
 *	modified by more people than can be counted
 *
 *	Rewritten as a common file by H. Peter Anvin (Apr 2007)
 *
 * BIG FAT NOTE: We're in real mode using 64k segments.  Therefore segment
 * addresses must be multiplied by 16 to obtain their respective linear
 * addresses. To avoid confusion, linear addresses are written using leading
 * hex while segment addresses are written as segment:offset.
 *
 */
#include <linux/pe.h>
#include <asm/segment.h>
#include <asm/boot.h>
#include <asm/page_types.h>
#include <asm/setup.h>
#include <asm/bootparam.h>
#include "boot.h"
#include "voffset.h"
#include "zoffset.h"

BOOTSEG		= 0x07C0		/* original address of boot-sector */
SYSSEG		= 0x1000		/* historical load address >> 4 */

#ifndef SVGA_MODE
#define SVGA_MODE ASK_VGA
#endif

#ifndef ROOT_RDONLY
#define ROOT_RDONLY 1
#endif

	.set	salign, 0x1000
	.set	falign, 0x200

	.code16
	.section ".bstext", "ax"
#ifdef CONFIG_EFI_STUB
	# "MZ", MS-DOS header
	.word	IMAGE_DOS_SIGNATURE
	.org	0x38
	#
	# Offset to the PE header.
	#
	.long	LINUX_PE_MAGIC
	.long	pe_header
pe_header:
	.long	IMAGE_NT_SIGNATURE

coff_header:
#ifdef CONFIG_X86_32
	.set	image_file_add_flags, IMAGE_FILE_32BIT_MACHINE
	.set	pe_opt_magic, IMAGE_NT_OPTIONAL_HDR32_MAGIC
	.word	IMAGE_FILE_MACHINE_I386
#else
	.set	image_file_add_flags, 0
	.set	pe_opt_magic, IMAGE_NT_OPTIONAL_HDR64_MAGIC
	.word	IMAGE_FILE_MACHINE_AMD64
#endif
	.word	section_count			# nr_sections
	.long	0 				# TimeDateStamp
	.long	0				# PointerToSymbolTable
	.long	1				# NumberOfSymbols
	.word	section_table - optional_header	# SizeOfOptionalHeader
	.word	IMAGE_FILE_EXECUTABLE_IMAGE	| \
		image_file_add_flags		| \
		IMAGE_FILE_DEBUG_STRIPPED	| \
		IMAGE_FILE_LINE_NUMS_STRIPPED	# Characteristics

optional_header:
	.word	pe_opt_magic
	.byte	0x02				# MajorLinkerVersion
	.byte	0x14				# MinorLinkerVersion

	.long	ZO__data			# SizeOfCode

	.long	ZO__end - ZO__data		# SizeOfInitializedData
	.long	0				# SizeOfUninitializedData

	.long	setup_size + ZO_efi_pe_entry	# AddressOfEntryPoint

	.long	setup_size			# BaseOfCode
#ifdef CONFIG_X86_32
	.long	0				# data
#endif

extra_header_fields:
#ifdef CONFIG_X86_32
	.long	0				# ImageBase
#else
	.quad	0				# ImageBase
#endif
	.long	salign				# SectionAlignment
	.long	falign				# FileAlignment
	.word	0				# MajorOperatingSystemVersion
	.word	0				# MinorOperatingSystemVersion
	.word	LINUX_EFISTUB_MAJOR_VERSION	# MajorImageVersion
	.word	LINUX_EFISTUB_MINOR_VERSION	# MinorImageVersion
	.word	0				# MajorSubsystemVersion
	.word	0				# MinorSubsystemVersion
	.long	0				# Win32VersionValue

	.long	setup_size + ZO__end		# SizeOfImage

	.long	salign				# SizeOfHeaders
	.long	0				# CheckSum
	.word	IMAGE_SUBSYSTEM_EFI_APPLICATION	# Subsystem (EFI application)
	.word	IMAGE_DLLCHARACTERISTICS_NX_COMPAT	# DllCharacteristics
#ifdef CONFIG_X86_32
	.long	0				# SizeOfStackReserve
	.long	0				# SizeOfStackCommit
	.long	0				# SizeOfHeapReserve
	.long	0				# SizeOfHeapCommit
#else
	.quad	0				# SizeOfStackReserve
	.quad	0				# SizeOfStackCommit
	.quad	0				# SizeOfHeapReserve
	.quad	0				# SizeOfHeapCommit
#endif
	.long	0				# LoaderFlags
	.long	(section_table - .) / 8		# NumberOfRvaAndSizes

	.quad	0				# ExportTable
	.quad	0				# ImportTable
	.quad	0				# ResourceTable
	.quad	0				# ExceptionTable
	.quad	0				# CertificationTable
	.quad	0				# BaseRelocationTable

	# Section table
section_table:
	.ascii	".setup"
	.byte	0
	.byte	0
	.long	pecompat_fstart - salign 	# VirtualSize
	.long	salign				# VirtualAddress
	.long	pecompat_fstart - salign	# SizeOfRawData
	.long	salign				# PointerToRawData

	.long	0, 0, 0
	.long	IMAGE_SCN_CNT_INITIALIZED_DATA	| \
		IMAGE_SCN_MEM_READ		| \
		IMAGE_SCN_MEM_DISCARDABLE	# Characteristics

#ifdef CONFIG_EFI_MIXED
	.asciz	".compat"

	.long	pecompat_fsize			# VirtualSize
	.long	pecompat_fstart			# VirtualAddress
	.long	pecompat_fsize			# SizeOfRawData
	.long	pecompat_fstart			# PointerToRawData

	.long	0, 0, 0
	.long	IMAGE_SCN_CNT_INITIALIZED_DATA	| \
		IMAGE_SCN_MEM_READ		| \
		IMAGE_SCN_MEM_DISCARDABLE	# Characteristics

	/*
	 * Put the IA-32 machine type and the associated entry point address in
	 * the .compat section, so loaders can figure out which other execution
	 * modes this image supports.
	 */
	.pushsection ".pecompat", "a", @progbits
	.balign	salign
	.globl	pecompat_fstart
pecompat_fstart:
	.byte	0x1				# Version
	.byte	8				# Size
	.word	IMAGE_FILE_MACHINE_I386		# PE machine type
	.long	setup_size + ZO_efi32_pe_entry	# Entrypoint
	.byte	0x0				# Sentinel
	.popsection
#else
	.set	pecompat_fstart, setup_size
#endif
	.ascii	".text\0\0\0"
	.long	textsize            		# VirtualSize
	.long	setup_size			# VirtualAddress
	.long	textsize			# SizeOfRawData
	.long	setup_size			# PointerToRawData
	.long	0				# PointerToRelocations
	.long	0				# PointerToLineNumbers
	.word	0				# NumberOfRelocations
	.word	0				# NumberOfLineNumbers
	.long	IMAGE_SCN_CNT_CODE		| \
		IMAGE_SCN_MEM_READ		| \
		IMAGE_SCN_MEM_EXECUTE		# Characteristics

#ifdef CONFIG_EFI_SBAT
	.ascii	".sbat\0\0\0"
	.long	ZO__esbat - ZO__sbat            # VirtualSize
	.long	setup_size + ZO__sbat           # VirtualAddress
	.long	ZO__esbat - ZO__sbat            # SizeOfRawData
	.long	setup_size + ZO__sbat           # PointerToRawData

	.long	0, 0, 0
	.long	IMAGE_SCN_CNT_INITIALIZED_DATA	| \
		IMAGE_SCN_MEM_READ		| \
		IMAGE_SCN_MEM_DISCARDABLE	# Characteristics

	.set	textsize, ZO__sbat
#else
	.set	textsize, ZO__data
#endif

	.ascii	".data\0\0\0"
	.long	ZO__end - ZO__data		# VirtualSize
	.long	setup_size + ZO__data		# VirtualAddress
	.long	ZO__edata - ZO__data		# SizeOfRawData
	.long	setup_size + ZO__data		# PointerToRawData

	.long	0, 0, 0
	.long	IMAGE_SCN_CNT_INITIALIZED_DATA	| \
		IMAGE_SCN_MEM_READ		| \
		IMAGE_SCN_MEM_WRITE		# Characteristics

	.set	section_count, (. - section_table) / 40
#endif /* CONFIG_EFI_STUB */

	# Kernel attributes; used by setup.  This is part 1 of the
	# header, from the old boot sector.

	.section ".header", "a"
	.globl	sentinel
sentinel:	.byte 0xff, 0xff        /* Used to detect broken loaders */

	.globl	hdr
hdr:
		.byte setup_sects - 1
root_flags:	.word ROOT_RDONLY
syssize:	.long ZO__edata / 16
ram_size:	.word 0			/* Obsolete */
vid_mode:	.word SVGA_MODE
root_dev:	.word 0			/* Default to major/minor 0/0 */
boot_flag:	.word 0xAA55

	# offset 512, entry point

	.globl	_start
_start:
		# Explicitly enter this as bytes, or the assembler
		# tries to generate a 3-byte jump here, which causes
		# everything else to push off to the wrong offset.
		.byte	0xeb		# short (2-byte) jump
		.byte	start_of_setup-1f
1:

	# Part 2 of the header, from the old setup.S

		.ascii	"HdrS"		# header signature
		.word	0x020f		# header version number (>= 0x0105)
					# or else old loadlin-1.5 will fail)
		.globl realmode_swtch
realmode_swtch:	.word	0, 0		# default_switch, SETUPSEG
start_sys_seg:	.word	SYSSEG		# obsolete and meaningless, but just
					# in case something decided to "use" it
		.word	kernel_version-512 # pointing to kernel version string
					# above section of header is compatible
					# with loadlin-1.5 (header v1.5). Don't
					# change it.

type_of_loader:	.byte	0		# 0 means ancient bootloader, newer
					# bootloaders know to change this.
					# See Documentation/arch/x86/boot.rst for
					# assigned ids

# flags, unused bits must be zero (RFU) bit within loadflags
loadflags:
		.byte	LOADED_HIGH	# The kernel is to be loaded high

setup_move_size: .word  0x8000		# size to move, when setup is not
					# loaded at 0x90000. We will move setup
					# to 0x90000 then just before jumping
					# into the kernel. However, only the
					# loader knows how much data behind
					# us also needs to be loaded.

code32_start:				# here loaders can put a different
					# start address for 32-bit code.
		.long	0x100000	# 0x100000 = default for big kernel

ramdisk_image:	.long	0		# address of loaded ramdisk image
					# Here the loader puts the 32-bit
					# address where it loaded the image.
					# This only will be read by the kernel.

ramdisk_size:	.long	0		# its size in bytes

bootsect_kludge:
		.long	0		# obsolete

heap_end_ptr:	.word	_end+STACK_SIZE-512
					# (Header version 0x0201 or later)
					# space from here (exclusive) down to
					# end of setup code can be used by setup
					# for local heap purposes.

ext_loader_ver:
		.byte	0		# Extended boot loader version
ext_loader_type:
		.byte	0		# Extended boot loader type

cmd_line_ptr:	.long	0		# (Header version 0x0202 or later)
					# If nonzero, a 32-bit pointer
					# to the kernel command line.
					# The command line should be
					# located between the start of
					# setup and the end of low
					# memory (0xa0000), or it may
					# get overwritten before it
					# gets read.  If this field is
					# used, there is no longer
					# anything magical about the
					# 0x90000 segment; the setup
					# can be located anywhere in
					# low memory 0x10000 or higher.

initrd_addr_max: .long 0x7fffffff
					# (Header version 0x0203 or later)
					# The highest safe address for
					# the contents of an initrd
					# The current kernel allows up to 4 GB,
					# but leave it at 2 GB to avoid
					# possible bootloader bugs.

kernel_alignment:  .long CONFIG_PHYSICAL_ALIGN	#physical addr alignment
						#required for protected mode
						#kernel
#ifdef CONFIG_RELOCATABLE
relocatable_kernel:    .byte 1
#else
relocatable_kernel:    .byte 0
#endif
min_alignment:		.byte MIN_KERNEL_ALIGN_LG2	# minimum alignment

xloadflags:
#ifdef CONFIG_X86_64
# define XLF0 XLF_KERNEL_64			/* 64-bit kernel */
#else
# define XLF0 0
#endif

#if defined(CONFIG_RELOCATABLE) && defined(CONFIG_X86_64)
   /* kernel/boot_param/ramdisk could be loaded above 4g */
# define XLF1 XLF_CAN_BE_LOADED_ABOVE_4G
#else
# define XLF1 0
#endif

#ifdef CONFIG_EFI_HANDOVER_PROTOCOL
# ifdef CONFIG_EFI_MIXED
#  define XLF23 (XLF_EFI_HANDOVER_32|XLF_EFI_HANDOVER_64)
# else
#  ifdef CONFIG_X86_64
#   define XLF23 XLF_EFI_HANDOVER_64		/* 64-bit EFI handover ok */
#  else
#   define XLF23 XLF_EFI_HANDOVER_32		/* 32-bit EFI handover ok */
#  endif
# endif
#else
# define XLF23 0
#endif

#if defined(CONFIG_X86_64) && defined(CONFIG_EFI) && defined(CONFIG_KEXEC_CORE)
# define XLF4 XLF_EFI_KEXEC
#else
# define XLF4 0
#endif

#ifdef CONFIG_X86_64
#define XLF56 (XLF_5LEVEL|XLF_5LEVEL_ENABLED)
#else
#define XLF56 0
#endif

			.word XLF0 | XLF1 | XLF23 | XLF4 | XLF56

cmdline_size:   .long   COMMAND_LINE_SIZE-1     #length of the command line,
                                                #added with boot protocol
                                                #version 2.06

hardware_subarch:	.long 0			# subarchitecture, added with 2.07
						# default to 0 for normal x86 PC

hardware_subarch_data:	.quad 0

payload_offset:		.long ZO_input_data
payload_length:		.long ZO_z_input_len

setup_data:		.quad 0			# 64-bit physical pointer to
						# single linked list of
						# struct setup_data

pref_address:		.quad LOAD_PHYSICAL_ADDR	# preferred load addr

#
# Getting to provably safe in-place decompression is hard. Worst case
# behaviours need to be analyzed. Here let's take the decompression of
# a gzip-compressed kernel as example, to illustrate it:
#
# The file layout of gzip compressed kernel is:
#
#    magic[2]
#    method[1]
#    flags[1]
#    timestamp[4]
#    extraflags[1]
#    os[1]
#    compressed data blocks[N]
#    crc[4] orig_len[4]
#
# ... resulting in +18 bytes overhead of uncompressed data.
#
# (For more information, please refer to RFC 1951 and RFC 1952.)
#
# Files divided into blocks
# 1 bit (last block flag)
# 2 bits (block type)
#
# 1 block occurs every 32K -1 bytes or when there 50% compression
# has been achieved. The smallest block type encoding is always used.
#
# stored:
#    32 bits length in bytes.
#
# fixed:
#    magic fixed tree.
#    symbols.
#
# dynamic:
#    dynamic tree encoding.
#    symbols.
#
#
# The buffer for decompression in place is the length of the uncompressed
# data, plus a small amount extra to keep the algorithm safe. The
# compressed data is placed at the end of the buffer.  The output pointer
# is placed at the start of the buffer and the input pointer is placed
# where the compressed data starts. Problems will occur when the output
# pointer overruns the input pointer.
#
# The output pointer can only overrun the input pointer if the input
# pointer is moving faster than the output pointer.  A condition only
# triggered by data whose compressed form is larger than the uncompressed
# form.
#
# The worst case at the block level is a growth of the compressed data
# of 5 bytes per 32767 bytes.
#
# The worst case internal to a compressed block is very hard to figure.
# The worst case can at least be bounded by having one bit that represents
# 32764 bytes and then all of the rest of the bytes representing the very
# very last byte.
#
# All of which is enough to compute an amount of extra data that is required
# to be safe.  To avoid problems at the block level allocating 5 extra bytes
# per 32767 bytes of data is sufficient.  To avoid problems internal to a
# block adding an extra 32767 bytes (the worst case uncompressed block size)
# is sufficient, to ensure that in the worst case the decompressed data for
# block will stop the byte before the compressed data for a block begins.
# To avoid problems with the compressed data's meta information an extra 18
# bytes are needed.  Leading to the formula:
#
# extra_bytes = (uncompressed_size >> 12) + 32768 + 18
#
# Adding 8 bytes per 32K is a bit excessive but much easier to calculate.
# Adding 32768 instead of 32767 just makes for round numbers.
#
# Above analysis is for decompressing gzip compressed kernel only. Up to
# now 6 different decompressor are supported all together. And among them
# xz stores data in chunks and has maximum chunk of 64K. Hence safety
# margin should be updated to cover all decompressors so that we don't
# need to deal with each of them separately. Please check
# the description in lib/decompressor_xxx.c for specific information.
#
# extra_bytes = (uncompressed_size >> 12) + 65536 + 128
#
# LZ4 is even worse: data that cannot be further compressed grows by 0.4%,
# or one byte per 256 bytes. OTOH, we can safely get rid of the +128 as
# the size-dependent part now grows so fast.
#
# extra_bytes = (uncompressed_size >> 8) + 65536
#
# ZSTD compressed data grows by at most 3 bytes per 128K, and only has a 22
# byte fixed overhead but has a maximum block size of 128K, so it needs a
# larger margin.
#
# extra_bytes = (uncompressed_size >> 8) + 131072

#define ZO_z_extra_bytes	((ZO_z_output_len >> 8) + 131072)
#if ZO_z_output_len > ZO_z_input_len
# define ZO_z_extract_offset	(ZO_z_output_len + ZO_z_extra_bytes - \
				 ZO_z_input_len)
#else
# define ZO_z_extract_offset	ZO_z_extra_bytes
#endif

/*
 * The extract_offset has to be bigger than ZO head section. Otherwise when
 * the head code is running to move ZO to the end of the buffer, it will
 * overwrite the head code itself.
 */
#if (ZO__ehead - ZO_startup_32) > ZO_z_extract_offset
# define ZO_z_min_extract_offset ((ZO__ehead - ZO_startup_32 + 4095) & ~4095)
#else
# define ZO_z_min_extract_offset ((ZO_z_extract_offset + 4095) & ~4095)
#endif

#define ZO_INIT_SIZE	(ZO__end - ZO_startup_32 + ZO_z_min_extract_offset)

#define VO_INIT_SIZE	(VO__end - VO__text)
#if ZO_INIT_SIZE > VO_INIT_SIZE
# define INIT_SIZE ZO_INIT_SIZE
#else
# define INIT_SIZE VO_INIT_SIZE
#endif

	.macro		__handover_offset
#ifndef CONFIG_EFI_HANDOVER_PROTOCOL
	.long		0
#elif !defined(CONFIG_X86_64)
	.long		ZO_efi32_stub_entry
#else
	/* Yes, this is really how we defined it :( */
	.long		ZO_efi64_stub_entry - 0x200
#ifdef CONFIG_EFI_MIXED
	.if		ZO_efi32_stub_entry != ZO_efi64_stub_entry - 0x200
	.error		"32-bit and 64-bit EFI entry points do not match"
	.endif
#endif
#endif
	.endm

init_size:		.long INIT_SIZE		# kernel initialization size
handover_offset:	__handover_offset
kernel_info_offset:	.long ZO_kernel_info

# End of setup header #####################################################

	.section ".entrytext", "ax"
start_of_setup:
# Force %es = %ds
	movw	%ds, %ax
	movw	%ax, %es
	cld

# Apparently some ancient versions of LILO invoked the kernel with %ss != %ds,
# which happened to work by accident for the old code.  Recalculate the stack
# pointer if %ss is invalid.  Otherwise leave it alone, LOADLIN sets up the
# stack behind its own code, so we can't blindly put it directly past the heap.

	movw	%ss, %dx
	cmpw	%ax, %dx	# %ds == %ss?
	movw	%sp, %dx
	je	2f		# -> assume %sp is reasonably set

	# Invalid %ss, make up a new stack
	movw	$_end, %dx
	testb	$CAN_USE_HEAP, loadflags
	jz	1f
	movw	heap_end_ptr, %dx
1:	addw	$STACK_SIZE, %dx
	jnc	2f
	xorw	%dx, %dx	# Prevent wraparound

2:	# Now %dx should point to the end of our stack space
	andw	$~3, %dx	# dword align (might as well...)
	jnz	3f
	movw	$0xfffc, %dx	# Make sure we're not zero
3:	movw	%ax, %ss
	movzwl	%dx, %esp	# Clear upper half of %esp
	sti			# Now we should have a working stack

# We will have entered with %cs = %ds+0x20, normalize %cs so
# it is on par with the other segments.
	pushw	%ds
	pushw	$6f
	lretw
6:

# Check signature at end of setup
	cmpl	$0x5a5aaa55, setup_sig
	jne	setup_bad

# Zero the bss
	movw	$__bss_start, %di
	movw	$_end+3, %cx
	xorl	%eax, %eax
	subw	%di, %cx
	shrw	$2, %cx
	rep stosl

# Jump to C code (should not return)
	calll	main

# Setup corrupt somehow...
setup_bad:
	movl	$setup_corrupt, %eax
	calll	puts
	# Fall through...

	.globl	die
	.type	die, @function
die:
	hlt
	jmp	die

	.size	die, .-die

	.section ".initdata", "a"
setup_corrupt:
	.byte	7
	.string	"No setup signature found...\n"

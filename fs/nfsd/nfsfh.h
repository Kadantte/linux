/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 *
 * This file describes the layout of the file handles as passed
 * over the wire.
 */
#ifndef _LINUX_NFSD_NFSFH_H
#define _LINUX_NFSD_NFSFH_H

#include <linux/crc32.h>
#include <linux/sunrpc/svc.h>
#include <linux/iversion.h>
#include <linux/exportfs.h>
#include <linux/nfs4.h>

/*
 * The file handle starts with a sequence of four-byte words.
 * The first word contains a version number (1) and three descriptor bytes
 * that tell how the remaining 3 variable length fields should be handled.
 * These three bytes are auth_type, fsid_type and fileid_type.
 *
 * All four-byte values are in host-byte-order.
 *
 * The auth_type field is deprecated and must be set to 0.
 *
 * The fsid_type identifies how the filesystem (or export point) is
 *    encoded.
 *  Current values:
 *     0  - 4 byte device id (ms-2-bytes major, ls-2-bytes minor), 4byte inode number
 *        NOTE: we cannot use the kdev_t device id value, because kdev_t.h
 *              says we mustn't.  We must break it up and reassemble.
 *     1  - 4 byte user specified identifier
 *     2  - 4 byte major, 4 byte minor, 4 byte inode number - DEPRECATED
 *     3  - 4 byte device id, encoded for user-space, 4 byte inode number
 *     4  - 4 byte inode number and 4 byte uuid
 *     5  - 8 byte uuid
 *     6  - 16 byte uuid
 *     7  - 8 byte inode number and 16 byte uuid
 *
 * The fileid_type identifies how the file within the filesystem is encoded.
 *   The values for this field are filesystem specific, exccept that
 *   filesystems must not use the values '0' or '0xff'. 'See enum fid_type'
 *   in include/linux/exportfs.h for currently registered values.
 */

struct knfsd_fh {
	unsigned int	fh_size;	/*
					 * Points to the current size while
					 * building a new file handle.
					 */
	u8		fh_raw[NFS4_FHSIZE];
};

#define fh_version		fh_raw[0]
#define fh_auth_type		fh_raw[1]
#define fh_fsid_type		fh_raw[2]
#define fh_fileid_type		fh_raw[3]

static inline u32 *fh_fsid(const struct knfsd_fh *fh)
{
	return (u32 *)&fh->fh_raw[4];
}

static inline __u32 ino_t_to_u32(ino_t ino)
{
	return (__u32) ino;
}

static inline ino_t u32_to_ino_t(__u32 uino)
{
	return (ino_t) uino;
}

/*
 * This is the internal representation of an NFS handle used in knfsd.
 * pre_mtime/post_version will be used to support wcc_attr's in NFSv3.
 */
typedef struct svc_fh {
	struct knfsd_fh		fh_handle;	/* FH data */
	int			fh_maxsize;	/* max size for fh_handle */
	struct dentry *		fh_dentry;	/* validated dentry */
	struct svc_export *	fh_export;	/* export pointer */

	bool			fh_want_write;	/* remount protection taken */
	bool			fh_no_wcc;	/* no wcc data needed */
	bool			fh_no_atomic_attr;
						/*
						 * wcc data is not atomic with
						 * operation
						 */
	bool			fh_use_wgather;	/* NFSv2 wgather option */
	bool			fh_64bit_cookies;/* readdir cookie size */
	int			fh_flags;	/* FH flags */
	bool			fh_post_saved;	/* post-op attrs saved */
	bool			fh_pre_saved;	/* pre-op attrs saved */

	/* Pre-op attributes saved when inode is locked */
	__u64			fh_pre_size;	/* size before operation */
	struct timespec64	fh_pre_mtime;	/* mtime before oper */
	struct timespec64	fh_pre_ctime;	/* ctime before oper */
	/*
	 * pre-op nfsv4 change attr: note must check IS_I_VERSION(inode)
	 *  to find out if it is valid.
	 */
	u64			fh_pre_change;

	/* Post-op attributes saved in fh_fill_post_attrs() */
	struct kstat		fh_post_attr;	/* full attrs after operation */
	u64			fh_post_change; /* nfsv4 change; see above */
} svc_fh;
#define NFSD4_FH_FOREIGN (1<<0)
#define SET_FH_FLAG(c, f) ((c)->fh_flags |= (f))
#define HAS_FH_FLAG(c, f) ((c)->fh_flags & (f))

enum nfsd_fsid {
	FSID_DEV = 0,
	FSID_NUM,
	FSID_MAJOR_MINOR,
	FSID_ENCODE_DEV,
	FSID_UUID4_INUM,
	FSID_UUID8,
	FSID_UUID16,
	FSID_UUID16_INUM,
};

enum fsid_source {
	FSIDSOURCE_DEV,
	FSIDSOURCE_FSID,
	FSIDSOURCE_UUID,
};
extern enum fsid_source fsid_source(const struct svc_fh *fhp);


/*
 * This might look a little large to "inline" but in all calls except
 * one, 'vers' is constant so moste of the function disappears.
 *
 * In some cases the values are considered to be host endian and in
 * others, net endian. fsidv is always considered to be u32 as the
 * callers don't know which it will be. So we must use __force to keep
 * sparse from complaining. Since these values are opaque to the
 * client, that shouldn't be a problem.
 */
static inline void mk_fsid(int vers, u32 *fsidv, dev_t dev, ino_t ino,
			   u32 fsid, unsigned char *uuid)
{
	u32 *up;
	switch(vers) {
	case FSID_DEV:
		fsidv[0] = (__force __u32)htonl((MAJOR(dev)<<16) |
				 MINOR(dev));
		fsidv[1] = ino_t_to_u32(ino);
		break;
	case FSID_NUM:
		fsidv[0] = fsid;
		break;
	case FSID_MAJOR_MINOR:
		fsidv[0] = (__force __u32)htonl(MAJOR(dev));
		fsidv[1] = (__force __u32)htonl(MINOR(dev));
		fsidv[2] = ino_t_to_u32(ino);
		break;

	case FSID_ENCODE_DEV:
		fsidv[0] = new_encode_dev(dev);
		fsidv[1] = ino_t_to_u32(ino);
		break;

	case FSID_UUID4_INUM:
		/* 4 byte fsid and inode number */
		up = (u32*)uuid;
		fsidv[0] = ino_t_to_u32(ino);
		fsidv[1] = up[0] ^ up[1] ^ up[2] ^ up[3];
		break;

	case FSID_UUID8:
		/* 8 byte fsid  */
		up = (u32*)uuid;
		fsidv[0] = up[0] ^ up[2];
		fsidv[1] = up[1] ^ up[3];
		break;

	case FSID_UUID16:
		/* 16 byte fsid - NFSv3+ only */
		memcpy(fsidv, uuid, 16);
		break;

	case FSID_UUID16_INUM:
		/* 8 byte inode and 16 byte fsid */
		*(u64*)fsidv = (u64)ino;
		memcpy(fsidv+2, uuid, 16);
		break;
	default: BUG();
	}
}

static inline int key_len(int type)
{
	switch(type) {
	case FSID_DEV:		return 8;
	case FSID_NUM: 		return 4;
	case FSID_MAJOR_MINOR:	return 12;
	case FSID_ENCODE_DEV:	return 8;
	case FSID_UUID4_INUM:	return 8;
	case FSID_UUID8:	return 8;
	case FSID_UUID16:	return 16;
	case FSID_UUID16_INUM:	return 24;
	default: return 0;
	}
}

/*
 * Shorthand for dprintk()'s
 */
extern char * SVCFH_fmt(struct svc_fh *fhp);

/*
 * Function prototypes
 */
__be32	fh_verify(struct svc_rqst *, struct svc_fh *, umode_t, int);
__be32	fh_verify_local(struct net *, struct svc_cred *, struct auth_domain *,
			struct svc_fh *, umode_t, int);
__be32	fh_compose(struct svc_fh *, struct svc_export *, struct dentry *, struct svc_fh *);
__be32	fh_update(struct svc_fh *);
void	fh_put(struct svc_fh *);

static __inline__ struct svc_fh *
fh_copy(struct svc_fh *dst, const struct svc_fh *src)
{
	WARN_ON(src->fh_dentry);

	*dst = *src;
	return dst;
}

static inline void
fh_copy_shallow(struct knfsd_fh *dst, const struct knfsd_fh *src)
{
	dst->fh_size = src->fh_size;
	memcpy(&dst->fh_raw, &src->fh_raw, src->fh_size);
}

static __inline__ struct svc_fh *
fh_init(struct svc_fh *fhp, int maxsize)
{
	memset(fhp, 0, sizeof(*fhp));
	fhp->fh_maxsize = maxsize;
	return fhp;
}

static inline bool fh_match(const struct knfsd_fh *fh1,
			    const struct knfsd_fh *fh2)
{
	if (fh1->fh_size != fh2->fh_size)
		return false;
	if (memcmp(fh1->fh_raw, fh2->fh_raw, fh1->fh_size) != 0)
		return false;
	return true;
}

static inline bool fh_fsid_match(const struct knfsd_fh *fh1,
				 const struct knfsd_fh *fh2)
{
	u32 *fsid1 = fh_fsid(fh1);
	u32 *fsid2 = fh_fsid(fh2);

	if (fh1->fh_fsid_type != fh2->fh_fsid_type)
		return false;
	if (memcmp(fsid1, fsid2, key_len(fh1->fh_fsid_type)) != 0)
		return false;
	return true;
}

/**
 * knfsd_fh_hash - calculate the crc32 hash for the filehandle
 * @fh - pointer to filehandle
 *
 * returns a crc32 hash for the filehandle that is compatible with
 * the one displayed by "wireshark".
 */
static inline u32 knfsd_fh_hash(const struct knfsd_fh *fh)
{
	return ~crc32_le(0xFFFFFFFF, fh->fh_raw, fh->fh_size);
}

/**
 * fh_clear_pre_post_attrs - Reset pre/post attributes
 * @fhp: file handle to be updated
 *
 */
static inline void fh_clear_pre_post_attrs(struct svc_fh *fhp)
{
	fhp->fh_post_saved = false;
	fhp->fh_pre_saved = false;
}

u64 nfsd4_change_attribute(const struct kstat *stat);
__be32 __must_check fh_fill_pre_attrs(struct svc_fh *fhp);
__be32 fh_fill_post_attrs(struct svc_fh *fhp);
__be32 __must_check fh_fill_both_attrs(struct svc_fh *fhp);
#endif /* _LINUX_NFSD_NFSFH_H */

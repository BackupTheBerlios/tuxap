/**
 * runlist.c - NTFS runlist handling code.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2004 Anton Altaparmakov
 * Copyright (c) 2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "debug.h"
#include "dir.h"
#include "endian.h"
#include "malloc.h"
#include "ntfs.h"

/**
 * ntfs_rl_mm - runlist memmove
 *
 * It is up to the caller to serialize access to the runlist @base.
 */
static inline void ntfs_rl_mm(runlist_element *base, int dst, int src,
		int size)
{
	if (likely((dst != src) && (size > 0)))
		memmove(base + dst, base + src, size * sizeof (*base));
}

/**
 * ntfs_rl_mc - runlist memory copy
 *
 * It is up to the caller to serialize access to the runlists @dstbase and
 * @srcbase.
 */
static inline void ntfs_rl_mc(runlist_element *dstbase, int dst,
		runlist_element *srcbase, int src, int size)
{
	if (likely(size > 0))
		memcpy(dstbase + dst, srcbase + src, size * sizeof(*dstbase));
}

/**
 * ntfs_rl_realloc - Reallocate memory for runlists
 * @rl:		original runlist
 * @old_size:	number of runlist elements in the original runlist @rl
 * @new_size:	number of runlist elements we need space for
 *
 * As the runlists grow, more memory will be required.  To prevent the
 * kernel having to allocate and reallocate large numbers of small bits of
 * memory, this function returns and entire page of memory.
 *
 * It is up to the caller to serialize access to the runlist @rl.
 *
 * N.B.  If the new allocation doesn't require a different number of pages in
 *       memory, the function will return the original pointer.
 *
 * On success, return a pointer to the newly allocated, or recycled, memory.
 * On error, return -errno. The following error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 */
static inline runlist_element *ntfs_rl_realloc(runlist_element *rl,
		int old_size, int new_size)
{
	runlist_element *new_rl;

	old_size = PAGE_ALIGN(old_size * sizeof(*rl));
	new_size = PAGE_ALIGN(new_size * sizeof(*rl));
	if (old_size == new_size)
		return rl;

	new_rl = ntfs_malloc_nofs(new_size);
	if (unlikely(!new_rl))
		return ERR_PTR(-ENOMEM);

	if (likely(rl != NULL)) {
		if (unlikely(old_size > new_size))
			old_size = new_size;
		memcpy(new_rl, rl, old_size);
		ntfs_free(rl);
	}
	return new_rl;
}

/**
 * ntfs_are_rl_mergeable - test if two runlists can be joined together
 * @dst:	original runlist
 * @src:	new runlist to test for mergeability with @dst
 *
 * Test if two runlists can be joined together. For this, their VCNs and LCNs
 * must be adjacent.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * Return: TRUE   Success, the runlists can be merged.
 *	   FALSE  Failure, the runlists cannot be merged.
 */
static inline BOOL ntfs_are_rl_mergeable(runlist_element *dst,
		runlist_element *src)
{
	BUG_ON(!dst);
	BUG_ON(!src);

	if ((dst->lcn < 0) || (src->lcn < 0))     /* Are we merging holes? */
		return FALSE;
	if ((dst->lcn + dst->length) != src->lcn) /* Are the runs contiguous? */
		return FALSE;
	if ((dst->vcn + dst->length) != src->vcn) /* Are the runs misaligned? */
		return FALSE;

	return TRUE;
}

/**
 * __ntfs_rl_merge - merge two runlists without testing if they can be merged
 * @dst:	original, destination runlist
 * @src:	new runlist to merge with @dst
 *
 * Merge the two runlists, writing into the destination runlist @dst. The
 * caller must make sure the runlists can be merged or this will corrupt the
 * destination runlist.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 */
static inline void __ntfs_rl_merge(runlist_element *dst, runlist_element *src)
{
	dst->length += src->length;
}

/**
 * ntfs_rl_merge - test if two runlists can be joined together and merge them
 * @dst:	original, destination runlist
 * @src:	new runlist to merge with @dst
 *
 * Test if two runlists can be joined together. For this, their VCNs and LCNs
 * must be adjacent. If they can be merged, perform the merge, writing into
 * the destination runlist @dst.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * Return: TRUE   Success, the runlists have been merged.
 *	   FALSE  Failure, the runlists cannot be merged and have not been
 *		  modified.
 */
static inline BOOL ntfs_rl_merge(runlist_element *dst, runlist_element *src)
{
	BOOL merge = ntfs_are_rl_mergeable(dst, src);

	if (merge)
		__ntfs_rl_merge(dst, src);
	return merge;
}

/**
 * ntfs_rl_append - append a runlist after a given element
 * @dst:	original runlist to be worked on
 * @dsize:	number of elements in @dst (including end marker)
 * @src:	runlist to be inserted into @dst
 * @ssize:	number of elements in @src (excluding end marker)
 * @loc:	append the new runlist @src after this element in @dst
 *
 * Append the runlist @src after element @loc in @dst.  Merge the right end of
 * the new runlist, if necessary. Adjust the size of the hole before the
 * appended runlist.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * On success, return a pointer to the new, combined, runlist. Note, both
 * runlists @dst and @src are deallocated before returning so you cannot use
 * the pointers for anything any more. (Strictly speaking the returned runlist
 * may be the same as @dst but this is irrelevant.)
 *
 * On error, return -errno. Both runlists are left unmodified. The following
 * error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 */
static inline runlist_element *ntfs_rl_append(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	BOOL right;
	int magic;

	BUG_ON(!dst);
	BUG_ON(!src);

	/* First, check if the right hand end needs merging. */
	right = ntfs_are_rl_mergeable(src + ssize - 1, dst + loc + 1);

	/* Space required: @dst size + @src size, less one if we merged. */
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize - right);
	if (IS_ERR(dst))
		return dst;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlists.
	 */

	/* First, merge the right hand end, if necessary. */
	if (right)
		__ntfs_rl_merge(src + ssize - 1, dst + loc + 1);

	magic = loc + ssize;

	/* Move the tail of @dst out of the way, then copy in @src. */
	ntfs_rl_mm(dst, magic + 1, loc + 1 + right, dsize - loc - 1 - right);
	ntfs_rl_mc(dst, loc + 1, src, 0, ssize);

	/* Adjust the size of the preceding hole. */
	dst[loc].length = dst[loc + 1].vcn - dst[loc].vcn;

	/* We may have changed the length of the file, so fix the end marker */
	if (dst[magic + 1].lcn == LCN_ENOENT)
		dst[magic + 1].vcn = dst[magic].vcn + dst[magic].length;

	return dst;
}

/**
 * ntfs_rl_insert - insert a runlist into another
 * @dst:	original runlist to be worked on
 * @dsize:	number of elements in @dst (including end marker)
 * @src:	new runlist to be inserted
 * @ssize:	number of elements in @src (excluding end marker)
 * @loc:	insert the new runlist @src before this element in @dst
 *
 * Insert the runlist @src before element @loc in the runlist @dst. Merge the
 * left end of the new runlist, if necessary. Adjust the size of the hole
 * after the inserted runlist.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * On success, return a pointer to the new, combined, runlist. Note, both
 * runlists @dst and @src are deallocated before returning so you cannot use
 * the pointers for anything any more. (Strictly speaking the returned runlist
 * may be the same as @dst but this is irrelevant.)
 *
 * On error, return -errno. Both runlists are left unmodified. The following
 * error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 */
static inline runlist_element *ntfs_rl_insert(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	BOOL left = FALSE;
	BOOL disc = FALSE;	/* Discontinuity */
	BOOL hole = FALSE;	/* Following a hole */
	int magic;

	BUG_ON(!dst);
	BUG_ON(!src);

	/* disc => Discontinuity between the end of @dst and the start of @src.
	 *	   This means we might need to insert a hole.
	 * hole => @dst ends with a hole or an unmapped region which we can
	 *	   extend to match the discontinuity. */
	if (loc == 0)
		disc = (src[0].vcn > 0);
	else {
		s64 merged_length;

		left = ntfs_are_rl_mergeable(dst + loc - 1, src);

		merged_length = dst[loc - 1].length;
		if (left)
			merged_length += src->length;

		disc = (src[0].vcn > dst[loc - 1].vcn + merged_length);
		if (disc)
			hole = (dst[loc - 1].lcn == LCN_HOLE);
	}

	/* Space required: @dst size + @src size, less one if we merged, plus
	 * one if there was a discontinuity, less one for a trailing hole. */
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize - left + disc - hole);
	if (IS_ERR(dst))
		return dst;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlist.
	 */

	if (left)
		__ntfs_rl_merge(dst + loc - 1, src);

	magic = loc + ssize - left + disc - hole;

	/* Move the tail of @dst out of the way, then copy in @src. */
	ntfs_rl_mm(dst, magic, loc, dsize - loc);
	ntfs_rl_mc(dst, loc + disc - hole, src, left, ssize - left);

	/* Adjust the VCN of the last run ... */
	if (dst[magic].lcn <= LCN_HOLE)
		dst[magic].vcn = dst[magic - 1].vcn + dst[magic - 1].length;
	/* ... and the length. */
	if (dst[magic].lcn == LCN_HOLE || dst[magic].lcn == LCN_RL_NOT_MAPPED)
		dst[magic].length = dst[magic + 1].vcn - dst[magic].vcn;

	/* Writing beyond the end of the file and there's a discontinuity. */
	if (disc) {
		if (hole)
			dst[loc - 1].length = dst[loc].vcn - dst[loc - 1].vcn;
		else {
			if (loc > 0) {
				dst[loc].vcn = dst[loc - 1].vcn +
						dst[loc - 1].length;
				dst[loc].length = dst[loc + 1].vcn -
						dst[loc].vcn;
			} else {
				dst[loc].vcn = 0;
				dst[loc].length = dst[loc + 1].vcn;
			}
			dst[loc].lcn = LCN_RL_NOT_MAPPED;
		}

		magic += hole;

		if (dst[magic].lcn == LCN_ENOENT)
			dst[magic].vcn = dst[magic - 1].vcn +
					dst[magic - 1].length;
	}
	return dst;
}

/**
 * ntfs_rl_replace - overwrite a runlist element with another runlist
 * @dst:	original runlist to be worked on
 * @dsize:	number of elements in @dst (including end marker)
 * @src:	new runlist to be inserted
 * @ssize:	number of elements in @src (excluding end marker)
 * @loc:	index in runlist @dst to overwrite with @src
 *
 * Replace the runlist element @dst at @loc with @src. Merge the left and
 * right ends of the inserted runlist, if necessary.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * On success, return a pointer to the new, combined, runlist. Note, both
 * runlists @dst and @src are deallocated before returning so you cannot use
 * the pointers for anything any more. (Strictly speaking the returned runlist
 * may be the same as @dst but this is irrelevant.)
 *
 * On error, return -errno. Both runlists are left unmodified. The following
 * error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 */
static inline runlist_element *ntfs_rl_replace(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	BOOL left = FALSE;
	BOOL right;
	int magic;

	BUG_ON(!dst);
	BUG_ON(!src);

	/* First, merge the left and right ends, if necessary. */
	right = ntfs_are_rl_mergeable(src + ssize - 1, dst + loc + 1);
	if (loc > 0)
		left = ntfs_are_rl_mergeable(dst + loc - 1, src);

	/* Allocate some space. We'll need less if the left, right, or both
	 * ends were merged. */
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize - left - right);
	if (IS_ERR(dst))
		return dst;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlists.
	 */
	if (right)
		__ntfs_rl_merge(src + ssize - 1, dst + loc + 1);
	if (left)
		__ntfs_rl_merge(dst + loc - 1, src);

	/* FIXME: What does this mean? (AIA) */
	magic = loc + ssize - left;

	/* Move the tail of @dst out of the way, then copy in @src. */
	ntfs_rl_mm(dst, magic, loc + right + 1, dsize - loc - right - 1);
	ntfs_rl_mc(dst, loc, src, left, ssize - left);

	/* We may have changed the length of the file, so fix the end marker */
	if (dst[magic].lcn == LCN_ENOENT)
		dst[magic].vcn = dst[magic - 1].vcn + dst[magic - 1].length;
	return dst;
}

/**
 * ntfs_rl_split - insert a runlist into the centre of a hole
 * @dst:	original runlist to be worked on
 * @dsize:	number of elements in @dst (including end marker)
 * @src:	new runlist to be inserted
 * @ssize:	number of elements in @src (excluding end marker)
 * @loc:	index in runlist @dst at which to split and insert @src
 *
 * Split the runlist @dst at @loc into two and insert @new in between the two
 * fragments. No merging of runlists is necessary. Adjust the size of the
 * holes either side.
 *
 * It is up to the caller to serialize access to the runlists @dst and @src.
 *
 * On success, return a pointer to the new, combined, runlist. Note, both
 * runlists @dst and @src are deallocated before returning so you cannot use
 * the pointers for anything any more. (Strictly speaking the returned runlist
 * may be the same as @dst but this is irrelevant.)
 *
 * On error, return -errno. Both runlists are left unmodified. The following
 * error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 */
static inline runlist_element *ntfs_rl_split(runlist_element *dst, int dsize,
		runlist_element *src, int ssize, int loc)
{
	BUG_ON(!dst);
	BUG_ON(!src);

	/* Space required: @dst size + @src size + one new hole. */
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize + 1);
	if (IS_ERR(dst))
		return dst;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlists.
	 */

	/* Move the tail of @dst out of the way, then copy in @src. */
	ntfs_rl_mm(dst, loc + 1 + ssize, loc, dsize - loc);
	ntfs_rl_mc(dst, loc + 1, src, 0, ssize);

	/* Adjust the size of the holes either size of @src. */
	dst[loc].length		= dst[loc+1].vcn       - dst[loc].vcn;
	dst[loc+ssize+1].vcn    = dst[loc+ssize].vcn   + dst[loc+ssize].length;
	dst[loc+ssize+1].length = dst[loc+ssize+2].vcn - dst[loc+ssize+1].vcn;

	return dst;
}

/**
 * ntfs_runlists_merge - merge two runlists into one
 * @drl:	original runlist to be worked on
 * @srl:	new runlist to be merged into @drl
 *
 * First we sanity check the two runlists @srl and @drl to make sure that they
 * are sensible and can be merged. The runlist @srl must be either after the
 * runlist @drl or completely within a hole (or unmapped region) in @drl.
 *
 * It is up to the caller to serialize access to the runlists @drl and @srl.
 *
 * Merging of runlists is necessary in two cases:
 *   1. When attribute lists are used and a further extent is being mapped.
 *   2. When new clusters are allocated to fill a hole or extend a file.
 *
 * There are four possible ways @srl can be merged. It can:
 *	- be inserted at the beginning of a hole,
 *	- split the hole in two and be inserted between the two fragments,
 *	- be appended at the end of a hole, or it can
 *	- replace the whole hole.
 * It can also be appended to the end of the runlist, which is just a variant
 * of the insert case.
 *
 * On success, return a pointer to the new, combined, runlist. Note, both
 * runlists @drl and @srl are deallocated before returning so you cannot use
 * the pointers for anything any more. (Strictly speaking the returned runlist
 * may be the same as @dst but this is irrelevant.)
 *
 * On error, return -errno. Both runlists are left unmodified. The following
 * error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EINVAL	- Invalid parameters were passed in.
 *	-ERANGE	- The runlists overlap and cannot be merged.
 */
runlist_element *ntfs_runlists_merge(runlist_element *drl,
		runlist_element *srl)
{
	int di, si;		/* Current index into @[ds]rl. */
	int sstart;		/* First index with lcn > LCN_RL_NOT_MAPPED. */
	int dins;		/* Index into @drl at which to insert @srl. */
	int dend, send;		/* Last index into @[ds]rl. */
	int dfinal, sfinal;	/* The last index into @[ds]rl with
				   lcn >= LCN_HOLE. */
	int marker = 0;
	VCN marker_vcn = 0;

#ifdef DEBUG
	ntfs_debug("dst:");
	ntfs_debug_dump_runlist(drl);
	ntfs_debug("src:");
	ntfs_debug_dump_runlist(srl);
#endif

	/* Check for silly calling... */
	if (unlikely(!srl))
		return drl;
	if (IS_ERR(srl) || IS_ERR(drl))
		return ERR_PTR(-EINVAL);

	/* Check for the case where the first mapping is being done now. */
	if (unlikely(!drl)) {
		drl = srl;
		/* Complete the source runlist if necessary. */
		if (unlikely(drl[0].vcn)) {
			/* Scan to the end of the source runlist. */
			for (dend = 0; likely(drl[dend].length); dend++)
				;
			drl = ntfs_rl_realloc(drl, dend, dend + 1);
			if (IS_ERR(drl))
				return drl;
			/* Insert start element at the front of the runlist. */
			ntfs_rl_mm(drl, 1, 0, dend);
			drl[0].vcn = 0;
			drl[0].lcn = LCN_RL_NOT_MAPPED;
			drl[0].length = drl[1].vcn;
		}
		goto finished;
	}

	si = di = 0;

	/* Skip any unmapped start element(s) in the source runlist. */
	while (srl[si].length && srl[si].lcn < LCN_HOLE)
		si++;

	/* Can't have an entirely unmapped source runlist. */
	BUG_ON(!srl[si].length);

	/* Record the starting points. */
	sstart = si;

	/*
	 * Skip forward in @drl until we reach the position where @srl needs to
	 * be inserted. If we reach the end of @drl, @srl just needs to be
	 * appended to @drl.
	 */
	for (; drl[di].length; di++) {
		if (drl[di].vcn + drl[di].length > srl[sstart].vcn)
			break;
	}
	dins = di;

	/* Sanity check for illegal overlaps. */
	if ((drl[di].vcn == srl[si].vcn) && (drl[di].lcn >= 0) &&
			(srl[si].lcn >= 0)) {
		ntfs_error(NULL, "Run lists overlap. Cannot merge!");
		return ERR_PTR(-ERANGE);
	}

	/* Scan to the end of both runlists in order to know their sizes. */
	for (send = si; srl[send].length; send++)
		;
	for (dend = di; drl[dend].length; dend++)
		;

	if (srl[send].lcn == LCN_ENOENT)
		marker_vcn = srl[marker = send].vcn;

	/* Scan to the last element with lcn >= LCN_HOLE. */
	for (sfinal = send; sfinal >= 0 && srl[sfinal].lcn < LCN_HOLE; sfinal--)
		;
	for (dfinal = dend; dfinal >= 0 && drl[dfinal].lcn < LCN_HOLE; dfinal--)
		;

	{
	BOOL start;
	BOOL finish;
	int ds = dend + 1;		/* Number of elements in drl & srl */
	int ss = sfinal - sstart + 1;

	start  = ((drl[dins].lcn <  LCN_RL_NOT_MAPPED) ||    /* End of file   */
		  (drl[dins].vcn == srl[sstart].vcn));	     /* Start of hole */
	finish = ((drl[dins].lcn >= LCN_RL_NOT_MAPPED) &&    /* End of file   */
		 ((drl[dins].vcn + drl[dins].length) <=      /* End of hole   */
		  (srl[send - 1].vcn + srl[send - 1].length)));

	/* Or we'll lose an end marker */
	if (start && finish && (drl[dins].length == 0))
		ss++;
	if (marker && (drl[dins].vcn + drl[dins].length > srl[send - 1].vcn))
		finish = FALSE;
#if 0
	ntfs_debug("dfinal = %i, dend = %i", dfinal, dend);
	ntfs_debug("sstart = %i, sfinal = %i, send = %i", sstart, sfinal, send);
	ntfs_debug("start = %i, finish = %i", start, finish);
	ntfs_debug("ds = %i, ss = %i, dins = %i", ds, ss, dins);
#endif
	if (start) {
		if (finish)
			drl = ntfs_rl_replace(drl, ds, srl + sstart, ss, dins);
		else
			drl = ntfs_rl_insert(drl, ds, srl + sstart, ss, dins);
	} else {
		if (finish)
			drl = ntfs_rl_append(drl, ds, srl + sstart, ss, dins);
		else
			drl = ntfs_rl_split(drl, ds, srl + sstart, ss, dins);
	}
	if (IS_ERR(drl)) {
		ntfs_error(NULL, "Merge failed.");
		return drl;
	}
	ntfs_free(srl);
	if (marker) {
		ntfs_debug("Triggering marker code.");
		for (ds = dend; drl[ds].length; ds++)
			;
		/* We only need to care if @srl ended after @drl. */
		if (drl[ds].vcn <= marker_vcn) {
			int slots = 0;

			if (drl[ds].vcn == marker_vcn) {
				ntfs_debug("Old marker = 0x%llx, replacing "
						"with LCN_ENOENT.",
						(unsigned long long)
						drl[ds].lcn);
				drl[ds].lcn = LCN_ENOENT;
				goto finished;
			}
			/*
			 * We need to create an unmapped runlist element in
			 * @drl or extend an existing one before adding the
			 * ENOENT terminator.
			 */
			if (drl[ds].lcn == LCN_ENOENT) {
				ds--;
				slots = 1;
			}
			if (drl[ds].lcn != LCN_RL_NOT_MAPPED) {
				/* Add an unmapped runlist element. */
				if (!slots) {
					/* FIXME/TODO: We need to have the
					 * extra memory already! (AIA) */
					drl = ntfs_rl_realloc(drl, ds, ds + 2);
					if (!drl)
						goto critical_error;
					slots = 2;
				}
				ds++;
				/* Need to set vcn if it isn't set already. */
				if (slots != 1)
					drl[ds].vcn = drl[ds - 1].vcn +
							drl[ds - 1].length;
				drl[ds].lcn = LCN_RL_NOT_MAPPED;
				/* We now used up a slot. */
				slots--;
			}
			drl[ds].length = marker_vcn - drl[ds].vcn;
			/* Finally add the ENOENT terminator. */
			ds++;
			if (!slots) {
				/* FIXME/TODO: We need to have the extra
				 * memory already! (AIA) */
				drl = ntfs_rl_realloc(drl, ds, ds + 1);
				if (!drl)
					goto critical_error;
			}
			drl[ds].vcn = marker_vcn;
			drl[ds].lcn = LCN_ENOENT;
			drl[ds].length = (s64)0;
		}
	}
	}

finished:
	/* The merge was completed successfully. */
	ntfs_debug("Merged runlist:");
	ntfs_debug_dump_runlist(drl);
	return drl;

critical_error:
	/* Critical error! We cannot afford to fail here. */
	ntfs_error(NULL, "Critical error! Not enough memory.");
	panic("NTFS: Cannot continue.");
}

/**
 * ntfs_mapping_pairs_decompress - convert mapping pairs array to runlist
 * @vol:	ntfs volume on which the attribute resides
 * @attr:	attribute record whose mapping pairs array to decompress
 * @old_rl:	optional runlist in which to insert @attr's runlist
 *
 * It is up to the caller to serialize access to the runlist @old_rl.
 *
 * Decompress the attribute @attr's mapping pairs array into a runlist. On
 * success, return the decompressed runlist.
 *
 * If @old_rl is not NULL, decompressed runlist is inserted into the
 * appropriate place in @old_rl and the resultant, combined runlist is
 * returned. The original @old_rl is deallocated.
 *
 * On error, return -errno. @old_rl is left unmodified in that case.
 *
 * The following error codes are defined:
 *	-ENOMEM	- Not enough memory to allocate runlist array.
 *	-EIO	- Corrupt runlist.
 *	-EINVAL	- Invalid parameters were passed in.
 *	-ERANGE	- The two runlists overlap.
 *
 * FIXME: For now we take the conceptionally simplest approach of creating the
 * new runlist disregarding the already existing one and then splicing the
 * two into one, if that is possible (we check for overlap and discard the new
 * runlist if overlap present before returning ERR_PTR(-ERANGE)).
 */
runlist_element *ntfs_mapping_pairs_decompress(const ntfs_volume *vol,
		const ATTR_RECORD *attr, runlist_element *old_rl)
{
	VCN vcn;		/* Current vcn. */
	LCN lcn;		/* Current lcn. */
	s64 deltaxcn;		/* Change in [vl]cn. */
	runlist_element *rl;	/* The output runlist. */
	u8 *buf;		/* Current position in mapping pairs array. */
	u8 *attr_end;		/* End of attribute. */
	int rlsize;		/* Size of runlist buffer. */
	u16 rlpos;		/* Current runlist position in units of
				   runlist_elements. */
	u8 b;			/* Current byte offset in buf. */

#ifdef DEBUG
	/* Make sure attr exists and is non-resident. */
	if (!attr || !attr->non_resident || sle64_to_cpu(
			attr->data.non_resident.lowest_vcn) < (VCN)0) {
		ntfs_error(vol->sb, "Invalid arguments.");
		return ERR_PTR(-EINVAL);
	}
#endif
	/* Start at vcn = lowest_vcn and lcn 0. */
	vcn = sle64_to_cpu(attr->data.non_resident.lowest_vcn);
	lcn = 0;
	/* Get start of the mapping pairs array. */
	buf = (u8*)attr + le16_to_cpu(
			attr->data.non_resident.mapping_pairs_offset);
	attr_end = (u8*)attr + le32_to_cpu(attr->length);
	if (unlikely(buf < (u8*)attr || buf > attr_end)) {
		ntfs_error(vol->sb, "Corrupt attribute.");
		return ERR_PTR(-EIO);
	}
	/* Current position in runlist array. */
	rlpos = 0;
	/* Allocate first page and set current runlist size to one page. */
	rl = ntfs_malloc_nofs(rlsize = PAGE_SIZE);
	if (unlikely(!rl))
		return ERR_PTR(-ENOMEM);
	/* Insert unmapped starting element if necessary. */
	if (vcn) {
		rl->vcn = 0;
		rl->lcn = LCN_RL_NOT_MAPPED;
		rl->length = vcn;
		rlpos++;
	}
	while (buf < attr_end && *buf) {
		/*
		 * Allocate more memory if needed, including space for the
		 * not-mapped and terminator elements. ntfs_malloc_nofs()
		 * operates on whole pages only.
		 */
		if (((rlpos + 3) * sizeof(*old_rl)) > rlsize) {
			runlist_element *rl2;

			rl2 = ntfs_malloc_nofs(rlsize + (int)PAGE_SIZE);
			if (unlikely(!rl2)) {
				ntfs_free(rl);
				return ERR_PTR(-ENOMEM);
			}
			memcpy(rl2, rl, rlsize);
			ntfs_free(rl);
			rl = rl2;
			rlsize += PAGE_SIZE;
		}
		/* Enter the current vcn into the current runlist element. */
		rl[rlpos].vcn = vcn;
		/*
		 * Get the change in vcn, i.e. the run length in clusters.
		 * Doing it this way ensures that we signextend negative values.
		 * A negative run length doesn't make any sense, but hey, I
		 * didn't make up the NTFS specs and Windows NT4 treats the run
		 * length as a signed value so that's how it is...
		 */
		b = *buf & 0xf;
		if (b) {
			if (unlikely(buf + b > attr_end))
				goto io_error;
			for (deltaxcn = (s8)buf[b--]; b; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
		} else { /* The length entry is compulsory. */
			ntfs_error(vol->sb, "Missing length entry in mapping "
					"pairs array.");
			deltaxcn = (s64)-1;
		}
		/*
		 * Assume a negative length to indicate data corruption and
		 * hence clean-up and return NULL.
		 */
		if (unlikely(deltaxcn < 0)) {
			ntfs_error(vol->sb, "Invalid length in mapping pairs "
					"array.");
			goto err_out;
		}
		/*
		 * Enter the current run length into the current runlist
		 * element.
		 */
		rl[rlpos].length = deltaxcn;
		/* Increment the current vcn by the current run length. */
		vcn += deltaxcn;
		/*
		 * There might be no lcn change at all, as is the case for
		 * sparse clusters on NTFS 3.0+, in which case we set the lcn
		 * to LCN_HOLE.
		 */
		if (!(*buf & 0xf0))
			rl[rlpos].lcn = LCN_HOLE;
		else {
			/* Get the lcn change which really can be negative. */
			u8 b2 = *buf & 0xf;
			b = b2 + ((*buf >> 4) & 0xf);
			if (buf + b > attr_end)
				goto io_error;
			for (deltaxcn = (s8)buf[b--]; b > b2; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
			/* Change the current lcn to its new value. */
			lcn += deltaxcn;
#ifdef DEBUG
			/*
			 * On NTFS 1.2-, apparently can have lcn == -1 to
			 * indicate a hole. But we haven't verified ourselves
			 * whether it is really the lcn or the deltaxcn that is
			 * -1. So if either is found give us a message so we
			 * can investigate it further!
			 */
			if (vol->major_ver < 3) {
				if (unlikely(deltaxcn == (LCN)-1))
					ntfs_error(vol->sb, "lcn delta == -1");
				if (unlikely(lcn == (LCN)-1))
					ntfs_error(vol->sb, "lcn == -1");
			}
#endif
			/* Check lcn is not below -1. */
			if (unlikely(lcn < (LCN)-1)) {
				ntfs_error(vol->sb, "Invalid LCN < -1 in "
						"mapping pairs array.");
				goto err_out;
			}
			/* Enter the current lcn into the runlist element. */
			rl[rlpos].lcn = lcn;
		}
		/* Get to the next runlist element. */
		rlpos++;
		/* Increment the buffer position to the next mapping pair. */
		buf += (*buf & 0xf) + ((*buf >> 4) & 0xf) + 1;
	}
	if (unlikely(buf >= attr_end))
		goto io_error;
	/*
	 * If there is a highest_vcn specified, it must be equal to the final
	 * vcn in the runlist - 1, or something has gone badly wrong.
	 */
	deltaxcn = sle64_to_cpu(attr->data.non_resident.highest_vcn);
	if (unlikely(deltaxcn && vcn - 1 != deltaxcn)) {
mpa_err:
		ntfs_error(vol->sb, "Corrupt mapping pairs array in "
				"non-resident attribute.");
		goto err_out;
	}
	/* Setup not mapped runlist element if this is the base extent. */
	if (!attr->data.non_resident.lowest_vcn) {
		VCN max_cluster;

		max_cluster = (sle64_to_cpu(
				attr->data.non_resident.allocated_size) +
				vol->cluster_size - 1) >>
				vol->cluster_size_bits;
		/*
		 * If there is a difference between the highest_vcn and the
		 * highest cluster, the runlist is either corrupt or, more
		 * likely, there are more extents following this one.
		 */
		if (deltaxcn < --max_cluster) {
			ntfs_debug("More extents to follow; deltaxcn = 0x%llx, "
					"max_cluster = 0x%llx",
					(unsigned long long)deltaxcn,
					(unsigned long long)max_cluster);
			rl[rlpos].vcn = vcn;
			vcn += rl[rlpos].length = max_cluster - deltaxcn;
			rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
			rlpos++;
		} else if (unlikely(deltaxcn > max_cluster)) {
			ntfs_error(vol->sb, "Corrupt attribute. deltaxcn = "
					"0x%llx, max_cluster = 0x%llx",
					(unsigned long long)deltaxcn,
					(unsigned long long)max_cluster);
			goto mpa_err;
		}
		rl[rlpos].lcn = LCN_ENOENT;
	} else /* Not the base extent. There may be more extents to follow. */
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;

	/* Setup terminating runlist element. */
	rl[rlpos].vcn = vcn;
	rl[rlpos].length = (s64)0;
	/* If no existing runlist was specified, we are done. */
	if (!old_rl) {
		ntfs_debug("Mapping pairs array successfully decompressed:");
		ntfs_debug_dump_runlist(rl);
		return rl;
	}
	/* Now combine the new and old runlists checking for overlaps. */
	old_rl = ntfs_runlists_merge(old_rl, rl);
	if (likely(!IS_ERR(old_rl)))
		return old_rl;
	ntfs_free(rl);
	ntfs_error(vol->sb, "Failed to merge runlists.");
	return old_rl;
io_error:
	ntfs_error(vol->sb, "Corrupt attribute.");
err_out:
	ntfs_free(rl);
	return ERR_PTR(-EIO);
}

/**
 * ntfs_rl_vcn_to_lcn - convert a vcn into a lcn given a runlist
 * @rl:		runlist to use for conversion
 * @vcn:	vcn to convert
 *
 * Convert the virtual cluster number @vcn of an attribute into a logical
 * cluster number (lcn) of a device using the runlist @rl to map vcns to their
 * corresponding lcns.
 *
 * It is up to the caller to serialize access to the runlist @rl.
 *
 * Since lcns must be >= 0, we use negative return values with special meaning:
 *
 * Return value			Meaning / Description
 * ==================================================
 *  -1 = LCN_HOLE		Hole / not allocated on disk.
 *  -2 = LCN_RL_NOT_MAPPED	This is part of the runlist which has not been
 *				inserted into the runlist yet.
 *  -3 = LCN_ENOENT		There is no such vcn in the attribute.
 *
 * Locking: - The caller must have locked the runlist (for reading or writing).
 *	    - This function does not touch the lock.
 */
LCN ntfs_rl_vcn_to_lcn(const runlist_element *rl, const VCN vcn)
{
	int i;

	BUG_ON(vcn < 0);
	/*
	 * If rl is NULL, assume that we have found an unmapped runlist. The
	 * caller can then attempt to map it and fail appropriately if
	 * necessary.
	 */
	if (unlikely(!rl))
		return LCN_RL_NOT_MAPPED;

	/* Catch out of lower bounds vcn. */
	if (unlikely(vcn < rl[0].vcn))
		return LCN_ENOENT;

	for (i = 0; likely(rl[i].length); i++) {
		if (unlikely(vcn < rl[i+1].vcn)) {
			if (likely(rl[i].lcn >= (LCN)0))
				return rl[i].lcn + (vcn - rl[i].vcn);
			return rl[i].lcn;
		}
	}
	/*
	 * The terminator element is setup to the correct value, i.e. one of
	 * LCN_HOLE, LCN_RL_NOT_MAPPED, or LCN_ENOENT.
	 */
	if (likely(rl[i].lcn < (LCN)0))
		return rl[i].lcn;
	/* Just in case... We could replace this with BUG() some day. */
	return LCN_ENOENT;
}

/**
 * ntfs_get_nr_significant_bytes - get number of bytes needed to store a number
 * @n:		number for which to get the number of bytes for
 *
 * Return the number of bytes required to store @n unambiguously as
 * a signed number.
 *
 * This is used in the context of the mapping pairs array to determine how
 * many bytes will be needed in the array to store a given logical cluster
 * number (lcn) or a specific run length.
 *
 * Return the number of bytes written.  This function cannot fail.
 */
static inline int ntfs_get_nr_significant_bytes(const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (n >> 8 * (i - 1)) & 0xff;
	/* If the sign bit is wrong, we need an extra byte. */
	if ((n < 0 && j >= 0) || (n > 0 && j < 0))
		i++;
	return i;
}

/**
 * ntfs_get_size_for_mapping_pairs - get bytes needed for mapping pairs array
 * @vol:	ntfs volume (needed for the ntfs version)
 * @rl:		locked runlist to determine the size of the mapping pairs of
 * @start_vcn:	vcn at which to start the mapping pairs array
 *
 * Walk the locked runlist @rl and calculate the size in bytes of the mapping
 * pairs array corresponding to the runlist @rl, starting at vcn @start_vcn.
 * This for example allows us to allocate a buffer of the right size when
 * building the mapping pairs array.
 *
 * If @rl is NULL, just return 1 (for the single terminator byte).
 *
 * Return the calculated size in bytes on success.  On error, return -errno.
 * The following error codes are defined:
 *	-EINVAL	- Run list contains unmapped elements.  Make sure to only pass
 *		  fully mapped runlists to this function.
 *	-EIO	- The runlist is corrupt.
 *
 * Locking: @rl must be locked on entry (either for reading or writing), it
 *	    remains locked throughout, and is left locked upon return.
 */
int ntfs_get_size_for_mapping_pairs(const ntfs_volume *vol,
		const runlist_element *rl, const VCN start_vcn)
{
	LCN prev_lcn;
	int rls;

	BUG_ON(start_vcn < 0);
	if (!rl) {
		BUG_ON(start_vcn);
		return 1;
	}
	/* Skip to runlist element containing @start_vcn. */
	while (rl->length && start_vcn >= rl[1].vcn)
		rl++;
	if ((!rl->length && start_vcn > rl->vcn) || start_vcn < rl->vcn)
		return -EINVAL;
	prev_lcn = 0;
	/* Always need the termining zero byte. */
	rls = 1;
	/* Do the first partial run if present. */
	if (start_vcn > rl->vcn) {
		s64 delta;

		/* We know rl->length != 0 already. */
		if (rl->length < 0 || rl->lcn < LCN_HOLE)
			goto err_out;
		delta = start_vcn - rl->vcn;
		/* Header byte + length. */
		rls += 1 + ntfs_get_nr_significant_bytes(rl->length - delta);
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just store the lcn.
		 * Note: this assumes that on NTFS 1.2-, holes are stored with
		 * an lcn of -1 and not a delta_lcn of -1 (unless both are -1).
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			prev_lcn = rl->lcn;
			if (rl->lcn >= 0)
				prev_lcn += delta;
			/* Change in lcn. */
			rls += ntfs_get_nr_significant_bytes(prev_lcn);
		}
		/* Go to next runlist element. */
		rl++;
	}
	/* Do the full runs. */
	for (; rl->length; rl++) {
		if (rl->length < 0 || rl->lcn < LCN_HOLE)
			goto err_out;
		/* Header byte + length. */
		rls += 1 + ntfs_get_nr_significant_bytes(rl->length);
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just store the lcn.
		 * Note: this assumes that on NTFS 1.2-, holes are stored with
		 * an lcn of -1 and not a delta_lcn of -1 (unless both are -1).
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			/* Change in lcn. */
			rls += ntfs_get_nr_significant_bytes(rl->lcn -
					prev_lcn);
			prev_lcn = rl->lcn;
		}
	}
	return rls;
err_out:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		rls = -EINVAL;
	else
		rls = -EIO;
	return rls;
}

/**
 * ntfs_write_significant_bytes - write the significant bytes of a number
 * @dst:	destination buffer to write to
 * @dst_max:	pointer to last byte of destination buffer for bounds checking
 * @n:		number whose significant bytes to write
 *
 * Store in @dst, the minimum bytes of the number @n which are required to
 * identify @n unambiguously as a signed number, taking care not to exceed
 * @dest_max, the maximum position within @dst to which we are allowed to
 * write.
 *
 * This is used when building the mapping pairs array of a runlist to compress
 * a given logical cluster number (lcn) or a specific run length to the minumum
 * size possible.
 *
 * Return the number of bytes written on success.  On error, i.e. the
 * destination buffer @dst is too small, return -ENOSPC.
 */
static inline int ntfs_write_significant_bytes(s8 *dst, const s8 *dst_max,
		const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		if (dst > dst_max)
			goto err_out;
		*dst++ = l & 0xffll;
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (n >> 8 * (i - 1)) & 0xff;
	/* If the sign bit is wrong, we need an extra byte. */
	if (n < 0 && j >= 0) {
		if (dst > dst_max)
			goto err_out;
		i++;
		*dst = (s8)-1;
	} else if (n > 0 && j < 0) {
		if (dst > dst_max)
			goto err_out;
		i++;
		*dst = (s8)0;
	}
	return i;
err_out:
	return -ENOSPC;
}

/**
 * ntfs_mapping_pairs_build - build the mapping pairs array from a runlist
 * @vol:	ntfs volume (needed for the ntfs version)
 * @dst:	destination buffer to which to write the mapping pairs array
 * @dst_len:	size of destination buffer @dst in bytes
 * @rl:		locked runlist for which to build the mapping pairs array
 * @start_vcn:	vcn at which to start the mapping pairs array
 * @stop_vcn:	first vcn outside destination buffer on success or -ENOSPC
 *
 * Create the mapping pairs array from the locked runlist @rl, starting at vcn
 * @start_vcn and save the array in @dst.  @dst_len is the size of @dst in
 * bytes and it should be at least equal to the value obtained by calling
 * ntfs_get_size_for_mapping_pairs().
 *
 * If @rl is NULL, just write a single terminator byte to @dst.
 *
 * On success or -ENOSPC error, if @stop_vcn is not NULL, *@stop_vcn is set to
 * the first vcn outside the destination buffer.  Note that on error, @dst has
 * been filled with all the mapping pairs that will fit, thus it can be treated
 * as partial success, in that a new attribute extent needs to be created or
 * the next extent has to be used and the mapping pairs build has to be
 * continued with @start_vcn set to *@stop_vcn.
 *
 * Return 0 on success and -errno on error.  The following error codes are
 * defined:
 *	-EINVAL	- Run list contains unmapped elements.  Make sure to only pass
 *		  fully mapped runlists to this function.
 *	-EIO	- The runlist is corrupt.
 *	-ENOSPC	- The destination buffer is too small.
 *
 * Locking: @rl must be locked on entry (either for reading or writing), it
 *	    remains locked throughout, and is left locked upon return.
 */
int ntfs_mapping_pairs_build(const ntfs_volume *vol, s8 *dst,
		const int dst_len, const runlist_element *rl,
		const VCN start_vcn, VCN *const stop_vcn)
{
	LCN prev_lcn;
	s8 *dst_max, *dst_next;
	int err = -ENOSPC;
	s8 len_len, lcn_len;

	BUG_ON(start_vcn < 0);
	BUG_ON(dst_len < 1);
	if (!rl) {
		BUG_ON(start_vcn);
		if (stop_vcn)
			*stop_vcn = 0;
		/* Terminator byte. */
		*dst = 0;
		return 0;
	}
	/* Skip to runlist element containing @start_vcn. */
	while (rl->length && start_vcn >= rl[1].vcn)
		rl++;
	if ((!rl->length && start_vcn > rl->vcn) || start_vcn < rl->vcn)
		return -EINVAL;
	/*
	 * @dst_max is used for bounds checking in
	 * ntfs_write_significant_bytes().
	 */
	dst_max = dst + dst_len - 1;
	prev_lcn = 0;
	/* Do the first partial run if present. */
	if (start_vcn > rl->vcn) {
		s64 delta;

		/* We know rl->length != 0 already. */
		if (rl->length < 0 || rl->lcn < LCN_HOLE)
			goto err_out;
		delta = start_vcn - rl->vcn;
		/* Write length. */
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				rl->length - delta);
		if (len_len < 0)
			goto size_err;
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just write the lcn
		 * change.  FIXME: Do we need to write the lcn change or just
		 * the lcn in that case?  Not sure as I have never seen this
		 * case on NT4. - We assume that we just need to write the lcn
		 * change until someone tells us otherwise... (AIA)
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			prev_lcn = rl->lcn;
			if (rl->lcn >= 0)
				prev_lcn += delta;
			/* Write change in lcn. */
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, prev_lcn);
			if (lcn_len < 0)
				goto size_err;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (dst_next > dst_max)
			goto size_err;
		/* Update header byte. */
		*dst = lcn_len << 4 | len_len;
		/* Position at next mapping pairs array element. */
		dst = dst_next;
		/* Go to next runlist element. */
		rl++;
	}
	/* Do the full runs. */
	for (; rl->length; rl++) {
		if (rl->length < 0 || rl->lcn < LCN_HOLE)
			goto err_out;
		/* Write length. */
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				rl->length);
		if (len_len < 0)
			goto size_err;
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just write the lcn
		 * change.  FIXME: Do we need to write the lcn change or just
		 * the lcn in that case?  Not sure as I have never seen this
		 * case on NT4. - We assume that we just need to write the lcn
		 * change until someone tells us otherwise... (AIA)
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			/* Write change in lcn. */
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, rl->lcn - prev_lcn);
			if (lcn_len < 0)
				goto size_err;
			prev_lcn = rl->lcn;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (dst_next > dst_max)
			goto size_err;
		/* Update header byte. */
		*dst = lcn_len << 4 | len_len;
		/* Position at next mapping pairs array element. */
		dst = dst_next;
	}
	/* Success. */
	err = 0;
size_err:
	/* Set stop vcn. */
	if (stop_vcn)
		*stop_vcn = rl->vcn;
	/* Add terminator byte. */
	*dst = 0;
	return err;
err_out:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		err = -EINVAL;
	else
		err = -EIO;
	return err;
}

/**
 * ntfs_rl_truncate_nolock - truncate a runlist starting at a specified vcn
 * @runlist:	runlist to truncate
 * @new_length:	the new length of the runlist in VCNs
 *
 * Truncate the runlist described by @runlist as well as the memory buffer
 * holding the runlist elements to a length of @new_length VCNs.
 *
 * If @new_length lies within the runlist, the runlist elements with VCNs of
 * @new_length and above are discarded.
 *
 * If @new_length lies beyond the runlist, a sparse runlist element is added to
 * the end of the runlist @runlist or if the last runlist element is a sparse
 * one already, this is extended.
 *
 * Return 0 on success and -errno on error.
 *
 * Locking: The caller must hold @runlist->lock for writing.
 */
int ntfs_rl_truncate_nolock(const ntfs_volume *vol, runlist *const runlist,
		const s64 new_length)
{
	runlist_element *rl;
	int old_size;

	ntfs_debug("Entering for new_length 0x%llx.", (long long)new_length);
	BUG_ON(!runlist);
	BUG_ON(new_length < 0);
	rl = runlist->rl;
	if (unlikely(!rl)) {
		/*
		 * Create a runlist consisting of a sparse runlist element of
		 * length @new_length followed by a terminator runlist element.
		 */
		rl = ntfs_malloc_nofs(PAGE_SIZE);
		if (unlikely(!rl)) {
			ntfs_error(vol->sb, "Not enough memory to allocate "
					"runlist element buffer.");
			return -ENOMEM;
		}
		runlist->rl = rl;
		rl[1].length = rl->vcn = 0;
		rl->lcn = LCN_HOLE;
		rl[1].vcn = rl->length = new_length;
		rl[1].lcn = LCN_ENOENT;
		return 0;
	}
	BUG_ON(new_length < rl->vcn);
	/* Find @new_length in the runlist. */
	while (likely(rl->length && new_length >= rl[1].vcn))
		rl++;
	/*
	 * If not at the end of the runlist we need to shrink it.
	 * If at the end of the runlist we need to expand it.
	 */
	if (rl->length) {
		runlist_element *trl;
		BOOL is_end;

		ntfs_debug("Shrinking runlist.");
		/* Determine the runlist size. */
		trl = rl + 1;
		while (likely(trl->length))
			trl++;
		old_size = trl - runlist->rl + 1;
		/* Truncate the run. */
		rl->length = new_length - rl->vcn;
		/*
		 * If a run was partially truncated, make the following runlist
		 * element a terminator.
		 */
		is_end = FALSE;
		if (rl->length) {
			rl++;
			if (!rl->length)
				is_end = TRUE;
			rl->vcn = new_length;
			rl->length = 0;
		}
		rl->lcn = LCN_ENOENT;
		/* Reallocate memory if necessary. */
		if (!is_end) {
			int new_size = rl - runlist->rl + 1;
			rl = ntfs_rl_realloc(runlist->rl, old_size, new_size);
			if (IS_ERR(rl))
				ntfs_warning(vol->sb, "Failed to shrink "
						"runlist buffer.  This just "
						"wastes a bit of memory "
						"temporarily so we ignore it "
						"and return success.");
			else
				runlist->rl = rl;
		}
	} else if (likely(/* !rl->length && */ new_length > rl->vcn)) {
		ntfs_debug("Expanding runlist.");
		/*
		 * If there is a previous runlist element and it is a sparse
		 * one, extend it.  Otherwise need to add a new, sparse runlist
		 * element.
		 */
		if ((rl > runlist->rl) && ((rl - 1)->lcn == LCN_HOLE))
			(rl - 1)->length = new_length - (rl - 1)->vcn;
		else {
			/* Determine the runlist size. */
			old_size = rl - runlist->rl + 1;
			/* Reallocate memory if necessary. */
			rl = ntfs_rl_realloc(runlist->rl, old_size,
					old_size + 1);
			if (IS_ERR(rl)) {
				ntfs_error(vol->sb, "Failed to expand runlist "
						"buffer, aborting.");
				return PTR_ERR(rl);
			}
			runlist->rl = rl;
			/*
			 * Set @rl to the same runlist element in the new
			 * runlist as before in the old runlist.
			 */
			rl += old_size - 1;
			/* Add a new, sparse runlist element. */
			rl->lcn = LCN_HOLE;
			rl->length = new_length - rl->vcn;
			/* Add a new terminator runlist element. */
			rl++;
			rl->length = 0;
		}
		rl->vcn = new_length;
		rl->lcn = LCN_ENOENT;
	} else /* if (unlikely(!rl->length && new_length == rl->vcn)) */ {
		/* Runlist already has same size as requested. */
		rl->lcn = LCN_ENOENT;
	}
	ntfs_debug("Done.");
	return 0;
}

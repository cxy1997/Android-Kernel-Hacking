/* drivers/mtd/devices/goldfish_nand_cache.h
**
** Copyright (C) 2015 Google, Inc.
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
*/

#ifndef GOLDFISH_NAND_CACHE_H
#define GOLDFISH_NAND_CACHE_H

#include <linux/mtd/mtd.h>

struct goldfish_nand_cache;

/* Allocate a software cache in driver's non-pageable memory for the given
 * Goldfish device. Return true on success.
 * This function is not thread-protected. The caller is responsible to make
 * sure no more than one thread is allocating/deallocating cache at a time.
 */
bool goldfish_allocate_cache(struct mtd_info *device);

/* Release the cache allocated by goldfish_allocate_cache() for the given
 * Goldfish device.
 * This function is not thread-protected. The caller is responsible to make
 * sure no more than one thread is allocating/deallocating cache at a time.
 */
void goldfish_deallocate_cache(struct mtd_info *device);

/* Return the cache allocated by goldfish_allocate_cache() for the given
 * Goldfish device. Return NULL if the cache doesn't exist.
 */
struct goldfish_nand_cache *goldfish_locate_nand_cache(
	struct mtd_info *device);

/* Invalidate the cache for the given |device| if needed when the data at
 * address |from| of |length| bytes becomes dirty.
 */
void goldfish_invalidate_cache(struct mtd_info *device, loff_t from,
			       size_t length);

/* Return the number of bytes that the given |cache| should fetch in next
 * refresh.
 */
size_t goldfish_get_cache_refresh_size(struct goldfish_nand_cache *cache);

/* Return the pointer to the data buffer of the given |cache|. */
u_char *goldfish_get_cache_buffer(struct goldfish_nand_cache *cache);

/* Update state of the given |cache| that it holds a valid copy of data at
 * address |from| of |length| bytes.
 */
bool goldfish_update_refreshed_cache(struct goldfish_nand_cache *cache,
				     loff_t from, size_t length);

/* Read data at address |from| of |length| bytes from the given |cache|.
 * Return true on success, and if success, update |buffer| with the data from
 * the cache and set |read_length| to the number of bytes that have been read.
 */
bool goldfish_read_from_cache(struct goldfish_nand_cache *cache, loff_t from,
			      size_t length, size_t *read_length,
			      u_char *buffer);

#endif  /* GOLDFISH_NAND_CACHE_H */

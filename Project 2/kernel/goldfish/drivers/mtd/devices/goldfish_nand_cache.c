/* drivers/mtd/devices/goldfish_nand_cache.C
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

#include <linux/gfp.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "goldfish_nand_cache.h"

#define CACHE_CAPACITY              (128 * 1024)  /* 128 KB. */

/* Use an adaptive two-level cache refresh policy.
 * Fetch 128 KB if the miss is caused by a continuous read following
 * what's already in the cache. Otherwise, fetch 4 KB.
 */
#define CACHE_REFRESH_LEVEL_NUM     2
size_t cache_refresh_sizes[CACHE_REFRESH_LEVEL_NUM] = {
	(4 * 1024),
	(128 * 1024)
};

/* A cache serves read from kernel.
 * It's invisible to the file system. It does not require any change to the
 * Goldfish NAND device in the emulator, either. However it is possible for the
 * emulator / virtual device to detect the existence of this cache through the
 * pattern of read requests.
 */
struct goldfish_nand_cache {
	struct mtd_info *device;

	/* The buffer holding cached data.
	 * It is allocated separately from this struct because when making the
	 * buffer as an array inside this struct, kmalloc() will add excessive
	 * padding, for example, allocating 256 KB for this struct if asked
	 * 128 KB for the buffer.
	 */
	u_char *data;

	loff_t start_addr;
	size_t size;
	bool valid;

	/* Support multiple-level cache refreshing policy. */
	unsigned refresh_level;

	struct list_head list;
};

/* The list of caches in the system.
 * This list itself is not thread-protected. It is manipulated by
 * goldfish_allocate_cache() and goldfish_deallocate_cache(). The caller of
 * the two functions is responsible to make sure no more than one thread is
 * changing the list at a time.
 */
static LIST_HEAD(cache_list);

/* Return a pointer to data at address |from| of |length| if it is present
 * in |cache|. Return NULL if it is not in the cache.
 */
static inline u_char *GoldfishLookupInCache(
	struct goldfish_nand_cache *cache, loff_t from, size_t length)
{
	/* Note cache->start_addr + cache->size - 1 is the index of the last
	 * item in the cache. from + length - 1 is the index of the last item
	 * being sought.
	 */
	if (cache != NULL &&
		cache->valid &&
		cache->start_addr <= from &&
		cache->start_addr + cache->size - 1 >= from + length - 1) {
		return cache->data + (from - cache->start_addr);
	} else {
		return NULL;
	}
}

bool goldfish_allocate_cache(struct mtd_info *device)
{
	struct goldfish_nand_cache *cache = goldfish_locate_nand_cache(device);
	u_char *buffer = NULL;

	if (cache == NULL) {
		cache =  kmalloc(sizeof(struct goldfish_nand_cache)
,				 GFP_KERNEL | GFP_DMA);
		buffer =  kmalloc(CACHE_CAPACITY, GFP_KERNEL | GFP_DMA);
		if (cache == NULL || buffer == NULL) {
			pr_err("Cannot create a cache for Goldfish NAND.\n");
			if (cache != NULL)
				kfree(cache);
			if (buffer != NULL)
				kfree(buffer);
			return false;
		}
		cache->device = device;
		cache->valid = false;
		cache->data = buffer;
		list_add(&(cache->list), &cache_list);
		return true;
	}
	pr_err("goldfish_allocate_cache: cache already exists.\n");
	return false;
}

void goldfish_deallocate_cache(struct mtd_info *device)
{
	struct goldfish_nand_cache *cache = goldfish_locate_nand_cache(device);
	if (cache != NULL) {
		if (cache->data != NULL)
			kfree(cache->data);
		list_del(&(cache->list));
		kfree(cache);
	}
}

struct goldfish_nand_cache *goldfish_locate_nand_cache(struct mtd_info *device)
{
	struct goldfish_nand_cache *cache = NULL;
	list_for_each_entry(cache, &cache_list, list) {
		if (cache->device == device)
			return cache;
	}
	return NULL;
}

void goldfish_invalidate_cache(
	struct mtd_info *device, loff_t from, size_t length)
{
	struct goldfish_nand_cache *cache = goldfish_locate_nand_cache(device);
	if (cache != NULL &&
		cache->valid &&
		cache->start_addr <= from + length - 1 &&
		cache->start_addr + cache->size - 1 >= from) {
		cache->valid = false;
	}
}

size_t goldfish_get_cache_refresh_size(struct goldfish_nand_cache *cache)
{
	if (cache->refresh_level < CACHE_REFRESH_LEVEL_NUM) {
		return cache_refresh_sizes[cache->refresh_level];
	} else {
		pr_err("%s: unexpected refresh level.\n", __func__);
		return 0;
	}
}

u_char *goldfish_get_cache_buffer(struct goldfish_nand_cache *cache)
{
	if (cache != NULL)
		return cache->data;
	return NULL;
}


bool goldfish_update_refreshed_cache(struct goldfish_nand_cache *cache,
				     loff_t from, size_t length)
{
	if (cache != NULL) {
		cache->start_addr = from;
		cache->size = length;
		cache->valid = true;
		return true;
	}
	return false;
}

bool goldfish_read_from_cache(struct goldfish_nand_cache *cache, loff_t from,
			      size_t length, size_t *read_length,
			      u_char *buffer)
{
	u_char *cached = GoldfishLookupInCache(cache, from, length);
	if (cached != NULL) {
		memcpy(buffer, cached, length);
		*read_length = length;
		return true;
	} else {
		/* Be smart in refreshing cache. */
		if (cache->valid && cache->start_addr + cache->size == from) {
			/* Continuous read from what's already in the cache. */
			if (cache->refresh_level < CACHE_REFRESH_LEVEL_NUM - 1)
				cache->refresh_level++;
		} else {
			/* Not continuous read. Reset. */
			cache->refresh_level = 0;
		}
		return false;
	}
}

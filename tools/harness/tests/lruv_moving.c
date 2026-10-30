/*
LRUV_MOVING.C (test)

The real lruv_block_new and lruv_block_new_moving (lruv_cache.c) on a cache of
64 pages of 16 bytes whose memory is an array here: a request that no run of
pages unused this tick can hold is placed by moving this tick's blocks, with
their bytes, and never a locked block. test_lruv_moving.py takes the
structures, constants and code (under_test.inc).
*/

#include "harness.h"

#pragma clang diagnostic ignored "-Wmultichar"

#undef match_assert
#define match_assert(file, line, condition) CHECK(condition, "assertion")
#define datum_try_and_get datum_get
#define data_verify(array) ((void)(array))
#define actual_count count

struct data_iterator
{
	struct data_array *data;
	short absolute_index;
	long datum_index;
};

static void data_iterator_new(struct data_iterator *iterator, struct data_array *data)
{
	iterator->data = data;
	iterator->absolute_index = -1;
	iterator->datum_index = NONE;
}

static void *data_iterator_next(struct data_iterator *iterator)
{
	while (++iterator->absolute_index < iterator->data->maximum_count)
	{
		struct datum_header *header = (struct datum_header *)(iterator->data->data +
			iterator->absolute_index * iterator->data->size);

		if (header->identifier)
		{
			iterator->datum_index = ((long)(unsigned short)header->identifier << 16) | iterator->absolute_index;
			return header;
		}
	}
	return NULL;
}

#include "under_test.inc"

enum
{
	PAGE_COUNT = 64,
	PAGE_SIZE_BITS = 4,
	PAGE_SIZE = 1 << PAGE_SIZE_BITS,
	MAXIMUM_BLOCKS = 64
};

static unsigned char memory[PAGE_COUNT * PAGE_SIZE];
static long locked_block;
static long moves, deletes, lock_queries;

static boolean locked(long block_index)
{
	lock_queries++;
	return block_index == locked_block;
}

static void deleted(long block_index)
{
	deletes++;
}

static void moved(long block_index, unsigned long old_offset, unsigned long new_offset, long size)
{
	CHECK(new_offset + size <= sizeof(memory) && old_offset + size <= sizeof(memory), "moved outside the memory");
	memcpy(memory + new_offset, memory + old_offset, (size_t)size);
	moves++;
}

static struct lruv_cache *cache_new(void)
{
	struct lruv_cache *cache = calloc(1, sizeof(*cache));

	cache->locked_block_proc = locked;
	cache->delete_block_proc = deleted;
	cache->page_count = PAGE_COUNT;
	cache->page_size_bits = PAGE_SIZE_BITS;
	cache->blocks = game_state_data_new("lruv test", MAXIMUM_BLOCKS, sizeof(struct lruv_cache_block));
	cache->signature = LRUV_CACHE_SIGNATURE;
	cache->first_block_index = NONE;
	cache->last_block_index = NONE;
	cache->tick = 10;
	locked_block = NONE;
	moves = deletes = lock_queries = 0;
	return cache;
}

/* a block at first_page_index, after every block placed before it, its bytes
its own mark */
static long place(struct lruv_cache *cache, long first_page_index, long page_count, long last_used_tick)
{
	long block_index = datum_new(cache->blocks);
	struct lruv_cache_block *block = datum_get(cache->blocks, block_index);

	block->first_page_index = first_page_index;
	block->page_count = page_count;
	block->last_used_tick = last_used_tick;
	block->next_block_index = NONE;
	block->previous_block_index = cache->last_block_index;
	if (cache->last_block_index == NONE)
		cache->first_block_index = block_index;
	else
		((struct lruv_cache_block *)datum_get(cache->blocks, cache->last_block_index))->next_block_index = block_index;
	cache->last_block_index = block_index;
	memset(memory + (first_page_index << PAGE_SIZE_BITS), (int)(block_index & 0x7F) + 1,
		(size_t)(page_count << PAGE_SIZE_BITS));
	return block_index;
}

static boolean intact(struct lruv_cache *cache, long block_index)
{
	struct lruv_cache_block *block = datum_get(cache->blocks, block_index);
	long byte_index;

	for (byte_index = 0; byte_index < block->page_count << PAGE_SIZE_BITS; byte_index++)
	{
		if (memory[(block->first_page_index << PAGE_SIZE_BITS) + byte_index] != (block_index & 0x7F) + 1)
			return FALSE;
	}
	return TRUE;
}

/* 8 rows of 8 pages: 2 used this tick, then 6 unused (3 blocks of 2), so
48 pages are unused and no 7 of them in a row */
static void fill(struct lruv_cache *cache, long used[8])
{
	long row;

	for (row = 0; row < 8; row++)
	{
		used[row] = place(cache, row * 8, 2, cache->tick);
		place(cache, row * 8 + 2, 2, cache->tick - 1);
		place(cache, row * 8 + 4, 2, cache->tick - 2);
		place(cache, row * 8 + 6, 2, cache->tick - 3);
	}
	lruv_cache_verify(cache, TRUE);
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	struct lruv_cache *cache = cache_new();
	long used[8];
	long row;
	long block_index;
	struct lruv_cache_block *block;

	/* the Xbox allocator finds no room; moving this tick's blocks makes it */
	CASE("moves-used-blocks")
	{
		fill(cache, used);
		CHECK(lruv_block_new(cache, 12 * PAGE_SIZE) == NONE, "the test's cache has a run of 12 unused pages");
		block_index = lruv_block_new_moving(cache, 12 * PAGE_SIZE, moved);
		CHECK(block_index != NONE, "no room made for 12 pages");
		block = datum_get(cache->blocks, block_index);
		CHECK(block->page_count == 12, "%ld pages", block->page_count);
		CHECK(moves > 0 && moves <= 2, "%ld blocks moved for 12 pages", moves);
		for (row = 0; row < 8; row++)
		{
			struct lruv_cache_block *kept = datum_get(cache->blocks, used[row]);

			CHECK(((struct datum_header *)kept)->identifier, "used block %ld deleted", row);
			CHECK(intact(cache, used[row]), "used block %ld lost its bytes", row);
			CHECK(kept->first_page_index + kept->page_count <= block->first_page_index ||
				kept->first_page_index >= block->first_page_index + block->page_count,
				"used block %ld overlaps the new one", row);
		}
		lruv_cache_verify(cache, TRUE);
		return 0;
	}
	/* a run the Xbox allocator finds is taken as it is */
	CASE("no-move-when-a-run-is-free")
	{
		fill(cache, used);
		block_index = lruv_block_new_moving(cache, 6 * PAGE_SIZE, moved);
		CHECK(block_index != NONE, "6 unused pages in a row not found");
		CHECK(moves == 0, "%ld blocks moved", moves);
		return 0;
	}
	/* a locked block stays where it is, and a range that needs it moved is not
	taken */
	CASE("keeps-locked-blocks")
	{
		fill(cache, used);
		locked_block = used[3];
		block = datum_get(cache->blocks, locked_block);
		block_index = lruv_block_new_moving(cache, 12 * PAGE_SIZE, moved);
		CHECK(block_index != NONE, "no room made for 12 pages");
		CHECK(block->first_page_index == 24, "the locked block moved to page %ld", block->first_page_index);
		CHECK(intact(cache, locked_block), "the locked block lost its bytes");
		lruv_cache_verify(cache, TRUE);
		return 0;
	}
	/* every page used this tick: nothing to move into, nothing deleted */
	CASE("none-when-all-used")
	{
		for (row = 0; row < 8; row++)
			place(cache, row * 8, 8, cache->tick);
		CHECK(lruv_block_new_moving(cache, 2 * PAGE_SIZE, moved) == NONE, "room made in a full cache");
		CHECK(moves == 0 && deletes == 0, "%ld moves, %ld deletes", moves, deletes);
		return 0;
	}
	/* a size no run could be cleared for is not planned again in the same
	tick: lruv_block_new asks for each of the 8 blocks, the plan once more */
	CASE("skips-a-failed-size-this-tick")
	{
		long full[8];

		for (row = 0; row < 8; row++)
			full[row] = place(cache, row * 8, 8, cache->tick);
		CHECK(lruv_block_new_moving(cache, 2 * PAGE_SIZE, moved) == NONE, "room made in a full cache");
		CHECK(lock_queries == 16, "%ld lock queries for the first request", lock_queries);
		lock_queries = 0;
		CHECK(lruv_block_new_moving(cache, 2 * PAGE_SIZE, moved) == NONE, "room made in a full cache");
		CHECK(lock_queries == 8, "the same size planned again in the same tick (%ld lock queries)", lock_queries);
		lock_queries = 0;
		CHECK(lruv_block_new_moving(cache, 3 * PAGE_SIZE, moved) == NONE, "room made in a full cache");
		CHECK(lock_queries == 16, "another size not planned (%ld lock queries)", lock_queries);
		cache->tick++;
		for (row = 0; row < 8; row++)
			((struct lruv_cache_block *)datum_get(cache->blocks, full[row]))->last_used_tick = cache->tick;
		lock_queries = 0;
		CHECK(lruv_block_new_moving(cache, 3 * PAGE_SIZE, moved) == NONE, "room made in a full cache");
		CHECK(lock_queries == 16, "not planned in the next tick (%ld lock queries)", lock_queries);
		return 0;
	}
	/* more than the cache: refused at once */
	CASE("none-when-too-large")
	{
		fill(cache, used);
		CHECK(lruv_block_new_moving(cache, (PAGE_COUNT + 1) * PAGE_SIZE, moved) == NONE, "room made past the cache");
		CHECK(moves == 0 && deletes == 0, "%ld moves, %ld deletes", moves, deletes);
		return 0;
	}

	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}

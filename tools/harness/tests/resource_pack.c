/*
RESOURCE_PACK.C (test)

The real custom_edition_resource_pack, custom_edition_packed_resource and
custom_edition_packed_read (custom_edition_cache.c).
A resource map larger than its part of the combined offset space packs only
the resources the map uses, each found again by offset, reads crossing pieces.
test_resource_pack.py takes the constants (config.inc) and the code (under_test.inc),
the files are arrays.
*/

#include <stdint.h>

#include "harness.h"
#include "config.inc"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define csmemcpy memcpy
#define csmemset memset
#define error(level, ...) ((void)0)

static long pixels_arrived;
#define custom_edition_bitmap_pixels_arrived(tag_cache, loaded_bytes, tag_index, offset, buffer) (pixels_arrived++)

/* a resource map's bytes */
struct custom_edition_file
{
	byte const *data;
	uint32_t size;
};

struct custom_edition_cache_globals
{
	byte *tag_cache;
	unsigned long loaded_bytes;
	struct custom_edition_file resource_files[NUMBER_OF_RESOURCE_MAP_TYPES];
	struct packed_resource *packed;
	long packed_count;
	long packed_capacity;
	uint32_t packed_end;
};

static struct custom_edition_cache_globals custom_edition_cache_globals;

static boolean custom_edition_file_read_staged(
	struct custom_edition_file *file,
	unsigned long file_offset,
	long size,
	void *buffer)
{
	if (!file->data || size < 0 || file_offset > file->size || (unsigned long)size > file->size - file_offset)
		return FALSE;
	memcpy(buffer, file->data + file_offset, size);
	return TRUE;
}

#include "under_test.inc"

enum { FILE_BYTES = 0x40000, PIECES = 3000 };

static byte bitmaps[FILE_BYTES];
static byte sounds[FILE_BYTES];

static uint32_t pack(enum resource_map_type type, uint32_t file_offset, uint32_t size)
{
	return custom_edition_resource_pack(&custom_edition_cache_globals, type, file_offset, size);
}

/* each byte of a file tells which file and where */
static byte file_byte(enum resource_map_type type, uint32_t file_offset)
{
	return (byte)(file_offset * 7 + (file_offset >> 8) + type * 101);
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	struct custom_edition_cache_globals *globals = &custom_edition_cache_globals;
	uint32_t offset;
	long piece;

	for (offset = 0; offset < FILE_BYTES; offset++)
	{
		bitmaps[offset] = file_byte(_resource_map_bitmaps, offset);
		sounds[offset] = file_byte(_resource_map_sounds, offset);
	}
	globals->resource_files[_resource_map_bitmaps].data = bitmaps;
	globals->resource_files[_resource_map_bitmaps].size = FILE_BYTES;
	globals->resource_files[_resource_map_sounds].data = sounds;
	globals->resource_files[_resource_map_sounds].size = FILE_BYTES;
	globals->packed_end = COMBINED_BITMAPS_OFFSET;

	/* a resource continuing the previous one in the same file joins it,
	one in the other file or further on starts a piece */
	CASE("joins-continuing-resources")
	{
		CHECK(pack(_resource_map_bitmaps, 0x100, 0x100) == COMBINED_BITMAPS_OFFSET, "first resource misplaced");
		CHECK(pack(_resource_map_bitmaps, 0x200, 0x80) == COMBINED_BITMAPS_OFFSET + 0x100, "second resource misplaced");
		CHECK(pack(_resource_map_sounds, 0x280, 0x40) == COMBINED_BITMAPS_OFFSET + 0x180, "sound misplaced");
		CHECK(pack(_resource_map_sounds, 0x1000, 0x40) == COMBINED_BITMAPS_OFFSET + 0x1C0, "far sound misplaced");
		CHECK(globals->packed_count == 3, "%ld pieces, not 3", globals->packed_count);
		CHECK(globals->packed[0].size == 0x180 && globals->packed[0].file_offset == 0x100, "bitmaps not joined");
		CHECK(globals->packed[1].type == _resource_map_sounds, "a sound joined the bitmaps");
		CHECK(globals->packed_end == COMBINED_BITMAPS_OFFSET + 0x200, "ends at 0x%X", (unsigned)globals->packed_end);
		return 0;
	}
	/* what does not fit below COMBINED_PACKED_LIMIT is refused, never wrapped to a low offset */
	CASE("refuses-past-the-limit")
	{
		CHECK(pack(_resource_map_bitmaps, 0, 0xFFFFFFFFUL) == CUSTOM_EDITION_RESOURCE_UNPLACED,
			"a resource larger than the space was placed");
		globals->packed_end = COMBINED_PACKED_LIMIT - 0x100;
		CHECK(pack(_resource_map_bitmaps, 0, 0x100) == COMBINED_PACKED_LIMIT - 0x100, "the last room was refused");
		CHECK(pack(_resource_map_sounds, 0, 1) == CUSTOM_EDITION_RESOURCE_UNPLACED, "a byte past the limit was placed");
		CHECK(globals->packed_end == COMBINED_PACKED_LIMIT, "a refused resource moved the end");
		CHECK(custom_edition_packed_resource(COMBINED_PACKED_LIMIT - 1) == &globals->packed[0],
			"the last byte below the limit was not found");
		return 0;
	}
	/* thousands of pieces (the table grows): each byte found in its own piece,
	nothing before or after them */
	CASE("finds-every-piece")
	{
		for (piece = 0; piece < PIECES; piece++)
			CHECK(pack(piece & 1 ? _resource_map_sounds : _resource_map_bitmaps, (uint32_t)piece * 64,
				1 + piece % 40) != CUSTOM_EDITION_RESOURCE_UNPLACED, "piece %ld refused", piece);
		CHECK(globals->packed_count == PIECES, "%ld pieces, not %d", globals->packed_count, PIECES);
		for (piece = 0; piece < PIECES; piece++)
		{
			struct packed_resource const *resource = &globals->packed[piece];

			CHECK(custom_edition_packed_resource(resource->offset) == resource, "piece %ld not found at its start", piece);
			CHECK(custom_edition_packed_resource(resource->offset + resource->size - 1) == resource,
				"piece %ld not found at its last byte", piece);
		}
		CHECK(!custom_edition_packed_resource(COMBINED_BITMAPS_OFFSET - 1), "found a piece before the first");
		CHECK(!custom_edition_packed_resource(globals->packed_end), "found a piece after the last");
		return 0;
	}
	/* a read from the middle of one piece into the next gets each file's own bytes */
	CASE("reads-across-pieces")
	{
		byte buffer[0x300];
		uint32_t at;

		pack(_resource_map_bitmaps, 0x1000, 0x100);
		pack(_resource_map_sounds, 0x8000, 0x100);
		pack(_resource_map_bitmaps, 0x20000, 0x100);
		custom_edition_packed_read(NONE, COMBINED_BITMAPS_OFFSET + 0x80, 0x200, buffer);
		for (at = 0; at < 0x200; at++)
		{
			uint32_t packed_at = 0x80 + at;
			enum resource_map_type type = packed_at < 0x100 || packed_at >= 0x200 ? _resource_map_bitmaps : _resource_map_sounds;
			uint32_t file_offset = packed_at < 0x100 ? 0x1000 + packed_at :
				packed_at < 0x200 ? 0x8000 + packed_at - 0x100 : 0x20000 + packed_at - 0x200;

			CHECK(buffer[at] == file_byte(type, file_offset), "byte 0x%X of the read is wrong", (unsigned)at);
		}
		memset(buffer, 0xAA, sizeof(buffer));
		custom_edition_packed_read(NONE, COMBINED_BITMAPS_OFFSET + 0x280, 0x100, buffer);
		CHECK(buffer[0] == 0 && buffer[0xFF] == 0, "a read past the last piece was not cleared");
		CHECK(pixels_arrived == 0, "pixels arrived for a read without a tag");
		return 0;
	}
	fprintf(stderr, "unknown case '%s'\n", case_name);
	return 2;
}

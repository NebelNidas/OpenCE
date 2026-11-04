/*
HUD_POSITION.C

Graphics' HUD POSITION (tools/port_settings.py's screen, its functions in
menu_functions.c): red corner marks, an L at each corner of the window
the HUD keeps to (display.hud_inset_x and _y included), over the view
of the player who opened it. Left and right make it narrower and wider, up
and down taller and shorter, one unit of a screen 480 high a press, four
once a direction has been held for a second; the HUD follows at once
(hud_inset_preview). A saves the inset to config.toml, B leaves it as it
was, Y sets it back to none.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "game/game_globals.h"
#include "interface/hud_draw.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"

#include "hud_position.h"

#include <stdio.h>

/* the platform layer's (port/linux/src/port_config.c) */
int config_write(const char *name, const char *value);

/* ---------- constants */

/* the corner marks, in the units of a screen 480 high: each arm's length
along the window's edge and its width inside the window, and the dark edge
around them */
#define HUD_POSITION_ARM_LENGTH 32.0f
#define HUD_POSITION_ARM_WIDTH 4.0f
#define HUD_POSITION_EDGE 2.0f
#define HUD_POSITION_MARK_COLOR 0xFFFF0000u
#define HUD_POSITION_EDGE_COLOR 0xC0000000u
/* a direction pressed again within this long counts as held */
#define HUD_POSITION_HELD_MILLISECONDS 400
#define HUD_POSITION_HELD_PRESSES 4
#define HUD_POSITION_HELD_STEP 4

/* ---------- globals */

static struct
{
	boolean open;
	short local_player_index;
	short x;
	short y;
	short last_x;
	short last_y;
	short presses;
	unsigned long last_press;
} hud_position = { FALSE, NONE, 0, 0, 0, 0, 0, 0 };

/* ---------- private code */

/* four corners in the view's units, a triangle with the last twice (draw_quad's way) */
static void hud_position_quad(
	real_point2d const positions[4],
	pixel32 color)
{
	struct game_globals *game_globals;
	struct game_globals_rasterizer_data *rasterizer_data;
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	struct dynamic_screen_vertex vertices[4];
	short vertex_index;

	global_scenario_get();
	game_globals = scenario_get_game_globals();
	if (!game_globals || !game_globals->rasterizer_data.count)
		return;
	rasterizer_data = TAG_BLOCK_GET_ELEMENT(&game_globals->rasterizer_data, 0, struct game_globals_rasterizer_data);
	for (vertex_index = 0; vertex_index < NUMBEROF(vertices); vertex_index++)
	{
		vertices[vertex_index].position.x = positions[vertex_index].x;
		vertices[vertex_index].position.y = positions[vertex_index].y;
		vertices[vertex_index].texture_coordinates.x = 0.0f;
		vertices[vertex_index].texture_coordinates.y = 0.0f;
		vertices[vertex_index].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = parameters.map_scale[0].j = 1.0f;
	parameters.map[0] = TAG_BLOCK_GET_ELEMENT(
		&bitmap_group_get(rasterizer_data->default_textures[0].index)->bitmaps,
		1,
		struct bitmap_data);
	rasterizer_globals.current_lock_operation = _rasterizer_lock_cinematics;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
	rasterizer_globals.current_lock_operation = _rasterizer_lock_none;
}

/* a rectangle between (x0, y0) and (x1, y1), in any order, widened by edge all round */
static void hud_position_rectangle(
	real x0,
	real y0,
	real x1,
	real y1,
	real edge,
	pixel32 color)
{
	real_point2d corners[4];
	real left = MIN(x0, x1) - edge;
	real right = MAX(x0, x1) + edge;
	real top = MIN(y0, y1) - edge;
	real bottom = MAX(y0, y1) + edge;

	/* (clockwise on the screen, like every other quad here) */
	corners[0].x = left;
	corners[0].y = top;
	corners[1].x = right;
	corners[1].y = top;
	corners[2].x = right;
	corners[2].y = bottom;
	corners[3].x = left;
	corners[3].y = bottom;
	hud_position_quad(corners, color);
}

/* an L at the corner (x, y) whose side is sx and sy (-1 left and top, 1 right
and bottom), its arms along the window's edges, inside it */
static void hud_position_corner(
	real x,
	real y,
	real sx,
	real sy,
	real edge,
	pixel32 color)
{
	hud_position_rectangle(x, y, x - sx * HUD_POSITION_ARM_LENGTH, y - sy * HUD_POSITION_ARM_WIDTH, edge, color);
	hud_position_rectangle(x, y - sy * HUD_POSITION_ARM_WIDTH, x - sx * HUD_POSITION_ARM_WIDTH,
		y - sy * HUD_POSITION_ARM_LENGTH, edge, color);
}

/* ---------- public code */

void hud_position_begin(
	short local_player_index)
{
	hud_position.open = TRUE;
	hud_position.local_player_index = local_player_index;
	hud_inset_preview(FALSE, 0, 0);
	hud_inset_get(&hud_position.x, &hud_position.y);
	hud_inset_preview(TRUE, hud_position.x, hud_position.y);
}

void hud_position_end(
	void)
{
	hud_position.open = FALSE;
	hud_position.local_player_index = NONE;
	hud_inset_preview(FALSE, 0, 0);
}

void hud_position_move(
	short x,
	short y)
{
	unsigned long now = system_milliseconds();
	short step;

	if (x == hud_position.last_x && y == hud_position.last_y &&
		now - hud_position.last_press < HUD_POSITION_HELD_MILLISECONDS)
	{
		hud_position.presses++;
	}
	else
	{
		hud_position.presses = 0;
	}
	hud_position.last_x = x;
	hud_position.last_y = y;
	hud_position.last_press = now;
	step = hud_position.presses >= HUD_POSITION_HELD_PRESSES ? HUD_POSITION_HELD_STEP : 1;
	x *= step;
	y *= step;
	hud_position.x = (short)PIN(hud_position.x + x, -HUD_INSET_MAXIMUM, HUD_INSET_MAXIMUM);
	hud_position.y = (short)PIN(hud_position.y + y, -HUD_INSET_MAXIMUM, HUD_INSET_MAXIMUM);
	hud_inset_preview(TRUE, hud_position.x, hud_position.y);
}

void hud_position_reset(
	void)
{
	hud_position.x = 0;
	hud_position.y = 0;
	hud_inset_preview(TRUE, 0, 0);
}

boolean hud_position_save(
	void)
{
	char value[16];
	boolean saved;

	snprintf(value, sizeof(value), "%d", hud_position.x);
	saved = config_write("display.hud_inset_x", value) != 0;
	snprintf(value, sizeof(value), "%d", hud_position.y);
	saved = config_write("display.hud_inset_y", value) != 0 && saved;

	return saved;
}

void hud_position_text(
	wchar_t *text,
	long count)
{
	char shown[96];
	long index;

	snprintf(shown, sizeof(shown), "LEFT AND RIGHT: %d     TOP AND BOTTOM: %d", hud_position.x, hud_position.y);
	for (index = 0; shown[index] && index < count - 1; index++)
		text[index] = (wchar_t)(unsigned char)shown[index];
	text[index] = 0;
}

/* in the view being drawn (render.camera), before its menus */
void hud_position_draw(
	short local_player_index)
{
	rectangle2d const *view = &render.camera.viewport_bounds;
	rectangle2d window;
	real x0, y0, x1, y1;
	short pass;

	if (!hud_position.open ||
		(hud_position.local_player_index != NONE && local_player_index != hud_position.local_player_index))
	{
		return;
	}
	window = render.camera.window_bounds;
	hud_inset_apply(&window);
	x0 = (real)(window.x0 - view->x0);
	y0 = (real)(window.y0 - view->y0);
	x1 = (real)(window.x1 - view->x0);
	y1 = (real)(window.y1 - view->y0);
	/* the dark edges, then the marks */
	for (pass = 0; pass < 2; pass++)
	{
		real edge = pass ? 0.0f : HUD_POSITION_EDGE;
		pixel32 color = pass ? HUD_POSITION_MARK_COLOR : HUD_POSITION_EDGE_COLOR;

		hud_position_corner(x0, y0, -1.0f, -1.0f, edge, color);
		hud_position_corner(x1, y0, 1.0f, -1.0f, edge, color);
		hud_position_corner(x0, y1, -1.0f, 1.0f, edge, color);
		hud_position_corner(x1, y1, 1.0f, 1.0f, edge, color);
	}
}

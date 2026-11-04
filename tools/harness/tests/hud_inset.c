/*
HUD_INSET.C (test)

display.hud_inset_x and _y (source/interface/hud_draw.c): how far the HUD's
window moves in or out, the same on both sides, capped at half the view
across and at the nearer of the view's edges, the settings pinned, HUD POSITION's preview in their place; and how
far a press of HUD POSITION moves it (port/linux/game/hud_position.c).
test_hud_inset.py takes the code (under_test.inc).
*/

#include "harness.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define PIN(n, low, high) ((n) < (low) ? (low) : ((n) > (high) ? (high) : (n)))

typedef struct
{
	short x0, y0, x1, y1;
} rectangle2d;

enum
{
	HUD_INSET_MAXIMUM = 200
};

/* the fake world: the view being drawn, config.toml, the clock */
static struct
{
	struct
	{
		rectangle2d viewport_bounds;
	} camera;
} render;
static long setting_x, setting_y;
static unsigned long changes = 1;
static unsigned long now_milliseconds = 10000;

static long config_integer(char const *name)
{
	return !strcmp(name, "display.hud_inset_x") ? setting_x : !strcmp(name, "display.hud_inset_y") ? setting_y : 0;
}

static unsigned long config_changes(void)
{
	return changes;
}

#define system_milliseconds() (now_milliseconds)

#include "under_test.inc"

static rectangle2d rectangle(short x0, short y0, short x1, short y1)
{
	rectangle2d bounds;

	bounds.x0 = x0;
	bounds.y0 = y0;
	bounds.x1 = x1;
	bounds.y1 = y1;
	return bounds;
}

static rectangle2d inset(rectangle2d window, long x, long y)
{
	setting_x = x;
	setting_y = y;
	changes++;
	hud_inset_apply(&window);
	return window;
}

#define CHECK_WINDOW(window, ex0, ey0, ex1, ey1) \
	CHECK((window).x0 == (ex0) && (window).y0 == (ey0) && (window).x1 == (ex1) && (window).y1 == (ey1), \
		"window %d %d %d %d, not %d %d %d %d", (window).x0, (window).y0, (window).x1, (window).y1, \
		(ex0), (ey0), (ex1), (ey1))

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	rectangle2d window;

	/* a 640x480 screen, its title-safe window */
	render.camera.viewport_bounds = rectangle(0, 0, 640, 480);

	/* in on both sides of each axis */
	CASE("in")
	{
		window = inset(rectangle(48, 36, 592, 444), 20, 10);
		CHECK_WINDOW(window, 68, 46, 572, 434);
		window = inset(rectangle(48, 36, 592, 444), 0, 0);
		CHECK_WINDOW(window, 48, 36, 592, 444);
		return 0;
	}
	/* out, to the view's edges and no further */
	CASE("out")
	{
		window = inset(rectangle(48, 36, 592, 444), -20, -10);
		CHECK_WINDOW(window, 28, 26, 612, 454);
		window = inset(rectangle(48, 36, 592, 444), -200, -200);
		CHECK_WINDOW(window, 0, 0, 640, 480);
		return 0;
	}
	/* in, keeping at least half the view across */
	CASE("half-the-view")
	{
		window = inset(rectangle(48, 36, 592, 444), 200, 200);
		CHECK_WINDOW(window, 160, 120, 480, 360);
		/* a window already narrower than half the view stays as it is */
		window = inset(rectangle(200, 36, 400, 444), 50, 0);
		CHECK_WINDOW(window, 200, 36, 400, 444);
		return 0;
	}
	/* a split screen view (the lower half), its window 4 below its top and
	36 above its bottom: out as far as the nearer edge, its middle kept */
	CASE("split-screen")
	{
		render.camera.viewport_bounds = rectangle(0, 240, 640, 480);
		window = inset(rectangle(48, 244, 592, 444), -100, -100);
		CHECK_WINDOW(window, 0, 240, 640, 448);
		window = inset(rectangle(48, 244, 592, 444), 0, 200);
		CHECK_WINDOW(window, 48, 284, 592, 404);
		return 0;
	}
	/* the settings pinned, read again when config.toml changes */
	CASE("settings")
	{
		short x, y;

		setting_x = 500;
		setting_y = -500;
		changes++;
		hud_inset_get(&x, &y);
		CHECK(x == 200 && y == -200, "inset %d %d, not 200 -200", x, y);
		setting_x = 7;
		hud_inset_get(&x, &y);
		CHECK(x == 200, "read again with no change: %d", x);
		changes++;
		hud_inset_get(&x, &y);
		CHECK(x == 7, "not read again after a change: %d", x);
		return 0;
	}
	/* HUD POSITION's values in place of the settings until it closes */
	CASE("preview")
	{
		short x, y;

		setting_x = 10;
		setting_y = 5;
		changes++;
		hud_position_begin(0);
		CHECK(hud_position.x == 10 && hud_position.y == 5, "opened at %d %d", hud_position.x, hud_position.y);
		hud_position_move(1, 0);
		hud_inset_get(&x, &y);
		CHECK(x == 11 && y == 5, "preview %d %d, not 11 5", x, y);
		window = inset(rectangle(48, 36, 592, 444), 10, 5);
		CHECK_WINDOW(window, 59, 41, 581, 439);
		hud_position_reset();
		hud_inset_get(&x, &y);
		CHECK(x == 0 && y == 0, "reset to %d %d", x, y);
		hud_position_end();
		hud_inset_get(&x, &y);
		CHECK(x == 10 && y == 5, "closed: %d %d, not the settings' 10 5", x, y);
		return 0;
	}
	/* one unit a press, four once a direction is held; pinned at the maximum */
	CASE("presses")
	{
		long press;

		setting_x = setting_y = 0;
		changes++;
		hud_position_begin(0);
		for (press = 0; press < 5; press++)
		{
			hud_position_move(1, 0);
			now_milliseconds += 100;
		}
		CHECK(hud_position.x == 8, "five quick presses: %d, not 1+1+1+1+4", hud_position.x);
		now_milliseconds += 1000;
		hud_position_move(1, 0);
		CHECK(hud_position.x == 9, "a press after a pause: %d, not one more", hud_position.x);
		hud_position_move(0, -1);
		CHECK(hud_position.x == 9 && hud_position.y == -1, "another direction: %d %d", hud_position.x, hud_position.y);
		for (press = 0; press < 100; press++)
		{
			hud_position_move(0, -1);
			now_milliseconds += 50;
		}
		CHECK(hud_position.y == -HUD_INSET_MAXIMUM, "held up to %d", hud_position.y);
		hud_position_end();
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}

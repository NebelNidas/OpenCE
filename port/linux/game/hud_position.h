#ifndef HUD_POSITION_H
#define HUD_POSITION_H

#include "cseries.h"

/* Graphics' HUD POSITION (hud_position.c) */
void hud_position_begin(
	short local_player_index);
void hud_position_end(
	void);
void hud_position_move(
	short x,
	short y);
void hud_position_reset(
	void);
boolean hud_position_save(
	void);
void hud_position_text(
	wchar_t *text,
	long count);
void hud_position_draw(
	short local_player_index);

#endif

#include "harness.h"
/* a co-op player who left: game.c's quit and the save/speak tests of players.c and game.c */
struct player_datum {short identifier;long unit_index;long quit_out_of_game_time;boolean quit_out_of_game;struct {short deaths;} statistics;};
struct data_iterator {long datum_index;long next;};
struct unit_datum {int unused;};
static struct data_array *player_data;
static struct unit_datum units[8];
static long tick;
static boolean coop=TRUE,held=TRUE,projectiles;
static void data_iterator_new(struct data_iterator *i,struct data_array *array){(void)array;i->next=0;}
static void *data_iterator_next(struct data_iterator *i)
{
 while(i->next<player_data->maximum_count){
  long slot=i->next++;struct player_datum *p=(void *)(player_data->data+slot*player_data->size);
  if(p->identifier){i->datum_index=((long)(unsigned short)p->identifier<<16)|slot;return p;}
 }
 return NULL;
}
static boolean game_engine_running(void){return FALSE;}
static boolean network_coop_active(void){return coop;}
static boolean players_coop_extras_held(void){return held;}
static long game_time_get(void){return tick;}
static boolean dangerous_projectiles_near_player(void){return projectiles;}
static struct player_datum *player_try_and_get(long index){return index==NONE?NULL:datum_get(player_data,index);}
static struct unit_datum *unit_get(long index){return &units[index];}
/* the unit dies and its player has none (players.c's player_died) */
static void unit_kill_no_statistics(long unit_index)
{
 struct data_iterator i;struct player_datum *p;
 data_iterator_new(&i,player_data);
 while((p=data_iterator_next(&i))!=NULL)if(p->unit_index==unit_index)p->unit_index=NONE;
}
#include "under_test.inc"
static long players[3];
static void setup(void)
{
 int i;
 player_data=game_state_data_new("players",4,sizeof(struct player_datum));
 for(i=0;i<3;i++){
  struct player_datum *p;
  players[i]=datum_new(player_data);p=datum_get(player_data,players[i]);
  p->unit_index=i;p->quit_out_of_game_time=NONE;p->statistics.deaths=1;
 }
 tick=100;
}
static struct player_datum *player(int i){return datum_get(player_data,players[i]);}
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";setup();
 CHECK(!any_player_is_dead()&&game_safe_to_speak(),"all alive but not safe to speak");
 CASE("quit"){
  player(2)->quit_out_of_game_time=tick;remove_quitting_players_from_game();
  CHECK(player(2)->quit_out_of_game&&player(2)->unit_index==NONE,"the quit did not take the unit");
  CHECK(!any_player_is_dead(),"a player who left counts as dead");
  CHECK(game_safe_to_speak(),"a player who left keeps the game from speaking");
  return 0;
 }
 CASE("quit-later"){
  player(2)->quit_out_of_game_time=tick+30;remove_quitting_players_from_game();
  CHECK(!player(2)->quit_out_of_game&&player(2)->unit_index==2,"quit before its tick");
  tick+=30;remove_quitting_players_from_game();
  CHECK(player(2)->quit_out_of_game&&!any_player_is_dead()&&game_safe_to_speak(),"quit at its tick still dead");
  return 0;
 }
 CASE("dead"){
  /* one waiting to respawn still holds both off */
  player(1)->unit_index=NONE;
  CHECK(any_player_is_dead()&&!game_safe_to_speak(),"a dead player did not count");
  player(2)->quit_out_of_game_time=tick;remove_quitting_players_from_game();
  CHECK(any_player_is_dead(),"a quit hid the dead player");
  return 0;
 }
 CASE("waiting"){
  /* held back before the first checkpoint: not dead; once released, dead */
  player(1)->unit_index=NONE;player(1)->statistics.deaths=0;
  CHECK(!any_player_is_dead(),"a held player counts as dead");
  held=FALSE;CHECK(any_player_is_dead(),"a released player without a unit is not dead");
  return 0;
 }
 CASE("projectiles"){
  player(2)->quit_out_of_game_time=tick;remove_quitting_players_from_game();
  projectiles=TRUE;CHECK(!game_safe_to_speak(),"projectiles ignored");
  return 0;
 }
 fprintf(stderr,"unknown case %s\n",case_name);
 return 2;
}

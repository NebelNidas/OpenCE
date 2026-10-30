#include "harness.h"
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 4
#define FAST_FORWARD_HELD_MILLISECONDS 1000
#define csmemset memset
#define _error_silent 0
#define error(...) ((void)0)
enum {_game_connection_local,_game_connection_network_client,_game_connection_network_server};
static short connection=_game_connection_network_server;
static boolean coop=TRUE,scene=TRUE,host_player=TRUE;
static unsigned long now;
static real set_factor;
static long clients[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
static short client_count;
static short game_connection(void){return connection;}
static boolean coop_game(void){return coop;}
static long game_time_get(void){return 0;}
static unsigned long system_milliseconds(void){return now;}
static boolean player_control_fast_forward_scene(void){return scene;}
static short local_player_get_next(short index){(void)index;return host_player?0:NONE;}
static void game_time_set_fast_forward(real factor){set_factor=factor;}
static short distributed_client_machines(long *indices,short maximum)
{
 for(short i=0;i<client_count&&i<maximum;i++)indices[i]=clients[i];
 return client_count;
}
#include "types.inc"
#include "under_test.inc"
/* a client's vote message, with or without its players holding */
static void hear(long machine,boolean held)
{
 struct distributed_coop_skip_vote vote={0};
 vote.fast_forward=(byte)held;network_coop_handle_skip_vote(machine,&vote);
}
static void setup(short count)
{
 memset(&fast_forward,0,sizeof(fast_forward));memset(&skip_vote,0,sizeof(skip_vote));
 connection=_game_connection_network_server;coop=TRUE;scene=TRUE;host_player=TRUE;now=5000;set_factor=0.f;
 client_count=count;for(short i=0;i<count;i++)clients[i]=i+1;
 network_coop_fast_forward_hold(FALSE,6.f);
}
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";short holders,machines;boolean on;
 CASE("majority") {
  setup(2);network_coop_fast_forward_hold(TRUE,6.f);hear(1,FALSE);hear(2,FALSE);host_count_fast_forward();
  CHECK(!fast_forward.on&&set_factor==1.f,"one of three ran fast");
  hear(1,TRUE);host_count_fast_forward();
  CHECK(fast_forward.on&&set_factor==6.f&&fast_forward.holders==2&&fast_forward.machines==3,"two of three: %d of %d",
   fast_forward.holders,fast_forward.machines);
  network_coop_fast_forward_hold(FALSE,6.f);host_count_fast_forward();CHECK(!fast_forward.on&&set_factor==1.f,"let go, still fast");
 } else CASE("even") {
  setup(1);network_coop_fast_forward_hold(TRUE,6.f);hear(1,FALSE);host_count_fast_forward();
  CHECK(!fast_forward.on,"half the machines ran it fast");
  hear(1,TRUE);host_count_fast_forward();CHECK(fast_forward.on,"both holding, not fast");
 } else CASE("dedicated") {
  setup(2);host_player=FALSE;hear(1,TRUE);hear(2,FALSE);host_count_fast_forward();
  CHECK(fast_forward.machines==2&&!fast_forward.on,"a host without players counted: %d machines",fast_forward.machines);
  hear(2,TRUE);host_count_fast_forward();CHECK(fast_forward.on,"every client holding, not fast");
 } else CASE("silent") {
  setup(1);host_player=FALSE;hear(1,TRUE);now+=FAST_FORWARD_HELD_MILLISECONDS+1;host_count_fast_forward();
  CHECK(fast_forward.machines==0&&!fast_forward.on,"a client not heard for a second counted");
  hear(1,TRUE);now+=FAST_FORWARD_HELD_MILLISECONDS;host_count_fast_forward();CHECK(fast_forward.on,"a second after its message, dropped");
 } else CASE("scene") {
  setup(1);network_coop_fast_forward_hold(TRUE,6.f);hear(1,TRUE);scene=FALSE;host_count_fast_forward();
  CHECK(!fast_forward.on&&set_factor==1.f&&fast_forward.machines==0,"fast outside a scene that can be fast-forwarded");
  CHECK(!network_coop_fast_forward_status(&holders,&machines,&on),"status shown outside such a scene");
 } else CASE("status") {
  setup(2);hear(1,TRUE);hear(2,FALSE);host_count_fast_forward();
  CHECK(network_coop_fast_forward_status(&holders,&machines,&on)&&holders==1&&machines==3&&!on,"status %d of %d",holders,machines);
  hear(1,FALSE);host_count_fast_forward();CHECK(!network_coop_fast_forward_status(&holders,&machines,&on),"status with none holding");
 } else CASE("outside") {
  setup(0);coop=FALSE;CHECK(!network_coop_fast_forward_hold(TRUE,6.f),"a network game that is not co-op voted");
  connection=_game_connection_local;coop=TRUE;CHECK(!network_coop_fast_forward_hold(TRUE,6.f),"single player voted");
 } else return 2;
 return 0;
}

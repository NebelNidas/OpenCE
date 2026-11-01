#include "harness.h"
#define MAXIMUM_TRACKED_OBJECTS 16
#define MIN(a,b) ((a)<(b)?(a):(b))
#define csmemset memset
#define RELIABLE_ENTRIES(type) MAXIMUM_ENTRIES_PER_MESSAGE
#include "types.inc"
/* the map's objects by absolute index (NONE: none there) */
struct object_datum {struct {short type;unsigned long damage_flags;} object;};
static struct object_datum objects[MAXIMUM_TRACKED_OBJECTS];
static long object_indices[MAXIMUM_TRACKED_OBJECTS];
struct object_iterator {long index;int next;};
#define object_iterator_new(iterator,type_flags,flags) ((iterator)->next=0)
static void *object_iterator_next(struct object_iterator *i)
{
 while(i->next<MAXIMUM_TRACKED_OBJECTS){int slot=i->next++;if(object_indices[slot]!=NONE){i->index=object_indices[slot];return &objects[slot];}}
 return NULL;
}
static struct object_datum *object_get(long index){return &objects[DATUM_INDEX_TO_ABSOLUTE_INDEX(index)];}
#define distributed_object_networked(index) TRUE
/* what the host sends its clients */
struct distributed_object_change {short change;long object_index;};
struct distributed_object_change_message {long header;struct distributed_object_change changes[MAXIMUM_ENTRIES_PER_MESSAGE];};
static struct distributed_object_change sent[64];
static int sent_count;
static void distributed_change_from_object(long index,struct distributed_object_change *change){csmemset(change,0,sizeof(*change));change->change=_object_change_create;change->object_index=index;}
static void send_changes(struct distributed_object_change const *changes,int count){for(int i=0;i<count&&sent_count<64;i++)sent[sent_count++]=changes[i];}
#define distributed_send(message,type,count,size,to) send_changes((message)->changes,(count))
static struct {long creates,deletes;} objects_statistics;
#include "under_test.inc"
static void setup(void)
{
 for(int i=0;i<MAXIMUM_TRACKED_OBJECTS;i++){object_indices[i]=NONE;objects_host_told[i]=NONE;objects_host_told_dead[i]=FALSE;}
 memset(objects,0,sizeof(objects));memset(objects_host_inventories,0,sizeof(objects_host_inventories));objects_host_told_count=0;
}
static long place(int slot,int identifier,short type){object_indices[slot]=((long)identifier<<16)|slot;objects[slot].object.type=type;objects[slot].object.damage_flags=0;return object_indices[slot];}
static void set_dead(int slot,boolean dead){if(dead)objects[slot].object.damage_flags|=FLAG(_object_dead_bit);else objects[slot].object.damage_flags&=~FLAG(_object_dead_bit);}
static void update(void){sent_count=0;distributed_host_update_objects();}
static boolean sent_is(int at,short change,long index){return at<sent_count&&sent[at].change==change&&sent[at].object_index==index;}
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";setup();
 CASE("revert") {
  short types[]={_object_type_biped,_object_type_vehicle};
  for(unsigned t=0;t<sizeof(types)/sizeof(types[0]);t++){
   setup();long unit=place(2,0x1234,types[t]);
   update();CHECK(sent_count==1&&sent_is(0,_object_change_create,unit),"type %d: new unit not told",types[t]);
   set_dead(2,TRUE);update();CHECK(sent_count==0,"type %d: a death sent %d changes",types[t],sent_count);
   update();CHECK(sent_count==0,"type %d: a corpse sent %d changes",types[t],sent_count);
   /* a revert restores the saved unit, alive, under the same index */
   objects_host_inventories[2].checksum=objects_host_inventories[2].weapons_checksum=0xabcd;
   set_dead(2,FALSE);update();
   CHECK(sent_count==2&&sent_is(0,_object_change_delete,unit)&&sent_is(1,_object_change_create,unit),"type %d: revived unit not told anew (%d changes)",types[t],sent_count);
   CHECK(!objects_host_inventories[2].checksum&&!objects_host_inventories[2].weapons_checksum,"type %d: revived unit's inventory not sent again",types[t]);
   update();CHECK(sent_count==0,"type %d: revived unit told again (%d changes)",types[t],sent_count);
  }
 } else CASE("corpse") {
  long unit=place(5,0x2000,_object_type_biped);set_dead(5,TRUE);
  update();CHECK(sent_count==1&&sent_is(0,_object_change_create,unit),"corpse not told");
  for(int i=0;i<3;i++){update();CHECK(sent_count==0,"corpse told again");}
  object_indices[5]=NONE;update();CHECK(sent_count==1&&sent_is(0,_object_change_delete,unit),"deleted corpse not told");
 } else CASE("item") {
  long weapon=place(1,0x3000,_object_type_weapon);update();CHECK(sent_count==1&&sent_is(0,_object_change_create,weapon),"weapon not told");
  set_dead(1,TRUE);update();set_dead(1,FALSE);update();CHECK(sent_count==0,"an item's damage flags made it told again");
 } else CASE("reuse") {
  long old=place(3,0x4000,_object_type_biped);update();set_dead(3,TRUE);update();
  long unit=place(3,0x4001,_object_type_biped);update();
  CHECK(sent_count==2&&sent_is(0,_object_change_delete,old)&&sent_is(1,_object_change_create,unit),"new object in a used place not told");
  set_dead(3,TRUE);update();CHECK(sent_count==0,"its death sent changes");
 } else return 2;
 return 0;
}

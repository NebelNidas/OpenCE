#include "harness.h"
/* objects.c's objects_scripting_attach: which children a script attaches */
#include "flags.inc"
struct object_datum {struct {long parent_object_index;unsigned long flags;} object;};
static struct object_datum objects[4];
static int attached;
static struct object_datum *object_get(long index){return &objects[index];}
static void network_coop_note_attach(long parent,char const *parent_marker,long child,char const *child_marker)
{(void)parent;(void)parent_marker;(void)child;(void)child_marker;}
static void object_attach_to_marker(long parent,char const *parent_marker,long child,char const *child_marker)
{(void)parent;(void)parent_marker;(void)child;(void)child_marker;attached++;}
#include "under_test.inc"
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";
 int i;
 for(i=0;i<4;i++){objects[i].object.parent_object_index=NONE;objects[i].object.flags=FLAG(_object_connected_to_map_bit);}
 CASE("loose"){
  objects_scripting_attach(0,"",1,"");
  CHECK(attached==1,"a loose object was not attached");
  return 0;
 }
 CASE("stowed"){
  /* put away in a unit's inventory: no parent, not connected to the map */
  objects[1].object.flags=0;
  objects_scripting_attach(0,"",1,"");
  CHECK(attached==0,"a weapon put away in an inventory was attached");
  return 0;
 }
 CASE("held"){
  objects[1].object.parent_object_index=2;
  objects_scripting_attach(0,"",1,"");
  objects_scripting_attach(NONE,"",3,"");
  CHECK(attached==0,"a held object or no parent attached");
  return 0;
 }
 fprintf(stderr,"unknown case %s\n",case_name);
 return 2;
}

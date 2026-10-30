#include "harness.h"
/* objects.c's object name list: a name holds no object that was freed */
#include "flags.inc"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define VALID_INDEX(index,count) ((index)>=0&&(index)<(count))
#define TAG_BLOCK_GET_ELEMENT(block,index,type) (&((type *)(block)->address)[index])
enum {_error_silent};
struct tag_block {long count;void *address;};
struct scenario_object_name {char name[32];};
struct scenario {struct tag_block object_names;};
struct object_datum {long definition_index;struct {unsigned long flags;long first_child_object_index,next_object_index;short name_index;} object;};
struct object_definition {int unused;};
static struct scenario_object_name names[8];
static struct scenario scenario={{8,names}};
static long object_name_list[MAXIMUM_OBJECT_NAMES_PER_SCENARIO];
static struct object_datum objects[4];
static struct object_definition definition;
static boolean freed[4];
static int errors;
static void *object_header_data;
static struct scenario *global_scenario_get(void){return &scenario;}
static struct object_datum *object_get(long index){return &objects[DATUM_INDEX_TO_ABSOLUTE_INDEX(index)];}
static struct object_definition *object_definition_get(long index){(void)index;return &definition;}
static char const *tag_get_name(long index){(void)index;return "";}
static void error(int level,char const *format,...){(void)level;(void)format;errors++;}
#define object_set_garbage(index,garbage) ((void)(index))
#define object_deleted_procs_call(index) ((void)(index))
#define object_deactivate(index) ((void)(index))
#define object_delete_attachments(index) ((void)(index))
#define object_disconnect_from_map(index) ((void)(index))
#define object_type_delete(index) ((void)(index))
#define object_header_delete(data,index) (freed[DATUM_INDEX_TO_ABSOLUTE_INDEX(index)]=TRUE)
static void object_name_list_forget(long object_index);
#include "under_test.inc"
/* a new object in a slot, under a new salt */
static long object_new_in(long slot,long salt)
{
 objects[slot].object.flags=0;objects[slot].object.first_child_object_index=NONE;
 objects[slot].object.next_object_index=NONE;objects[slot].object.name_index=NONE;freed[slot]=FALSE;
 return (salt<<16)|slot;
}
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";
 long i;
 for(i=0;i<MAXIMUM_OBJECT_NAMES_PER_SCENARIO;i++)object_name_list[i]=NONE;
 CASE("placed-dead"){
  /* object_new_from_scenario: object_type_place deletes the unit, then it is named */
  long unit=object_new_in(1,0x11),again;
  object_name_list_delete(unit);
  object_name_list_new(unit,3);
  object_delete_recursive(unit,FALSE);
  CHECK(object_index_from_name_index(3)==NONE,"the name holds the freed unit 0x%lx",object_index_from_name_index(3));
  again=object_new_in(1,0x12);
  object_name_list_new(again,3);
  CHECK(errors==0&&object_index_from_name_index(3)==again,"the name could not be given again");
  return 0;
 }
 CASE("conversation"){
  /* a conversation names its participant's unit, which has no name of its own */
  long unit=object_new_in(1,0x21);
  object_set_object_index_for_name_index(5,unit);
  object_name_list_delete(unit);
  object_delete_recursive(unit,FALSE);
  CHECK(object_index_from_name_index(5)==NONE,"the name holds the freed unit 0x%lx",object_index_from_name_index(5));
  return 0;
 }
 CASE("named"){
  long a=object_new_in(1,0x31),b=object_new_in(2,0x32);
  object_name_list_new(a,2);
  object_name_list_new(b,4);
  object_name_list_delete(a);
  CHECK(object_index_from_name_index(2)==NONE&&objects[1].object.name_index==NONE,"the deleted object kept its name");
  object_delete_recursive(a,FALSE);
  CHECK(object_index_from_name_index(4)==b&&errors==0,"another object lost its name");
  return 0;
 }
 CASE("child"){
  long parent=object_new_in(1,0x41),child=object_new_in(2,0x42);
  objects[1].object.first_child_object_index=child;
  object_set_object_index_for_name_index(6,child);
  object_name_list_delete(parent);
  object_delete_recursive(parent,FALSE);
  CHECK(freed[2]&&object_index_from_name_index(6)==NONE,"the name holds the freed child");
  return 0;
 }
 fprintf(stderr,"unknown case %s\n",case_name);
 return 2;
}

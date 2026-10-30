#include <math.h>
#include "harness.h"
/* A particle's tint between two colours in hsv (source/bitmaps/bitmap_utilities.c,
rgb_colors_interpolate): every result is a valid colour written by the call,
also at a bound of hue 0 shifted to hue 1. */
#include "macros.inc"
#define display_assert(text, file, line, halt) ((void)0)
#define system_exit(code) exit(code)
static int invalid_results;
#define match_assert_valid_real_rgb_color(file, line, rgb) \
	((void)(valid_real_rgb_color(rgb) ? 0 : invalid_results++))
#include "under_test.inc"

/* the result as it was left before the call: the effect's particle data is not cleared */
static union real_rgb_color interpolated(real lower_red, real lower_blue, real upper_red, real upper_blue, real factor)
{
 union real_rgb_color lower={{lower_red,0.f,lower_blue}}, upper={{upper_red,0.f,upper_blue}}, result;
 result.red=result.blue=0.f;
 result.green=NAN;
 rgb_colors_interpolate(&result,FLAG(_rgb_color_interpolation_hsv_bit),&lower,&upper,factor);
 return result;
}

int main(int argc,char **argv)
{
 const char *case_name=argc>1?argv[1]:"";
 CASE("hue_one") {
  /* red (hue 0) to blue (hue 2/3): the red bound goes to hue 1 */
  union real_rgb_color at_lower=interpolated(1.f,0.f,0.f,1.f,0.f);
  union real_rgb_color at_upper=interpolated(0.f,1.f,1.f,0.f,1.f);
  CHECK(invalid_results==0,"%d invalid results",invalid_results);
  CHECK(at_lower.red==1.f && at_lower.green==0.f && at_lower.blue==0.f,"factor 0: %f %f %f",
   at_lower.red,at_lower.green,at_lower.blue);
  CHECK(at_upper.red==1.f && at_upper.green==0.f && at_upper.blue==0.f,"factor 1: %f %f %f",
   at_upper.red,at_upper.green,at_upper.blue);
 }
 CASE("between") {
  union real_rgb_color middle=interpolated(1.f,0.f,0.f,1.f,0.5f);
  CHECK(invalid_results==0,"%d invalid results",invalid_results);
  CHECK(middle.red>0.99f && middle.blue>0.99f && middle.green<0.01f,"magenta expected: %f %f %f",
   middle.red,middle.green,middle.blue);
 }
 return 0;
}

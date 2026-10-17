#include <assert.h>
#include <stdio.h>

#include "../src/c/zones.h"

static void check(int activity, int rest, const int expected[4]) {
  for (int z = 0; z < 4; z++) {
    assert(zones_upper_limit_bpm(activity, z, 183, rest) == expected[z]);
  }
}

int main(void) {
  static const int walk_none[4] = {96, 117, 137, 157};
  static const int run_none[4] = {109, 128, 146, 164};
  static const int walk_50[4] = {69, 96, 123, 149};
  static const int run_50[4] = {89, 112, 136, 159};
  static const int walk_60[4] = {78, 103, 127, 152};
  static const int run_60[4] = {96, 118, 139, 160};
  static const int walk_70[4] = {86, 109, 132, 154};
  static const int run_70[4] = {103, 123, 143, 162};
  static const int walk_80[4] = {95, 116, 136, 157};
  static const int run_80[4] = {110, 128, 146, 164};

  check(0, 0, walk_none);
  check(1, 0, run_none);
  check(2, 0, run_none);
  check(0, 50, walk_50);
  check(1, 50, run_50);
  check(0, 60, walk_60);
  check(1, 60, run_60);
  check(2, 60, run_60);
  check(0, 70, walk_70);
  check(1, 70, run_70);
  check(0, 80, walk_80);
  check(1, 80, run_80);

  check(0, 150, walk_none);  // reserve de 33 bpm : retour a la methode actuelle
  assert(zones_upper_limit_bpm(1, 0, 183, 143) == 143 + 30 * 40 / 100);

  printf("OK : zones\n");
  return 0;
}

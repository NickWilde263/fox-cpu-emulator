#include "util/base.h"

float _fmod(float n1, float n2) {
  return (float) ((long long) (n1 * 100000) % (long long) (n2 * 100000) / 100000);
}

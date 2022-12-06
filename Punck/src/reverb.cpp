#include "reverb.h"

Reverb __attribute__((section (".ram_d2_data"))) reverb;
float __attribute__((section (".ram_d1_data"))) reverbMixBuffer[94000];



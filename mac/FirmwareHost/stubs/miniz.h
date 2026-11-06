#pragma once
// HOST_SIM miniz/tinfl surface. Decompression stubs return failure so firmware
// cover/map inflate paths no-op until a vendored miniz is linked.
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "miniz_tinfl.h"

#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO
#endif

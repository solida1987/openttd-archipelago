/*
 * Compilation unit for stb_vorbis implementation.
 * This file MUST NOT include OpenTTD's safeguards.h, as stb_vorbis
 * uses malloc/realloc/free internally which the safeguards redefine.
 */

/* Silence MSVC warnings in third-party code */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4245) /* signed/unsigned mismatch */
#pragma warning(disable: 4244) /* possible loss of data */
#pragma warning(disable: 4456) /* declaration hides previous local */
#pragma warning(disable: 4457) /* declaration hides function parameter */
#pragma warning(disable: 4701) /* potentially uninitialized variable */
#pragma warning(disable: 4702) /* unreachable code */
#pragma warning(disable: 4100) /* unreferenced formal parameter */
#pragma warning(disable: 4305) /* truncation from double to float */
#endif

#include "stb_vorbis.c"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

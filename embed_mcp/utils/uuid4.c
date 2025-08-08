// (‑●‑●)> dual licensed under the WTFPL v2 and MIT licenses
//   without any warranty.
//   by Gregory Pakosz (@gpakosz)
// https://github.com/gpakosz/uuid4

#if defined(__linux__)
  #if !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
  #endif
#endif

// in case you want to #include "uuid4.c" in a larger compilation unit
#if !defined(UUID_4H)
#include "uuid4.h"
#endif

#if !defined(UUID4_ASSERT)
#include <assert.h>
#define UUID4_ASSERT(expression) assert(expression)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// —————————————————————————————————————————————————————————————————————————————

// http://xoshiro.di.unimi.it/splitmix64.c
// Written in 2015 by Sebastiano Vigna (vigna@acm.org)
/*
   This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush.
*/
static inline uint64_t UUID4_PREFIX(splitmix64)(uint64_t* state)
{
  uint64_t z = (*state += 0x9E3779B97F4A7C15u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
  return z ^ (z >> 31);
}

// http://www.pcg-random.org/posts/developing-a-seed_seq-alternative.html
// Written in 2015 by Melissa O'Neil (oneill@pcg-random.org)
static inline uint32_t UUID4_PREFIX(hash)(uint32_t value)
{
  static uint32_t multiplier = 0x43b0d7e5u;

  value ^= multiplier;
  multiplier *= 0x931e8875u;
  value *= multiplier;
  value ^= value >> 16;

  return value;
}

static inline uint32_t UUID4_PREFIX(mix)(uint32_t x, uint32_t y)
{
  uint32_t result = 0xca01f9ddu * x - 0x4973f715u * y;
  result ^= result >> 16;
  return result;
}

// —————————————————————————————————————————————————————————————————————————————

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

UUID4_FUNCSPEC
void UUID4_PREFIX(seed)(uint64_t* state)
{
  static uint64_t state0 = 0;

  LARGE_INTEGER time;
  BOOL ok = QueryPerformanceCounter(&time);
  UUID4_ASSERT(ok);

  *state = state0++ + ((uintptr_t)&time ^ (uint64_t)time.QuadPart);

  uint32_t pid = (uint32_t)GetCurrentProcessId();
  uint32_t tid = (uint32_t)GetCurrentThreadId();

  *state = *state * 6364136223846793005u + ((uint64_t)(UUID4_PREFIX(mix)(UUID4_PREFIX(hash)(pid), UUID4_PREFIX(hash)(tid))) << 32);
  *state = *state * 6364136223846793005u + (uintptr_t)GetCurrentProcessId;
  *state = *state * 6364136223846793005u + (uintptr_t)UUID4_PREFIX(gen);
}

#elif defined(__linux__)

#if !defined(UUID4_CLOCK_ID)
  #define UUID4_CLOCK_ID CLOCK_MONOTONIC_RAW
#endif

#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

UUID4_FUNCSPEC
void UUID4_PREFIX(seed)(uint64_t* state)
{
  static uint64_t state0 = 0;

  struct timespec time;
  bool ok = clock_gettime(UUID4_CLOCK_ID, &time) == 0;
  UUID4_ASSERT(ok);

  *state = state0++ + ((uintptr_t)&time ^ (uint64_t)(time.tv_sec * 1000000000 + time.tv_nsec));

  uint32_t pid = (uint32_t)getpid();
  uint32_t tid = (uint32_t)syscall(SYS_gettid);
  *state = *state * 6364136223846793005u + ((uint64_t)(UUID4_PREFIX(mix)(UUID4_PREFIX(hash)(pid), UUID4_PREFIX(hash)(tid))) << 32);
  *state = *state * 6364136223846793005u + (uintptr_t)getpid;
  *state = *state * 6364136223846793005u + (uintptr_t)UUID4_PREFIX(gen);
}

#elif defined(__APPLE__)

#include <mach/mach_time.h>
#include <unistd.h>
#include <pthread.h>

UUID4_FUNCSPEC
void UUID4_PREFIX(seed)(uint64_t* state)
{
  static uint64_t state0 = 0;

  uint64_t time = mach_absolute_time();

  *state = state0++ + time;

  uint32_t pid = (uint32_t)getpid();
  uint64_t tid = (uint64_t)pthread_self();  // Use pthread_self() for compatibility
  *state = *state * 6364136223846793005u + ((uint64_t)(UUID4_PREFIX(mix)(UUID4_PREFIX(hash)(pid), UUID4_PREFIX(hash)((uint32_t)tid))) << 32);
  *state = *state * 6364136223846793005u + (uintptr_t)getpid;
  *state = *state * 6364136223846793005u + (uintptr_t)UUID4_PREFIX(gen);
}

#else

#error unsupported platform

#endif

#include <stdio.h>
#include <inttypes.h>

static void UUID4_PREFIX(randomize)(UUID4_STATE_T* state, UUID4_T* out)
{
  out->qwords[0] = UUID4_PREFIX(splitmix64)(state);
  out->qwords[1] = UUID4_PREFIX(splitmix64)(state);
}

UUID4_FUNCSPEC
void UUID4_PREFIX(gen)(UUID4_STATE_T* state, UUID4_T* out)
{
  UUID4_PREFIX(randomize)(state, out);

  out->bytes[6] = (out->bytes[6] & 0xf) | 0x40;
  out->bytes[8] = (out->bytes[8] & 0x3f) | 0x80;
}

UUID4_FUNCSPEC
bool UUID4_PREFIX(to_s)(const UUID4_T uuid, char* out, int capacity)
{
  static const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  static const int groups[] = { 8, 4, 4, 4, 12 };
  int b = 0;

  if (capacity < UUID4_STR_BUFFER_SIZE)
    return false;

  for (int i = 0; i < (int)(sizeof(groups) / sizeof(groups[0])); ++i)
  {
    for (int j = 0; j < groups[i]; j += 2)
    {
      uint8_t byte = uuid.bytes[b++];

      *out++ = hex[byte >> 4];
      *out++ = hex[byte & 0xf];
    }
    *out++ = '-';
  }

  *--out = 0;

  return true;
}

#ifdef __cplusplus
}
#endif

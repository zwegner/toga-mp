
// smp.h

#ifndef SMP_H
#define SMP_H

// includes

#include <csetjmp>
#include "move.h"
#include "search.h"
#include "util.h"

#ifdef SMP

//#define SMP_STATS
//#define SMP_DEBUG

#ifdef USE_ASM_x86

typedef volatile int lock_t[1];

// asm macros go here

#elif defined(_MSC_VER) // !USE_ASM_x86

#  include <windows.h>

typedef CRITICAL_SECTION lock_t;

#  define LOCK_INIT(l)			InitializeCriticalSection(l)
#  define LOCK_SET(l)			EnterCriticalSection(l)
#  define LOCK_CLEAR(l)			LeaveCriticalSection(l)
#  define LOCK_DESTROY(l)		DeleteCriticalSection(l)

#else // !_MSC_VER

#  ifdef __PPC__

#    include <libkern/OSAtomic.h>

typedef OSSpinLock lock_t[1];

#    define LOCK_INIT(l)		((l)[0] = 0)
#    define LOCK_SET(l)			(OSSpinLockLock(l))
#    define LOCK_CLEAR(l)		(OSSpinLockUnlock(l))
#    define LOCK_DESTROY(l)		((l)[0] = 0)

#  else

#    include <pthread.h>

typedef pthread_mutex_t lock_t[1];

#    define LOCK_INIT(l)		pthread_mutex_init((l), NULL)
#    define LOCK_SET(l)			pthread_mutex_lock(l)
#    define LOCK_CLEAR(l)		pthread_mutex_unlock(l)
#    define LOCK_DESTROY(l)		pthread_mutex_destroy(l)

#  endif // !__PPC__ 

#endif // !_MSC_VER

// constants

const int SplitMax = 2;

// types

struct split_t {
   struct board_t board[ThreadMax];
   int parent;
   volatile bool child[ThreadMax];
   volatile bool active;
   volatile int child_count;
   volatile bool stop;
   int alpha;
   int beta;
   int depth;
   int height;
   struct sort_t *sort;
   int node_type;
   bool mate_threat;
   mv_t *pv;
   lock_t lock;
};

struct thread_t {
   volatile bool running;
   volatile bool idle;
   volatile bool work_available;
   volatile bool split_stop;
   split_t *split_point;
   sint64 node_nb;
#ifdef _MSC_VER
   HANDLE thread_id;
#else
   pthread_t thread_id;
#endif
};

// variables

extern int thread_count;
extern split_t SplitPoint[ThreadMax][SplitMax];
extern thread_t Thread[ThreadMax];
extern int SplitCount[ThreadMax];
extern int idle_count;
extern lock_t SmpLock;
extern lock_t IOLock;
#ifdef SMP_STATS
extern int split_nb;
extern int stop_nb;
#endif

// functions

extern void smp_init(int threads);
extern int split(board_t * board, int alpha, int beta, int depth, int height, sort_t * sort, int node_type, bool mate_threat);
extern int split(board_t * board, int alpha, int beta, int depth, int height, mv_t * pv, sort_t * sort, int node_type, bool mate_threat);
int split_sort_next(board_t * board, sort_t * sort, int * alpha, int * sort_value);

#else // SMP

#  define LOCK_INIT(l)
#  define LOCK_SET(l)
#  define LOCK_CLEAR(l)
#  define LOCK_DESTROY(l)

extern struct thread_t {
   sint64 node_nb;
} Thread[ThreadMax];

#endif // !SMP

#endif // smp.h

// end of smp.h


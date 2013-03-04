
// smp.cpp

// includes

#include "board.h"
#include "protocol.h"
#include "pv.h"
#include "search.h"
#include "search_full.h"
#include "smp.h"
#include "sort.h"
#include "util.h"
#include "value.h"

#ifdef SMP

// functions

#ifdef _MSC_VER
static DWORD WINAPI thread_start(LPVOID arg);
#else
static void * thread_start(void * arg);
#endif // _MSC_VER

static int thread_wait(int id, split_t * split_point);

// globals
int thread_count;
split_t SplitPoint[ThreadMax][SplitMax];
thread_t Thread[ThreadMax];
int SplitCount[ThreadMax];
int idle_count;
lock_t SmpLock;
lock_t IOLock;

#  ifdef SMP_STATS
int split_nb;
int stop_nb;
#endif

// init_smp()

void smp_init(int threads) {
   static bool initialized = false;
   int thread;
   int split_point;

   if (initialized) {
      // Terminate threads from the last initialization.
      for (thread = 0; thread < thread_count; thread++) {
#ifdef _MSC_VER
         TerminateThread(Thread[thread].thread_id, 0);
#else
         pthread_cancel(Thread[thread].thread_id);
#endif
      }
   } else {
      LOCK_INIT(SmpLock);
      LOCK_INIT(IOLock);
      for (thread = 0; thread < ThreadMax; thread++) {
         for (split_point = 0; split_point < SplitMax; split_point++) {
            SplitPoint[thread][split_point].active = false;
            SplitPoint[thread][split_point].child_count = 0;
            LOCK_INIT(SplitPoint[thread][split_point].lock);
         }
      }
   }

   thread_count = threads;
   idle_count = threads - 1;

   for (thread = 0; thread < thread_count; thread++) {
      Thread[thread].work_available = false;
      Thread[thread].running = false;
      SplitCount[thread] = 0;
      if (thread > 0) {
#ifdef _MSC_VER
         DWORD id[1];
         Thread[thread].thread_id = CreateThread(NULL, 0, thread_start, (LPVOID)&thread, 0, id);
#else
         pthread_create(&Thread[thread].thread_id, NULL, thread_start, (void *)&thread);
#endif
         while (!Thread[thread].running);
         Thread[thread].idle = true;
      }
   }
   initialized = true;
}

// thread_start()

#ifdef _MSC_VER
static DWORD WINAPI thread_start(LPVOID arg) {
#else
void * thread_start(void * arg) {
#endif

   int id = *(int *)arg;
   Thread[id].running = true;
   // We wait for a search to start first before we enter the idle loop. This is because the thread will constantly spin in the loop, which consumes lots of CPU time.
   while (true) {
      thread_wait(id, NULL);
   }
#ifdef _MSC_VER
   return 0;
#else
   return NULL;
#endif
}

// thread_wait()

int thread_wait(int id, split_t * split_point) {
   board_t *board;
   int value, alpha, beta, depth, height, node_type;
   mv_t pv[HeightMax];
   sort_t *sort;
   bool mate_threat;

   while (1) {
      while (!Thread[id].work_available && (split_point == NULL || split_point->child_count > 0)) ;

      Thread[id].idle = false;
      if (!Thread[id].work_available) {
         ASSERT(split_point != NULL);
         return split_point->alpha;
      }
      Thread[id].work_available = false;
      alpha = Thread[id].split_point->alpha;
      beta = Thread[id].split_point->beta;
      depth = Thread[id].split_point->depth;
      height = Thread[id].split_point->height;
      sort = Thread[id].split_point->sort;
      node_type = Thread[id].split_point->node_type;
      mate_threat = Thread[id].split_point->mate_threat;
      // Set the place to return in case the thread needs to stop
#ifdef SMP_DEBUG
      send("full_split called(id=%i,a=%i,b=%i)\n",id,alpha,beta);
#endif
      value = full_split(&Thread[id].split_point->board[id],SearchStack[id],alpha,beta,depth,height,sort,pv,node_type,mate_threat);
#ifdef SMP_DEBUG
      send("full_split returned=%i(id=%i,a=%i,b=%i)\n",value,id,alpha,beta);
#endif
      // Update the split point
      LOCK_SET(Thread[id].split_point->lock);
      // Check to see if we got a new best score. Make sure another thread hasn't failed high already.
      if (!Thread[id].split_point->stop && value > Thread[id].split_point->alpha) {
         if (value >= Thread[id].split_point->beta) {
#ifdef SMP_STATS
            LOCK_SET(SmpLock);
            stop_nb++;
            LOCK_CLEAR(SmpLock);
#endif
            // We stop the other threads before updating the bounds so no processors get any invalid parameters.
            Thread[id].split_point->stop = true;
         }
         Thread[id].split_point->alpha = value;
         pv_copy(Thread[id].split_point->pv, pv);
      }
      Thread[id].split_point->child_count--;
      Thread[id].split_point->child[id] = false;
      LOCK_CLEAR(Thread[id].split_point->lock);
      if (SearchInfo->stop)
         return ValueNone;
      if (split_point != NULL && split_point->child_count == 0)
         return split_point->alpha;
      LOCK_SET(SmpLock);
      idle_count++;
      LOCK_CLEAR(SmpLock);
      Thread[id].idle = true;
   }
}

// split()

int split(board_t * board, int alpha, int beta, int depth, int height, mv_t * pv, sort_t * sort, int node_type, bool mate_threat) {
   int thread;
   int value;
   split_t *split_point;
   split_t *old_split_point;

   if (SplitCount[board->id] == SplitMax)
      return ValueNone;
   split_point = &SplitPoint[board->id][SplitCount[board->id]++];

   LOCK_SET(SmpLock);
   // Check that there are still idle threads.
   if (idle_count == 0) {
      LOCK_CLEAR(SmpLock);
      return ValueNone;
   }
   ASSERT(!Thread[board->id].idle);
   old_split_point = Thread[board->id].split_point;
   Thread[board->id].idle = true;
   idle_count++;

   // Set up the split point
#ifdef SMP_STATS
   split_nb++;
#endif
   split_point->active = true;
   split_point->stop = false;
   split_point->alpha = alpha;
   split_point->beta = beta;
   split_point->depth = depth;
   split_point->height = height;
   split_point->pv = pv;
   split_point->sort = sort;
   split_point->node_type = node_type;
   split_point->mate_threat = mate_threat;
   split_point->parent = board->id;

   split_point->child_count = 0;
   for (thread = 0; thread < thread_count; thread++) {
      if (Thread[thread].idle && !Thread[thread].work_available) {
         idle_count--;
         split_point->child_count++;
         split_point->child[thread] = true;
         board_copy(&split_point->board[thread], board);
         split_point->board[thread].id = thread;
         Thread[thread].split_point = split_point;
         Thread[thread].work_available = true;
      } else
         split_point->child[thread] = false;
   }
   // Tell all the processors where to go.
/*   for (thread = 0; thread < thread_count; thread++) {
      if (split_point->child[thread]) {
      }
   }
  */ LOCK_CLEAR(SmpLock);

   // The split point is set up. Now enter the wait function so that we can search and then wait for the other threads.
   value = thread_wait(board->id, split_point);
   // Destroy the split point.
   split_point->active = false;
   split_point->stop = false;
   SplitCount[board->id]--;

   // Restore any split-related information that might have been in this thread.
   Thread[board->id].split_point = old_split_point;
   return value;
}

// split_sort_next()

int split_sort_next(board_t * board, sort_t * sort, int * alpha, int * sort_value) {
   int move;
   // Get a move from the parent's move list.
   LOCK_SET(Thread[board->id].split_point->lock);
   move = sort_next(sort);
   *sort_value = sort->value;
   if (Thread[board->id].split_point->alpha > *alpha)
      *alpha = Thread[board->id].split_point->alpha;
   LOCK_CLEAR(Thread[board->id].split_point->lock);
   return move;
}

#else // SMP

thread_t Thread[ThreadMax];

#endif // !SMP

// end of smp.cpp


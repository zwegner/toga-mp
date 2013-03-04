
// search_full.h

#ifndef SEARCH_FULL_H
#define SEARCH_FULL_H

// includes

#include "board.h"
#include "sort.h"
#include "util.h"

// functions

extern void search_full_init (list_t * list, board_t * board);
extern int  search_full_root (list_t * list, board_t * board, int depth, int search_type);
#ifdef SMP
extern int  full_split       (board_t * board, search_param_t * search_stack, int alpha, int beta, int depth, int height, sort_t * sort, mv_t pv[], int node_type, bool mate_threat);
#endif

#endif // !defined SEARCH_FULL_H

// end of search_full.h


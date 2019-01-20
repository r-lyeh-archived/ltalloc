#pragma once

#include <stdlib.h>  /*a more portable std::size_t definition than stddef.h itself*/
#ifdef __cplusplus
extern "C" {
#endif
void*  ltmalloc(std::size_t);
void   ltfree(void*);
void*  ltrealloc(void*, std::size_t);
void*  ltcalloc(std::size_t, std::size_t);
void*  ltmemalign(std::size_t, std::size_t);
void   ltsqueeze(std::size_t); /*return memory to system (see README.md)*/
std::size_t ltmsize(void*);
void   ltonthreadexit();
#ifdef __cplusplus
}
#endif

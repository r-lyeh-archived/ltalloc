void*  ltalloc(size_t);
void   ltfree(void*);
size_t ltalloc_usable_size(void*);
void   ltalloc_squeeze(size_t pad);//return memory to the system (see http://code.google.com/p/ltalloc/wiki/Main#rmem for more details)

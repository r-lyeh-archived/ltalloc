## Overview ##
Simple (yet very efficient) multi-threaded memory allocator based on free lists.<br>
It is best suited for applications doing a lot of small (<256B) memory allocations (as usually C++ stl containers do), and from many simultaneously running threads.<br>
<h2>Features</h2>
<ul><li>O(1) cost for alloc, free (for blocks of size <56KB)<br>
</li><li>Low fragmentation<br>
</li><li>Near zero size overhead for small allocations (no header per allocation, just one common 64 bytes header for all blocks inside 64KB chunk)<br>
</li><li>High efficiency and scalability for multi-threaded programs (almost lock-free, at maximum one spin-lock per 256 alloc/free calls for small allocations, even if all memory allocated in one thread then freed inside another thread)<br>
<h2>Usage</h2>
To use ltalloc in your C++ application just add <a href='http://ltalloc.googlecode.com/hg/ltalloc.cc'>ltalloc.cc</a> source file into your project's source files list. It overrides global operators new and delete, which is a fully C++ standard compliant way to replace almost all memory alocation routines in C++ applications (as stl container's default allocators call global operator new). But if this way is not well suilable for you, the other options of plug-in ltalloc into your application are exists as well. Actually, ltalloc.cc source is written in C (and overriding of operators new/delete is disabled automatically if <code>__cplusplus</code> is not defined), so it can be compiled both as C and C++ code.<br>
<a href='http://code.google.com/p/ltalloc/wiki/Main'>Go to wiki page for more info.</a>
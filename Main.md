# Introduction #
Almost every C++ programmer knows about opportunity to substitute your own custom allocator for the default of stl containers, but almost no one actually use this opportunity. :)<br>
And I agree, that this feature is become obviously almost unusable when dealing with large enough real projects, especially when a lot of third-party C++ libraries used, and you quickly realize that containers with different allocators are just incompatible with each other<a href='Hidden comment:  (especially relevant to std::string)'></a>.<br>
After all, why custom allocators (for containers) are actually may needed for?<br>
I do not believe that control over memory allocation per container can give at least some benefits. I mean, that control over memory allocation should be done not per container, but per {thread, blocksize} pair. <a href='Hidden comment: So the main meaningful advantage is when every memory request is identical in size. '></a>Otherwise, memory obtained from a custom allocator is not shared with other objects of the same size (lead to memory wastage), and there are potential multi-threading issues. So, when you think of usefulness of custom allocators there are more questions than answers.<br>
After all, I thought what if specific pool can be chosen at compile-time when the size of requested memory block is known beforehand. Then, a single application-wide allocator can completely eliminate any need for custom allocators! This idea looks like somewhat unrealistic, but still I decided to try implementing it.<br>
<br>
<h1>Design Principles</h1>

<b>1. Inlining and compile-time size class calculation.</b>

<a href='Hidden comment: The main reason why custom (mainly pool-based) allocators are fast is because of their simplicity their code often can be inlined.'></a><br>
When code of allocation function is small enough, compiler can inline it to eliminate need of call. And also, when size of object is known beforehand, it would be very good if size class (i.e. specific pool to satisfy that allocation request) can be chosen at compile-time. To make this possible, computation of the size class itself should rely only on built-in operators (no asm or external function calls) and must not access any dynamically calculated data. After all, application's source code should be compiled with link-time optimization turned on (/GL for MSVC, -flto for GCC/Clang, and -ipo for ICC) to make possible the inlining of operator new calls. As a sample output, here is a result of compilation of single statement "new std::array<int, 10>":<br>
<table cellpadding='4' cellspacing='0' border='1'><thead align='center'><tr><td>Source Code</td><td>MSVC 2012 compiler 32-bit asm output</td><td>GCC 4.8.1 64-bit asm output</td></tr></thead><tr><td>
<pre><code>NOINLINE void *test_function()<br>
{<br>
    return new std::array&lt;int, 10&gt;;<br>
}<br>
<br>
void *operator new(size_t size) { return ltalloc&lt;true&gt;(size); }<br>
void *operator new(size_t size, const std::nothrow_t&amp;) { return ltalloc&lt;false&gt;(size); }<br>
<br>
template &lt;bool throw_&gt; static void *ltalloc(size_t size)<br>
{<br>
    unsigned int sizeClass = get_size_class(size); //computed at compile-time<br>
    ThreadCache *tc = &amp;threadCache[sizeClass];<br>
    FreeBlock *fb = tc-&gt;freeList;<br>
    if (likely(fb))<br>
    {<br>
        tc-&gt;freeList = fb-&gt;next;<br>
        tc-&gt;counter++;<br>
        return fb;<br>
    }<br>
    else<br>
        return fetch_from_central_cache&lt;throw_&gt;(size, tc, sizeClass);<br>
}<br>
</code></pre>
</td><td>
<pre><code>mov         eax,dword ptr fs:[0000002Ch]<br>
mov         edx,dword ptr [eax]<br>
add         edx,128h ;296=sizeClass*sizeof(tc[0])<br>
mov         eax,dword ptr [edx]<br>
test        eax,eax<br>
je          L1 ; probability is just about 1%<br>
mov         ecx,dword ptr [eax]<br>
inc         dword ptr [edx+8]<br>
mov         dword ptr [edx],ecx<br>
ret<br>
</code></pre>
<font color='gray'><pre>
L1:<br>
push        18h ; =24 (size class)<br>
mov         ecx,28h ; =40 (bytes size)<br>
call        fetch_from_central_cache<1> (0851380h)<br>
add         esp,4<br>
ret<br>
</pre></font></td><td>
<pre><code>mov    rdx,0xffffffffffffe7a0<br>
mov    rax,QWORD PTR fs:[rdx+0x240]<br>
test   rax,rax<br>
je     L1 ; prob 1%<br>
mov    rcx,QWORD PTR [rax]<br>
add    DWORD PTR fs:[rdx+0x250],0x1<br>
mov    QWORD PTR fs:[rdx+0x240],rcx<br>
ret<br>
</code></pre>
<font color='gray'><pre>
L1:<br>
add    rdx,QWORD PTR fs:0x0<br>
mov    edi,0x28 ; =40 (bytes size)<br>
lea    rsi,[rdx+0x240]<br>
mov    edx,0x18 ; =24 (size class)<br>
jmp    <_Z24fetch_from_central_cache...><br>
</pre></font></td></tr>
<tr><td>As you can see, the "new array" statement takes a just 9 asm instructions (or even 7 for GCC).<br>
<br>
Here is another example - function that do many allocations in a loop to create a singly-linked list of arrays:<br>
</td></tr><tr><td>
<pre><code>NOINLINE void *create_list_of_arrays()<br>
{<br>
    struct node<br>
    {<br>
        node *next;<br>
        std::array&lt;int, 9&gt; arr;<br>
    } *p = NULL;<br>
<br>
    for (int i=0; i&lt;1000; i++)<br>
    {<br>
        node *n = new node;<br>
        n-&gt;next = p;<br>
        p = n;<br>
    }<br>
<br>
    return p;<br>
}<br>
</code></pre>
</td><td>
<font color='gray'><pre>
mov         eax,dword ptr fs:[0000002Ch]<br>
push        ebx<br>
push        esi<br>
mov         esi,dword ptr [eax]<br>
push        edi<br>
xor         edi,edi<br>
add         esi,128h<br>
mov         ebx,3E8h    ; =1000       </pre></font>
<pre><code>L2:<br>
mov         eax,dword ptr [esi]<br>
test        eax,eax<br>
je          L1 ; prob 1%<br>
mov         ecx,dword ptr [eax]<br>
inc         dword ptr [esi+8]<br>
mov         dword ptr [esi],ecx<br>
dec         ebx                  ; i++<br>
mov         dword ptr [eax],edi  ; n-&gt;next = p;<br>
mov         edi,eax              ; p = n;<br>
jne         L2                   ; if (i&lt;1000) goto L2<br>
</code></pre>
<font color='gray'><pre>
pop         edi<br>
pop         esi<br>
pop         ebx<br>
ret<br>
L1:<br>
...</pre></font></td><td>
<font color='gray'><pre>
...    </pre></font>
<pre><code>L2:<br>
mov    r12,rax                      ; p = n;<br>
mov    rax,QWORD PTR fs:[rbx+0x258]<br>
test   rax,rax<br>
je     L1 ; prob 1%<br>
mov    rdx,QWORD PTR [rax]<br>
add    DWORD PTR fs:[rbx+0x268],0x1<br>
mov    QWORD PTR fs:[rbx+0x258],rdx<br>
L3:<br>
sub    ebp,0x1                      ; i++<br>
mov    QWORD PTR [rax],r12          ; n-&gt;next = p;<br>
jne    L2                           ; if (i&lt;1000) goto L2<br>
</code></pre>
<font color='gray'><pre>
add    rsp,0x8<br>
pop    rbx<br>
pop    rbp<br>
pop    r12<br>
pop    r13<br>
ret<br>
L1:<br>
mov    edx,0x19<br>
mov    rsi,r13<br>
mov    edi,0x30<br>
call   <_Z24fetch_from_central_cache...><br>
jmp    L3   </pre></font></td></tr></table>

For this case, compiler has optimized a whole "new node;" statement inside the loop to a mere 6 asm instructions!<br>
I think, that execution speed of this resulting asm-code (generated for general enough C++ code) can quite compete with a good custom pool-based allocator implementation.<br>
(Although, inlining can give some performance improvement, it is not extremely necessary, and even a regular call of ltalloc function still will be working very fast<a href='Hidden comment:  - just about 30% slower without inlining according to the test results below'></a>.)<br>
<br>
<b>2. Thread-efficiency and scalability.</b>

To achieve high multithreading efficiency ltalloc uses an approach based on <a href='http://gperftools.googlecode.com/svn/trunk/doc/tcmalloc.html'>TCMalloc</a> (I didn't take any code from TCMalloc, but rather just a main idea).<br>
So, there is per-thread cache (based on native thread_local variables). And all allocations (except the large ones, >56KB) are satisfied from the thread-local cache (just simple singly linked list of free blocks per size class).<br>
If the free list of the thread cache is empty, then batch (256 or less) of memory blocks is fetched from a central free list (list of batches, shared by all threads) for this size class, placed in the thread-local free list, and one of blocks of this batch returned to the application. When an object is deallocated, it is inserted into the appropriate free list in the current thread's thread cache. If the thread cache free list now reaches a certain number of blocks (256 or less, depending on the block size), then a whole free list of blocks moved back to the central list as a single batch.<br>
This simple batching approach alone gives enough scalability (i.e. with applicable low contention) for theoretically up to 128-core SMP system if memory allocation operations will be interleaved with at least 100 CPU cycles of another work (this is a rough average of single operation of moving batch to the central cache or fetch it from). And this approach especially effective for a producer�consumer pattern, when memory allocated in one thread then released on another.<br>
<br>
<b>3. Compact layout.</b>

While most memory allocators store at least one pointer at the beginning (header) of each memory block allocated (so, for example, each 16 bytes (or even 13) block request actually wastes 32 bytes, because of 16B-alignment requirement), ltalloc rather just keeps a small header (64 bytes) per chunk (64KB by default), while all allocated blocks are just stored contiguously inside chunk without any metadata interleaved, which is much more efficient for small memory allocations.<br>
So, if there is no any pointer at beginning of each block, there should be another way to find metadata for allocated objects<a href='Hidden comment: distinguish block obtained directly from the system, and that belongs to some chunk'></a>.<br>
Some allocators to solve this problem keeps sbrk pointer, but this has such drawbacks as necessity to emulate sbrk on systems that don't support it, and that memory allocated up to sbrk limit can not be effectively returned to the system. So I decided to use another approach: all big blocks (obtained directly from the system) are always aligned to multiples of the chunk size, thus all blocks within any chunk will be not aligned as opposed to sysblocks, and this check can be done with simple if (uintptr_t(p)&(CHUNK_SIZE-1)), and pointer to chunk header is calculated as (uintptr_t)p & ~(CHUNK_SIZE-1). (Similar approach used in jemalloc.)<br>
Finally, mapping of block size to corresponding size class is done via a simple approach of rounding up to the nearest "subpower" of two (i.e. 2<sup>n</sup>, 1.25<code>*</code>2<sup>n</sup>, 1.5<code>*</code>2<sup>n</sup>, and 1.75<code>*</code>2<sup>n</sup> by default, but this can be configured, and it can be reduced to exact power of two sizes), so there are 51 size classes (for all small blocks <56KB), and size overhead (internal fragmentation) is no more than 25%, in average 12%.<br>
As a free bonus, this approach combined with contiguously blocks placement gives a "perfect alignment feature" for all memory pointers returned (see <a href='#5._Why_there_is_no_separate_function_to_allocate_aligned_memory.md'>below</a>).<br>
<br>
<h1>FAQ</h1>
<h3>1. Is ltalloc faster than all other general purpose memory allocators?</h3>
Yes, of course, why else to start writing own memory allocator. :-)<br>
But, joking aside, let's look at the performance comparison table below (results obtained with <a href='http://code.google.com/p/ltalloc/source/browse/test.cpp?repo=wiki'>this simple test</a>, which is just continuously allocating and freeing memory blocks of 128 bytes size from simultaneously running threads).<br>
(Results are given in millions of operations (pairs of alloc+free) per second for a single thread, i.e. to obtain a total amount of operations/sec, you should multiply corresponding result by the number of threads.)<br>
<table cellpadding='4' cellspacing='0' border='1'><tbody align='center'>
<tr><td>Sys. Configuration</td>  <td></td><td>i3 M350 (2.3 GHz)<br />Windows 7</td>                            <td></td><td>Core2 Quad Q8300<br />Windows XP SP3</td>                         <td></td><td><span title='actually, 2600K, but I disabled Turbo Boost in the BIOS'>i7 2600</span> (3.4 GHz)<br />Windows 7</td> <td></td><td>2x Xeon E5620 (2.4 GHz, 4 cores x 2)<br />Windows Server 2008 <code>R2</code></td>                          <td></td><td>2x Xeon E5620 (2.4 GHz, 4 cores x 2)<br />Debian GNU/Linux 6.0.6 (squeeze)</td>                                        </tr>
<tr><td>Allocator \ Threads</td> <td></td><td>1</td><td>2</td><td>4 (HT)</td>                                             <td></td><td>1</td><td>2</td><td>4</td>                                                   <td></td><td>1</td><td>2</td><td>4</td><td>8 (HT)</td>                                                                <td></td><td>1</td><td>2</td><td>4</td><td>8</td><td>16 (HT)</td>                                                                              <td></td><td>1</td><td>2</td><td>4</td><td>8</td><td>16 (HT)</td>                                                                              </tr> <tr><td align='left'>default</td>
<blockquote><td></td><td>10.0<br />(<b>6.97</b>)</td><td>8.4<br />(<b>8.04</b>)</td><td>5.3<br />(<b>9.53</b>)</td>    <td></td><td>8.3<br />(<b>12.61</b>)</td><td>1.3<br />(<b>79.23</b>)</td><td>0.4<br />(<b>218.75</b>)</td>  <td></td><td>16.0<br />(<b>10.31</b>)</td><td>15.2<br />(<b>10.86</b>)</td><td>13.3<br />(<b>10.26</b>)</td><td>8.5<br />(<b>10.73</b>)</td>  <td></td><td>11.6<br />(<b>9.53</b>)</td><td>11.2<br />(<b>9.88</b>)</td><td>10.8<br />(<b>9.70</b>)</td><td>10.7<br />(<b>6.21</b>)</td><td>5.8<br />(<b>10.45</b>)</td>    <td></td><td>14.9<br />(<b>8.49</b>)</td><td>14.9<br />(<b>8.46</b>)</td><td>15.3<br />(<b>8.25</b>)</td><td>4.8<br />(<b>25.48</b>)</td><td>0.8<br />(<b>79.00</b>)</td>    </tr> <tr><td align='left'><a href='http://www.nedprod.com/programs/portable/nedmalloc/'>nedmalloc</a></td>
<td></td><td>14.2<br />(<b>4.91</b>)</td><td>13.1<br />(<b>5.15</b>)</td><td>7.9<br />(<b>6.39</b>)</td>   <td></td><td>15.6<br />(<b>6.71</b>)</td><td>15.6<br />(<b>6.60</b>)</td><td>14.0<br />(<b>6.25</b>)</td>   <td></td><td>22.4<br />(<b>7.37</b>)</td><td>22.3<br />(<b>7.40</b>)</td><td>19.5<br />(<b>6.99</b>)</td><td>13.0<br />(<b>7.02</b>)</td>     <td></td><td>16.7<br />(<b>6.62</b>)</td><td>16.6<br />(<b>6.67</b>)</td><td>16.4<br />(<b>6.39</b>)</td><td>10.7<br />(<b>6.21</b>)</td><td>8.4<br />(<b>7.21</b>)</td>     <td></td><td>22.5<br />(<b>5.62</b>)</td><td>21.3<br />(<b>5.92</b>)</td><td>21.8<br />(<b>5.79</b>)</td><td>19.6<br />(<b>6.24</b>)</td><td>11.3<br />(<b>5.59</b>)</td>    </tr> <tr><td align='left'><a href='http://www.hoard.org/'>Hoard</a></td>
<td></td><td>32.2<br />(<b>2.16</b>)</td><td>31.3<br />(<b>2.16</b>)</td><td>23.8<br />(<b>2.12</b>)</td>  <td></td><td>44.7<br />(<b>2.34</b>)</td><td>44.4<br />(<b>2.32</b>)</td><td>40.3<br />(<b>2.17</b>)</td>   <td></td><td>77.6<br />(<b>2.13</b>)</td><td>76.3<br />(<b>2.16</b>)</td><td>64.0<br />(<b>2.13</b>)</td><td>43.6<br />(<b>2.09</b>)</td>     <td></td><td>57.5<br />(<b>1.92</b>)</td><td>56.2<br />(<b>1.97</b>)</td><td>54.6<br />(<b>1.92</b>)</td><td>34.2<br />(<b>1.94</b>)</td><td>31.9<br />(<b>1.90</b>)</td>    <td></td><td>31.2<br />(<b>4.05</b>)</td><td>30.4<br />(<b>4.14</b>)</td><td>27.4<br />(<b>4.61</b>)</td><td>25.3<br />(<b>4.83</b>)</td><td>16.4<br />(<b>3.85</b>)</td>    </tr> <tr><td align='left'><a href='http://www.canonware.com/jemalloc/'>jemalloc</a></td>
<td></td><td>11.5<br />(<b>6.06</b>)</td><td>11.0<br />(<b>6.14</b>)</td><td>7.0<br />(<b>7.21</b>)</td>   <td></td><td>13.4<br />(<b>7.81</b>)</td><td>13.3<br />(<b>7.74</b>)</td><td>5.0<br />(<b>17.50</b>)</td>   <td></td><td>18.2<br />(<b>9.07</b>)</td><td>18.1<br />(<b>9.12</b>)</td><td>10.1<br />(<b>13.50</b>)</td><td>7.4<br />(<b>12.32</b>)</td>    <td></td><td>14.1<br />(<b>7.84</b>)</td><td>14.0<br />(<b>7.91</b>)</td><td>6.7<br />(<b>15.64</b>)</td><td>4.4<br />(<b>15.09</b>)</td><td>2.9<br />(<b>20.90</b>)</td>    <td></td><td>30.1<br />(<b>4.20</b>)</td><td>30.0<br />(<b>4.20</b>)</td><td>30.1<br />(<b>4.20</b>)</td><td>27.9<br />(<b>4.38</b>)</td><td>16.2<br />(<b>3.90</b>)</td>    </tr> <tr><td align='left'><a href='http://gperftools.googlecode.com/svn/trunk/doc/tcmalloc.html'>TCMalloc</a></td>
<td></td><td>15.5<br />(<b>4.50</b>)</td><td>13.9<br />(<b>4.86</b>)</td><td>8.9<br />(<b>5.67</b>)</td>   <td></td><td>17.0<br />(<b>6.16</b>)</td><td>16.7<br />(<b>6.17</b>)</td><td>15.4<br />(<b>5.68</b>)</td>   <td></td><td>34.2<br />(<b>4.82</b>)</td><td>34.0<br />(<b>4.85</b>)</td><td>28.2<br />(<b>4.84</b>)</td><td>18.1<br />(<b>5.04</b>)</td>     <td></td><td>21.6<br />(<b>5.12</b>)</td><td>20.7<br />(<b>5.35</b>)</td><td>20.1<br />(<b>5.21</b>)</td><td>13.6<br />(<b>4.88</b>)</td><td>10.2<br />(<b>5.94</b>)</td>    <td></td><td>36.5<br />(<b>3.47</b>)</td><td>36.4<br />(<b>3.46</b>)</td><td>33.5<br />(<b>3.77</b>)</td><td>31.4<br />(<b>3.89</b>)</td><td>18.6<br />(<b>3.40</b>)</td>    </tr> <tr><td align='left'>ltalloc (w/o LTO)</td>
<td></td><td>31.9<br />(<b>2.18</b>)</td><td>31.0<br />(<b>2.18</b>)</td><td>20.8<br />(<b>2.43</b>)</td>  <td></td><td>50.9<br />(<b>2.06</b>)</td><td>50.9<br />(<b>2.02</b>)</td><td>44.5<br />(<b>1.97</b>)</td>   <td></td><td>81.6<br />(<b>2.02</b>)</td><td>79.7<br />(<b>2.07</b>)</td><td>67.4<br />(<b>2.02</b>)</td><td>48.1<br />(<b>1.90</b>)</td>     <td></td><td>62.2<br />(<b>1.78</b>)</td><td>62.5<br />(<b>1.77</b>)</td><td>62.1<br />(<b>1.69</b>)</td><td>36.3<br />(<b>1.83</b>)</td><td>31.1<br />(<b>1.95</b>)</td>    <td></td><td>91.1<br />(<b>1.39</b>)</td><td>91.0<br />(<b>1.38</b>)</td><td>90.9<br />(<b>1.39</b>)</td><td>84.8<br />(<b>1.44</b>)</td><td>46.8<br />(<b>1.35</b>)</td>    </tr> <tr><td align='left'><a href='http://locklessinc.com/downloads/'>lockless</a></td>
<td></td><td>43.3<br />(<b>1.61</b>)</td><td>40.3<br />(<b>1.67</b>)</td><td>30.2<br />(<b>1.67</b>)</td>  <td></td><td>The results are not available<br />(Windows Vista required)</td>  <td></td><td>88.7<br />(<b>1.86</b>)</td><td>75.7<br />(<b>2.18</b>)</td><td>75.4<br />(<b>1.81</b>)</td><td>50.9<br />(<b>1.79</b>)</td>     <td></td><td>60.9<br />(<b>1.81</b>)</td><td>61.1<br />(<b>1.81</b>)</td><td>60.3<br />(<b>1.74</b>)</td><td>39.9<br />(<b>1.66</b>)</td><td>33.5<br />(<b>1.81</b>)</td>    <td></td><td>114.4<br />(<b>1.11</b>)</td><td>107.6<br />(<b>1.17</b>)</td><td>107.5<br />(<b>1.17</b>)</td><td>102.0<br />(<b>1.20</b>)</td><td>55.9<br />(<b>1.13</b>)</td></tr> <tr><td align='left'>ltalloc</td>
<td></td><td>69.7<br />(<b>1.00</b>)</td><td>67.5<br />(<b>1.00</b>)</td><td>50.5<br />(<b>1.00</b>)</td>  <td></td><td>104.7<br />(<b>1.00</b>)</td><td>103.0<br />(<b>1.00</b>)</td><td>87.5<br />(<b>1.00</b>)</td> <td></td><td>165.0<br />(<b>1.00</b>)</td><td>165.0<br />(<b>1.00</b>)</td><td>136.4<br />(<b>1.00</b>)</td><td>91.2<br />(<b>1.00</b>)</td>  <td></td><td>110.5<br />(<b>1.00</b>)</td><td>110.7<br />(<b>1.00</b>)</td><td>104.8<br />(<b>1.00</b>)</td><td>66.4<br />(<b>1.00</b>)</td><td>60.6<br />(<b>1.00</b>)</td> <td></td><td>126.5<br />(<b>1.00</b>)</td><td>126.0<br />(<b>1.00</b>)</td><td>126.3<br />(<b>1.00</b>)</td><td>122.3<br />(<b>1.00</b>)</td><td>63.2<br />(<b>1.00</b>)</td></tr> <tr><td align='left'><a href='http://www.boost.org/doc/libs/release/libs/pool/doc/html/boost/fast_pool_allocator.html' title='boost::fast_pool_allocator with null_mutex'>fast_pool_allocator</a></td>
<td></td><td>116.5<br />(<b>0.60</b>)</td><td>58.3<br />(<b>1.16</b>)</td><td>12.5<br />(<b>4.04</b>)</td> <td></td><td>152.0<br />(<b>0.69</b>)</td><td>22.5<br />(<b>4.58</b>)</td><td>5.6<br />(<b>15.63</b>)</td>  <td></td><td>277.8<br />(<b>0.59</b>)</td><td>48.7<br />(<b>3.39</b>)</td><td>25.1<br />(<b>5.43</b>)</td><td>11.8<br />(<b>7.73</b>)</td>    <td></td><td>189.8<br />(<b>0.58</b>)</td><td>62.0<br />(<b>1.79</b>)</td><td>29.9<br />(<b>3.51</b>)</td><td>3.9<br />(<b>17.03</b>)</td><td>2.0<br />(<b>30.30</b>)</td>   <td></td><td>156.4<br />(<b>0.81</b>)</td><td>11.5<br />(<b>10.96</b>)</td><td>3.1<br />(<b>40.74</b>)</td><td>1.3<br />(<b>94.08</b>)</td><td>1.3<br />(<b>48.62</b>)</td>  </tr>
</tbody></table></blockquote>

Here is a chart for 2x Xeon E5620/Debian:<br>
<br>
<img src='http://wiki.ltalloc.googlecode.com/hg/benchmark.png' />

While this test is completely synthetic (and may be too biased), it measures precisely just an allocation/deallocation cost, excluding influence of all other things, such as cache misses (which are very important, but not always). So even this benchmark can be quite representative for some applications with small working memory set (which entirely fits inside cpu cache), or some specific algorithms<a href='Hidden comment:  (consider individual function, which use some temporary complex containers (like std::map) allocating a lot of small objects)'></a>.<br>
<br>
<h3>2. What makes ltalloc so fast?</h3>
Briefly, that its ultimately minimalistic design and extremely polished implementation, especially minimization of conditional branches per a regular alloc call.<br>
Consider a typical implementation of memory allocation function:<br>
<ol><li>if (size == 0) return NULL (or size = 1)<br>
</li><li>if (!initialized) initialize_allocator()<br>
</li><li>if (size < some_threshold) (to test if size requested is small enough)<br>
</li><li>if (freeList) {result = freeList, freeList = freeList->next}<br>
</li><li>if (result == NULL) throw std::bad_alloc() (for an implementation of operator new)</li></ol>

But in case of call to operator new overloaded via ltalloc there will be just <b>one</b> conditional branch (4th in the list above) in 99% cases, while all other checks are doing only when necessary in the remaining 1% rare cases.<br>
<font color='white'>
<h6>rmem</h6>
</font>
<h3>3. Does ltalloc return memory to the system automatically?</h3>
Well, it is not (except for large blocks).<br>
But you can always call ltalloc_squeeze() manually at any time (e.g., in separate thread), which almost have no any impact on performance of allocations/deallocations in the others simultaneously running threads (except the obvious fact of having to re-obtain memory from the system when allocating new memory after that). And this function can release as much memory as possible (not only at the top of the heap, like malloc_trim does).<br>
I don't want doing this automatically, because it highly depends on application's memory allocation pattern (e.g., imagine some server app that periodically (say, once a minute) should process some complex user request as quickly as possible, and after that it destroys all objects used for processing - returning any memory to the system in this case may significantly degrade performance of allocation of objects on each new request processing). Also I dislike any customizable threshold parameters, because it is usually hard to tune optimally for the end user, and this has some overhead as some additional checks should be done inside alloc/free call (non necessary at each call, but sometimes they should be done). So, instead I've just provided a mechanism to manually release memory at the most appropriate time for a specific application (e.g. when user inactive, or right after closing any subwindow/tab).<br>
But, if you really want this, you can run a separate thread which will just periodically call ltalloc_squeeze(0). Here is one-liner for C++11:<br>
<pre><code>std::thread([] {for (;;ltalloc_squeeze(0)) std::this_thread::sleep_for(std::chrono::seconds(3));}).detach();<br>
</code></pre>
<a href='Hidden comment: 
5. Are there any plans to add NUMA support?
No, as I don"t see an effective way (i.e. without significant overhead) of integrating it.
But I want to note two things about it.
Firstly, impact of "NUMA non-awareness" on the overall performance of real applications is often overestimated. Surely, in synthetic tests you can obtain more than double drop in performance, but in many real applications benefits of NUMA-awareness will be "not so significant" (for example, Linchi Shea showed here just 5% performance drop in the worst case (i.e. 100% remote memory access scenario) in MS SQL Server 2008 http://sqlblog.com/blogs/linchi_shea/archive/2012/01/30/performance-impact-the-cost-of-numa-remote-memory-access.aspx).
In the second place, NUMA-awareness is not just a memory allocator problem - the whole application as well should be designed taking it into account. What if, for example, your application uses a producer�consumer pattern for doing its main work, and producer thread appeared to be assigned to the processor on the other NUMA-node, than the consumer thread. So, objects allocated (and initialized) in one node are then used and freed by the processor on different node. How can any allocator effectiently handle this case? Or can it at all or not? (Implementing a simple node separation scheme in allocator will lead to all memory be leaked from one NUMA node to another.) Moreover, most applications have some common static (read-only) data, which often read at all threads. And the questions is: in which NUMA node memory for such data should be allocated? Anyway, if your computationally-intensive server application (personally I don"t expect that NUMA architecture will ever appear on desktop machines) is well designed for distributed computing, then it should support running distributively on separate machines, so I suggest to just run several copies (by number of nodes) of your application on single machine and assign process affinity mask such that each process bound totally on a specific NUMA node.
'></a><br>
<br>
<h3>4. Why there are no any memory statistics provided by the allocator?</h3>
Because it causes additional overhead, and I don't see any reason to include some sort of things into such simple allocator.<br>
Anyway there will be some preprocessor macro define to turn it on, so you can take any suitable malloc implementation and optionally hook it up in place of ltalloc with preprocessor directives like this:<br>
<pre><code>#ifdef ENABLE_ADDITIONAL_MEMORY_INFO<br>
#include "some_malloc.cxx"<br>
#else<br>
#include "ltalloc.cc"<br>
#endif<br>
</code></pre>

<h3>5. Why there is no separate function to allocate aligned memory (like aligned_alloc)?</h3>
Just because it's not needed! :)<br>
ltalloc implicitly implements a "perfect alignment feature" for all memory pointers returned just because of its design.<br>
All allocated memory blocks are automatically aligned to appropriate the requested size, i.e. alignment of any allocation is at least <code>pow(2, CountTrailingZeroBits(objectSize))</code> bytes. E.g., 4 bytes blocks are always 4 bytes aligned, 24 bytes blocks are 8B-aligned, 1024 bytes blocks are 1024B-aligned, 1280 bytes blocks are 256B-aligned, and so on.<br>
(Remember, that in C/C++ size of struct is always a multiple of its largest basic element, so for example <code>sizeof(struct {__m128 a; char s[4];})</code> = 32, not 20 (16+4) ! So, for any struct S operator "new S" will always return a suitably aligned pointer.)<br>
So, if you need a 4KB aligned memory, then just request (desired_size+4095)&~4095 bytes size (description of aligned_alloc function from C11 standard already states that the value of size shall be an integral multiple of alignment, so ltalloc() can be safely called in place of aligned_alloc() even without need of additional argument to specify the alignment).<br>
But to be completely honest, that "perfect alignment" breaks after size of block exceeds a chunk size, and after that all blocks of greater size are aligned by the size of chunk (which is 64KB by default, so, generally, this shouldn't be an issue).<br>
Here is a complete table for all allocation sizes and corresponding alignment (for 32-bit platform):<br>
<table><thead><th> <b>Requested size</b> </th><th> <b>Alignment</b> </th><th> </th><th> <b>Requested size</b> </th><th> <b>Alignment</b> </th><th> </th><th> <b>Requested size</b> </th><th> <b>Alignment</b> </th><th> </th><th> <b>Requested size</b> </th><th> <b>Alignment</b> </th></thead><tbody>
<tr><td> 1..4                  </td><td> 4                </td><td>            </td><td> 81..96                </td><td> 32               </td><td>              </td><td> 769..896              </td><td> 128              </td><td>           </td><td> 7169..8192            </td><td> 8192             </td></tr>
<tr><td> 5..8                  </td><td> 8                </td><td>            </td><td> 97..112               </td><td> 16               </td><td>             </td><td> 897..1024             </td><td> 1024             </td><td>         </td><td> 8193..10240           </td><td> 2048             </td></tr>
<tr><td> 9..12                 </td><td> 4                </td><td>           </td><td> 113..128              </td><td> 128              </td><td>           </td><td> 1025..1280            </td><td> 256              </td><td>         </td><td> 10241..12288          </td><td> 4096             </td></tr>
<tr><td> 13..16                </td><td> 16               </td><td>         </td><td> 129..160              </td><td> 32               </td><td>            </td><td> 1281..1536            </td><td> 512              </td><td>         </td><td> 12289..14336          </td><td> 2048             </td></tr>
<tr><td> 17..20                </td><td> 4                </td><td>          </td><td> 161..192              </td><td> 64               </td><td>            </td><td> 1537..1792            </td><td> 256              </td><td>         </td><td> 14337..16384          </td><td> 16384            </td></tr>
<tr><td> 21..24                </td><td> 8                </td><td>          </td><td> 193..224              </td><td> 32               </td><td>            </td><td> 1793..2048            </td><td> 2048             </td><td>        </td><td> 16385..20480          </td><td> 4096             </td></tr>
<tr><td> 25..28                </td><td> 4                </td><td>          </td><td> 225..256              </td><td> 256              </td><td>           </td><td> 2049..2560            </td><td> 512              </td><td>         </td><td> 20481..24576          </td><td> 8192             </td></tr>
<tr><td> 29..32                </td><td> 32               </td><td>         </td><td> 257..320              </td><td> 64               </td><td>            </td><td> 2561..3072            </td><td> 1024             </td><td>        </td><td> 24577..28672          </td><td> 4096             </td></tr>
<tr><td> 33..40                </td><td> 8                </td><td>          </td><td> 321..384              </td><td> 128              </td><td>           </td><td> 3073..3584            </td><td> 512              </td><td>         </td><td> 28673..32768          </td><td> 32768            </td></tr>
<tr><td> 41..48                </td><td> 16               </td><td>         </td><td> 385..448              </td><td> 64               </td><td>            </td><td> 3585..4096            </td><td> 4096             </td><td>        </td><td> 32769..40960          </td><td> 8192             </td></tr>
<tr><td> 49..56                </td><td> 8                </td><td>          </td><td> 449..512              </td><td> 512              </td><td>           </td><td> 4097..5120            </td><td> 1024             </td><td>        </td><td> 40961..49152          </td><td> 16384            </td></tr>
<tr><td> 57..64                </td><td> 64               </td><td>         </td><td> 513..640              </td><td> 128              </td><td>           </td><td> 5121..6144            </td><td> 2048             </td><td>        </td><td> 49153..57344          </td><td> 8192             </td></tr>
<tr><td> 65..80                </td><td> 16               </td><td>         </td><td> 641..768              </td><td> 256              </td><td>           </td><td> 6145..7168            </td><td> 1024             </td></tr></tbody></table>

Blocks of size greater than 57344 bytes are allocated directly from the system (actual consumed physical memory is a multiple of page size (4K), but virtual is a multiple of alignment - 65536 bytes).<br>
<br>
<h3>6. Why big allocations are not cached, and always directly requested from the system?</h3>
Actually, I don't think that caching of big allocations can give significant performance improvement for real usage, as simple time measurements show that allocating even 64K of memory directly with <code>VirtualAlloc</code> or mmap is faster (2-15x depending on the system) than simple memset to zero that allocated memory (except the first time, which takes 4-10x more time because of physical page allocation on first access). But, obviously, that for greater allocation sizes, overhead of the system call would be even less noticeable. However, if that really matters for your application, then just increase constant parameter CHUNK_SIZE to a desired value.<br>
<br>
<h1>Usage</h1>
<h2>GNU/Linux</h2>
<ol><li><code>gcc /path/to/ltalloc.cc ...</code>
</li><li><code>gcc ... /path/to/libltalloc.a</code>
</li><li><code>LD_PRELOAD=/path/to/libltalloc.so &lt;appname&gt; [&lt;args...&gt;]</code>
For use options 2 and 3 you should build libltalloc:<br>
<pre><code>hg clone https://code.google.com/p/ltalloc/<br>
cd ltalloc/gnu.make.lib<br>
make<br>
(then libltalloc.a and libltalloc.so files are created in the current directory)<br>
</code></pre>
And with this options (2 or 3) all malloc/free routines (calloc, posix_memalign, etc.) are redirected to ltalloc.<br>
Also be aware, that GCC when using options -flto and -O3 with p.2 will not inline calls to malloc/free until you also add options -fno-builtin-malloc and -fno-builtin-free (however, this is rather small performance issue, and is not necessary for correct work).<br>
<h2>Windows</h2>
Unfortunately, there is no simple way to override all malloc/free crt function calls under Windows, so far there is only one simple option to override almost all memory allocations in C++ programs via global operator new override - just add <a href='http://ltalloc.googlecode.com/hg/ltalloc.cc'>ltalloc.cc</a> file into your project and you are done.</li></ol>

ltalloc was successfully compiled with <code>MSVC 2008/2010/2012, GCC 4.*, Intel Compiler 13, Clang 3.*</code>, but it's source code is very simple, so it can be trivially ported to any other C or C++ compiler with native thread local variables support. (Warning: in some builds of MinGW there is a problem with emutls and order of execution of thread destructor (all thread local variables destructed before it), and termination of any thread will lead to application crash.)
// include <cstdlib>
#include <cstddef>
#include <new>

#define __THROW_BAD_ALLOC cerr << "out of memory"; exit(1)

// 第一级分配器

template <int inst>
class __malloc_alloc_template {
private:
  static void* oom_malloc(size_t);
  static void* oom_realloc(void*, size_t);
  static void (*__malloc_alloc_oom_handler)();
  // 上面这个指针是用来指向new-handler
public:
  static void* allocate(size_t n)
  {
    void* result = malloc(n); // 直接使用malloc()
    if(0 == result) result = oom_malloc(n);
    return result;
  }
  static void deallocate(void *p, size_t /* n */)
  {
    free(p);
  }
  static void* reallocate(void *p, size_t /* old_sz */, size_t new_sz)
  {
    void* result = realloc(p, new_sz); // 直接使用realloca()
    if (0 == result) result = oom_realloc(p, new_sz);
    return result;
  }
  /* 炫技行为，它等于(typedef void (*H)();static H set_malloc_handler(H f);)也等于(typedef void(*new_handler)();new_handler set_new_handler(new_handler p) throw();)*/ 
  static void (*set_malloc_handler(void (*f)()))()
  { // 类似C++的set_new_handler()
    void (*lod)() = __malloc_alloc_oom_handler; // 记录原new-handler
    __malloc_alloc_oom_handler = f; // 把f记录起来以便后续使用
    return (old);
  }
};

template <int inst>
void (*__malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = 0;

template <int inst>
void* __malloc_alloc_template<inst>::oom_malloc(size_t n)
{
  void (*my_malloc_handler)();
  void* result;

  for(;;) { // 不断尝试释放、分配、再释放、再分配...
    my_malloc_handler = __malloc_alloc_oom_handler;
    if(0 == my_malloc_handler) {__THROW_BAD_ALLOC;}
    (*my_malloc_handler)(); // 呼叫handler，企图释放memory
    result = malloc(n); // 再次尝试分配memory
    if (result) return (result);
  }
}

template <int inst>
void* __malloc_alloc_template<inst>::oom_realloc(void* p, size_t n)
{
  void (*my_malloc_handler)();
  void* result;

  for (;;) { // 不断尝试释放、分配、再释放、再分配...
    my_malloc_handler = __malloc_alloc_oom_handler;
    if(0 == my_malloc_handler) {__THROW_BAD_ALLOC;}
    (*my_malloc_handler)();
    result = realloc(p, n);
    if (result) return (result);
  }
}
//---------------------------------------------------------------------
typedef __malloc_alloc_template<0> malloc_alloc;

// 一个转换工程,将分配单位由bytes个数改为元素个数
template <class T, class Alloc>
class simple_alloc {
public:
  static T* allocate(size_t n) // 一次分配n个T objects
  { return 0 == n ? 0 : (T*)Alloc::allocate(n*sizeof(T)); }
  static T* allocate(void) // 一次分配一个T objects
  { return (T*)Alloc::allocate(sizeof(T)); }
  static void deallocate(T* p, size_t n) // 一次归还n个T objects
  { if (0 != n) Alloc::deallocate(p, n*sizeof(T)); }
  static void deallocate(T *p) // 一次归还一个T objects
  { Alloc::deallocate(p, sizeof(T)); }
};
//---------------------------------------------------------------------
// 第二级分配器
//---------------------------------------------------------------------
enum {__ALIGN = 8}; // 小区块的上调边界
enum {__MAX_BYTES = 128}; // 小区块的上限
enum {__NFREELISTS = __MAX_BYTES/__ALIGN}; // free-list


// 这两个template参数貌似完全没用
template <bool threads, int inst>
class __default_alloc_template {
private:
  // 实际上应该使用 static const int x = N
  // 取代 #094~#096的 enum (x = N),但目前支持该心智的编译器不多

  static size_t ROUND_UP(size_t bytes) { // 上调到8的倍数
    return (((bytes) + __ALIGN - 1) & ~(__ALIGN - 1));
  }

private:
  union obj {
    union obj* free_list_link;
  }; // 也可以改用struct

private:
  static obj* volatile free_list[__NFREELISTS];
  static size_t FREELIST_INDEX(size_t bytes) { // 可以说是寻找free-list上相应位置
    return (((bytes) + __ALING -1)/__ALIGN - 1);
  }

  // Returns an objects of size n, and optionally adds to size n free list.
  static void *refill(size_t n);

  // Allocates a chunk for nobjs of size "size". nobjs may be reduced
  // if it is inconvenient to allocate the requested number.
  static char* chunk_alloc(size_t size, int &nobjs);

  // Chunk allocation state.
  static char* start_free; // 指向'pool'的头
  static char* end_free; // 指向'pool'的尾
  static size_t heap_size; // 累计分配内存的量

public:

  static void* allocate(size_t n) // n must be > 0
  {
    obj* volatile *my_free_list; // obj** /* volatile:与多线程有关 */
    obj* result;

    if(n > (size_t)__MAX_BYTES) { // 改用第一级
      return (malloc_alloc::allocate(n));
    }

    my_free_list = free_list + FREELIST_INDEX(N); // 定位到free-list上的挂载点的那个链表
    result = *my_free_list;
    if(result == 0) { // 查看list是否为空
      void* r = refill(ROUND_UP(n)); // refill()会填充free-list并返回一个区块的起始地址
      return r;
    } // 可以继续表示list内有可用区块
    // 给它一块，链表往下移动一块
    *my_free_list = result->free_list_link;
    return (result);
  }

  static void deallocate(void* p, size_t n) // p不为空
  { // 它并没有检查p是否来自于这个alloc，直接就可以把它并入alloc，不是很好，如果p指向的大小不是8的倍数，会带来一些不好的影响
    obj* q = (obj*)p;
    obj* volatile *my_free_list;

    if(n > (size_t)__MAX_BYTES) {
      malloc_alloc::deallocate(p,n);
      return;
    }
    my_free_list = free_list + FREELIST_INDEX(n);
    q->free_list_link = *my_free_list;
    *my_free_list = q; // 它并没有还给操作系统，仍然挂载程序上
  }

  static void * reallocate(void* p, size_t old_sz, size_t new_sz);

};

//---------------------------------------------------------------------
// We allocate memory in large chunks in order to avoid fragmenting the
// malloc heap too much. We assume that size is properly aligned.
// We hold the allocation lock.
//---------------------------------------------------------------------

// 从pool要内存
template <bool threads, int inst>
char* __default_alloc_template<threads, inst>::chunk_alloc(size_t size, int& nobjs)
{
  char* result;
  size_t total_bytes = size * nobjs;
  size_t bytes_left = end_free - start_free;

  if (bytes_left >= total_bytes) { // pool空间足够满足20块请求量
    result = start_free;
    start_free += total_bytes; // 调整pool水位
    return (result);
  } else if (bytes_left >= size) { // pool空间可以满足1-19块请求量
    nobjs = bytes_left / size; // 调整请求量
    total_bytes = size * nobjs;
    result = start_free;
    start_free += total_bytes; // 调整水位
    return (result);
  } else { // pool空间不足以满足一块的需求
    // 从system free-store取得空闲的空间注入
    size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);
    // 先尝试将pool清空
    if(bytes_left > 0) { // pool仍有些空间
      // 找出空间碎片的悬挂点
      obj* volatile * my_free_list = free_list + FREELIST_INDEX(bytes_left);
      // 将pool空间编入悬挂点
      ((obj*)start_free)->free_list_link = *my_free_list;
      *my_free_list = (obj*)start_free;
    }
    start_free = (char*)malloc(bytes_to_get); // 从system free-store获取更多内存注入pool
    if (0 == start_free) { // 系统已无更多空闲空间
      int i;
      obj* volatile *my_free_list, *p;

      // Try to make do with what we have. That can't hurt.
      // We do not try smaller requests, since that tends
      // toresult in disater on multi-process machines.
      for(i=size; i<=__MAX_BYTES; i+=__ALIGN) { // i=size表明了它从当前点向右侧找
        my_free_list = free_list + FREELIST_INDEX(i);
        p = *my_free_list;
        if (0 != p) { // 该free-list内有可用区块，释放一块给pool
          *my_free_list = p->free_list_link;
          start_free = (char*)p;
          end_free = start_free + i;
          return (chunk_alloc(size, nobjs)); // 池添水，重新申请
          // 因为是从当前块向右，所以必定能够提供至少一块
          // 而且剩余的零头将会被编入适当的free-list
        }
      }
      end_free = 0; // 山穷水尽，再无memory可用
      // 改用第一级，看看oom-handler可否解决
      start_free = (char*)malloc_alloc::allocate(bytes_to_get);
      // 这样可能会导致抛出异常，或memory不足的情况得到改善
    }
    // 至此，表示已经从system free-store成功获取memory
    heap_size += bytes_to_get; // 更新累计分配量
    end_free = start_free + bytes_to_get; // 注入pool
    return (chunk_alloc(size, nobjs));
  }
}

//---------------------------------------------------------------------
// Returns an object of size n, and optionally adds to size n free list.
// We assume that n is properly aligned. We hold the allocation lock.
//---------------------------------------------------------------------
// 将要到的内存挂到free-list上
template <bool threads, int inst>
void* __default_alloc_template<threads, inst>::refill(size_t n)
{
  int nobjs = 20; // 预计取20块
  char* chunk = chunk_alloc(n, nobjs);
  obj* volatile *my_free_list;
  obj* result;
  obj* current_obj;
  obj* next_obj;
  int i;

  if (1 == nobjs) return (chunk);
  // 以下开始将所得区块挂上free-list
  my_free_list = free_list + FREELIST_INDEX(n);
  // 在chunk内建立 free list
  result = (obj*)chunk;
  *my_free_list = next_obj = (obj*)(chunk + n);
  for (i=1; ; ++i) { // 把剩下的那些切割开，依次挂到free-list上
    current_obj = next_obj;
    next_obj = (obj*)((char*)next_obj + n);
    if (nobjs-1 == i) { // 最后一个
      current_obj->free_list_link = 0;
      break;
    } else {
      current_obj->free_list_ink = next_obj;
    }
  }
  return (result);
}
//---------------------------------------------------------------------
template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::start_free = 0;

template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::end_free = 0;

template <bool threads, int inst>
size_t __default_alloc_template<threads, inst>::heap_size = 0;

template <bool threads, int inst>
__default_alloc_template<threads, inst>::obj* volatile
__default_alloc_template<threads, inst>::free_list[__NFREELISTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//---------------------------------------------------------------------

// 令第二级分配器名称为alloc
typedef __default_alloc_template<false, 0> alloc;
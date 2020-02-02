
/*
LICENCE
-------

This code is made available under the MIT licence,
which is distributed alongside this file as LICENCE.txt
*/


/*
WHAT?
-----

This playground explores using boost.object_pool backed by a static array of char.
Turns out this is just about possible, but it's ugly and fragile:
the size of the underlying char array depends on the behaviour of object_pool.

The result is a memory-pool of Thing objects with a predetermined maximum capacity,
something object_pool does not typically support despite the presence of a max_size variable
(which seems to refer to the maximum block-size it is permitted to request when adding to its pool).
We never use 'new' or 'malloc'.

In principle, the pool capacity limit probably isn't guaranteed to work precisely as expected:
perhaps object_pool would be permitted to over-allocate and provide room for additional objects.
*/



#include <type_traits>
#include <iostream>

#include <boost/pool/object_pool.hpp>


class Thing
{
private:
  int p;

public:
  inline  Thing()      : p(0) { std::cout << "Thing constructed\n";                               }
  inline  Thing(int i) : p(i) { std::cout << "Thing constructed with int " << i << '\n';          }
  inline ~Thing()             { std::cout << "Thing destructed, holding int " << this->p << '\n'; }
};

static_assert( !std::is_pod<Thing>::value, "Thing class should not be POD" ); // User-defined ctor means not POD

#define STATIC_POOL_CAPACITY 6 // Maximum number of Thing objects which may be allocated at once

// The required capacity isn't simply STATIC_POOL_CAPACITY * sizeof(Thing),
// as it's up to Boost::object_pool to decide what capacity to request.
// When STATIC_POOL_CAPACITY = 6, this seems to turn out as 64. (This is fragile!)
#define REQUIRED_SIZE 64 // Number of chars of underlying space needed

alignas(sizeof(Thing)) static char static_block[REQUIRED_SIZE];

static bool StaticUserAllocator_full = false; // We want internal linkage so we put this outside the class

class StaticUserAllocator
{
private:

public:
  typedef size_t size_type;
  typedef size_t difference_type;


  // Boost.object_pool is 'stable', and never copies objects into new locations.
  // (This is like Boost::stable_vector. It's unlike std::vector, which guarantees element contiguity.)
  // This means that, as we allocate more objects, the value it passes to malloc may not always increase.
  // Also, if malloc returns nullptr, it may re-try with a smaller request.
  //
  // Intended behaviour: for the first call made, check that the expected size was passed,
  // and if so, return pointer to static array. If not, terminate immediately.
  // On subsequent invocations, always return nullptr.
  static inline char * malloc(size_t s)
  {
    std::cout << "Pool malloc has been called, requesting size: " << s << '\n';
    char * ret = nullptr;

    if (!StaticUserAllocator_full)
    {
      if (s != REQUIRED_SIZE) {
        std::cout << "Unexpected pool malloc size request. Terminating.\n";
        exit(1);
      }
      else
      {
        std::cout << "Pool malloc size request is as expected...\n";
        std::cout << "Not full, so returning static block\n";
        StaticUserAllocator_full = true;
        ret = static_block; // TODO or &(static_block[0]) ??
      }
    }
    else
    {
      std::cout << "Full, so returning nullptr unconditionally\n";
    }
    return ret;
  }

  static inline void free(char *block) {
    // Static array cannot be freed, so do nothing.
    std::cout << "pool free (to free the underlying block) has been called\n";
  }
};


int main(int argc, char *argv[])
{

  {
    boost::object_pool<Thing, StaticUserAllocator> pool(
      STATIC_POOL_CAPACITY, // Next size (size always starts out at 0)
      STATIC_POOL_CAPACITY  // Max size... which is really a lie https://stackoverflow.com/a/22953544/
    );

    Thing *t1 = pool.construct(1);
    std::cout << "t1 is " << (t1 ? "not null" : "null") << '\n';

    Thing *t2 = pool.construct(2);
    std::cout << "t2 is " << (t2 ? "not null" : "null") << '\n';

    Thing *t3 = pool.construct(3);
    std::cout << "t3 is " << (t3 ? "not null" : "null") << '\n';

    Thing *t4 = pool.construct(4);
    std::cout << "t4 is " << (t4 ? "not null" : "null") << '\n';

    Thing *t5 = pool.construct(5);
    std::cout << "t5 is " << (t5 ? "not null" : "null") << '\n';

    Thing *t6 = pool.construct(6);
    std::cout << "t6 is " << (t6 ? "not null" : "null") << '\n';

    // Expect null:
    Thing *t7 = pool.construct(7);
    std::cout << "t7 is " << (t7 ? "not null" : "null") << '\n';

    // Expect null:
    Thing *t8 = pool.construct(8);
    std::cout << "t8 is " << (t8 ? "not null" : "null") << '\n';

    if (t1) pool.destroy(t1); // Unlike the delete operator, it is not safe to destroy(nullptr)
    if (t2) pool.destroy(t2);
    if (t3) pool.destroy(t3);
    if (t4) pool.destroy(t4);
    if (t5) pool.destroy(t5);
    if (t6) pool.destroy(t6);
    if (t7) pool.destroy(t7);
    if (t8) pool.destroy(t8);


    Thing *t9 = pool.construct(9);
    std::cout << "t9 is " << (t9 ? "not null" : "null") << '\n';

    Thing *t10 = pool.construct(10);
    std::cout << "t10 is " << (t10 ? "not null" : "null") << '\n';

    Thing *t11 = pool.construct(11);
    std::cout << "t11 is " << (t11 ? "not null" : "null") << '\n';


    if (t10) pool.destroy(t10);

    // These will get destroyed safely at the end of the block,
    // as the object_pool itself is destroyed.
    // object_pool does not guarantee last-to-first order of destruction.
//  if (t11) pool.destroy(t11);
//  if (t9) pool.destroy(t8);

  }

  std::cout << "Program terminating normally.\n";
  return 0;
}


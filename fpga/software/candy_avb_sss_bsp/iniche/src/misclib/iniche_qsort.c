
#include "ipport.h"

#ifdef INCLUDE_QSORT

#define CUTOFF 8

static void shortsort(char *lo, char *hi, unsigned width, int (*comp)(const void *, const void *));
static void swap(char *p, char *q, unsigned int width);

void iniche_qsort(void *base, unsigned num, unsigned width, int (*comp)(const void *, const void *))
{
  char *lo, *hi;
  char *mid;
  char *loguy, *higuy;
  unsigned size;
  char *lostk[30], *histk[30];
  int stkptr;

  if (num < 2 || width == 0) return;
  stkptr = 0;

  lo = base;
  hi = (char *) base + width * (num - 1);

recurse:
  size = (hi - lo) / width + 1;

  if (size <= CUTOFF) 
  {
    shortsort(lo, hi, width, comp);
  }
  else 
  {
    mid = lo + (size / 2) * width;
    swap(mid, lo, width);

    loguy = lo;
    higuy = hi + width;

    for (;;) 
    {
      do { loguy += width; } while (loguy <= hi && comp(loguy, lo) <= 0);
      do { higuy -= width; } while (higuy > lo && comp(higuy, lo) >= 0);
      if (higuy < loguy) break;
      swap(loguy, higuy, width);
    }

    swap(lo, higuy, width);

    if (higuy - 1 - lo >= hi - loguy) 
    {
      if (lo + width < higuy) 
      {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy - width;
        ++stkptr;
      }

      if (loguy < hi) 
      {
        lo = loguy;
        goto recurse;
      }
    }
    else
    {
      if (loguy < hi) 
      {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        ++stkptr;
      }

      if (lo + width < higuy) 
      {
        hi = higuy - width;
        goto recurse;
      }
    }
  }

  --stkptr;
  if (stkptr >= 0) 
  {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;
  }
  else
    return;
}

static void shortsort(char *lo, char *hi, unsigned width, int (*comp)(const void *, const void *))
{
  char *p, *max;

  while (hi > lo) 
  {
    max = lo;
    for (p = lo+width; p <= hi; p += width) if (comp(p, max) > 0) max = p;
    swap(max, hi, width);
    hi -= width;
  }
}

static void swap(char *a, char *b, unsigned width)
{
  char tmp;

  if (a != b)
  {
    while (width--) 
    {
      tmp = *a;
      *a++ = *b;
      *b++ = tmp;
    }
  }
}

void *iniche_bsearch(const void *key,
   const void *base0, size_t nmemb, size_t size,
   int (*compar)(const void *, const void *)) 
{  
   const char *base = base0;
   size_t lim;
   int cmp;
   const void *p;  
   for(lim = nmemb; lim != 0; lim >>= 1) 
   {
      p = base + (lim >> 1) * size;
      cmp = (*compar)(key, p);
      if (cmp == 0)
         return ((void *)p);
      if (cmp > 0) 
      {  /* key > p: move right */
         base = (char *)p + size;
         lim--;
      }  /* else move left */
   }
   return(NULL);
}

#if 0
void iniche_swap(void *v[], int i, int j)
{
   void *temp;
   temp = v[i];
   v[i] = v[j];
   v[j] = temp;
}

void iniche_qsort(void *v[], int left, int right, int (*comp)(void *, void *))
{
   int i, last;
   void swap(void *v[], int, int);
   if(left >= right)
      return;
   iniche_swap(v, left, (left+right)/2);
   last = left;
   for(i = left+1, i <= right; i++)
   {
      if((*comp)(v[i], v[left]) < 0)
      {
         iniche_swap(v, ++last, i);
      }
   }
   iniche_swap(v, left, last);
   iniche_qsort(v, left, last-1, comp);
   iniche_qsort(v, last+1, right, comp);
}
#endif
#endif

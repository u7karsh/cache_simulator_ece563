#ifndef _ALL_H
#define _ALL_H

#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include<malloc.h>

typedef enum{
   FALSE  = 0,
   TRUE   = 1,
}boolean;

// An assert block is all that we need
#define ASSERT( condition, statement, ... ) if( condition ) { \
   printf( "[ASSERT] In File: %s, Line: %d => " #statement "\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
   exit(1); }

#endif

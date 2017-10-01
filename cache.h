/*H**********************************************************************
* FILENAME    :       cache.h
* DESCRIPTION :       Contains structures and prototypes for generic
*                     cache simulator
* NOTES       :       -NA-
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    18 Sep 17
*
* CHANGES :
*
*H***********************************************************************/


#ifndef _CACHE_H
#define _CACHE_H

#include "all.h"

// Mask for 32 bit address
#define   ADDRESS_MASK     0xFFFFFFFF
#define   ADDRESS_SIZE     32

// Pointer translations
typedef  struct  _cmdDirT             *cmdDirPT;
typedef  struct  _replacementPolicyT  *replacementPolicyPT;
typedef  struct  _writePolicyT        *writePolicyPT;
typedef  struct  _tagT                *tagPT;
typedef  struct  _tagStoreT           *tagStorePT;
typedef  struct  _cacheT              *cachePT;
typedef  struct  _cacheTimingTrayT    *cacheTimingTrayPT;

// Enum to hold direction like read/write
typedef enum {
   CMD_DIR_READ                             = 0,
   CMD_DIR_WRITE                            = 1,
}cmdDirT;

// Enum to hold replacement policies
typedef enum {
   POLICY_REP_LRU                           = 0,      /* Least Recently Used */
   POLICY_REP_LFU                           = 1,      /* Least Frequently Used */
   POLICY_REP_LRFU                          = 2,      /* Least Recently/Frequently Used */
}replacementPolicyT;

// Enum to hold write policies
typedef enum {
   POLICY_WRITE_BACK_WRITE_ALLOCATE         = 0,
   POLICY_WRITE_THROUGH_WRITE_NOT_ALLOCATE  = 1,
}writePolicyT;

typedef struct _tagT{
   int               tag;
   boolean           valid;
   boolean           dirty;
   
   // Common counter for LRU and LFU
   // We will use this counter as LAST_REF_TIMESTAMP for LRFU
   int               counter;

   // Only for LRFU
   double            crf;
}tagT;

// Tag store unit cell
typedef struct _tagStoreT{
   tagPT             *rowP;
   // COUNT_SET value for LFU
   int               countSet;
}tagStoreT;

// Cache communication struct
typedef struct _cacheCommT{
   boolean            hit;
   int                index;
   int                setIndex;
}cacheCommT;

// Generic CACHE structure.
typedef struct _cacheT{
   /*
    * Configutration params
    */
   // Placeholder for name like L1, L2 or MEM
   char                  name[128];
   int                   size;
   int                   assoc;
   int                   blockSize;
   int                   nSets;
   // Pointer to next level cache.
   // If NULL, it signifies it is highest level
   // cache and no further translations can be made
   // below this level, we have main memory
   cachePT               nextLevel; 
   // Optional Victim cache
   cachePT               victimP;
   // Policies
   replacementPolicyT    repPolicy;
   writePolicyT          writePolicy; 
   // Relevant only for LRFU
   double                lambda;
   /*
    * Address decoder variables
    */   
   // In number of bits
   int                  tagSize;
   int                  indexSize;
   int                  boSize;

   /*
    * Internal variables
    */
   int                  tagMask;
   int                  indexMask;
   int                  boMask;

   int                  readHitCount;
   int                  readMissCount;

   int                  writeHitCount;
   int                  writeMissCount;

   int                  writeBackCount;
   int                  swaps;
   int                  numAccess;


   // Timing params
   double               missPenalty;
   double               hitTime;
   
   tagStorePT           *tagStoreP;

   // Following variable are only for victim cache related config
   boolean              isVictimCache;
}cacheT;

// Since the timing related values are a lot, lets
// create a nice and tidy structure
typedef struct _cacheTimingTrayT{
   // All timings are in ns
   double               tMissOffset;
   double               tMissMult;

   double               tHitOffset;
   double               tHitSizeMult;
   double               tHitBlockMult;
   double               tHitAssocMult;

   int                  blockBytesPerNs;
   int                  sizeBytesPerNs;
}cacheTimingTrayT;


// Function prototypes. This part can be automated
// lets keep hardcoded as of now
cachePT  cacheInit(   
      char*              name, 
      int                size, 
      int                assoc, 
      int                blockSize,
      double             lambda,
      replacementPolicyT repPolicy,
      writePolicyT       writePolicy,
      cacheTimingTrayPT  trayP );

void cacheConnect( cachePT cacheAP, cachePT cacheBP );
cacheCommT cacheCommunicate( cachePT cacheP, int address, cmdDirT dir );
void cacheDecodeAddress( cachePT cacheP, int address, int* tag, int* index, int* offset );

boolean cacheDoReadWriteCommon( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP, cmdDirT dir, int allocate );
boolean cacheDoRead( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP);
boolean cacheDoWrite( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP );
void cacheWriteBackData( cachePT cacheP, int address );
int cacheFindReplacementUpdateCounterLRU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride );
int cacheFindReplacementUpdateCounterLFU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride );
int cacheFindReplacementUpdateCounterLRFU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride );
void cacheHitUpdateLRU( cachePT cacheP, int index, int setIndex );
void cacheHitUpdateLFU( cachePT cacheP, int index, int setIndex );
void cacheHitUpdateLRFU( cachePT cacheP, int index, int setIndex );


char* cacheGetNameReplacementPolicyT(replacementPolicyT policy);
char* cacheGetNamewritePolicyT(writePolicyT policy);
void cachePrettyPrintConfig( cachePT cacheP );
void cachePrintContents( cachePT cacheP );
double cacheComputeMissPenalty( cachePT cacheP, cacheTimingTrayPT trayP );
double cacheComputeHitTime( cachePT cacheP, cacheTimingTrayPT trayP );
void cacheAttachVictimCache( cachePT cacheP, int size, int blockSize, cacheTimingTrayPT trayP );
void cacheVictimSwap( cachePT cacheP, int index, int setIndex, int victimIndex, int victimSetIndex );
double cacheCRF_F( cachePT cacheP, tagPT *rowP, int setIndex );

double cacheGetAAT( cachePT cacheP );
int cacheGetWBCount( cachePT cacheP );
void cacheGetStats( cachePT cacheP, 
                    int     *readCount, 
                    int     *readMisses, 
                    int     *writeCount, 
                    int     *writeMisses, 
                    double  *missRate, 
                    int     *swaps, 
                    int     *writeBacks, 
                    int     *memoryTraffic );
#endif

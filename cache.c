/*H**********************************************************************
* FILENAME    :       cache.c 
* DESCRIPTION :       Consists all cache related operations
* NOTES       :       *Caution* Data inside the original cacheP
*                     might change based on functions.
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    18 Sep 17
*
* CHANGES :
*                     Added Support to multi level cache : UM : 20 Sep 17
*                     Added Victim cache                 : UM : 26 Sep 17
*
*H***********************************************************************/

#include "cache.h"

// Since this is a small proj, add all utils in this
// file instead of a separate file
//-------------- UTILITY BEGIN -----------------

#define LOG2(A)      log(A) / log(2)
// This is a ceil LOG2
#define CLOG2(A)     ceil( LOG2(A) )

// Creates a mask with lower nBits set to 1
int  utilCreateMask( int nBits )
{
   int mask = 0, index;
   for( index=0; index<nBits; index++ )
      mask |= 1 << index;

   return mask;
}

int utilShiftAndMask( int input, int shiftValue, int mask )
{
   return (input >> shiftValue) & mask;
}

//-------------- UTILITY END   -----------------

// Allocates and inits all internal variables
cachePT  cacheInit(   
      char*              name, 
      int                size, 
      int                assoc, 
      int                blockSize,
      double             lambda,
      replacementPolicyT repPolicy,
      writePolicyT       writePolicy,
      cacheTimingTrayPT  trayP )
{
   if( size <= 0 ) return NULL;

   // Do calloc so that mem is reset to 0
   cachePT  cacheP        = ( cachePT ) calloc( 1, sizeof( cacheT ) );
   strcpy( cacheP->name, name );
   cacheP->size           = size;
   cacheP->assoc          = assoc;
   cacheP->blockSize      = blockSize;
   cacheP->nSets          = ceil( (double) size / (double) ( assoc*blockSize ) );
   cacheP->repPolicy      = repPolicy;
   cacheP->writePolicy    = writePolicy;
   cacheP->lambda         = lambda;
   // Let's assume there is no cache below this level during the init phase
   // To add, use the connect phase
   cacheP->nextLevel      = NULL;

   // Compute number of bits
   cacheP->boSize         = CLOG2(blockSize);
   cacheP->indexSize      = CLOG2(cacheP->nSets);
   cacheP->tagSize        = ADDRESS_SIZE - ( cacheP->boSize + cacheP->indexSize );
   
   // Create respective masks to ease future computes
   cacheP->boMask         = utilCreateMask( cacheP->boSize )     & ADDRESS_MASK;
   cacheP->indexMask      = utilCreateMask( cacheP->indexSize )  & ADDRESS_MASK;
   cacheP->tagMask        = utilCreateMask( cacheP->tagSize )    & ADDRESS_MASK;

   // Create the tag store. Calloc it so that we have 0s set (including valid, dirty bit)
   cacheP->tagStoreP      = (tagStorePT*) calloc(cacheP->nSets, sizeof(tagStorePT));
   for( int index = 0; index < cacheP->nSets; index++ ){
       cacheP->tagStoreP[index]                             = (tagStorePT) calloc(1, sizeof(tagStoreT));
       cacheP->tagStoreP[index]->rowP                       = (tagPT*) calloc(cacheP->assoc, sizeof(tagPT));
       for( int setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
          cacheP->tagStoreP[index]->rowP[setIndex]          = (tagPT)  calloc(1, sizeof(tagT));

          // Update the counter values to comply with LRU and LFU defaults
          if( cacheP->repPolicy == POLICY_REP_LRU )
             cacheP->tagStoreP[index]->rowP[setIndex]->counter = setIndex;
          else
             cacheP->tagStoreP[index]->rowP[setIndex]->counter = 0;
       }
   }
   
   // Initialize timing params. Only one time compute
   cacheP->hitTime         = cacheComputeHitTime( cacheP, trayP );
   cacheP->missPenalty     = cacheComputeMissPenalty( cacheP, trayP );

   return cacheP;
}

// This will do a cache connection
// Cache A -> Cache B
// Thus, cache A is more close to processor
// NOTE: Implicit connection to victim cache
inline void cacheConnect( cachePT cacheAP, cachePT cacheBP )
{
   cacheAP->nextLevel              = cacheBP;
   // Since victim cache also has access to next level cache, do the correct connection
   if( cacheAP->victimP  != NULL ){
      cacheAP->victimP->nextLevel  = cacheBP;
   }
}

// Attach victim cache to an existing cache
void cacheAttachVictimCache( cachePT cacheP, int size, int blockSize, cacheTimingTrayPT trayP )
{
   if( size > 0 ){
      char name[128];
      int assoc                 = ceil( (double) size / (double) ( blockSize ) );
      //TODO: Add parent cache identifier
      sprintf(name, "Victim Cache");
      //, cacheP->name);
      cachePT victimP           = cacheInit( name, size, assoc, blockSize, 0.0, POLICY_REP_LRU, POLICY_WRITE_BACK_WRITE_ALLOCATE, trayP );
      // Since victim cache also has access to next level cache, do the correct connection
      victimP->nextLevel        = cacheP->nextLevel;
      victimP->isVictimCache    = TRUE;
      cacheP->victimP           = victimP;
   }
}

// Entry function for communicating with this cache
// Its the input of cache
// address: input address of cache
// dir    : read/write
cacheCommT cacheCommunicate( cachePT cacheP, int address, cmdDirT dir )
{
   if( cacheP == NULL ) return (cacheCommT){FALSE, 0, 0};

   // For safety, init with 0s
   int tag=0, index=0, offset=0, setIndex=0;
   cacheCommT comm;
   boolean hit;
   cacheDecodeAddress(cacheP, address, &tag, &index, &offset);
   cacheP->numAccess++;
   
   if( dir == CMD_DIR_READ ){
      // Drop data as it is of no concern as of now
      hit                               = cacheDoRead( cacheP, address, tag, index, offset, &setIndex );
      cacheP->readHitCount             += (hit)  ? 1 : 0;
      cacheP->readMissCount            += (!hit) ? 1 : 0;
   } else{
      // Write random data as it is of no use as of now
      hit                               = cacheDoWrite( cacheP, address, tag, index, offset, &setIndex );
      cacheP->writeHitCount            += (hit)  ? 1 : 0;
      cacheP->writeMissCount           += (!hit) ? 1 : 0;
   }

   comm.index                           = index;
   comm.setIndex                        = setIndex;
   comm.hit                             = hit;

   return comm;
}

// Address decoder for cache based on config
//    --------------------------------------------
//   |    Tag      |     Index   |  Block Offset  |
//    --------------------------------------------
void cacheDecodeAddress( cachePT cacheP, int address, int* tagP, int* indexP, int* offsetP )
{
   //TODO: Optimize this code for speedup by storing interm result. Will it speedup?
   // Do the sanity masking
   address         = address & ADDRESS_MASK;
   // No shift
   *offsetP        = utilShiftAndMask( address, 0, cacheP->boMask );
   // boSize shift
   *indexP         = utilShiftAndMask( address, cacheP->boSize, cacheP->indexMask );
   // boSize shift + indexSize shift
   *tagP           = utilShiftAndMask( address, cacheP->boSize + cacheP->indexSize, cacheP->tagMask );
}

// Address decoder for cache based on config
//    --------------------------------------------
//   |    Tag      |     Index   |  Block Offset  |
//    --------------------------------------------
int cacheEncodeAddress( cachePT cacheP, int tag, int index, int offset )
{
   return ( cacheP->tagMask  & tag     ) << (cacheP->boSize + cacheP->indexSize) |
          ( cacheP->indexMask & index  ) << (cacheP->boSize) |
          ( cacheP->boMask & offset);
}

// Utility function to help doRead and doWrite
// hit =  0 => miss
// hit =  1 => hit
boolean cacheDoReadWriteCommon( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP, cmdDirT dir, int allocate )
{
   // Assume capacity miss for default case
   boolean hit               = FALSE;
   int setIndex;

   ASSERT(cacheP->nSets <= index, "index translated to more than available! index: %d, nSets: %d", 
          index, cacheP->nSets);

   tagPT* rowP     = cacheP->tagStoreP[index]->rowP;

   // Check if its a hit or a miss by looking in each set
   for( setIndex = 0; setIndex < cacheP->assoc ; setIndex++ ){
      // Check tag IFF data is valid
      if( rowP[setIndex]->valid == 1 && rowP[setIndex]->tag == tag ){
         hit       = TRUE;
         break;
      }
   }

   *setIndexP = setIndex;
   // For read in Victim Cache, end right here!
   if( cacheP->isVictimCache && dir == CMD_DIR_READ ){
      return hit;
   }

   if( hit ){
      // Update replacement unit's counter value
      if( cacheP->repPolicy == POLICY_REP_LRU ){
         cacheHitUpdateLRU( cacheP, index, setIndex );
      } else if( cacheP->repPolicy == POLICY_REP_LFU ){
         cacheHitUpdateLFU( cacheP, index, setIndex );
      } else{
         cacheHitUpdateLRFU( cacheP, index, setIndex );
      }
   }

   // In case of a miss, read back from higher level cache/memory
   // and do a replacement based on policy
   else{
      // Evict and allocate a block IIF allocate is set or read
      if( ( dir == CMD_DIR_WRITE && allocate ) || dir == CMD_DIR_READ ){
         // Irrespective of replacement policy, check if there exists
         // a block with valid bit unset
         int success   = 0;
         for( setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
            if( rowP[setIndex]->valid == 0 ){
               success = 1;
               break;
            }
         }

         // If we don't have success, use the replacement policy
         if( cacheP->repPolicy == POLICY_REP_LRU ){
            setIndex           = cacheFindReplacementUpdateCounterLRU( cacheP, index, tag, setIndex, success );
         } else if( cacheP->repPolicy == POLICY_REP_LFU ){
            setIndex           = cacheFindReplacementUpdateCounterLFU( cacheP, index, tag, setIndex, success );
         } else{
            setIndex           = cacheFindReplacementUpdateCounterLRFU( cacheP, index, tag, setIndex, success );
         }

         //-------------- VICTIM CACHE SPECIFIC CODE BEGIN ---------------------------------
         // If Victim cache is present, check if it has data or not
         boolean bypassWriteback = FALSE;
         if( cacheP->victimP != NULL ){
            cacheCommT comm = cacheCommunicate( cacheP->victimP, address, CMD_DIR_READ );
            // If it was a hit in victim cache, do a swap on relevant position
            // If it was a miss in victim cache, fetch data from lower level.
            // if eviction is caused, write that data to victim cache
            if( comm.hit ){
               cacheVictimSwap( cacheP, index, setIndex, comm.index, comm.setIndex );
   
               *setIndexP = setIndex;
               // Since there is nothing else to do, exit the function here
               // Also since data was found in victim, it is considered as a hit
               // for current cache as well
               return TRUE;
            } else{
               // Write the evicted data to victim cache and continue fetching
               // new data for current block
               if( rowP[setIndex]->valid ){
                  cacheCommT victimComm               = cacheCommunicate( cacheP->victimP, 
                                                           cacheEncodeAddress( cacheP, rowP[setIndex]->tag, index, 0 ), 
                                                           CMD_DIR_WRITE );
                  // Exclusively update the dirty bit to make data consistent
                  // TODO: Create a wrapper function to stop direct access
                  //       to internal structures. For now it will work
                  cacheP->victimP
                        ->tagStoreP[victimComm.index]
                        ->rowP[victimComm.setIndex]
                        ->dirty                       = rowP[setIndex]->dirty;
                  bypassWriteback                     = TRUE;
               }
            }
         }
         //-------------- VICTIM CACHE SPECIFIC CODE END -----------------------------------

         // Is the data we are updating dirty?
         if( rowP[setIndex]->valid && !bypassWriteback ){
            if( rowP[setIndex]->dirty ){
               // Construct the address and write it back
               cacheWriteBackData( cacheP, cacheEncodeAddress( cacheP, rowP[setIndex]->tag, index, 0 ) );
            }
         }

         // In case of write allocate/read, miss will cause data to be read back from
         // memory to be overridden
         // This is not needed in case of victim cache
         if( !cacheP->isVictimCache )
            cacheCommunicate( cacheP->nextLevel, address, CMD_DIR_READ );

         // Update tag value and make bit non-dirty
         // CAUTION: setting dirty will be handled in
         // doWrite
         rowP[setIndex]->tag   = tag;
         rowP[setIndex]->valid = 1;
         rowP[setIndex]->dirty = 0;
      }

      // Since its a miss, refil the value from higer level cache/memory if 
      // for write miss, write through based on policy
      if( dir == CMD_DIR_WRITE && !allocate ){
         cacheCommunicate( cacheP->nextLevel, address, CMD_DIR_WRITE );
      }

   }
   *setIndexP = setIndex;

   return hit;
}

// Module to swap contents of victim cache
void cacheVictimSwap( cachePT cacheP, int index, int setIndex, int victimIndex, int victimSetIndex )
{
   tagPT   victimTagP   = cacheP->victimP->tagStoreP[victimIndex]->rowP[victimSetIndex];
   tagPT   cacheTagP    = cacheP->tagStoreP[index]->rowP[setIndex];

   // Encode
   int cacheAddress     = cacheEncodeAddress( cacheP, cacheTagP->tag, index, 0 );
   int victimAddress    = cacheEncodeAddress( cacheP->victimP, victimTagP->tag, victimIndex, 0 );

   // Decode
   int newCacheTag, newVictimTag, offset;
   cacheDecodeAddress(cacheP, victimAddress, &newCacheTag, &index, &offset);
   cacheDecodeAddress(cacheP->victimP, cacheAddress, &newVictimTag, &index, &offset);
   
   // Swap the dirty bits
   int victimDirty      = victimTagP->dirty;
   victimTagP->dirty    = cacheTagP->dirty;
   cacheTagP->dirty     = victimDirty;

   // Swap the computed tags
   victimTagP->tag      = newVictimTag;
   cacheTagP->tag       = newCacheTag;

   // Update counters as if a hit
   // TODO: Fix this if victim supports anything more than LRU
   cacheHitUpdateLRU( cacheP->victimP, victimIndex, victimSetIndex );

   // Update swaps for both current cache and victim
   cacheP->swaps++;
   cacheP->victimP->swaps++;
}

boolean cacheDoRead( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP )
{
   return cacheDoReadWriteCommon( cacheP, address, tag, index, offset, setIndexP, CMD_DIR_READ, 1 );
}

// hit =  0 => miss
// hit =  1 => hit
boolean cacheDoWrite( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP )
{
   boolean hit;
   if( cacheP->writePolicy == POLICY_WRITE_BACK_WRITE_ALLOCATE ){
      hit           = cacheDoReadWriteCommon( cacheP, address, tag, index, offset, setIndexP, CMD_DIR_WRITE, 1 );
      // Irrespective of everything, set the dirty bit
      // If it was a miss, common utility will take care of the replacement and will update the
      // tag accordingly
      cacheP->tagStoreP[index]->rowP[*setIndexP]->dirty = 1;
   } else{
      // No block gets dirty in write through
      hit           = cacheDoReadWriteCommon( cacheP, address, tag, index, offset, setIndexP, CMD_DIR_WRITE, 0 );
   }
   return hit;
}

void cacheWriteBackData( cachePT cacheP, int address )
{
   cacheP->writeBackCount++;
   // Pass on to next level cache/memory
   cacheCommunicate( cacheP->nextLevel, address, CMD_DIR_WRITE );
}

// LRU replacement policy engine
// For LRU, we dont need overrideSetIndex or doOverride as the minimum value will always 
// point to the invalid date. Keeping the signature for inter-operatibility
int cacheFindReplacementUpdateCounterLRU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride )
{
   tagPT   *rowP = cacheP->tagStoreP[index]->rowP;
   int replIndex = 0;

   // Place data at max counter value and reset counter to 0
   // Increment all counters
   if( doOverride ){
      // Update the counter values just as if it was a hit for this index
      cacheHitUpdateLRU( cacheP, index, overrideSetIndex );
      replIndex  = overrideSetIndex;

   } else{
      // Speedup: do a increment % assoc to all elements and update tag
      // for value 0
      for( int setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
         rowP[setIndex]->counter   = (rowP[setIndex]->counter + 1) % cacheP->assoc;
         if( rowP[setIndex]->counter == 0 )
            replIndex              = setIndex;
      }
   }

   return replIndex;
}

// Least frequently used with dynamic aging
int cacheFindReplacementUpdateCounterLFU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride )
{
   tagPT   *rowP = cacheP->tagStoreP[index]->rowP;
   
   int replIndex = 0;

   // Don't search if overriden
   if( doOverride ){
      replIndex       = overrideSetIndex;
   } else{
      // Search for block having lowest counter value to evicit
      // Init the minValue tracker to first counter value
      int minValue    = rowP[0]->counter;

      // Start from 1 instead
      for( int setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
         if( rowP[setIndex]->counter < minValue ){
            minValue  = rowP[setIndex]->counter;
            replIndex = setIndex;
         }
      }
   }

   //TODO: there is no need for a countSet
   // By now we know which block has to be evicted
   // Set the age counter before evicting
   cacheP->tagStoreP[index]->countSet     = rowP[replIndex]->counter;

   // Update the counter value for the newly placed entry
   // Since its the job of this function to update all counter
   rowP[replIndex]->counter               = cacheP->tagStoreP[index]->countSet + 1;

   return replIndex;
}

inline double cacheCRF_F( cachePT cacheP, tagPT *rowP, int setIndex )
{
   return rowP[setIndex]->crf * (pow(0.5, cacheP->lambda * ( cacheP->numAccess - rowP[setIndex]->counter )));
}

// Least Recently/Frequently used
int cacheFindReplacementUpdateCounterLRFU( cachePT cacheP, int index, int tag, int overrideSetIndex, int doOverride )
{

   tagPT   *rowP   = cacheP->tagStoreP[index]->rowP;
   int replIndex   = 0;

   if( doOverride ){
      replIndex = overrideSetIndex;
   } else{
      double *tempCrf = (double*) calloc(cacheP->assoc, sizeof(double));

      // Computer temporary CRF
      for( int setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
         tempCrf[setIndex] = cacheCRF_F( cacheP, rowP, setIndex );
      }
      
      double minCrf = tempCrf[0];

      // Find the block
      for( int setIndex = 0; setIndex < cacheP->assoc; setIndex++ ){
         if( tempCrf[setIndex] < minCrf ){
            replIndex = setIndex;
            minCrf    = tempCrf[setIndex];
         }
      }

      // Free the allocated memory
      free( tempCrf );
   }

   // Update the vals
   rowP[replIndex]->crf     = 1;
   rowP[replIndex]->counter = cacheP->numAccess;

   return replIndex;
}

void cacheHitUpdateLRU( cachePT cacheP, int index, int setIndex )
{
   tagPT     *rowP = cacheP->tagStoreP[index]->rowP;

   // Increment the counter of other blocks whose counters are less
   // than the referenced block's old counter value
   for( int assocIndex = 0; assocIndex < cacheP->assoc; assocIndex++ ){
      if( rowP[assocIndex]->counter < rowP[setIndex]->counter )
         rowP[assocIndex]->counter++;
   }

   // Set the referenced block's counter to 0 (Most recently used)
   rowP[setIndex]->counter = 0;
}

void cacheHitUpdateLFU( cachePT cacheP, int index, int setIndex )
{
   // Increment the counter value of the set which is indexed
   cacheP->tagStoreP[index]->rowP[setIndex]->counter++;
}

void cacheHitUpdateLRFU( cachePT cacheP, int index, int setIndex )
{
   tagPT *rowP             = cacheP->tagStoreP[index]->rowP;
   rowP[setIndex]->crf     = 1 + cacheCRF_F( cacheP, rowP, setIndex );
   rowP[setIndex]->counter = cacheP->numAccess;
}

char* cacheGetNameReplacementPolicyT(replacementPolicyT policy)
{
   switch(policy){
      case POLICY_REP_LRU                         : return "LRU";
      case POLICY_REP_LFU                         : return "LFU";
      case POLICY_REP_LRFU                        : return "LRFU";
      default                                     : return "";
   }
}

char* cacheGetNamewritePolicyT(writePolicyT policy)
{
   switch(policy){
      case POLICY_WRITE_BACK_WRITE_ALLOCATE       : return "POLICY_WRITE_BACK_WRITE_ALLOCATE";
      case POLICY_WRITE_THROUGH_WRITE_NOT_ALLOCATE: return "POLICY_WRITE_THROUGH_WRITE_NOT_ALLOCATE";
      default                                     : return "";
   }
}

void cachePrettyPrintConfig( cachePT cacheP )
{
   if( !cacheP ) return;
   printf("\t%s_BLOCKSIZE:\t\t\t %d\n"         , cacheP->name, cacheP->blockSize);
   printf("\t%s_SIZE:\t\t\t %d\n"              , cacheP->name, cacheP->size);
   printf("\t%s_ASSOC:\t\t\t %d\n"             , cacheP->name, cacheP->assoc);
   printf("\t%s_REPLACEMENT_POLICY:\t\t %d\n"  , cacheP->name, cacheP->repPolicy);
   printf("\t%s_WRITE_POLICY:\t\t %d\n"        , cacheP->name, cacheP->writePolicy);
}

void cachePrintContents( cachePT cacheP )
{
   if( !cacheP ) return;
   printf("===== %s contents =====\n", cacheP->name);
   for( int setIndex = 0; setIndex < cacheP->nSets; setIndex++ ){
      printf("set\t\t%d:\t\t", setIndex);
      tagPT *rowP = cacheP->tagStoreP[setIndex]->rowP;
      for( int assocIndex = 0; assocIndex < cacheP->assoc; assocIndex++ ){
         printf("%x %c\t", rowP[assocIndex]->tag, (rowP[assocIndex]->dirty) ? 'D' : ' ' );
      }
      printf("\n");
   }
}

inline int cacheGetWBCount( cachePT cacheP )
{
   return (cacheP) ? cacheP->writeBackCount : 0;
}

void cacheGetStats( cachePT cacheP, 
                    int     *readCount, 
                    int     *readMisses, 
                    int     *writeCount, 
                    int     *writeMisses, 
                    double  *missRate, 
                    int     *swaps, 
                    int     *writeBacks, 
                    int     *memoryTraffic )
{
   if( !cacheP ) return;
   *readMisses    = cacheP->readMissCount;
   *writeMisses   = cacheP->writeMissCount;
   *readCount     = cacheP->readHitCount + *readMisses;
   *writeCount    = cacheP->writeHitCount + *writeMisses;
   *missRate      = ( (double)(*readMisses + *writeMisses) ) / ( (double)(*readCount + *writeCount) );
   *memoryTraffic = ( cacheP->writePolicy == POLICY_WRITE_BACK_WRITE_ALLOCATE ) ? 
                         *readMisses + *writeMisses + cacheP->writeBackCount :
                         *readMisses + *writeCount;
   if( cacheP->victimP != NULL ){
      int vRreads, vReadMisses, vWrites, vWriteMisses, vSwaps, vWB, vMemoryTraffic;
      double vMissRate;
      cacheGetStats( cacheP->victimP, &vRreads, &vReadMisses, &vWrites, &vWriteMisses, &vMissRate, &vSwaps, &vWB, &vMemoryTraffic ); 
      *memoryTraffic += vWB;
   }

   *writeBacks    = cacheP->writeBackCount;
   *swaps         = cacheP->swaps;
}

// Compute average access time
double cacheGetAAT( cachePT cacheP )
{
   if( cacheP == NULL )
      return 1.0;

   double missRate      = ( (double)(cacheP->readMissCount + cacheP->writeMissCount) ) / ( (double)(cacheP->numAccess) );

   if( cacheP->nextLevel == NULL )
      return cacheP->hitTime + (missRate * cacheP->missPenalty) ;
      
   return cacheP->hitTime + (missRate * cacheGetAAT( cacheP->nextLevel )) ;
}

// In ns
double cacheComputeMissPenalty( cachePT cacheP, cacheTimingTrayPT trayP )
{
   return trayP->tMissOffset + trayP->tMissMult * ( ( (double) cacheP->blockSize ) / ( (double) trayP->blockBytesPerNs ) );
}

// In ns
double cacheComputeHitTime( cachePT cacheP, cacheTimingTrayPT trayP )
{
   return trayP->tHitOffset + 
          ( trayP->tHitSizeMult  * ( ( (double) cacheP->size )      / ( (double) trayP->sizeBytesPerNs ) ) ) +
          ( trayP->tHitBlockMult * ( ( (double) cacheP->blockSize ) / ( (double) trayP->blockBytesPerNs ) ) ) +
          ( trayP->tHitAssocMult * ( cacheP->assoc ) );
}

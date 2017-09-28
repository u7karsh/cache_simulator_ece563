/*H**********************************************************************
* FILENAME    :       cache.c 
* DESCRIPTION :       Consists all cache related operations
* NOTES       :       *Caution* Data inside the original cacheP
*                     might change based on functions.
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    18 Sep 17
*
* CHANGES :
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
      replacementPolicyT repPolicy,
      writePolicyT       writePolicy,
      cacheTimingTrayPT  trayP )
{
   // Do calloc so that mem is reset to 0
   cachePT  cacheP        = ( cachePT ) calloc( 1, sizeof( cacheT ) );
   strcpy( cacheP->name, name );
   cacheP->size           = size;
   cacheP->assoc          = assoc;
   cacheP->blockSize      = blockSize;
   cacheP->nSets          = ceil( (double) size / (double) ( assoc*blockSize ) );
   cacheP->repPolicy      = repPolicy;
   cacheP->writePolicy    = writePolicy;
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
inline void cacheConnect( cachePT cacheAP, cachePT cacheBP )
{
   cacheAP->nextLevel     = cacheBP;
}

// Entry function for communicating with this cache
// Its the input of cache
// address: input address of cache
// dir    : read/write
void cacheCommunicate( cachePT cacheP, int address, cmdDirT dir )
{
   // For safety, init with 0s
   int tag=0, index=0, offset=0, hit;
   cacheDecodeAddress(cacheP, address, &tag, &index, &offset);
   if( dir == CMD_DIR_READ ){
      // Drop data as it is of no concern as of now
      hit                               = cacheDoRead( cacheP, address, tag, index, offset );
      cacheP->readHitCount             += (hit ==  1) ? 1 : 0;
      cacheP->readCompulsaryMissCount  += (hit == -1) ? 1 : 0;
      cacheP->readCapacityMissCount    += (hit ==  0) ? 1 : 0;
   } else{
      // Write random data as it is of no use as of now
      hit                               = cacheDoWrite( cacheP, address, tag, index, offset );
      cacheP->writeHitCount            += (hit ==  1) ? 1 : 0;
      cacheP->writeCompulsaryMissCount += (hit == -1) ? 1 : 0;
      cacheP->writeCapacityMissCount   += (hit ==  0) ? 1 : 0;
   }
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
   // printf("Decode[%x] tag: %x index: %x offset: %x\n", address, *tagP, *indexP, *offsetP);
}

// hit =  0 => miss (capacity miss)
// hit = -1 => Compulsary miss
// hit =  1 => hit
int cacheDoReadWriteCommon( cachePT cacheP, int address, int tag, int index, int offset, int* setIndexP, cmdDirT dir, int allocate )
{
   // Assume capacity miss for default case
   int hit         = 0;
   int setIndex;

   ASSERT(cacheP->nSets < index, "index translated to more than available! index: %d, nSets: %d", 
          index, cacheP->nSets);

   tagPT* rowP     = cacheP->tagStoreP[index]->rowP;

   // Check if its a hit or a miss by looking in each set
   for( setIndex = 0; setIndex < cacheP->assoc ; setIndex++ ){
      if( rowP[setIndex]->tag == tag ){
         hit       = 1;
         break;
      }
   }

   if( hit == 1 ){
      // Update replacement unit's counter value
      if( cacheP->repPolicy == POLICY_REP_LRU ){
         cacheHitUpdateLRU( cacheP, index, setIndex );
      } else{
         cacheHitUpdateLFU( cacheP, index, setIndex );
      }
   }
   // In case of a miss, read back from higher level cache/memory
   // and do a replacement based on policy
   else{
      // Evict and allocate a block IIF allocate is set
      if( allocate || dir == CMD_DIR_READ ){
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
         } else{
            setIndex           = cacheFindReplacementUpdateCounterLFU( cacheP, index, tag, setIndex, success );
         }

         // Is the data we are updating dirty?
         if( rowP[setIndex]->valid ){
            if( rowP[setIndex]->dirty ){
               // Write it back
               cacheWriteBackData( cacheP, address );
            }
         } else{
            // Compulsary miss
            hit                = -1;
         }

         // Update tag value and make bit non-dirty
         // CAUTION: setting dirty will be handled in
         // doWrite
         rowP[setIndex]->tag   = tag;
         rowP[setIndex]->valid = 1;
         rowP[setIndex]->dirty = 0;
      }

      // Since its a miss, refil the value from higer level cache/memory
      if( cacheP->nextLevel == NULL ){
         // There is no cache beyond this level.
         // Its just the memIF now
         // TODO: Add memIF if required
      } else{
         // Next level cache fetch
         cacheCommunicate( cacheP->nextLevel, address, dir );
      }

   }
   *setIndexP = setIndex;

   return hit;
}

int cacheDoRead( cachePT cacheP, int address, int tag, int index, int offset )
{
   int setIndex;
   return cacheDoReadWriteCommon( cacheP, address, tag, index, offset, &setIndex, CMD_DIR_READ, 1 );
}

// hit =  0 => miss (capacity miss)
// hit = -1 => Compulsary miss
// hit =  1 => hit
int cacheDoWrite( cachePT cacheP, int address, int tag, int index, int offset )
{
   int setIndex, hit;
   if( cacheP->writePolicy == POLICY_WRITE_BACK_WRITE_ALLOCATE ){
      hit           = cacheDoReadWriteCommon( cacheP, address, tag, index, offset, &setIndex, CMD_DIR_WRITE, 1 );
      // Irrespective of everything, set the dirty bit
      // If it was a miss, common utility will take care of the replacement and will update the
      // tag accordingly
      cacheP->tagStoreP[index]->rowP[setIndex]->dirty = 1;
   } else{
      // No block gets dirty in write through
      hit           = cacheDoReadWriteCommon( cacheP, address, tag, index, offset, &setIndex, CMD_DIR_WRITE, 0 );
   }
   return hit;
}

void cacheWriteBackData( cachePT cacheP, int address )
{
   cacheP->writeBackCount++;
   // Pass on to next level cache/memory
   if( cacheP->nextLevel == NULL ){
      // There is no cache beyond this level.
      // Its just the memIF now
      // TODO: Add memIF if required
   } else{
      // Next level cache fetch
      cacheCommunicate( cacheP->nextLevel, address, CMD_DIR_READ );
   }
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

   // By now we know which block has to be evicted
   // Set the age counter before evicting
   cacheP->tagStoreP[index]->countSet     = rowP[replIndex]->counter;

   // Update the counter value for the newly placed entry
   // Since its the job of this function to update all counter
   rowP[replIndex]->counter               = cacheP->tagStoreP[index]->countSet + 1;

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


char* cacheGetNameReplacementPolicyT(replacementPolicyT policy)
{
   switch(policy){
      case POLICY_REP_LRU                         : return "POLICY_REP_LRU";
      case POLICY_REP_LFU                         : return "POLICY_REP_LFU";
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
   printf("\t%s_BLOCKSIZE:\t\t\t %d\n"         , cacheP->name, cacheP->blockSize);
   printf("\t%s_SIZE:\t\t\t %d\n"              , cacheP->name, cacheP->size);
   printf("\t%s_ASSOC:\t\t\t %d\n"             , cacheP->name, cacheP->assoc);
   printf("\t%s_REPLACEMENT_POLICY:\t\t %d\n"  , cacheP->name, cacheP->repPolicy);
   printf("\t%s_WRITE_POLICY:\t\t %d\n"        , cacheP->name, cacheP->writePolicy);
}

void cachePrintContents( cachePT cacheP )
{
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

void cachePrintStats( cachePT cacheP )
{
   int readMisses    = cacheP->readCompulsaryMissCount + cacheP->readCapacityMissCount;
   int writeMisses   = cacheP->writeCompulsaryMissCount + cacheP->writeCapacityMissCount;
   int readCount     = cacheP->readHitCount + readMisses;
   int writeCount    = cacheP->writeHitCount + writeMisses;
   double missRate   = ( (double)(readMisses + writeMisses) ) / ( (double)(readCount + writeCount) );
   int memoryTraffic = ( cacheP->writePolicy == POLICY_WRITE_BACK_WRITE_ALLOCATE ) ? 
                         readMisses + writeMisses + cacheP->writeBackCount :
                         readMisses + writeCount;
   double accessTime = cacheP->hitTime + (missRate * cacheP->missPenalty) ;

   printf("\n\t====== Simulation results (raw) ======\n");
   printf("\ta. number of %s reads:\t\t%d\n", cacheP->name, readCount);
   printf("\tb. number of %s read misses:\t\t%d\n", cacheP->name, readMisses);
   printf("\tc. number of %s writes:\t\t%d\n", cacheP->name, writeCount);
   printf("\td. number of %s write misses:\t\t%d\n", cacheP->name, writeMisses);
   printf("\te. %s miss rate:\t\t%0.4f\n", cacheP->name, missRate);
   printf("\tf. number of writebacks from %s:\t\t%d\n", cacheP->name, cacheP->writeBackCount);
   printf("\tg. total memory traffic:\t\t%d\n\n", memoryTraffic);
   printf("\t==== Simulation results (performance) ====\n");
   printf("\t1. average access time:\t\t%0.4f ns", accessTime);
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

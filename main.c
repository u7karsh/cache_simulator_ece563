/*H**********************************************************************
* FILENAME    :       main.c 
* DESCRIPTION :       Program1 for ECE 563. Generic cache simulator
* NOTES       :       Entry to sim_cache program
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    18 Sep 17
*
* CHANGES :
*
*H***********************************************************************/


#include "all.h"
#include "cache.h"


void endOfSim( cachePT l1P, cachePT l2P, int mode1 )
{
   int l1Reads, l1ReadMisses, l1Writes, l1WriteMisses, l1Swaps, l1WB;
   int l2Reads, l2ReadMisses, l2Writes, l2WriteMisses, l2Swaps, l2WB, memoryTraffic;
   double l1MissRate, l2MissRate;

   cacheGetStats( l1P, &l1Reads, &l1ReadMisses, &l1Writes, &l1WriteMisses, &l1MissRate, &l1Swaps, &l1WB, &memoryTraffic ); 
   if( l2P != NULL )
      cacheGetStats( l2P, &l2Reads, &l2ReadMisses, &l2Writes, &l2WriteMisses, &l2MissRate, &l2Swaps, &l2WB, &memoryTraffic ); 

   printf("\n\t====== Simulation results (raw) ======\n");
   if( mode1 ) printf("\n");
   printf("\ta. number of L1 reads:\t%d\n"                , l1Reads);
   printf("\tb. number of L1 read misses:\t%d\n"          , l1ReadMisses);
   printf("\tc. number of L1 writes:\t%d\n"               , l1Writes);
   printf("\td. number of L1 write misses:\t%d\n"         , l1WriteMisses);
   printf("\te. L1 miss rate:\t%0.4f\n"                   , l1MissRate);
   if( mode1 ){
      printf("\tf. number of swaps:\t%d\n"                   , l1Swaps);
      printf("\tg. number of victim cache writeback:\t%d\n"  , (l1P->victimP) ? cacheGetWBCount(l1P->victimP) : 0 );
      printf("\th. number of L2 reads:\t%d\n"                , (l2P == NULL ) ? 0 : l2Reads);
      printf("\ti. number of L2 read misses:\t%d\n"          , (l2P == NULL ) ? 0 : l2ReadMisses);
      printf("\tj. number of L2 writes:\t%d\n"               , (l2P == NULL ) ? 0 : l2Writes);
      printf("\tk. number of L2 write misses:\t%d\n"         , (l2P == NULL ) ? 0 : l2WriteMisses);
      if( l2P == NULL )
         printf("\tl. L2 miss rate:\t0\n");
      else
         printf("\tl. L2 miss rate:\t%0.4f\n"                , (l2P == NULL ) ? 0 : l2MissRate);
      printf("\tm. number of L2 writeback:\t%d\n"            , (l2P == NULL ) ? 0 : l2WB);
      printf("\tn. total memory traffic:\t%d\n"              , memoryTraffic);
   } else{
      printf("\tf. number of writebacks from L1:\t%d\n"      , l1WB);
      printf("\tg. total memory traffic:\t%d\n"              , memoryTraffic);
   }

   printf("\n\t==== Simulation results (performance) ====\n");
   printf("\t1. average access time:\t%0.4f ns\n"            , cacheGetAAT(l1P));
}

void beginOfSim( cachePT l1P, cachePT l2P, char* traceFile, int mode1 )
{
   printf("\t===== Simulator configuration =====\n");
   printf("\tL1_BLOCKSIZE:\t\t\t %d\n"         , l1P->blockSize);
   printf("\tL1_SIZE:\t\t\t %d\n"              , l1P->size);
   printf("\tL1_ASSOC:\t\t\t %d\n"             , l1P->assoc);
   if( mode1 ){
      printf("\tVictim_Cache_SIZE:\t\t\t %d\n"    , (l1P->victimP) ? l1P->victimP->size : 0);
      printf("\tL2_SIZE:\t\t\t %d\n"              , (l2P == NULL) ? 0 : l2P->size);
      printf("\tL2_ASSOC:\t\t\t %d\n"             , (l2P == NULL) ? 0 : l2P->assoc);
   } else{
      printf("\tL1_REPLACEMENT_POLICY:\t\t\t %d\n"       , l1P->repPolicy);
      printf("\tL1_WRITE_POLICY:\t\t\t %d\n"             , l1P->writePolicy);
   }
   printf("\ttrace_file:\t\t%s\n", traceFile);
   if( mode1 ){
      printf("\tReplacement Policy:\t\t %s\n"  , cacheGetNameReplacementPolicyT(l1P->repPolicy));
   }
   if( l1P->repPolicy == POLICY_REP_LRFU )
      printf("\tlambda:\t\t%0.2f\n", l1P->lambda);
   printf("\t===================================\n\n");
}

void doTrace( cachePT cacheP, FILE* fp )
{
   char rw;
   int address;
   int count = 1;

   do{
      int bytesRead = fscanf( fp, "%c %x\n", &rw, &address );
      // Just to safeguard on byte reading
      ASSERT(bytesRead <= 0, "fscanf read nothing!");

      if( rw == 'r' || rw == 'R' ){
         // Read
         cacheCommunicate( cacheP, address, CMD_DIR_READ );
      } else{
         //Write
         cacheCommunicate( cacheP, address, CMD_DIR_WRITE );
      }
      count++;
   } while( !feof(fp) );
}

int main( int argc, char** argv )
{
   ASSERT( !( argc == 9 || argc == 7 ), "Not enough arguments passed(=%d)\n", argc );

   int    blockSize      = atoi(argv[1]);
   int    l1Size         = atoi(argv[2]);
   int    l1Assoc        = atoi(argv[3]);
   
   int    victimSize     = 0;
   int    l2Size         = 0;
   int    l2Assoc        = 0;
   double lambda         = 0.0;
   int    repPolicy      = atoi(argv[4]);
   int    repPolicyL2    = POLICY_REP_LRU;
   int    writePolicy    = POLICY_WRITE_BACK_WRITE_ALLOCATE;
   char   traceFile[128];

   if( argc == 9 ){
      victimSize     = atoi(argv[4]);
      l2Size         = atoi(argv[5]);
      l2Assoc        = atoi(argv[6]);
      lambda         = atof(argv[7]);
      sprintf(traceFile, "%s", argv[8]);
      repPolicy      = ( lambda >= 0 && lambda <= 1 ) ? POLICY_REP_LRFU : ( lambda == 3 ? POLICY_REP_LFU : POLICY_REP_LRU );
      repPolicyL2    = ( repPolicy == POLICY_REP_LFU ) ? POLICY_REP_LFU : POLICY_REP_LRU;
   } else{
      writePolicy    = atoi(argv[5]);
      sprintf(traceFile, "%s", argv[6]);
   }


   FILE* fp                = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);

   // Init the timing tray. Its hardcoded as of now
   cacheTimingTrayT trayL1 = { 20.0, 0.5, 0.25, 2.5, 0.025, 0.025, 16, 512*1024 };
   cacheTimingTrayT trayL2 = { 20.0, 0.5, 2.5 , 2.5, 0.025, 0.025, 16, 512*1024 };

   // It's L (small case) 1P.. Don't confuse as 1 and l seem same :)
   cachePT  l1P          = cacheInit( "L1", l1Size, l1Assoc, blockSize, lambda, repPolicy, writePolicy, &trayL1 );
   cachePT  l2P          = cacheInit( "L2", l2Size, l2Assoc, blockSize, lambda, repPolicyL2, writePolicy, &trayL2 );
   cacheConnect( l1P, l2P );
   cacheAttachVictimCache( l1P, victimSize, blockSize, &trayL1 );

   beginOfSim( l1P, l2P, traceFile, (argc == 9) );
   
   doTrace( l1P, fp );
   
   cachePrintContents( l1P );
   cachePrintContents( l1P->victimP );
   cachePrintContents( l2P );

   // TODO: create a destructor for cache structure
   endOfSim( l1P, l2P, (argc == 9) );
   
}

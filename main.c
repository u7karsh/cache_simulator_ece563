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


void endOfSim( cachePT l1P, cachePT l2P )
{
   int l1Reads, l1ReadMisses, l1Writes, l1WriteMisses, l1Swaps, l1WB, l1l2Traffic;
   int l2Reads, l2ReadMisses, l2Writes, l2WriteMisses, l2Swaps, l2WB, memoryTraffic;
   double l1MissRate, l2MissRate;

   cacheGetStats( l1P, &l1Reads, &l1ReadMisses, &l1Writes, &l1WriteMisses, &l1MissRate, &l1Swaps, &l1WB, &l1l2Traffic ); 
   cacheGetStats( l2P, &l2Reads, &l2ReadMisses, &l2Writes, &l2WriteMisses, &l2MissRate, &l2Swaps, &l2WB, &memoryTraffic ); 

   printf("\n\t====== Simulation results (raw) ======\n\n");
   printf("\ta. number of L1 reads:\t%d\n"                , l1Reads);
   printf("\tb. number of L1 read misses:\t%d\n"          , l1ReadMisses);
   printf("\tc. number of L1 writes:\t%d\n"               , l1Writes);
   printf("\td. number of L1 write misses:\t%d\n"         , l1WriteMisses);
   printf("\te. L1 miss rate:\t%0.4f\n"                   , l1MissRate);
   printf("\tf. number of swaps:\t%d\n"                   , l1Swaps);
   printf("\tg. number of victim cache writeback:\t%d\n"  , (l1P->victimP) ? cacheGetWBCount(l1P->victimP) : 0 );
   printf("\th. number of L2 reads:\t%d\n"                , l2Reads);
   printf("\ti. number of L2 read misses:\t%d\n"          , l2ReadMisses);
   printf("\tj. number of L2 writes:\t%d\n"               , l2Writes);
   printf("\tk. number of L2 write misses:\t%d\n"         , l2WriteMisses);
   printf("\tl. L2 miss rate:\t%0.4f\n"                   , l2MissRate);
   printf("\tm. number of L2 writeback:\t%d\n"            , l2WB);
   printf("\tn. total memory traffic:\t%d\n"              , memoryTraffic);
}

void beginOfSim( cachePT l1P, cachePT l2P, char* traceFile )
{
   printf("\t===== Simulator configuration =====\n");
   printf("\tL1_BLOCKSIZE:\t\t\t %d\n"         , l1P->blockSize);
   printf("\tL1_SIZE:\t\t\t %d\n"              , l1P->size);
   printf("\tL1_ASSOC:\t\t\t %d\n"             , l1P->assoc);
   printf("\tVictim_Cache_SIZE:\t\t\t %d\n"    , (l1P->victimP) ? l1P->victimP->size : 0);
   printf("\tL2_SIZE:\t\t\t %d\n"              , l2P->size);
   printf("\tL2_ASSOC:\t\t\t %d\n"             , l2P->assoc);
   printf("\ttrace_file:\t\t%s\n", traceFile);
   printf("\tReplacement Policy:\t\t %s\n"  , cacheGetNameReplacementPolicyT(l1P->repPolicy));
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
   ASSERT( argc < 10, "Not enough arguments passed(=%d)\n", argc );

   int   blockSize       = atoi(argv[1]);
   int   l1Size          = atoi(argv[2]);
   int   l1Assoc         = atoi(argv[3]);
   int   victimSize      = atoi(argv[4]);
   int   l2Size          = atoi(argv[5]);
   int   l2Assoc         = atoi(argv[6]);
   int   lambda          = atoi(argv[7]);
   int   repPolicy       = atoi(argv[8]);
   //int   writePolicy     = atoi(argv[5]);
   char  traceFile[128];
   
   sprintf(traceFile, "%s", argv[9]);

   FILE* fp              = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);

   // Init the timing tray. Its hardcoded as of now
   cacheTimingTrayT tray = { 20.0, 0.5, 0.25, 2.5, 0.025, 0.025, 16, 512*1024 };

   // It's L (small case) 1P.. Don't confuse as 1 and l seem same :)
   cachePT  l1P          = cacheInit( "L1", l1Size, l1Assoc, blockSize, repPolicy, POLICY_WRITE_BACK_WRITE_ALLOCATE, &tray );
   cachePT  l2P          = cacheInit( "L2", l2Size, l2Assoc, blockSize, repPolicy, POLICY_WRITE_BACK_WRITE_ALLOCATE, &tray );
   cacheConnect( l1P, l2P );
   cacheAttachVictimCache( l1P, victimSize, blockSize, &tray );
   
   beginOfSim( l1P, l2P, traceFile );
   
   doTrace( l1P, fp );
   
   cachePrintContents( l1P );
   cachePrintContents( l1P->victimP );
   cachePrintContents( l2P );

   endOfSim( l1P, l2P );
   
}

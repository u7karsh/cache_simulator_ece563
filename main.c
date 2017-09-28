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
//#define DEBUG

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
#ifdef DEBUG
         printf("# %d : Read %x\n", count, address);
         printf("Address %x READ\n", address);
#endif
         // Read
         cacheCommunicate( cacheP, address, CMD_DIR_READ );
      } else{
#ifdef DEBUG
         printf("# %d : Write %x\n", count, address);
#endif
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
   cacheAttachVictimCache( l1P, victimSize, blockSize, &tray );
   cacheConnect( l1P, l2P );
   

   printf("\t===== Simulator configuration =====\n");
   cachePrettyPrintConfig( l1P );
   cachePrettyPrintConfig( l1P->victimP );
   cachePrettyPrintConfig( l2P );
   printf("\ttrace_file:\t\t%s\n", traceFile);
   printf("\t===================================\n\n");

   doTrace( l1P, fp );
   cachePrintContents( l1P );
   cachePrintContents( l1P->victimP );
   cachePrintContents( l2P );
   cachePrintStats( l1P );
   cachePrintStats( l2P );
}

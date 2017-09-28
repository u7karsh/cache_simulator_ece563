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

void doTrace( cachePT cacheP, FILE* fp )
{
   char rw;
   int address;

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
   } while( !feof(fp) );
}

int main( int argc, char** argv )
{
   ASSERT( argc < 7, "Not enough arguments passed(=%d)\n", argc );

   int   blockSize       = atoi(argv[1]);
   int   size            = atoi(argv[2]);
   int   assoc           = atoi(argv[3]);
   int   repPolicy       = atoi(argv[4]);
   int   writePolicy     = atoi(argv[5]);
   char  traceFile[128];
   
   sprintf(traceFile, "%s", argv[6]);

   FILE* fp              = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);

   // Init the timing tray. Its hardcoded as of now
   cacheTimingTrayT tray = { 20.0, 0.5, 0.25, 2.5, 0.025, 0.025, 16, 512*1024 };

   // It's L (small case) 1P.. Don't confuse as 1 and l seem same :)
   cachePT  l1P          = cacheInit( "L1", size, assoc, blockSize, repPolicy, writePolicy, &tray );

   printf("\t===== Simulator configuration =====\n");
   cachePrettyPrintConfig( l1P );
   printf("\ttrace_file:\t\t%s\n", traceFile);
   printf("\t===================================\n\n");

   doTrace( l1P, fp );
   cachePrintContents( l1P );
   cachePrintStats( l1P );
}

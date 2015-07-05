/* Command line flowmeter.

This program pumps stdin to stdout fairly efficiently, and notes the
rate and volume of data which has passed through it. It can display
the information it collects in a variety of forms.

*/

#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <wait.h>
#include <errno.h>

char * HelpText = 
  "flow v1.01\n"
  "\n"
  "Usage: flow [options]\n"
  "\n"
  "This program copies stdin to stdout, applying a variety of metering\n"
  "and monitoring tools. It is useful for example to monitor the progress\n"
  "of tar, cpio and similar tools.\n"
  "\n"
  "If flow knows the expected length of the stream, and it can use a\n"
  "variety of methods to find out, it can estimate time remaining and\n"
  "completeion time. Without knowing the length of the stream it can\n"
  "still display progress but cannot estimate completion time.\n"
  "\n"
  "Options are:\n"
  "\n"
  "-l num         Length is num bytes. 0xnnn hex format is acceptable.\n"
  "-f file        Use the length of file as the stream length\n"
  "-m mountpoint  Use the usage of the mounted file system as length\n"
  "-u path        Perform a du on path to estimate stream length\n"
  "\n"
  "-c             Display count of transferred byte and stream size\n"
  "-h             Make -c display in human readable form\n"
  "-b[width]      Display a bargraph [width] characters wide\n"
  "-s             Display seconds elapsed\n"
  "-e             Display ETA\n"
  "-r             Display seconds remaining\n"
  "-n             Print a newline after redrawing bargraph\n"
  "-t             Print totals on completion\n"
  ;


/* Bytes to trensfer in one operation */
#define BUFSIZE 0x10000

/* File Handles */
int In, Out ;
fd_set ReadFD ;
struct timeval Tv ;

/* Data buffer */
unsigned char * Buffer ;

/* Command line options */
char *Options = "l:f:m:u:cb::nserth" ;
int OptFlags [ 26 ] ;
extern char * optarg ;
#define Opt(X) ( OptFlags [ (X) - 'a' ] )

/* Time computations */
time_t Now, Start, LastDisp ;

/* Status line buffer */
char Status [ 256 ] ;
char * ToStatus ;
int LastStatus ;

/* Procedure to print a counter */
int PutCtr ( long long Val, char * Buf )
{
  int Power = 0 ;
  int Cnt ;

  if ( Opt('h') )
    {
      while ( Val >= 10000 )
	{
	  Val = Val >> 10 ;
	  Power ++ ;
	}

      Cnt = sprintf ( Buf, "%lld%c", Val, " KMGTPEZY" [ Power ] ) ;
    }
  else
    {
      Cnt = sprintf ( Buf, "%lld", Val ) ;
    }
  return Cnt ;
}

/* Procedure to print a time difference */
int PutSecs ( int Sec, char * Buf )
{
  int H, M, S ;
  char * Where ;

  M = Sec / 60 ;
  S = Sec % 60 ;
  H = M / 60 ;
  M = M % 60 ;

  Where = Buf ;
  if ( H )
    Where += sprintf ( Where, "%02d:", H ) ;
  Where += sprintf ( Where, "%02d:%02d", M, S ) ;

  return Where - Buf ;
}

/* Procedure to run du and return the total size */
long long RunDu ( char * Path )
{
  int Ret ;
  pid_t Pid ;
  int Pipe_stdout [ 2 ] ;
  long long Length ;

  Ret = pipe ( Pipe_stdout ) ;
  if ( Ret == -1 )
    {
      perror ( "pipe()" ) ;
      exit ( 20 ) ;
    }

  Pid = fork() ;
  if ( Pid == -1 )
    {
      perror ( "fork()" ) ;
      exit ( 21 ) ;
    }

  if ( Pid )
    {
      int Status ;
      FILE * From ;
      From = fdopen ( Pipe_stdout[0], "r" ) ;
      fscanf ( From, "%lld", &Length ) ;
      close ( Pipe_stdout[0] ) ;
      close ( Pipe_stdout[1] ) ;
      waitpid ( Pid, &Status, 0 ) ;
      return Length * 1024 ;
    }
  else
    {
      dup2 ( Pipe_stdout[1], 1 ) ;
      execlp ( "du", "du", "-s", Path, (char*) NULL ) ;
      perror ( "exec()" ) ;
      return -1 ;
    }
}

/* Body of the utility */
int main ( int Ac, char ** Av ) {
  int Ret, Ch ;
  long long Bc, Size ;
  char * EndPtr ;
  struct stat StatBuf ;
  struct statfs64 Stat64Buf ;
  int BarWidth = 80 ;
  int MinBar = 10 ;

  /* No options, print help */
  if ( Ac < 2 )
    {
      fprintf ( stderr, HelpText ) ;
      exit ( 0 ) ;
    }

  /* Get the options */
  while ( 1 )
    {
      Ch = getopt ( Ac, Av, Options ) ;
      if ( Ch < 0 ) break ;
      if ( Ch >= 'a' && Ch <= 'z' ) Opt ( Ch ) = 1 ;

      switch ( Ch )
	{
	case '?':
	  exit ( 7 ) ;
	  break ;

	case 'l':
	  /* Size given as number on command line */
	  Size = strtoll ( optarg, &EndPtr, 0 ) ;
	  if ( *EndPtr )
	    {
	      fprintf ( stderr, "Bad option value\n" ) ;
	      exit ( 4 ) ;
	    }
	  break ;

	case 'f':
	  /* Size given as length of a file */
	  Ret = stat ( optarg, &StatBuf ) ;
	  if ( Ret == -1 )
	    {
	      fprintf ( stderr, "Can't stat %s\n", optarg ) ;
	      exit ( 5 ) ;
	    }
	  Size = StatBuf . st_size ;
	  break ;

	case 'm':
	  /* Size as the contents of a mounted filesystem */
	  Ret = statfs64 ( optarg, &Stat64Buf ) ;
	  if ( Ret == -1 )
	    {
	      fprintf ( stderr, "Can't stat filesystem %s\n", optarg ) ;
	      exit ( 6 ) ;
	    }
	  Size = Stat64Buf . f_bsize * ( Stat64Buf . f_blocks - Stat64Buf . f_bfree ) ;
	  break ;

	case 'u':
	  /* Spawn du to get the size */
	  Size = RunDu ( optarg ) ;
	  break ;

	case 'b':
	  if ( optarg )
	    {
	      Ret = strtol ( optarg, &EndPtr, 0 ) ;
	      if ( *EndPtr )
		{
		  fprintf ( stderr, "Bargraph width must be a number\n" ) ;
		  exit ( 8 ) ;
		}
	      if ( Ret >= MinBar && Ret < BarWidth ) BarWidth = Ret ;
	    }
	  break ;
	}
    }

  /* Unless at least one of the display options (cbnsert) is set force c to get some output */
  if ( ! ( Opt('c') || Opt('b') || Opt('n') || Opt('s') || Opt('e') || Opt('r') || Opt('t') ) )
    Opt('c') = 1 ;

  /* Get the file descriptors */
  In = fileno ( stdin ) ;
  Out = fileno ( stdout ) ;
  fcntl ( In, F_SETFL, O_NONBLOCK ) ;

  FD_ZERO ( &ReadFD ) ;
  FD_SET ( In, &ReadFD ) ;

  /* Buffer */
  Buffer = malloc ( BUFSIZE ) ;

  /* Try to determine the length of the input if we havn't got it from
     the options. By default try to do a seek on stdin. If this
     doesn't work we can still display counts but not progress bars or
     ETA */
  if ( ! ( Opt('l') || Opt('f') || Opt('m') || Opt('u') ) )
    {
      Size = lseek64 ( In, 0, SEEK_END ) ;
      if ( Size < 0 ) Size = 0 ;
      lseek64 ( In, 0, SEEK_SET ) ;
    }

  /* Main copy loop */
  Bc = 0 ;
  Start = time ( NULL ) ;
  while (1)
    {
      int Cc, Cw ;
      Tv . tv_sec = 1 ;
      Tv . tv_usec = 0 ;
      Ret = select ( In + 1, &ReadFD, NULL, NULL, &Tv ) ;
      if ( Ret == -1 )
	{
	  perror ( "select() error" ) ;
	  exit ( 1 ) ;
	}
      else if ( Ret )
	{
	  Cc = read ( In, Buffer, BUFSIZE ) ;
	  if ( Cc == -1 )
	    {
	      perror ( "read() error" ) ;
	      exit ( 2 ) ;
	    }
	  if ( Cc )
	    {
	      Cw = write ( Out, Buffer, Cc ) ;
	      if ( Cw == -1 ) 
		{
		  perror ( "write() error" ) ;
		  exit ( 3 ) ;
		}
	      Bc += Cc ;
	    }
	}

      /* Determine if enough time has passed to redraw the status line */
      Now = time ( NULL ) ;
      if ( Now > LastDisp || !Cc )
	{
	  int Len, Pad ;
	  LastDisp = Now ;
	  ToStatus = Status ;

	  /* Elapsed time */
	  if ( Opt('s') )
	    {
	      ToStatus += PutSecs ( Now - Start, ToStatus ) ;
	      *ToStatus ++ = ' ' ;
	    }

	  /* Counters */
	  if ( Opt('c') )
	    {
	      ToStatus += PutCtr ( Bc, ToStatus ) ;
	      if ( Size )
		{
		  *ToStatus++ = '/' ;
		  ToStatus += PutCtr ( Size, ToStatus ) ;
		}
	      *ToStatus++ = ' ' ;
	    }

	  /* ETA or time remaining */
	  if ( ( Opt('e') || Opt ('r') ) && Size )
	    {
	      int Remain ;
	      Remain = (double) ( Size - Bc ) * (double) ( Now - Start ) / (double) Bc ;
	      if ( Opt('r') )
		{
		  ToStatus += PutSecs ( Remain, ToStatus ) ;
		  *ToStatus ++ = ' ' ;
		}
	      if ( Opt('e') )
		{
		  struct tm * Eta ;
		  time_t When ;
		  When = Now + Remain ;
		  Eta = localtime ( &When ) ;
		  ToStatus += sprintf ( ToStatus, "ETA: %02d:%02d:%02d ", Eta -> tm_hour, Eta -> tm_min, Eta -> tm_sec ) ;
		}
	    }

	  /* Bar graph */
	  if ( Opt('b') && Size )
	    {
	      int Pips, N ;
	      Pips = floor ( (double) BarWidth * (double) Bc / (double) Size ) ;
	      if ( BarWidth < Pips ) Pips = BarWidth ;
	      *ToStatus++ = '|' ;
	      for ( N = 0; N < BarWidth; N++ )
		*ToStatus++ = N > Pips ? '-' : '#' ;
	      *ToStatus++ = '|' ;
	      *ToStatus++ = ' ' ;
	    }

	  /* If the n option is set print with a newline at the end,
	     and that's it. */
	  if ( Opt('n') )
	    {
	      *ToStatus++ = '\n' ;
	    }
	  else
	    {
	      /* n option not set. Pad to overwrite the last line with
		 spaces */
	      Len = ToStatus - Status ;
	      if ( LastStatus > Len )
		{
		  Pad = LastStatus - Len ;
		  ToStatus += sprintf ( ToStatus, "%*s", Pad, "" ) ;
		}
	      *ToStatus++ = '\r' ;
	      LastStatus = Len ;
	    }

	  /* Either way add null terminator and print */
	  *ToStatus = 0 ;
	  fputs ( Status, stderr ) ;
	}
      if ( !Cc ) break ;
    }

  /* Final cleanup */
  ToStatus = Status ;

  /* Slip in a \n if status ended with \r */
  if ( !Opt('n') )
    *ToStatus++ = '\n' ;

  /* Print totals */
  if ( Opt('t') && ( Now - Start ) )
    {
      long long Rate ;
      ToStatus += PutCtr ( Bc, ToStatus ) ;
      *ToStatus++ = ' ' ; 
      ToStatus += PutSecs ( Now - Start, ToStatus ) ;
      *ToStatus ++ = ' ' ;
      Rate = (double) Bc / (double) ( Now - Start ) ;
      ToStatus += PutCtr ( Rate, ToStatus ) ;
      ToStatus += sprintf ( ToStatus, "/sec\n" ) ;
    }

  /* Final print */
  *ToStatus = 0 ;
  fputs ( Status, stderr ) ;
}

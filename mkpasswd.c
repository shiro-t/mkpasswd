#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>

#define PATH_RANDOM "/dev/urandom"

static int length          = 8;
static int number          = 1;
static int upper           = 2;
static int lower           = 3;
static int special         = 1;
static int already_fixed   = 0;

static int set_length      = 0;
static int set_number      = 0;
static int set_upper       = 0;
static int set_lower       = 0;
static int set_special     = 0;


static int isDistribute = 0;
static int isVerbose    = 0 ;
static int repeats      = 1 ;
static int use_debug    = 0 ;

#define CheckAndSet_arg( A, ARG ) \
    { \
      int n ; \
      n = atoi( ARG ) ; \
      if( n < 0 ) { fprintf( stderr, "number of " #A " is too small.\n" ); return -1 ; } \
      A = n;\
    } \

#define CheckAndSet( A )  CheckAndSet_arg( A , optarg )

void printhelp(void)
{
    fprintf( stderr, "\nusage: mkpasswd [args] [num]\n"
                         "  where arguments are:\n"
                         "    -l #      (length of password, default = %u )\n"
                         "    -d #      (min # of digits, default = %u )\n"
                         "    -c #      (min # of lowercase chars, default = %u )\n"
                         "    -C #      (min # of uppercase chars, default = %u )\n"
                         "    -s #      (min # of special chars, default = %u )\n" 
                         "   num        repeat num times\n\n", 
                         length, number, lower, upper, special );
    exit(2); 
}

int init( int argc, char *argv[] )
{
    int c, n ;

    while( EOF != ( c = getopt( argc, argv, "l:d:c:C:s:v21hD" ) ) )
    {
        switch( c )
        {
        default  : case '?' : case 'h' :
            printhelp();break;
        case 'D' : 
            use_debug = 1 ; break;

        case 'l' : CheckAndSet( length  ); set_length  = 1 ; break;
        case 'd' : CheckAndSet( number  ); set_number  = 1 ; break;
        case 'c' : CheckAndSet( lower   ); set_lower   = 1 ; break;
        case 'C' : CheckAndSet( upper   ); set_upper   = 1 ; break;
        case 's' : CheckAndSet( special ); set_special = 1 ; break;
        case 'v' : isVerbose    = 1 ; break;
        case '2' : isDistribute = 1 ; break;
        case '1' : isDistribute = 0 ; break;
        }
    }
    if( optind < argc ) 
    {
        CheckAndSet_arg( repeats, argv[optind] ); 
    }
    already_fixed =  ( set_number ? number  : 0 )
                   + ( set_lower  ? lower   : 0 ) 
                   + ( set_upper  ? upper   : 0 ) 
                   + ( set_special? special : 0 ) ;

    if( set_length )
    {
        if( length < already_fixed )
        {
            fprintf( stderr,"length is smaller than 'number + lower + upper + special'.\n" );
            return -1;
        }else if( length == already_fixed ) {
            if( ! set_number  ) number  = 0;
            if( ! set_lower   ) lower   = 0;
            if( ! set_upper   ) upper   = 0;
            if( ! set_special ) special = 0;
        }
    }else{
        if( length < already_fixed ) length = already_fixed ;
    }
    return 1;
}




unsigned int getrandom_int( int fh, int max )
{
    unsigned int c ;
    read( fh, &c, sizeof( int ) );
    return ( max == 0 ) ? c : c % max ;
}

unsigned char getrandom_byte( int fh, int max )
{
    unsigned char c ;
    read( fh, &c, sizeof( char ) );

    return ( max == 0 ) ? c : c % max ;
}

int fix_length( int fh )
{
    int d, i ;
    int p_number, p_lower, p_upper, p_special, r_maxval; 

    if( already_fixed == length ) return 1;

    d = length - ( number + lower + upper + special );

    if( d == 0 ) return 1;

    p_number  = set_number  ? 0        :     number  ;
    p_lower   = set_lower   ? p_number : ( p_number + lower + 1 );
    p_upper   = set_upper   ? p_lower  : ( p_lower  + upper + 1 );
    p_special = set_special ? p_upper  : ( p_upper  + 1 );
    r_maxval  = p_special +1;

    if(use_debug)
    {
        fprintf( stderr, "before fix rot:  p_number = %d, p_lower = %d, p_upper= %d, p_special=%d, r_maxval = %d )\n",
                                           p_number,      p_lower,      p_upper,     p_special, r_maxval );
        fprintf( stderr, "before fix vector: number = %d,   lower = %d,   upper= %d,   special=%d ( length = %d )\n",
                                             number,        lower,        upper,       special, length  );
    }
   // roulette
    if( d < 0 )
    {
         while( d < 0 )
        {
            i = getrandom_int( fh, r_maxval );

            if( i < p_number       && number  > 0 ) { number -- ; d++ ; }
            else if( i < p_lower   && lower   > 0 ) {  lower -- ; d++ ; }
            else if( i < p_upper   && upper   > 0 ) {  upper -- ; d++ ; }
            else if( i < p_special && special > 0 ) { special --; d++ ; }
        }
    }else{  // d > 0 
        while( d > 0 )
        {
            i = getrandom_int( fh, r_maxval );
            if( i < p_number )       { number ++ ; d--; }
            else if( i < p_lower )   {  lower ++ ; d--; }
            else if( i < p_upper )   {  upper ++ ; d--; }
            else if( i < p_special ) { special++ ; d--; }
        }
        
    } 
    if(use_debug)
    {
        fprintf( stderr, " after fix vector: number = %d,   lower = %d,   upper= %d,   special=%d ( length = %d )\n",
                                             number,        lower,        upper,       special, length  );
    }

    return 1;
}

int print_pass( const char *string, int len )
{
    int i ;
    fprintf( stderr, "pass = %c ", string[0] );
    for( i = 1 ; i < len ; i++ )
        fprintf( stderr, ", %c", string[i] );
    return 1;
}

int insert_char( char *string, int slen, int fh, char newch )
{
    int  pos = getrandom_int( fh, slen );
    int  ch;
//fprintf( stderr, "i: slen = %d, pos = %d, newch = %c \n",
//                     slen,      pos,      newch     );

    if( string[pos] == '\0' )
    {
        string[pos] = newch;
        return 1;
    }else{
        for( pos = 0 ; pos < slen ; pos ++ )
        {
            if( string[pos] == '\0' ) 
            {
                string[pos] = newch;
                return 2;
            }
        }
    }
    return -1 ; // NEVER REACH
}


static const char *alower = "abcdefghijklmnopqrstuvwxyz" ;
static const char *aupper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ;
static const char *anums  = "0123456789" ;
static const char *aspec  = "!@#$%^&*()-=_+[]{};:'\"<>,.?/" ;

static const char *llower = "qwertasdfgzxcvb" ;
static const char *lupper = "QWERTASDFGZXCVB" ;
static const char *rlower = "yuiophjklnm";
static const char *rupper = "YUIOPHJKLNM";
static const char *lnums  = "123456";
static const char *rnums  = "7890";
static const char *lspec  = "!@#$%" ;
static const char *rspec  = "^&*()-=_+[]{};:'\"<>,.?/";

int mkpasswd_1( int fh,  char *pass, int length )
{
    int i, m;

    if( number > 0 )
    {
        m = strlen( anums );
        for( i = 0 ; i < number ; i++ )
            insert_char( pass, length, fh, anums[ getrandom_byte( fh, m ) ]) ;
    }
    if( upper > 0 )
    {
        m = strlen( aupper );
        for( i = 0 ; i < upper ; i++ )
            insert_char( pass, length, fh, aupper[ getrandom_byte( fh, m ) ]) ;
    }
    if( lower > 0 )
    {
        m = strlen( aupper );
        for( i = 0 ; i < lower ; i++ )
            insert_char( pass, length, fh, alower[ getrandom_byte( fh, m ) ]) ;
    }
    if( special > 0 )
    {
        m = strlen( aspec );
        for( i = 0 ; i < special ; i++ )
            insert_char( pass, length, fh, aspec[ getrandom_byte( fh, m ) ]) ;
    }

    return 1;
}


int setrf( char *rpass, int rl,  char *lpass, int ll, int fh, int len, int start_left, const char *rval, const char *lval )
{
    int i ;
    int rm = strlen( rval );
    int lm = strlen( lval );

    for( i = 0 ; i < len ; i += 2 )
    {
        if( start_left )
            insert_char( lpass,  ll, fh, lval[ getrandom_byte(fh, lm) ] );
        else
            insert_char( rpass,  rl, fh, rval[ getrandom_byte(fh, rm) ] );

        if( i+1 < len )
        {
            if( start_left )
                insert_char( rpass,  rl, fh, rval[ getrandom_byte(fh, rm) ] );
            else
                insert_char( lpass,  ll, fh, lval[ getrandom_byte(fh, lm) ] );
        }else{
            start_left = ( start_left ? 0 : 1 );
        }
    }
    return start_left ;
}

int mkpasswd_2( int fh,  char *pass, int length )
{
    int start_left =  getrandom_byte( fh, 1 );
    char *rpass, *lpass ;
    int rl, ll;
    int i,j;
    int sl = start_left;

    if( start_left )
    {
        ll = length - length /2 ;
        rl = length /2 ;   
        rl += ( (ll + rl)  == length ? 0 : 1 );
    }else{
        rl = length - length /2 ;
        ll = length /2 ;   
        ll += ( (ll + rl)  == length ? 0 : 1 );
    }

    lpass = (char *)calloc( ll, sizeof(char )) ;
    rpass = (char *)calloc( rl, sizeof(char )) ;

    if( number > 0 )
        sl = setrf( rpass, rl, lpass, ll, fh, number,  sl, rnums,  lnums  );
    if( upper  > 0 )
        sl = setrf( rpass, rl, lpass, ll, fh, upper,   sl, rupper, lupper );
    if( lower > 0 )
        sl = setrf( rpass, rl, lpass, ll, fh, lower,   sl, rlower, llower );
    if( special > 0 )
        sl = setrf( rpass, rl, lpass, ll, fh, special, sl, rspec,  lspec  );

    if( start_left )
    {
       for( i = 0, j = 0 ; i < length ; i+=2, j++ )
       {
           pass[i] = lpass[j];
           if( i+1 < length ) pass[i+1] = rpass[j];
       }
    }else{
       for( i = 0, j = 0 ; i < length ; i+=2, j++ )
       {
           pass[i] = rpass[j];
           if( i+1 < length ) pass[i+1] = lpass[j];
       }
    }

    return 1;
}

int main( int argc, char *argv[] )
{
    int fh;
    int ret;
    char *pass;
    int (*mkpasswd)( int ,  char *, int );

   // read options.
    ret = init( argc, argv );
    if( ret < 0 ) exit( ret );

    if(use_debug)
        fprintf( stderr, "m: length = %d, number = %d, lower = %d, upper= %d, special=%d \n",
                             length ,     number,      lower,      upper,     special  );

   // open random device
    fh = open( PATH_RANDOM, O_RDONLY );
    if( fh < 0 )
    {
        return -1;
    } 

   // fix arguments
    if( length != number + special + lower + upper )
        fix_length( fh );

    if(use_debug)
        fprintf( stderr, "m: length = %d, number = %d, lower = %d, upper= %d, special=%d \n",
                             length ,     number,      lower,      upper,     special  );
    pass = (char *)malloc( length+2 );


    if( isDistribute )
        mkpasswd = mkpasswd_2;
    else
        mkpasswd = mkpasswd_1;

    do{ 
        memset( pass, 0x00, length+2 );
        mkpasswd( fh,  pass, length );
        printf( "%s\n", pass ); 
    } while( --repeats > 0 );

    close( fh );

    exit(0);
}



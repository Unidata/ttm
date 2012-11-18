/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

/**************************************************/

#define VERSION "1.0"

/**************************************************/

#if 0
#define DEBUG 1
#endif

/**************************************************/
/**
(Un)Define these if you do (not) have the specified capability.
This is in lieu of the typical config.h.
*/

/* Define if the equivalent of the standard Unix memove() is available */
#define HAVE_MEMMOVE 

/* Use ISO-8859-1 Character set for input/output */
#undef ISO_8859 

/**************************************************/

/* It is not clear what the correct Windows CPP Tag should be.
   Assume _WIN32, but this may not work with cygwin.
   In any case, create our own.
*/

#if  defined _WIN32 || defined _MSC_VER
#define MSWINDOWS 1
#endif

/* Reduce visual studio verbosity */
#ifdef MSWINDOWS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

/**************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef MSWINDOWS
#include <windows.h>  /* To get GetProcessTimes() */
#include <time.h> /* to get ctime() */
#else /*!MSWINDOWS*/
#include <unistd.h> /* This defines getopt */
#include <sys/times.h> /* to get times() */
#include <sys/time.h> /* to get gettimeofday() */
#endif /*!MSWINDOWS*/

/**************************************************/
/* Unix/Linux versus Windows Definitions */

/* snprintf */
#ifdef MSWINDOWS
#define snprintf _snprintf /* Microsoft has different name for snprintf */
#define strdup _strdup
#endif /*!MSWINDOWS*/

/* Getopt */
#ifdef MSWINDOWS
static char* optarg;            /* global argument pointer */
static int   optind = 0;        /* global argv index */
static int getopt(int argc, char **argv, char *optstring);
#else /*!MSWINDOWS*/
#include <unistd.h> /* This defines getopt */
#endif /*!MSWINDOWS*/

/* Wrap both unix and windows timing in this function */
static long long getRunTime(void);

/* Wrap both unix and windows time of day in this function */
static int timeofday(struct timeval *tv);

/**************************************************/
/* UTF and char Definitions */

/**
We use the standard "char" type for input/output,
but also assume that this type may point to
multibyte characters (i.e. utf8).
In order to identify such strings,
we use the alias "char_t".

Note also that it is assumed that all C string constants
in this file are restricted to US-ASCII, which is a subset
of both UTF-8 and ISO-8859-1.
*/

typedef char char_t;

typedef int utf32; /* 32-bit utf char type; signed is ok
                        because of limited # of utf characters
                        namely <= 0x10FFFF = 1,114,111
                     */
/* Maximum codepoint size when using char_t */
#define MAXCHARSIZE 4

#ifndef MININT
#define MININT -2147483647
#endif
#ifndef MAXINT
#define MAXINT 2147483647
#endif

/**************************************************/
static void
makespace(utf32* dst, utf32* src, unsigned int len)
{
#ifdef HAVE_MEMMOVE
    memmove((void*)dst,(void*)src,len*sizeof(utf32));
#else   
    src += len;
    dst += len;
    for(;len>0;len--) *{dst-- = *src--;}
#endif
}

/**************************************************/
/**
Constants
*/

/* Assign special meaning to some otherwise illegal utf-32 character values */

#define SEGMARK ((utf32)0x40000000)
#define CREATE  ((utf32)0x20000000)

/* Macros to set/clear/test these marks */
#define setMark(w,mark) ((w) | (mark))
#define clearMark(w,mark) ((w) & ~(mark))
#define testMark(w,mark) (((w) & (mark)) == 0 ? 0 : 1)

#define MAXMARKS 62
#define MAXARGS 63
#define MAXINCLUDES 1024
#define MAXEOPTIONS 1024
#define MAXINTCHARS 32

#define NUL '\0'
#define NUL32 ((utf32)0)
#define COMMA ','
#define LPAREN '('
#define RPAREN ')'
#define LBRACKET '['
#define RBRACKET ']'

#ifdef ISO_8859
#define MAXCHAR8859 ((char_t)255)i
#endif

#define MINBUFFERSIZE (1<<20)
#define MINSTACKSIZE 64
#define MINEXECCOUNT (1<<16)

#define CONTEXTLEN 20

#define CREATELEN 4 /* # of characters for a create mark */

#define HASHSIZE 128

/*Mnemonics*/
#define NESTED 1
#define KEEPESCAPE 1
#define TOSTRING 1
#define NOTTM NULL
#define TRACING 1
#define PRINTALL 1
#define TOEOS (0x7fffffff)

#ifdef DEBUG
#define PASSIVEMAX 20
#define ACTIVEMAX 20
#endif


/* TTM Flags */
#define FLAG_EXIT  1
#define FLAG_TRACE 2

/**************************************************/
/* Error Numbers */
typedef enum ERR {
ENOERR          =  0, /* No error; for completeness */
ENONAME         =  1, /* Dictionary Name or Character Class Name Not Found */
ENOPRIM         =  2, /* Primitives Not Allowed */
EFEWPARMS       =  3, /* Too Few Parameters Given */
EFORMAT         =  4, /* Incorrect Format */
EQUOTIENT       =  5, /* Quotient Is Too Large */
EDECIMAL        =  6, /* Decimal Integer Required */
EMANYDIGITS     =  7, /* Too Many Digits */
EMANYSEGMARKS   =  8, /* Too Many Segment Marks */
EMEMORY         =  9, /* Dynamic Storage Overflow */
EPARMROLL       = 10, /* Parm Roll Overflow */
EINPUTROLL      = 11, /* Input Roll Overflow */
#ifdef IMPLEMENTED
EDUPLIBNAME     = 12, /* Name Already On Library */
ELIBNAME        = 13, /* Name Not On Library */
ELIBSPACE       = 14, /* No Space On Library */
EINITIALS       = 15, /* Initials Not Allowed */
EATTACH         = 16, /* Could Not Attach */
#endif
EIO             = 17, /* An I/O Error Occurred */
#ifdef IMPLEMENTED
ETTM            = 18, /* A TTM Processing Error Occurred */
ESTORAGE        = 19, /* Error In Storage Format */
#endif
EPOSITIVE       = 20, 
/* Error messages new to this implementation */
ESTACKOVERFLOW  = 30, /* Leave room */
ESTACKUNDERFLOW = 31, 
EBUFFERSIZE     = 32, /* Buffer overflow */
EMANYINCLUDES   = 33, /* Too many includes */
EINCLUDE        = 34, /* Cannot read Include file */
ERANGE          = 35, /* index out of legal range */
EMANYPARMS      = 36, /* # parameters > MAXARGS */
EEOS            = 37, /* Unexpected end of string */
EASCII          = 38, /* ASCII characters only */
ECHAR8          = 39, /* Illegal 8-bit character set value */
EUTF32          = 40, /* Illegal utf-32 character set */
ETTMCMD         = 41, /* Illegal #<ttm> command */
ETIME           = 42, /* gettimeofday failed */
/* Default case */
EOTHER          = 99
} ERR;

/**************************************************/
/* "inline" functions */

#define isescape(c)((c) == ttm->escapec)

#define ismark(c)(testMark((c),SEGMARK)||testMark((c),CREATE)?1:0)
#define issegmark(c)(testMark((c),SEGMARK)?1:0)
#define iscreate(c)(testMark((c),CREATE)?1:0)

#define ismultibyte(c) (((c) & 0x80) == 0x80 ? 1 : 0)

#define iswhitespace(c) ((c) <= ' ' ? 1 : 0)

#define iscontrol(c) ((c) < ' ' || (c) == 127 ? 1 : 0)

#define isdec(c) (((c) >= '0' && (c) <= '9') ? 1 : 0)

#define ishex(c) ((c >= '0' && c <= '9') \
                  || (c >= 'a' && c <= 'f') \
                  || (c >= 'A' && c <= 'F') ? 1 : 0)

#define fromhex(c) \
    (c >= '0' && c <= '9' \
        ? (c - '0') \
        : (c >= 'a' && c <= 'f' \
            ? ((c - 'a') + 10) \
            : (c >= 'A' && c <= 'F' \
                ? ((c - 'a') + 10) \
                : -1)))

/**************************************************/
/**
Structure Type declarations
*/

typedef struct TTM TTM;
typedef struct Name Name;
typedef struct Charclass Charclass;
typedef struct Frame Frame;
typedef struct Buffer Buffer;

typedef void (*TTMFCN)(TTM*, Frame*);

/**************************************************/
/* Generic pseudo-hashtable */

struct HashEntry {
    utf32* name;
    unsigned int hash;
    struct HashEntry* next;
};

struct HashTable {
    struct HashEntry table[HASHSIZE];
};

/* Generic operations */
static int hashLocate(struct HashTable* table, utf32* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);

/**************************************************/
/**
TTM state object
*/

struct TTM {
    struct Limits {
        unsigned int buffersize;
        unsigned int stacksize;
        unsigned int execcount;
    } limits;
    unsigned int flags;
    unsigned int exitcode;
    unsigned int crcounter; /* for cr marks */
    utf32 sharpc; /* sharp-like char */
    utf32 openc; /* <-like char */
    utf32 closec; /* >-like char */
    utf32 semic; /* ;-like char */
    utf32 escapec; /* escape-like char */
    utf32 metac; /* read eof char */
    Buffer* buffer; /* contains the string being processed */
    Buffer* result; /* contains result strings from functions */
    unsigned int stacknext; /* |stack| == (stacknext) */
    Frame* stack;    
    FILE* output;    
    int   isstdout;
    FILE* rsinput;
    int   isstdin;
    /* Following 2 fields are hashtables indexed by low order 7 bits of some character */
    struct HashTable dictionary;
    struct HashTable charclasses;
};

/**
Define a fixed size byte buffer
for holding the current state of the expansion.
Buffer always has an extra terminating NUL ('\0').
Note that by using a buffer that is allocated once,
we can use pointers into the buffer space in e.g. struct Name.
 */

struct Buffer {
    unsigned int alloc;  /* including trailing NUL */
    unsigned int length;  /* including trailing NUL; defines what of
                             the allocated space is actual content. */
    utf32* active; /* characters yet to be scanned */
    utf32* passive; /* characters that will never be scanned again */
    utf32* end; /* into content */
    utf32* content;
};

/**
  Define a ttm frame
*/

struct Frame {
  utf32* argv[MAXARGS+1];
  unsigned int argc;
  int active; /* 1 => # 0 => ## */
};

/**
Name Storage and the Dictionary
*/

/* If you add field to this, you need
   to modify especially ttm_ds

Note: the rules of C casting allow this to be
cast to a struct HashEntry.
*/
struct Name {
    struct HashEntry entry;
    int trace;
    int locked;
    int builtin;
    unsigned int minargs;
    unsigned int maxargs;
    int novalue; /* must always return no value */
    unsigned int residual;
    unsigned int maxsegmark; /* highest segment mark number
                                in use in this string */
    TTMFCN fcn; /* builtin == 1 */
    utf32* body; /* builtin == 0 */
};

/**
Character Classes  and the Charclass table
*/

struct Charclass {
    struct HashEntry entry;
    utf32* characters;
    int negative;
};

/**************************************************/
/* Forward */

static TTM* newTTM(long,long,long);
static void freeTTM(TTM*);
static Buffer* newBuffer(TTM*, unsigned intbuffersize);
static void freeBuffer(TTM*, Buffer* bb);
static void expandBuffer(TTM*, Buffer* bb, unsigned int len);
static void resetBuffer(TTM*, Buffer* bb);
static void setBufferLength(TTM*, Buffer* bb, unsigned int len);
static Frame* pushFrame(TTM*);
static Frame* popFrame(TTM*);
static Name* newName(TTM*);
static void freeName(TTM*, Name* f);
static int dictionaryInsert(TTM*, Name* str);
static Name* dictionaryLookup(TTM*, utf32* name);
static Name* dictionaryRemove(TTM*, utf32* name);
static Charclass* newCharclass(TTM*);
static void freeCharclass(TTM*, Charclass* cl);
static int charclassInsert(TTM*, Charclass* cl);
static Charclass* charclassLookup(TTM*, utf32* name);
static Charclass* charclassRemove(TTM*, utf32* name);
static int charclassMatch(utf32 c, utf32* charclass);
static void scan(TTM*);
static void exec(TTM*, Buffer* bb);
static void parsecall(TTM*, Frame*);
static void call(TTM*, Frame*, utf32* body);
static void printstring(TTM*, FILE* output, utf32* s32, int printall);
static void ttm_ap(TTM*, Frame*);
static void ttm_cf(TTM*, Frame*);
static void ttm_cr(TTM*, Frame*);
static void ttm_ds(TTM*, Frame*);
static void ttm_es(TTM*, Frame*);
static int ttm_ss0(TTM*, Frame*);
static void ttm_sc(TTM*, Frame*);
static void ttm_ss(TTM*, Frame*);
static void ttm_cc(TTM*, Frame*);
static void ttm_cn(TTM*, Frame*);
static void ttm_cp(TTM*, Frame*);
static void ttm_cs(TTM*, Frame*);
static void ttm_isc(TTM*, Frame*);
static void ttm_rrp(TTM*, Frame*);
static void ttm_scn(TTM*, Frame*);
static void ttm_sn(TTM*, Frame*);
static void ttm_eos(TTM*, Frame*);
static void ttm_gn(TTM*, Frame*);
static void ttm_zlc(TTM*, Frame*);
static void ttm_zlcp(TTM*, Frame*);
static void ttm_flip(TTM*, Frame*);
static void ttm_ccl(TTM*, Frame*);
static void ttm_dcl0(TTM*, Frame*, int negative);
static void ttm_dcl(TTM*, Frame*);
static void ttm_dncl(TTM*, Frame*);
static void ttm_ecl(TTM*, Frame*);
static void ttm_scl(TTM*, Frame*);
static void ttm_tcl(TTM*, Frame*);
static void ttm_abs(TTM*, Frame*);
static void ttm_ad(TTM*, Frame*);
static void ttm_dv(TTM*, Frame*);
static void ttm_dvr(TTM*, Frame*);
static void ttm_mu(TTM*, Frame*);
static void ttm_su(TTM*, Frame*);
static void ttm_eq(TTM*, Frame*);
static void ttm_gt(TTM*, Frame*);
static void ttm_lt(TTM*, Frame*);
static void ttm_eql(TTM*, Frame*);
static void ttm_gtl(TTM*, Frame*);
static void ttm_ltl(TTM*, Frame*);
static void ttm_ps(TTM*, Frame*);
static void ttm_psr(TTM*, Frame*);
static void ttm_rs(TTM*, Frame*);
static void ttm_pf(TTM*, Frame*);
static void ttm_cm(TTM*, Frame*);
static void ttm_classes(TTM*, Frame*);
static void ttm_names(TTM*, Frame*);
static void ttm_exit(TTM*, Frame*);
static void ttm_ndf(TTM*, Frame*);
static void ttm_norm(TTM*, Frame*);
static void ttm_time(TTM*, Frame*);
static void ttm_xtime(TTM*, Frame*);
static void ttm_ctime(TTM*, Frame*);
static void ttm_tf(TTM*, Frame*);
static void ttm_tn(TTM*, Frame*);
static void ttm_argv(TTM*, Frame*);
static void ttm_include(TTM*, Frame*);
static void ttm_lf(TTM*, Frame*);
static void ttm_uf(TTM*, Frame*);
static void fail(TTM*, ERR eno);
static void fatal(TTM*, const char* msg);
static const char* errstring(ERR err);
static int int2string(utf32* dst, long long n);
static ERR toInt64(utf32* s, long long* lp);
static utf32 convertEscapeChar(utf32 c);
static void trace(TTM*, int entering, int tracing);
static void trace1(TTM*, int depth, int entering, int tracing);
static void dumpstack(TTM*);
static void dbgprint32(utf32* s, char quote);
static void dbgprint32c(utf32 c, char quote);
static int getOptionNameLength(char** list);
static int pushOptionName(char* option, unsigned int max, char** list);
static void initglobals();
static void usage(const char*);
static void convertDtoE(const char* def);
static void readinput(TTM*, const char* filename,Buffer* bb);
static int readbalanced(TTM*);
static void printbuffer(TTM*);
static int readfile(TTM*, FILE* file, Buffer* bb);

/* utf32 replacements for common unix strXXX functions */
static unsigned int strlen32(utf32* s);
static void strcpy32(utf32* dst, utf32* src);
static void strncpy32(utf32* dst, utf32* src, unsigned int len);
static utf32* strdup32(utf32* src);
static int strcmp32(utf32* s1, utf32* s2);
static int strncmp32(utf32* s1, utf32* s2, unsigned int len);
static void memcpy32(utf32* dst, utf32* src, int len);
/* Read/Write Management */
static void fputc32(utf32 c, FILE* f);
static utf32 fgetc32(FILE* f);

/* UTF32 <-> char management */
static int streq328(utf32* s32, char_t* s8);
static int toChar8(char_t* dst, utf32 codepoint);
static int toChar32(utf32* codepointp, char_t* src);
static int toString8(char_t* dst, utf32* src, int len);
static int toString32(utf32* dst, char_t* src, int len);
#ifndef ISO_8859
static int utf8count(unsigned int c);
#endif
/**************************************************/
/* Global variables */

static char* includes[MAXINCLUDES+1]; /* null terminated */
static char* eoptions[MAXEOPTIONS+1]; /* null terminated */
static char* argoptions[MAXARGS+1]; /* null terminated */

/**************************************************/
/**
HashTable Management.  The table is only pseudo-hash
simplified by making it an array of chains indexed by the
low order 7 bits of the name[0].
The hashcode is just the simple sum
of the characters in the name shifted by 1 bit each.
*/

/* Define a hash computing macro */
#define computehash(hash,name) {utf32* p; for(hash=0,p=name;*p!=NUL32;p++) {hash = hash + (*p <<1);} if(hash==0) hash=1;}

/* Locate a named entry in the hashtable;
   return 1 if found; 0 otherwise.
   Store the entry before the named entry
   or the entry that would have been the previous entry.
*/

static int
hashLocate(struct HashTable* table, utf32* name, struct HashEntry** prevp)
{
    struct HashEntry* prev;
    struct HashEntry* next;
    utf32 index;
    unsigned int hash;

    computehash(hash,name);
    if(!(table != NULL && name != NULL))
    assert(table != NULL && name != NULL);
    index = (name[0] & 0x7F);
    prev = &table->table[index];
    next = prev->next;
    while(next != NULL) {
	if(next->hash == hash
	   && strcmp32(name,next->name)==0)
	    break;
	prev = next;
	next = prev->next;
    }
    if(prevp) *prevp = prev;
    return (next == NULL ? 0 : 1);
}

/* Remove an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(prev->next == entry); /* validate the removal */
   prev->next = entry->next;
}


/* Insert an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(entry->hash != 0);
   entry->next = prev->next;
   prev->next = entry;
}

/**************************************************/
/* Provide subtype specific wrappers for the HashTable operations. */

static Name*
dictionaryLookup(TTM* ttm, utf32* name)
{
    struct HashTable* table = &ttm->dictionary;
    struct HashEntry* prev;    
    struct HashEntry* entry;
    Name* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	def = (Name*)entry;
    } /*else Not found */
    return def;
}

static Name*
dictionaryRemove(TTM* ttm, utf32* name)
{
    struct HashTable* table = &ttm->dictionary;
    struct HashEntry* prev;    
    struct HashEntry* entry;
    Name* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	hashRemove(table,prev,entry);
	entry->next = NULL;
	def = (Name*)entry;
    } /*else Not found */
    return def;
}

static int
dictionaryInsert(TTM* ttm, Name* str)
{
    struct HashTable* table = &ttm->dictionary;
    struct HashEntry* prev;    

    if(hashLocate(table,str->entry.name,&prev))
	return 0;
    /* Does not already exist */
    computehash(str->entry.hash,str->entry.name);/*make sure*/
    hashInsert(table,prev,(struct HashEntry*)str);
    return 1;
}

static Charclass*
charclassLookup(TTM* ttm, utf32* name)
{
    struct HashTable* table = &ttm->charclasses;
    struct HashEntry* prev;    
    struct HashEntry* entry;
    Charclass* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	def = (Charclass*)entry;
    } /*else Not found */
    return def;
}

static Charclass*
charclassRemove(TTM* ttm, utf32* name)
{
    struct HashTable* table = &ttm->charclasses;
    struct HashEntry* prev;    
    struct HashEntry* entry;
    Charclass* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	hashRemove(table,prev,entry);
	entry->next = NULL;
	def = (Charclass*)entry;
    } /*else Not found */
    return def;
}

static int
charclassInsert(TTM* ttm, Charclass* cl)
{
    struct HashTable* table = &ttm->charclasses;
    struct HashEntry* prev;    

    if(hashLocate(table,cl->entry.name,&prev))
	return 0;
    /* Not already exists */
    computehash(cl->entry.hash,cl->entry.name);
    hashInsert(table,prev,(struct HashEntry*)cl);
    return 1;
}

/**************************************************/

static TTM*
newTTM(long buffersize, long stacksize, long execcount)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) return NULL;
    ttm->limits.buffersize = buffersize;
    ttm->limits.stacksize = stacksize;
    ttm->limits.execcount = execcount;
    ttm->sharpc = (utf32)'#';
    ttm->openc = (utf32)'<';
    ttm->closec = (utf32)'>';
    ttm->semic = (utf32)';';
    ttm->escapec = (utf32)'\\';
    ttm->metac = (utf32)'\n';
    ttm->buffer = newBuffer(ttm,buffersize);
    ttm->result = newBuffer(ttm,buffersize);
    ttm->stacknext = 0;
    ttm->stack = (Frame*)malloc(sizeof(Frame)*stacksize);
    if(ttm->stack == NULL) fail(ttm,EMEMORY);
    memset((void*)&ttm->dictionary,0,sizeof(ttm->dictionary));
    memset((void*)&ttm->charclasses,0,sizeof(ttm->charclasses));
#ifdef DEBUG
    ttm->flags |= FLAG_TRACE;
#endif
    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    freeBuffer(ttm,ttm->buffer);
    freeBuffer(ttm,ttm->result);
    if(ttm->stack != NULL)
        free(ttm->stack);
    free(ttm);
}

/**************************************************/

static Buffer*
newBuffer(TTM* ttm, unsigned int buffersize)
{
    Buffer* bb;
    bb = (Buffer*)calloc(1,sizeof(Buffer));
    if(bb == NULL) fail(ttm,EMEMORY);
    bb->content = (utf32*)malloc(buffersize*sizeof(utf32));
    if(bb->content == NULL) fail(ttm,EMEMORY);
    bb->alloc = buffersize;
    bb->length = 0;
    bb->active = bb->content;
    bb->passive = bb->content;
    bb->end = bb->active;
    return bb;
}

static void
freeBuffer(TTM* ttm, Buffer* bb)
{
    if(bb->content != NULL)
        free(bb->content);
    free(bb);
}

/* Make room for a string of length n at current active position. */
static void
expandBuffer(TTM* ttm, Buffer* bb, unsigned int len)
{
    assert(bb != NULL);
    if((bb->alloc - bb->length) < len) fail(ttm,EBUFFERSIZE);
    if(bb->active < bb->end) {
        /* make room for len characters by moving bb->active and up*/
        unsigned int tomove = (bb->end - bb->active);
        makespace(bb->active+len,bb->active,tomove);
    }
    bb->active += len;
    bb->length += len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL32;
}

#if 0
/* Remove len characters at current position */
static void
compressBuffer(TTM* ttm, Buffer* bb, unsigned int len)
{
    assert(bb != NULL);
    if(len > 0 && bb->active < bb->end) {
        memcpy32(bb->active,bb->active+len,len);        
    }    
    bb->length -= len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL;
}
#endif

/* Reset buffer and set content */
static void
resetBuffer(TTM* ttm, Buffer* bb)
{
    assert(bb != NULL);
    bb->length = 0;
    bb->active = bb->content;
    bb->end = bb->content;
    *bb->end = NUL;
}

/* Change the buffer length without disturbing
   any existing content (unless shortening)
   If space is added, its content is undefined.
*/
static void
setBufferLength(TTM* ttm, Buffer* bb, unsigned int len)
{
    if(len >= bb->alloc) fail(ttm,EBUFFERSIZE);
    bb->length = len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL; /* make sure */    
}

/**************************************************/

/* Manage the frame stack */
static Frame*
pushFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext >= ttm->limits.stacksize)
        fail(ttm,ESTACKOVERFLOW);
    frame = &ttm->stack[ttm->stacknext];
    frame->argc = 0;
    frame->active = 0;
    ttm->stacknext++;
    return frame;
}

static Frame*
popFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext == 0)
        fail(ttm,ESTACKUNDERFLOW);
    ttm->stacknext--;
    if(ttm->stacknext == 0)
        frame = NULL;
    else
        frame = &ttm->stack[ttm->stacknext-1];    
    return frame;
}

/**************************************************/
static Name*
newName(TTM* ttm)
{
    Name* str = (Name*)calloc(1,sizeof(Name));
    if(str == NULL) fail(ttm,EMEMORY);
    return str;    
}

static void
freeName(TTM* ttm, Name* f)
{
    assert(f != NULL);
    if(f->entry.name != NULL) free(f->entry.name);
    if(!f->builtin && f->body != NULL) free(f->body);
    free(f);
}

/**************************************************/
static Charclass*
newCharclass(TTM* ttm)
{
    Charclass* cl = (Charclass*)calloc(1,sizeof(Charclass));
    if(cl == NULL) fail(ttm,EMEMORY);
    return cl;
}

static void
freeCharclass(TTM* ttm, Charclass* cl)
{
    assert(cl != NULL);
    if(cl->entry.name != NULL) free(cl->entry.name);
    if(cl->characters) free(cl->characters);
    free(cl);
}

static int
charclassMatch(utf32 c, utf32* charclass)
{
    utf32* p=charclass;
    utf32 pc;
    while((pc=*p++)) {if(c == pc) return 1;}
    return 0;
}

/**************************************************/

/**
This is basic top level scanner.
*/
static void
scan(TTM* ttm)
{
    utf32 c;
    Buffer* bb = ttm->buffer;

    for(;;) {
        c = *bb->active; /* NOTE that we do not bump here */
        if(c == NUL32) { /* End of buffer */
            break;
        } else if(isescape(c)) {
            bb->active++; /* skip the escape */
            *bb->passive++ = *bb->active++;
        } else if(c == ttm->sharpc) {/* Start of call? */
            if(bb->active[1] == ttm->openc
               || (bb->active[1] == ttm->sharpc
                    && bb->active[2] == ttm->openc)) {
                /* It is a real call */
                exec(ttm,bb);
                if(ttm->flags & FLAG_EXIT) goto exiting;
            } else {/* not an call; just pass the # along passively */
                *bb->passive++ = c;
                bb->active++;
            }
        } else if(c == ttm->openc) { /* Start of <...> escaping */
            /* skip the leading lbracket */
            int depth = 1;
            bb->active++;
            for(;;) {
                c = *(bb->active);
                if(c == NUL32) fail(ttm,EEOS); /* Unexpected EOF */
                *bb->passive++ = c;
                bb->active++;
                if(isescape(c)) {
                    *bb->passive++ = *bb->active++;
                } else if(c == ttm->openc) {
                    depth++;
                } else if(c == ttm->closec) {
                    if(--depth == 0) {bb->passive--; break;} /* we are done */
                } /* else keep moving */
            }/*<...> for*/
        } else { /* non-signficant character */
            *bb->passive++ = c;
            bb->active++;
        }
    } /*scan for*/

    /* When we get here, we are finished, so clean up */
    { 
        unsigned int newlen;
        /* reset the buffer length using bb->passive.*/
        newlen = bb->passive - bb->content;
        setBufferLength(ttm,bb,newlen);
        /* reset bb->active */
        bb->active = bb->passive;
    }
exiting:
    return;
}

static void
exec(TTM* ttm, Buffer* bb)
{
    Frame* frame;
    Name* fcn;
    utf32* savepassive;

    frame = pushFrame(ttm);
    /* Skip to the start of the function name */
    if(bb->active[1] == ttm->openc) {
        bb->active += 2;
        frame->active = 1;
    } else {
        bb->active += 3;
        frame->active = 0;
    }
    /* Parse and store relevant pointers into frame. */
    savepassive = bb->passive;
    parsecall(ttm,frame);
    bb->passive = savepassive;
    if(ttm->flags & FLAG_EXIT) goto exiting;

    /* Now execute this function, which will leave result in bb->result */
    if(frame->argc == 0) fail(ttm,ENONAME);
    if(strlen32(frame->argv[0])==0) fail(ttm,ENONAME);
    /* Locate the function to execute */
    fcn = dictionaryLookup(ttm,frame->argv[0]);
    if(fcn == NULL) fail(ttm,ENONAME);
    if(fcn->minargs > (frame->argc - 1)) /* -1 to account for function name*/
        fail(ttm,EFEWPARMS);
    /* Reset the result buffer */
    resetBuffer(ttm,ttm->result);
    if(ttm->flags & FLAG_TRACE || fcn->trace)
        trace(ttm,1,TRACING);
    if(fcn->builtin) {
        fcn->fcn(ttm,frame);
        if(fcn->novalue) resetBuffer(ttm,ttm->result);
        if(ttm->flags & FLAG_EXIT) goto exiting;
    } else /* invoke the pseudo function "call" */
        call(ttm,frame,fcn->body);

#ifdef DEBUG
fprintf(stderr,"result: ");
dbgprint32(ttm->result->content,'|');
fprintf(stderr,"\n");
#endif

    if(ttm->flags & FLAG_TRACE || fcn->trace)
        trace(ttm,0,TRACING);

    /* Now, put the result into the buffer */
    if(!fcn->novalue && ttm->result->length > 0) {
        utf32* insertpos;
        unsigned int resultlen = ttm->result->length;
        /*Compute the space avail between bb->passive and bb->active */
        unsigned int avail = (bb->active - bb->passive); 
        /* Compute amount we need to expand, if any */
        if(avail < resultlen)
            expandBuffer(ttm,bb,(resultlen - avail));/*will change bb->active*/
        /* We insert the result as follows:
           frame->passive => insert at bb->passive (and move bb->passive)
           frame->active => insert at bb->active - (ttm->result->length)
        */
        if(frame->active) {
            insertpos = (bb->active - resultlen);
            bb->active = insertpos;         
        } else { /*frame->passive*/
            insertpos = bb->passive;
            bb->passive += resultlen;
        }
        memcpy32((void*)insertpos,ttm->result->content,ttm->result->length);
#ifdef DEBUG
fprintf(stderr,"context:\n\tpassive=|");
/* Since passive is not normally null terminated, we need to fake it */
  {int i; utf32* p;
    for(p=ttm->buffer->passive,i=0;i<PASSIVEMAX && *p != NUL32;i++,p++)
      dbgprint32c(*p,'|');
    fprintf(stderr,"...|\n");
    fprintf(stderr,"\tactive=|");
    for(p=ttm->buffer->active,i=0;i<ACTIVEMAX && *p != NUL32;i++,p++)
      dbgprint32c(*p,'|');
    fprintf(stderr,"...|\n");
  }
#endif

    }
exiting:
    popFrame(ttm);
}

/**
Construct a frame; leave bb->active pointing just
past the call.
*/
static void
parsecall(TTM* ttm, Frame* frame)
{
    int done,depth;
    utf32 c;
    Buffer* bb = ttm->buffer;

    done = 0;
    do {
        utf32* arg = bb->passive;/* start of ith argument */
        while(!done) {
            c = *bb->active; /* Note that we do not bump here */
            if(c == NUL32) fail(ttm,EEOS); /* Unexpected end of buffer */
            if(isescape(c)) {
                bb->active++;
                *bb->passive++ = *bb->active++;
            } else if(c == ttm->semic || c == ttm->closec) {
                /* End of an argument */
                *bb->passive++ = NUL; /* null terminate the argument */
#ifdef DEBUG
fprintf(stderr,"parsecall: argv[%d]=",frame->argc);
dbgprint32(arg,'|');
fprintf(stderr,"\n");
#endif
                bb->active++; /* skip the semi or close */
                /* move to next arg */
                frame->argv[frame->argc++] = arg;
                if(c == ttm->closec) done=1;
                else if(frame->argc >= MAXARGS) fail(ttm,EMANYPARMS);
                else arg = bb->passive;
            } else if(c == ttm->sharpc) {
                /* check for call within call */
                if(bb->active[1] == ttm->openc
                   || (bb->active[1] == ttm->sharpc
                        && bb->active[2] == ttm->openc)) {
                    /* Recurse to compute inner call */
                    exec(ttm,bb);
                    if(ttm->flags & FLAG_EXIT) goto exiting;
                }
            } else if(c == ttm->openc) {/* <...> nested brackets */
                bb->active++; /* skip leading lbracket */
                depth = 1;
                for(;;) {
                    c = *(bb->active);
                    if(c == NUL) fail(ttm,EEOS); /* Unexpected EOF */
                    if(isescape(c)) {
                        *bb->passive++ = (char)c;
                        *bb->passive++ = *bb->active++;         
                    } else if(c == ttm->openc) {
                        *bb->passive++ = (char)c;
                        bb->active++;
                        depth++;
                    } else if(c == ttm->closec) {
                        depth--;
                        bb->active++;
                        if(depth == 0) break; /* we are done */
                        *bb->passive++ = (char)c;
                    } else {
                        *bb->passive++ = *bb->active++;
                    }
                }/*<...> for*/
            } else {
                /* keep moving */
                *bb->passive++ = c;
                bb->active++;           
            }
        } /* collect argument for */
    } while(!done);
exiting:
    return;
}

/**************************************************/
/**
Execute a non-builtin function
*/

static void
call(TTM* ttm, Frame* frame, utf32* body)
{
    utf32* p;
    utf32 c;
    unsigned int len;
    utf32* result;
    utf32* dst;

    /* Compute the size of the output */
    for(len=0,p=body;(c=*p++);) {
        if(testMark(c,SEGMARK)) {
            unsigned int segindex = (unsigned int)(*p++);
            if(segindex < frame->argc)
                len += strlen32(frame->argv[segindex]);
            /* else treat as empty string */
        } else if(c == CREATE) {
            len += CREATELEN;
        } else
            len++;
    }
    /* Compute the body using ttm->result  */
    resetBuffer(ttm,ttm->result);
    setBufferLength(ttm,ttm->result,len);
    result = ttm->result->content;
    dst = result;
    dst[0] = NUL32; /* so we can use strcat */
    for(p=body;(c=*p++);) {
        if(testMark(c,SEGMARK)) {
            unsigned int segindex = (unsigned int)(c & 0xFF);
            if(segindex < frame->argc) {
                utf32* arg = frame->argv[segindex];
                strcpy32(dst,arg);
                dst += strlen32(arg);
            } /* else treat as null string */
        } else if (c == CREATE) {
            int len;
            char crformat[16];
            char crval[CREATELEN+1];
            /* Construct the format string on the fly */
            snprintf(crformat,sizeof(crformat),"%%0%dd",CREATELEN);
            /* Construct the cr value */
            ttm->crcounter++;
            snprintf(crval,sizeof(crval),crformat,ttm->crcounter);
            len = toString32(dst,crval,TOEOS);
            dst += len;
        } else
            *dst++ = (char)c;
    }
    *dst = NUL32;
    setBufferLength(ttm,ttm->result,(dst-result));
}

/**************************************************/
/* Built-in Support Procedures */
static void
printstring(TTM* ttm, FILE* output, utf32* s32, int printall)
{
    int slen = strlen32(s32);
    utf32 c32,prev;

    if(slen == 0) return;
    prev = 0;
    while((c32=*s32++)) {
        if(isescape(c32)) {
            c32 = *s32++;
            c32 = convertEscapeChar(c32);
        }
        if(c32 != 0) {
            if(printall || c32 == '\n' || !iscontrol(c32)) {
                if(ismark(c32)) {
                    char_t* p;
                    char info[16+1];
                    if(iscreate(c32))
                        strcpy(info,"^00");
                    else /* segmark */
                        snprintf(info,sizeof(info),"^%02d",(int)(c32 & 0xFF));
                    for(p=info;*p;p++) fputc32((utf32)*p,output);
                } else
                    fputc32(c32,output);
            }
        }
        prev = c32;
    }
    if(prev != '\n')
        fputc32((utf32)'\n',output);
    fflush(output);
}

/**************************************************/
/* Builtin functions */

/* Original TTM functions */

/* Dictionary Operations */
static void
ttm_ap(TTM* ttm, Frame* frame) /* Append to a string */
{
    utf32* body;
    utf32* apstring;
    Name* str = dictionaryLookup(ttm,frame->argv[1]);
    unsigned int aplen, bodylen;

    if(str == NULL) {/* Define the string */
        ttm_ds(ttm,frame);
        return;
    }
    if(str->builtin) fail(ttm,ENOPRIM);
    apstring = frame->argv[2];
    aplen = strlen32(apstring);
    body = str->body;
    bodylen = strlen32(body);
    body = realloc(body,sizeof(utf32)*(bodylen+aplen+1));
    if(body == NULL) fail(ttm,EMEMORY);
    strcpy32(body+bodylen,apstring);
    str->residual = bodylen+aplen;
}

/**
The semantics of #<cf>
have been changed. See ttm.html
*/

static void
ttm_cf(TTM* ttm, Frame* frame) /* Copy a function */
{
    utf32* newname = frame->argv[1];
    utf32* oldname = frame->argv[2];
    Name* newstr = dictionaryLookup(ttm,newname);
    Name* oldstr = dictionaryLookup(ttm,oldname);
    struct HashEntry saveentry;

    if(oldstr == NULL)
        fail(ttm,ENONAME);
    if(newstr == NULL) {
        /* create a new string object */
        newstr = newName(ttm);
        newstr->entry.name = strdup32(newname);
        dictionaryInsert(ttm,newstr);
    }
    saveentry = newstr->entry;
    *newstr = *oldstr;
    newstr->entry = saveentry;
    /* Do fixup */
    if(newstr->body != NULL) {
        newstr->body = strdup32(newstr->body);
    }
}

static void
ttm_cr(TTM* ttm, Frame* frame) /* Mark for creation */
{
    Name* str;
    int bodylen,crlen;
    utf32* body;
    utf32* crstring;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    body = str->body;
    bodylen = strlen32(body);
    crstring = frame->argv[2];
    crlen = strlen32(crstring);
    if(crlen > 0) { /* search only if possible success */
        utf32* p;
        /* Search for occurrences of arg */
        p = body + str->residual;
        while(*p) {
            if(strncmp32(p,crstring,crlen) != 0)
                {p++; continue;}
            /* we have a match, replace match by a create marker */
            *p = CREATE;
            /* compress out all but 1 char of crstring match */
            if(crlen > 1)
                strcpy32(p+1,p+crlen);
        }
    }
}

static void
ttm_ds(TTM* ttm, Frame* frame)
{
    Name* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL) {
        /* create a new string object */
        str = newName(ttm);
        str->entry.name = strdup32(frame->argv[1]);
        dictionaryInsert(ttm,str);
    } else {
        /* reset as needed */
        str->builtin = 0;
        str->minargs = 0;
        str->maxargs = 0;
        str->residual = 0;
        str->maxsegmark = 0;
        str->fcn = NULL;
        if(str->body != NULL) free(str->body);
        str->body = NULL;
    }
    str->body = strdup32(frame->argv[2]);
}

static void
ttm_es(TTM* ttm, Frame* frame) /* Erase string */
{
    unsigned int i;
    for(i=1;i<frame->argc;i++) {
        utf32* strname = frame->argv[i];
        Name* str = dictionaryLookup(ttm,strname);
        if(str != NULL && !str->locked) {
            dictionaryRemove(ttm,strname);
            freeName(ttm,str); /* reclaim the string */
        }
    }
}

/* Helper function for #<sc> and #<ss> */
static int
ttm_ss0(TTM* ttm, Frame* frame)
{
    Name* str;
    unsigned int i,segcount,startseg,bodylen;
    utf32* startp;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    bodylen = strlen32(str->body);
    if(str->residual >= bodylen)
        return 0; /* no substitution possible */
    segcount = 0;
    startseg = str->maxsegmark;
    startp = str->body + str->residual;
    for(i=2;i<frame->argc;i++) {
        utf32* arg = frame->argv[i];
        unsigned int arglen = strlen32(arg);
        if(arglen > 0) { /* search only if possible success */
            int found;
            utf32* p;
            /* Search for occurrences of arg */
            p = str->body + str->residual;
            found = 0;
            while(*p) {
                if(strncmp32(p,arg,arglen) != 0)
                    {p++; continue;}
                /* we have a match, replace match by a segment marker */
                if(!found) {/* first match */
                    startseg++;
                    found++;
                }
                *p = (SEGMARK | startseg);
                /* compress out all but 1 char of match */
                if(arglen > 1)
                    strcpy32(p+1,p+arglen);
                segcount++;
            }
        }
    }
    str->maxsegmark = startseg;
    return segcount;
}

static void
ttm_sc(TTM* ttm, Frame* frame) /* Segment and count */
{
    char count[64];
    int n,nsegs = ttm_ss0(ttm,frame);
    snprintf(count,sizeof(count),"%d",nsegs);
    /* Insert into ttm->result */
    setBufferLength(ttm,ttm->result,strlen(count));
    n = toString32(ttm->result->content,count,TOEOS);
    setBufferLength(ttm,ttm->result,n);
}

static void
ttm_ss(TTM* ttm, Frame* frame) /* Segment and count */
{
    (void)ttm_ss0(ttm,frame);
}

/* Name Selection */

static void
ttm_cc(TTM* ttm, Frame* frame) /* Call one character */
{
    Name* str;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);
    /* Check for pointing at trailing NUL */
    if(str->residual < strlen32(str->body)) {
        utf32 c32 = *(str->body+str->residual);
        *ttm->result->content = c32;
        setBufferLength(ttm,ttm->result,1);
        str->residual++;
    }
}

static void
ttm_cn(TTM* ttm, Frame* frame) /* Call n characters */
{
    Name* str;
    long long ln;
    unsigned n;
    int isneg;
    ERR err;
    unsigned int bodylen,startn;
    unsigned int avail;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    /* Get number of characters to extract */
    err = toInt64(frame->argv[1],&ln);
    if(err != ENOERR) fail(ttm,err);
    if(ln >= MAXINT || ln <= MININT) fail(ttm,ERANGE);
    isneg = 0;
    if(ln < 0) {isneg = 1; ln = -ln;}
    n = (unsigned int)ln;

    /* See if we have enough space */
    bodylen = strlen32(str->body);
    if(str->residual >= bodylen)
	avail = 0;
    else
        avail = (bodylen - str->residual);
   
    if(n == 0 || avail == 0) goto nullreturn;
    if(avail < n) n = avail; /* return what is available */

    /* Figure out the starting and ending pointers for the transfer */
    if(isneg) {/* n was originally negative */
        /* We want n characters starting at bodylen - |n| */
        startn = bodylen - n;
	} else {
        /* We want n characters starting at residual */
        startn = str->residual;
    }
        
    /* ok, copy n characters from startn to endn into the return buffer */
    setBufferLength(ttm,ttm->result,n);
    strncpy32(ttm->result->content,str->body+startn,n);
    /* increment residual */
    str->residual += n;
    return;

nullreturn:
    setBufferLength(ttm,ttm->result,0);
    ttm->result->content[0] = NUL;
    return;
}

static void
ttm_cp(TTM* ttm, Frame* frame) /* Call parameter */
{
    Name* str;
    unsigned int delta;
    utf32* rp;
    utf32* rp0;
    utf32 c32;
    int depth;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    rp = str->body + str->residual;
    rp0 = rp;
    depth = 0;
    ttm->result->content[0] = NUL32; /* so we can strcat */
    for(;(c32=*rp);rp++) {
        if(c32 == ttm->semic) {
            if(depth == 0) break; /* reached unnested semicolon*/
        } else if(c32 == ttm->openc) {
            depth++;
        } else if(c32 == ttm->closec) {
            depth--;
        }
    }
    delta = (rp - rp0);
    setBufferLength(ttm,ttm->result,delta);
    strncpy32(ttm->result->content,rp0,delta);
    str->residual += delta;
    if(c32 != NUL32) str->residual++;
}

static void
ttm_cs(TTM* ttm, Frame* frame) /* Call segment */
{
    Name* str;
    utf32 c32;
    utf32* p;
    utf32* p0;
    unsigned int delta;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume yes */
    p0 = str->body + str->residual;
    p = p0;
    for(;(c32=*p);p++) {
        if(c32 == NUL32 || testMark(c32,SEGMARK) || testMark(c32,CREATE))
            break;
    }
    delta = (p - p0);
    if(delta > 0) {
        setBufferLength(ttm,ttm->result,delta);
        strncpy32(ttm->result->content,p0,delta);
    }
    /* set residual pointer correctly */
    str->residual += delta;
    if(c32 != NUL32) str->residual++;
}

static void
ttm_isc(TTM* ttm, Frame* frame) /* Initial character scan; moves residual pointer */
{
    Name* str;
    utf32* t;
    utf32* f;
    utf32* result;
    utf32* arg;
    unsigned int arglen;
    unsigned int slen;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    arg = frame->argv[1];
    arglen = strlen32(arg);
    t = frame->argv[3];
    f = frame->argv[4];

    /* check for initial string match */
    if(strncmp32(str->body+str->residual,arg,arglen)==0) {
        result = t;
        str->residual += arglen;
        slen = strlen32(str->body);
        if(str->residual > slen) str->residual = slen;
    } else
        result = f;
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);    
}

static void
ttm_rrp(TTM* ttm, Frame* frame) /* Reset residual pointer */
{
    Name* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);
    str->residual = 0;
}

static void
ttm_scn(TTM* ttm, Frame* frame) /* Character scan */
{
    Name* str;
    unsigned int arglen;
    unsigned int bodylen;
    utf32* f;
    utf32* result;
    utf32* arg;
    utf32* p;
    utf32* p0;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    arg = frame->argv[1];
    arglen = strlen32(arg);
    f = frame->argv[3];

    /* check for sub string match */
    p0 = str->body+str->residual;
    p = p0;
    result = NULL;
    for(;*p;p++) {
        if(strncmp32(p,arg,arglen)==0) {result = p; break;}
    }    
    if(result == NULL) {
        setBufferLength(ttm,ttm->result,strlen32(f));
        strcpy32(ttm->result->content,f);    
    } else {
        unsigned int len = (p - p0);
        setBufferLength(ttm,ttm->result,len);
        strncpy32(ttm->result->content,p0,len);
        str->residual += (len + arglen);
        bodylen = strlen32(str->body);
        if(str->residual > bodylen) str->residual = bodylen;
    }
}

static void
ttm_sn(TTM* ttm, Frame* frame) /* Skip n characters */
{
    ERR err;
    long long num;
    Name* str;
    unsigned int bodylen;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    err = toInt64(frame->argv[1],&num);
    if(err != ENOERR) fail(ttm,err);
    if(num < 0) fail(ttm,EPOSITIVE);   

    str->residual += (int)num;
    bodylen = strlen32(str->body);
    if(str->residual > bodylen)
        str->residual = bodylen;
}

static void
ttm_eos(TTM* ttm, Frame* frame) /* Test for end of string */
{
    Name* str;
    unsigned int bodylen;
    utf32* t;
    utf32* f;
    utf32* result;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);
    bodylen = strlen32(str->body);
    t = frame->argv[2];
    f = frame->argv[3];
    result = (str->residual >= bodylen ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

/* Name Scanning Operations */

static void
ttm_gn(TTM* ttm, Frame* frame) /* Give n characters from argument string*/
{
    utf32* snum = frame->argv[1];
    utf32* s = frame->argv[2];
    unsigned int slen = strlen32(s);
    ERR err;
    long long num;
    utf32* startp;

    err = toInt64(snum,&num);
    if(err != ENOERR) fail(ttm,err);
    if(num > 0) {
        if(slen < num) num = slen;
        startp = s;
    } else if(num < 0) {
        num = -num;
        startp = s + num;
        num = (slen - num);
    }
    if(num != 0) {
        setBufferLength(ttm,ttm->result,(unsigned int)num);
        strncpy32(ttm->result->content,startp,(unsigned int)num);
    }
}

static void
ttm_zlc(TTM* ttm, Frame* frame) /* Zero-level commas */
{
    utf32* s;
    utf32* q;
    int slen,c,depth;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen); /* result will be same length */
    for(depth=0,q=ttm->result->content;(c=*s);s++) {
        if(isescape(c)) {
            *q++ = c;
            *q++ = *s++; /* skip escaped character */
        } else if(c == COMMA && depth == 0) {
            *q++ = ttm->semic;  
        } else if(c == LPAREN) {
            depth++;
            *q++ = c;
        } else if(c == RPAREN) {
            depth--;
            *q++ = c;
        } else {
            *q++ = c;
        }
    }
    *q = NUL32; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(q - ttm->result->content));
}

static void
ttm_zlcp(TTM* ttm, Frame* frame) /* Zero-level commas and parentheses;
                                    exact algorithm is unknown */
{
    /* A(B) and A,B will both give A;B and (A),(B),C will give A;B;C */
    utf32* s;
    utf32* p;
    utf32* q;
    utf32 c;
    int slen,depth;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen); /* result may be shorter; handle below */
    q = ttm->result->content;
    p = s;
    for(depth=0;(c=*p);p++) {
        if(isescape(c)) {
            *q++ = c;
            p++;
            *q++ = *p;
        } else if(depth == 0 && c == COMMA) {
            if(p[1] != LPAREN) {*q++ = ttm->semic;}
        } else if(c == LPAREN) {
            if(depth == 0 && p > s) {*q++ = ttm->semic;}
            if(depth > 0) *q++ = c;
            depth++;
        } else if(c == RPAREN) {
            depth--;
            if(depth == 0 && p[1] == COMMA) {
            } else if(depth == 0 && p[1] == NUL32) {/* do nothing */
            } else if(depth == 0) {
                *q++ = ttm->semic;
            } else {/* depth > 0 */
                *q++ = c;
            }
        } else {
            *q++ = c;      
        }
    }
    *q = NUL32; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(q-ttm->result->content));
}

static void
ttm_flip(TTM* ttm, Frame* frame) /* Flip a string */
{
    utf32* s;
    utf32* q;
    utf32* p;
    int slen,i;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen);
    p = s + slen;
    q=ttm->result->content;
    for(i=0;i<slen;i++) {*q++ = *(--p);}
    *q = NUL32;
}

static void
ttm_ccl(TTM* ttm, Frame* frame) /* Call class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Name* str = dictionaryLookup(ttm,frame->argv[2]);
    utf32 c;
    utf32* p;
    utf32* start;
    int len;

    if(cl == NULL || str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    for(p=start;(c=*p);p++) {
        if(cl->negative && charclassMatch(c,cl->characters)) break;
        if(!cl->negative && !charclassMatch(c,cl->characters)) break;
    }
    len = (p - start);
    if(len > 0) {
        setBufferLength(ttm,ttm->result,len);
        strncpy32(ttm->result->content,start,len);
        ttm->result->content[len] = NUL32;
        str->residual += len;
    }
}

/* Shared helper for dcl and dncl */
static void
ttm_dcl0(TTM* ttm, Frame* frame, int negative)
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    if(cl == NULL) {
        /* create a new charclass object */
        cl = newCharclass(ttm);
        cl->entry.name = strdup32(frame->argv[1]);
        charclassInsert(ttm,cl);
    }
    if(cl->characters != NULL)
        free(cl->characters);
    cl->characters = strdup32(frame->argv[2]);
    cl->negative = negative;
}

static void
ttm_dcl(TTM* ttm, Frame* frame) /* Define a negative class */
{
    ttm_dcl0(ttm,frame,0);
}

static void
ttm_dncl(TTM* ttm, Frame* frame) /* Define a negative class */
{
    ttm_dcl0(ttm,frame,1);
}

static void
ttm_ecl(TTM* ttm, Frame* frame) /* Erase a class */
{
    unsigned int i;
    for(i=1;i<frame->argc;i++) {
        utf32* clname = frame->argv[i];
        Charclass* prev = charclassRemove(ttm,clname);
        if(prev != NULL) {
            freeCharclass(ttm,prev); /* reclaim the character class */
        }
    }
}

static void
ttm_scl(TTM* ttm, Frame* frame) /* Skip class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Name* str = dictionaryLookup(ttm,frame->argv[2]);
    utf32 c;
    utf32* p;
    utf32* start;
    int len;

    if(cl == NULL || str == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);

    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    for(p=start;(c=*p);p++) {
        if(cl->negative && charclassMatch(c,cl->characters)) break;
        if(!cl->negative && !charclassMatch(c,cl->characters)) break;
    }
    len = (p - start);
    str->residual += len;
}

static void
ttm_tcl(TTM* ttm, Frame* frame) /* Test class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Name* str = dictionaryLookup(ttm,frame->argv[2]);
    utf32* retval;
    int retlen;
    utf32* t;
    utf32* f;

    if(cl == NULL)
        fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);
    t = frame->argv[3];
    f = frame->argv[4];
    if(str == NULL)
        retval = f;
    else {
        /* see if char at str->residual is in class */
        utf32 c32 = *(str->body + str->residual);
        if(cl->negative && !charclassMatch(c32,cl->characters)) 
            retval = t;
        else if(!cl->negative && charclassMatch(c32,cl->characters))
            retval = t;
        else
            retval = f;
    }
    retlen = strlen32(retval);
    if(retlen > 0) {
        setBufferLength(ttm,ttm->result,retlen);
        strcpy32(ttm->result->content,retval);
    }    
}

/* Arithmetic Operators */

static void
ttm_abs(TTM* ttm, Frame* frame) /* Obtain absolute value */
{
    utf32* slhs;
    long long lhs;
    ERR err;
    char result[32];
    int count;

    slhs = frame->argv[1];    
    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    if(lhs < 0) lhs = -lhs;
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result)); /*overkill*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);/*fixup*/
}

static void
ttm_ad(TTM* ttm, Frame* frame) /* Add */
{
    utf32* snum;
    long long num;
    long long total;
    ERR err;
    unsigned int i,count;

    total = 0;
    for(i=1;i<frame->argc;i++) {
        snum = frame->argv[i];    
        err = toInt64(snum,&num);
        if(err != ENOERR) fail(ttm,err);
        total += num;
    }
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,total);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_dv(TTM* ttm, Frame* frame) /* Divide and give quotient */
{
    utf32* slhs;
    utf32* srhs;
    long long lhs,rhs;
    ERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs / rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_dvr(TTM* ttm, Frame* frame) /* Divide and give remainder */
{
    utf32* slhs;
    utf32* srhs;
    long long lhs,rhs;
    ERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs % rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_mu(TTM* ttm, Frame* frame) /* Multiply */
{
    utf32* snum;
    long long num;
    long long total;
    ERR err;
    unsigned int i,count;

    total = 1;
    for(i=1;i<frame->argc;i++) {
        snum = frame->argv[i];    
        err = toInt64(snum,&num);
        if(err != ENOERR) fail(ttm,err);
        total *= num;
    }
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,total);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_su(TTM* ttm, Frame* frame) /* Substract */
{
    utf32* slhs;
    utf32* srhs;
    long long lhs,rhs;
    ERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs - rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_eq(TTM* ttm, Frame* frame) /* Compare numeric equal */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    long long lhs,rhs;
    ERR err;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs == rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_gt(TTM* ttm, Frame* frame) /* Compare numeric greater-than */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    long long lhs,rhs;
    ERR err;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs > rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_lt(TTM* ttm, Frame* frame) /* Compare numeric less-than */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    long long lhs,rhs;
    ERR err;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs < rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_eql(TTM* ttm, Frame* frame) /* ? Compare logical equal */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp32(slhs,srhs) == 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_gtl(TTM* ttm, Frame* frame) /* ? Compare logical greater-than */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp32(slhs,srhs) > 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_ltl(TTM* ttm, Frame* frame) /* ? Compare logical less-than */
{
    utf32* slhs;
    utf32* srhs;
    utf32* t;
    utf32* f;
    utf32* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp32(slhs,srhs) < 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

/* Peripheral Input/Output Operations */

/**
In order to a void spoofing,
the string to be output is
modified to remove all control
characters except '\n', and a final
'\n' is forced.
*/
static void
ttm_ps(TTM* ttm, Frame* frame) /* Print a Name */
{
    utf32* s = frame->argv[1];
    utf32* stdxx = (frame->argc == 2 ? NULL : frame->argv[2]);
    FILE* target;
    if(stdxx != NULL && streq328(stdxx,"stderr"))
        target=stderr;
    else
        target = stdout;
    printstring(ttm,target,s,!PRINTALL);
}

/**
In order to avoid spoofing, the
string 'ttm>' is output before reading
if reading from stdin.
*/
static void
ttm_rs(TTM* ttm, Frame* frame) /* Read a Name */
{
    int len;
    utf32 c;
    if(ttm->isstdin)
        {fprintf(stdout,"ttm>");fflush(stdout);}
    for(len=0;;len++) {
        c=fgetc32(ttm->rsinput);
        if(c == EOF) break;
        if(c == ttm->metac) break;
        setBufferLength(ttm,ttm->result,len+1);
        ttm->result->content[len] = c;
    }
}

static void
ttm_psr(TTM* ttm, Frame* frame) /* Print Name and Read */
{
    /* force output to goto stdout */
    int argc = frame->argc;
    if(argc > 2) frame->argc = 2;
    ttm_ps(ttm,frame);
    ttm_rs(ttm,frame);
    frame->argc = argc;
}

static void
ttm_cm(TTM* ttm, Frame* frame) /* Change meta character */
{
    utf32* smeta = frame->argv[1];
    if(strlen32(smeta) > 0) {
        if(smeta[0] > 127) fail(ttm,EASCII);
        ttm->metac = smeta[0];
    }
}

static void
ttm_pf(TTM* ttm, Frame* frame) /* Flush stdout and/or stderr */
{
    utf32* stdxx = (frame->argc == 1 ? NULL : frame->argv[1]);
    if(stdxx == NULL || streq328(stdxx,"stdout"))
        fflush(stderr);
    if(stdxx == NULL || streq328(stdxx,"stderr"))
        fflush(stderr);
}

/* Library Operations */

static void
ttm_names(TTM* ttm, Frame* frame) /* Obtain all Name instance names in sorted order */
{
    int i,nnames,index,allnames;
    utf32** names;
    unsigned int len;
    utf32* p;

    allnames = (frame->argc > 1 ? 1 : 0);

    /* First, figure out the number of names and the total size */
    len = 0;
    for(nnames=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
        while(entry != NULL) {
	    Name* name = (Name*)entry;
	    if(allnames || !name->builtin) {
		len += strlen32(name->entry.name);
                nnames++;
            }
	    entry = entry->next;
        }
    }

    if(nnames == 0)
        return;

    /* Now collect all the names */
    names = (utf32**)malloc(sizeof(utf32*)*nnames);
    if(names == NULL) fail(ttm,EMEMORY);
    index = 0;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
        while(entry != NULL) {
	    Name* name = (Name*)entry;
            if(allnames || !name->builtin) {
                names[index++] = name->entry.name;                
            }
            entry = entry->next;
        }
    }

    /* Now bubble sort the set of names */
    for(;;) {
        int swapped = 0;
        for(i=1;i<nnames;i++) {
            /* test out of order */
            if(strcmp32(names[i-1],names[i]) > 0) {
                /* swap them and remember something changed */
                utf32* tmp = names[i-1];
                names[i-1] = names[i];
                names[i] = tmp;
                swapped = 1;
            }
        }
        if(!swapped) break;
    }
    /* Return the set of names separated by commas */    
    setBufferLength(ttm,ttm->result,len+(nnames-1));
    p = ttm->result->content;
    for(i=0;i<nnames;i++) {
        if(i > 0) {*p++ = ',';}
        strcpy32(p,names[i]);
        p += strlen32(names[i]);
    }
}

static void
ttm_exit(TTM* ttm, Frame* frame) /* Return from TTM */
{
    long long exitcode = 0;
    ttm->flags |= FLAG_EXIT;
    if(frame->argc > 1) {
        ERR err = toInt64(frame->argv[1],&exitcode);
        if(err != ENOERR) exitcode = 1;
        else if(exitcode < 0) exitcode = - exitcode;
    }   
    ttm->exitcode = (int)exitcode;
}

/* Utility Operations */

static void
ttm_ndf(TTM* ttm, Frame* frame) /* Determine if a Name is Defined */
{
    Name* str;
    utf32* t;
    utf32* f;
    utf32* result;

    str = dictionaryLookup(ttm,frame->argv[1]);
    t = frame->argv[2];
    f = frame->argv[3];
    result = (str == NULL ? f : t);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_norm(TTM* ttm, Frame* frame) /* Obtain the Norm of a string */
{
    utf32* s;
    char result[32];
    int count;

    s = frame->argv[1];
    snprintf(result,sizeof(result),"%u",strlen32(s));
    setBufferLength(ttm,ttm->result,strlen(result)); /*temp*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_time(TTM* ttm, Frame* frame) /* Obtain time of day */
{
    char result[MAXINTCHARS+1];
    int count;
    struct timeval tv;
    long long time;

    if(timeofday(&tv) < 0)
        fail(ttm,ETIME);
    time = (long long)tv.tv_sec;
    time *= 1000000; /* convert to microseconds */
    time += tv.tv_usec;
    time = time / 10000; /* Need time in 100th second */
    snprintf(result,sizeof(result),"%lld",time);
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_xtime(TTM* ttm, Frame* frame) /* Obtain Execution Time */
{
    long long time;
    char result[MAXINTCHARS+1];
    int count;

    time = getRunTime();
    snprintf(result,sizeof(result),"%lld",time);
    setBufferLength(ttm,ttm->result,strlen(result));/*temp*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_ctime(TTM* ttm, Frame* frame) /* Convert ##<time> to printable string */
{
    utf32* stod;
    ERR err;
    long long tod;
    char_t result[1024];
    time_t ttod;
    unsigned int count;

    stod = frame->argv[1];
    err = toInt64(stod,&tod);
    if(err != ENOERR) fail(ttm,err);
    tod = tod/100; /* need seconds */
    ttod = (time_t)tod;
    snprintf(result,sizeof(result),"%s",ctime(&ttod));
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_tf(TTM* ttm, Frame* frame) /* Turn Trace Off */
{
    if(frame->argc > 1) {/* trace off specific*/
        unsigned int i;
        for(i=1;i<frame->argc;i++) {
            Name* fcn = dictionaryLookup(ttm,frame->argv[i]);
            if(fcn == NULL) fail(ttm,ENONAME);      
            fcn->trace = 0;
        }
    } else { /* turn off all tracing */
        int i;
        for(i=0;i<HASHSIZE;i++) {
	    struct HashEntry* entry = ttm->dictionary.table[i].next;
            while(entry != NULL) {
		Name* name = (Name*)entry;
                name->trace = 0;
                entry = entry->next;
            }
        }
        ttm->flags &= ~(FLAG_TRACE);
    }
}

static void
ttm_tn(TTM* ttm, Frame* frame) /* Turn Trace On */
{
    if(frame->argc > 1) {/* trace specific*/
        unsigned int i;
        for(i=1;i<frame->argc;i++) {
            Name* fcn = dictionaryLookup(ttm,frame->argv[i]);
            if(fcn == NULL) fail(ttm,ENONAME);      
            fcn->trace = 1;
        }
    } else 
        ttm->flags |= (FLAG_TRACE); /*trace all*/
}

/* Functions new to this implementation */

 /* Get ith command line argument; zero is command */
static void
ttm_argv(TTM* ttm, Frame* frame)
{
    long long index = 0;
    ERR err;
    int count,arglen;
    char* arg;

    err = toInt64(frame->argv[1],&index);
    if(err != ENOERR) fail(ttm,err);
    if(index < 0 || index >= getOptionNameLength(argoptions))
        fail(ttm,ERANGE);
    arg = argoptions[index];
    arglen = strlen(arg);
    setBufferLength(ttm,ttm->result,arglen);/*temp*/
    count = toString32(ttm->result->content,arg,arglen);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_classes(TTM* ttm, Frame* frame) /* Obtain all character class names */
{
    int i,nclasses,index;
    utf32** classes;
    unsigned int len;
    utf32* p;

    /* First, figure out the number of classes */
    for(nclasses=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->charclasses.table[i].next;
	while(entry != NULL) {
            nclasses++;
            entry = entry->next;
        }
    }

    if(nclasses == 0)
        return;

    /* Now collect all the class and their total size */
    classes = (utf32**)malloc(sizeof(utf32*)*nclasses);
    if(classes == NULL) fail(ttm,EMEMORY);
    for(len=0,index=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->charclasses.table[i].next;
	while(entry != NULL) {
            Charclass* charclass = (Charclass*)entry;
            classes[index++] = charclass->entry.name;
            len += strlen32(charclass->entry.name);
            entry = entry->next;
        }
    }

    /* Now bubble sort the set of classes */
    for(;;) {
        int swapped = 0;
        for(i=1;i<nclasses;i++) {
            /* test out of order */
            if(strcmp32(classes[i-1],classes[i]) > 0) {
                /* swap them and remember something changed */
                utf32* tmp = classes[i-1];
                classes[i-1] = classes[i];
                classes[i] = tmp;
                swapped = 1;
            }
        }
        if(!swapped) break;
    }

    /* Return the set of classes separated by commas */    
    setBufferLength(ttm,ttm->result,len+(nclasses-1));
    p = ttm->result->content;
    for(i=0;i<nclasses;i++) {
        if(i > 0) {*p++ = ',';}
        strcpy32(p,classes[i]);
        p += strlen32(classes[i]);
    }
}

static void
ttm_lf(TTM* ttm, Frame* frame) /* Lock a function from being deleted */
{
    unsigned int i;
    for(i=1;i<frame->argc;i++) {
        Name* fcn = dictionaryLookup(ttm,frame->argv[i]);
        if(fcn == NULL) fail(ttm,ENONAME);          
        fcn->locked = 1;
    }
}

static void
ttm_uf(TTM* ttm, Frame* frame) /* Un-Lock a function from being deleted */
{
    unsigned int i;
    for(i=1;i<frame->argc;i++) {
        Name* fcn = dictionaryLookup(ttm,frame->argv[i]);
        if(fcn == NULL) fail(ttm,ENONAME);          
        fcn->locked = 0;
    }
}

/**
For security reasons, we impose the constraint
that the file name must only be accessible
through one of the include paths.
This has the possibly undesirable consequence
that if the user used #<include>, then the user
must also specify a -I on the command line.
*/
static void
ttm_include(TTM* ttm, Frame* frame)  /* Include text of a file */
{
    char** path;
    FILE* finclude;
    char suffix[8192];
    char filename[8192];
    utf32* suffix32;
    Buffer* bb = ttm->result;
    int suffixlen;
    int count;

    suffix32 = frame->argv[1];
    suffixlen = strlen32(suffix32);
    if(suffixlen == 0)
        fail(ttm,EINCLUDE);
    if(suffix32[0] == '/' || suffix32[0] == '\\') {
        suffix32++;
        suffixlen--;
    }
    /* convert */
    count = toString8((char_t*)suffix,suffix32,suffixlen);
    if(count >= 8192) fail(ttm,EBUFFERSIZE);
    suffix[count] = NUL;
    /* access thru the -I list */
    for(path=includes;*path;) {
        strcpy(filename,*path);
        strcat(filename,"/");
        strcat(filename,suffix);
        finclude = fopen(filename,"r");
        if(finclude != NULL) break;
    }
    if(finclude == NULL)
        fail(ttm,EINCLUDE);
    readfile(ttm,finclude,bb);
}

/**
Helper functions for all the ttm commands
and subcommands
*/

/**
#<ttm;meta;newmetachars>
*/
static void
ttm_ttm_meta(TTM* ttm, Frame* frame)
{
    utf32* arg = frame->argv[2];
    if(strlen32(arg) != 5) fail(ttm,ETTMCMD);
    ttm->sharpc = arg[0];
    ttm->openc = arg[1];
    ttm->semic = arg[2];
    ttm->closec = arg[3];
    ttm->escapec = arg[4];
}

/**
#<ttm;info;name;...>
*/
static void
ttm_ttm_info_name(TTM* ttm, Frame* frame)
{
    Name* str;
    Buffer* result = ttm->result;
    char info[8192];
    utf32* q;
    utf32* p;
    utf32 c32;
    unsigned int namelen,count,i;

    setBufferLength(ttm,result,result->alloc-1);
    q = result->content;
    *q = NUL32;
    for(i=3;i<frame->argc;i++) {
        str = dictionaryLookup(ttm,frame->argv[i]);
        if(str == NULL) { /* not defined*/
            strcpy32(q,frame->argv[i]);
            q += strlen32(frame->argv[i]);
            count = toString32(q,"-,-,-",TOEOS);
            *q++ = '\n';
            continue;       
        }
        namelen = strlen32(str->entry.name);
        strcpy32(q,str->entry.name);
        q += namelen;
        if(str->builtin) {
            snprintf(info,sizeof(info),",%d,",str->minargs);
            count = toString32(q,info,TOEOS);
            q += count;
            if(str->maxargs == MAXARGS)
                *q++ = '*';
            else {
                snprintf(info,sizeof(info),"%d",str->maxargs);
                count = toString32(q,info,TOEOS);
                q += count;
            }
            *q++ = ',';
            *q++ = (str->novalue?'S':'V');
        } else {
            snprintf(info,sizeof(info),",0,%d,V",str->maxsegmark);
            count = toString32(q,info,TOEOS);
            q += count;
        }
        if(!str->builtin) {
            snprintf(info,sizeof(info)," residual=%u body=|",str->residual);
            count = toString32(q,info,TOEOS);
            q += count;
            /* Walk the body checking for segment and creation marks */
            p=str->body;
            while((c32=*p++)) {
                if(ismark(c32)) {
                    if(iscreate(c32))
                        strcpy(info,"^00");
                    else /* segmark */
                        snprintf(info,sizeof(info),"^%02d",(int)(c32 & 0xFF));
                    count = toString32(q,info,TOEOS);
                    q += count;
                } else
                    *q++ = c32;
            }
            *q++ = '|';
        }
        *q++ = '\n';
    }
    setBufferLength(ttm,result,(q - result->content));
#ifdef DEBUG
    fprintf(stderr,"info.name: ");
    dbgprint32(result->content,'"');
    fprintf(stderr,"\n");
    fflush(stderr);
#endif
}

/**
#<ttm;info;class;...>
*/
static void
ttm_ttm_info_class(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    Charclass* cl;
    utf32* q;
    utf32* p;
    utf32 c32;
    unsigned int i,len;
    Buffer* result = ttm->result;

    q = result->content;
    *q = NUL;
    setBufferLength(ttm,result,result->alloc-1); /* max space avail */
    for(i=3;i<frame->argc;i++) {
        cl = charclassLookup(ttm,frame->argv[i]);
        if(cl == NULL) fail(ttm,ENONAME);
        len = strlen32(cl->entry.name);
        strcpy32(q,cl->entry.name);
        q += len;
        *q++ = ' ';
        *q++ = LBRACKET;
        if(cl->negative) *q++ = '^';
        for(p=cl->characters;(c32=*p++);) {
            if(c32 == LBRACKET || c32 == RBRACKET)
                *q++ = '\\';
            *q++ = c32;
        }
        *q++ = '\n';
    }
    setBufferLength(ttm,result,(q - result->content));
#ifdef DEBUG
    fprintf(stderr,"info.class: ");
    dbgprint32(result->content,'"');
    fprintf(stderr,"\n");
    fflush(stderr);
#endif
}

/**
#<ttm;info;class;...>
*/

static void
ttm_ttm(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    /* Get the discriminate string */
    char discrim[(4*255)+1]; /* upper bound for 255 characters + nul term */
    int count;
    
    count = toString8(discrim,frame->argv[1],TOEOS);
    discrim[count] = NUL;

    if(frame->argc >= 3 && strcmp("meta",discrim)==0) {
        ttm_ttm_meta(ttm,frame);
    } else if(frame->argc >= 4 && strcmp("info",discrim)==0) {
        count = toString8(discrim,frame->argv[2],TOEOS);
        discrim[count] = NUL;
        if(strcmp("name",discrim)==0) {
            ttm_ttm_info_name(ttm,frame);
        } else if(strcmp("class",discrim)==0) {
            ttm_ttm_info_class(ttm,frame);
        } else
            fail(ttm,ETTMCMD);
    } else {
        fail(ttm,ETTMCMD);
    }

}

/**************************************************/

/**
 Builtin function table
*/

struct Builtin {
    char* name;
    unsigned int minargs;
    unsigned int maxargs;
    char* sv;
    TTMFCN fcn;
};

/* TODO: fix the minargs values */

/* Define a subset of the original TTM functions */

/* Define some temporary macros */
#define ARB MAXARGS

static struct Builtin builtin_orig[] = {
    /* Dictionary Operations */
    {"ap",2,2,"S",ttm_ap}, /* Append to a string */
    {"cf",2,2,"S",ttm_cf}, /* Copy a function */
    {"cr",2,2,"S",ttm_cr}, /* Mark for creation */
    {"ds",2,2,"S",ttm_ds}, /* Define string */
    {"es",1,ARB,"S",ttm_es}, /* Erase string */
    {"sc",2,63,"SV",ttm_sc}, /* Segment and count */
    {"ss",2,2,"S",ttm_ss}, /* Segment a string */
    /* Name Selection */
    {"cc",1,1,"SV",ttm_cc}, /* Call one character */
    {"cn",2,2,"SV",ttm_cn}, /* Call n characters */
    {"sn",2,2,"S",ttm_sn}, /* Skip n characters */ /*Batch*/
    {"cp",1,1,"SV",ttm_cp}, /* Call parameter */
    {"cs",1,1,"SV",ttm_cs}, /* Call segment */
    {"isc",4,4,"SV",ttm_isc}, /* Initial character scan */
    {"rrp",1,1,"S",ttm_rrp}, /* Reset residual pointer */
    {"scn",3,3,"SV",ttm_scn}, /* Character scan */
    /* Name Scanning Operations */
    {"gn",2,2,"V",ttm_gn}, /* Give n characters */
    {"zlc",1,1,"V",ttm_zlc}, /* Zero-level commas */
    {"zlcp",1,1,"V",ttm_zlcp}, /* Zero-level commas and parentheses */
    {"flip",1,1,"V",ttm_flip}, /* Flip a string */ /*Batch*/
    /* Character Class Operations */
    {"ccl",2,2,"SV",ttm_ccl}, /* Call class */
    {"dcl",2,2,"S",ttm_dcl}, /* Define a class */
    {"dncl",2,2,"S",ttm_dncl}, /* Define a negative class */
    {"ecl",1,ARB,"S",ttm_ecl}, /* Erase a class */
    {"scl",2,2,"S",ttm_scl}, /* Skip class */
    {"tcl",4,4,"V",ttm_tcl}, /* Test class */
    /* Arithmetic Operations */
    {"abs",1,1,"V",ttm_abs}, /* Obtain absolute value */
    {"ad",2,ARB,"V",ttm_ad}, /* Add */
    {"dv",2,2,"V",ttm_dv}, /* Divide and give quotient */
    {"dvr",2,2,"V",ttm_dvr}, /* Divide and give remainder */
    {"mu",2,ARB,"V",ttm_mu}, /* Multiply */
    {"su",2,2,"V",ttm_su}, /* Substract */
    /* Numeric Comparisons */
    {"eq",4,4,"V",ttm_eq}, /* Compare numeric equal */
    {"gt",4,4,"V",ttm_gt}, /* Compare numeric greater-than */
    {"lt",4,4,"V",ttm_lt}, /* Compare numeric less-than */
    /* Logical Comparisons */
    {"eq?",4,4,"V",ttm_eql}, /* ? Compare logical equal */
    {"gt?",4,4,"V",ttm_gtl}, /* ? Compare logical greater-than */
    {"lt?",4,4,"V",ttm_ltl}, /* ? Compare logical less-than */
    /* Peripheral Input/Output Operations */
    {"cm",1,1,"S",ttm_cm}, /*Change Meta Character*/
    {"ps",1,2,"S",ttm_ps}, /* Print a Name */
    {"psr",1,1,"SV",ttm_psr}, /* Print Name and Read */
#ifdef IMPLEMENTED
    {"rcd",2,2,"S",ttm_rcd}, /* Set to Read Prom Cards */
#endif
    {"rs",0,0,"V",ttm_rs}, /* Read a Name */
    /*Formated Output Operations*/
#ifdef IMPLEMENTED
    {"fm",1,ARB,"S",ttm_fm}, /* Format a Line or Card */
    {"tabs",1,8,"S",ttm_tabs}, /* Declare Tab Positions */
    {"scc",2,2,"S",ttm_scc}, /* Set Continuation Convention */
    {"icc",1,1,"S",ttm_icc}, /* Insert a Control Character */
    {"outb",0,3,"S",ttm_outb}, /* Output the Buffer */
#endif
    /* Library Operations */
#ifdef IMPLEMENTED
    {"store",2,2,"S",ttm_store}, /* Store a Program */
    {"delete",1,1,"S",ttm_delete}, /* Delete a Program */
    {"copy",1,1,"S",ttm_copy}, /* Copy a Program */
    {"show",0,1,"S",ttm_show}, /* Show Program Names */
    {"libs",2,2,"S",ttm_libs}, /* Declare standard qualifiers */ /*Batch*/
#endif
    {"names",0,1,"V",ttm_names}, /* Obtain Name Names */
    /* Utility Operations */
#ifdef IMPLEMENTED
    {"break",0,1,"S",ttm_break}, /* Program Break */
#endif
    {"exit",0,0,"S",ttm_exit}, /* Return from TTM */
    {"ndf",3,3,"V",ttm_ndf}, /* Determine if a Name is Defined */
    {"norm",1,1,"V",ttm_norm}, /* Obtain the Norm of a Name */
    {"time",0,0,"V",ttm_time}, /* Obtain time of day (modified) */
    {"xtime",0,0,"V",ttm_xtime}, /* Obtain execution time */ /*Batch*/
    {"tf",0,0,"S",ttm_tf}, /* Turn Trace Off */
    {"tn",0,0,"S",ttm_tn}, /* Turn Trace On */
    {"eos",3,3,"V",ttm_eos}, /* Test for end of string */ /*Batch*/

#ifdef IMPLEMENTED
/* Batch Functions */
    {"insw",2,2,"S",ttm_insw}, /* Control output of input monitor */ /*Batch*/
    {"ttmsw",2,2,"S",ttm_ttmsw}, /* Control handling of ttm programs */ /*Batch*/
    {"cd",0,0,"V",ttm_cd}, /* Input one card */ /*Batch*/
    {"cdsw",2,2,"S",ttm_cdsw}, /* Control cd input */ /*Batch*/
    {"for",0,0,"V",ttm_for}, /* Input next complete fortran statement */ /*Batch*/
    {"forsw",2,2,"S",ttm_forsw}, /* Control for input */ /*Batch*/
    {"pk",0,0,"V",ttm_pk}, /* Look ahead one card */ /*Batch*/
    {"pksw",2,2,"S",ttm_pksw}, /* Control pk input */ /*Batch*/
    {"ps",1,1,"S",ttm_ps}, /* Print a string */ /*Batch*/ /*Modified*/
    {"page",1,1,"S",ttm_page}, /* Specify page length */ /*Batch*/
    {"sp",1,1,"S",ttm_sp}, /* Space before printing */ /*Batch*/
    {"fm",0,ARB,"S",ttm_fm}, /* Format a line or card */ /*Batch*/
    {"tabs",1,10,"S",ttm_tabs}, /* Declare tab positions */ /*Batch*/ /*Modified*/
    {"scc",3,3,"S",ttm_scc}, /* Set continuation convention */ /*Batch*/
    {"fmsw",2,2,"S",ttm_fmsw}, /* Control fm output */ /*Batch*/
    {"time",0,0,"V",ttm_time}, /* Obtain time of day */ /*Batch*/ /*Modified*/
    {"des",1,1,"S",ttm_des}, /* Define error string */ /*Batch*/
#endif

    {NULL,0,0,NULL} /* terminator */
    };
    
/* Functions new to this implementation */
static struct Builtin builtin_new[] = {
    {"argv",1,1,"V",ttm_argv}, /* Get ith command line argument */
    {"classes",0,0,"V",ttm_classes}, /* Obtain character class Names */
    {"ctime",1,1,"V",ttm_ctime}, /* Convert time to printable string */
    {"include",1,1,"S",ttm_include}, /* Include text of a file */
    {"lf",0,ARB,"S",ttm_lf}, /* Lock functions */
    {"pf",0,1,"S",ttm_pf}, /* flush stderr and/or stdout */
    {"uf",0,ARB,"S",ttm_uf}, /* Unlock functions */
    {"ttm",1,ARB,"SV",ttm_ttm}, /* Misc. combined actions */
    {NULL,0,0,NULL} /* terminator */
};

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    Name* function;
    utf32 binname[8192];
    int count;

    count = toString32(binname,bin->name,strlen(bin->name));
    binname[count] = NUL32;
    /* Make sure we did not define builtin twice */
    function = dictionaryLookup(ttm,binname);
    assert(function == NULL);
    /* create a new function object */
    function = newName(ttm);
    function->builtin = 1;
    function->locked = 1;
    function->minargs = bin->minargs;
    function->maxargs = bin->maxargs;
    if(strcmp(bin->sv,"S")==0)
        function->novalue = 1;
    function->fcn = bin->fcn;
    /* Convert name to utf32 */
    function->entry.name = strdup32(binname);
    if(!dictionaryInsert(ttm,function))
	fatal(ttm,"Dictionary insertion failed");
}

static void
defineBuiltinFunctions(TTM* ttm)
{
    struct Builtin* bin;
    for(bin=builtin_orig;bin->name != NULL;bin++)
        defineBuiltinFunction1(ttm,bin);
    for(bin=builtin_new;bin->name != NULL;bin++)
        defineBuiltinFunction1(ttm,bin);
}

/**************************************************/
/* Predefined strings */
struct Predefined {
    char* name;
    char* body;
};

/* Predefined Strings */
static struct Predefined predefines[] = {
    {"comment","#<ds;comment;>"},
    {"def","#<ds;def;<##<ds;name;<text>>##<ss;name;subs>>>#<ss;def;name;subs;text>"},
    {NULL,NULL} /* terminator */
};

static void
predefineNames(TTM* ttm)
{
    struct Predefined* pre;
    utf32 name32[64];
    int count,bodylen;
    Name* fcn;
    int saveflags = ttm->flags;
    ttm->flags &= ~FLAG_TRACE;

    for(pre=predefines;pre->name != NULL;pre++) {
        bodylen = strlen(pre->body);
        resetBuffer(ttm,ttm->buffer);
        setBufferLength(ttm,ttm->buffer,bodylen); /* temp */
        count = toString32(ttm->buffer->content,pre->body,bodylen);     
        setBufferLength(ttm,ttm->buffer,count);
        scan(ttm);
        resetBuffer(ttm,ttm->buffer); /* throw away any result */
        /* Validate */
        if(strlen(pre->name) >= sizeof(name32)/sizeof(utf32))
            fatal(ttm,"Predefined Name name is too long");
        count = toString32(name32,pre->name,TOEOS);
        name32[count] = NUL32;
        fcn = dictionaryLookup(ttm,name32);
        if(fcn == NULL) fatal(ttm,"Could not define a predefined name");
        fcn->locked = 1; /* do not allow predefines to be deleted */
    }
    ttm->flags = saveflags;

}

/**************************************************/
/* Error reporting */

static void
fail(TTM* ttm, ERR eno)
{
    char msg[4096];
    snprintf(msg,sizeof(msg),"(%d) %s",eno,errstring(eno));
    fatal(ttm,msg);
}

static void
fatal(TTM* ttm, const char* msg)
{
    fprintf(stderr,"Fatal error: %s\n",msg);
    if(ttm != NULL) {
        /* Dump the frame stack */
        dumpstack(ttm);
        /* Dump passive and active strings*/
        fprintf(stderr,"context: ");
        /* Since passive is not normally null terminated, we need to fake it */
        {
            utf32 save = *ttm->buffer->passive;
            *ttm->buffer->passive = NUL32;
            dbgprint32(ttm->buffer->content,'|');
            *ttm->buffer->passive = save;
        }
        fprintf(stderr," ... ");
        dbgprint32(ttm->buffer->active,'|');
        fprintf(stderr,"\n");
    }
    fflush(stderr);
    exit(1);
}

static const char*
errstring(ERR err)
{
    const char* msg = NULL;
    switch(err) {
    case ENOERR: msg="No error"; break;
    case ENONAME: msg="Dictionary Name or Character Class Name Not Found"; break;
    case ENOPRIM: msg="Primitives Not Allowed"; break;
    case EFEWPARMS: msg="Too Few Parameters Given"; break;
    case EFORMAT: msg="Incorrect Format"; break;
    case EQUOTIENT: msg="Quotient Is Too Large"; break;
    case EDECIMAL: msg="Decimal Integer Required"; break;
    case EMANYDIGITS: msg="Too Many Digits"; break;
    case EMANYSEGMARKS: msg="Too Many Segment Marks"; break;
    case EMEMORY: msg="Dynamic Storage Overflow"; break;
    case EPARMROLL: msg="Parm Roll Overflow"; break;
    case EINPUTROLL: msg="Input Roll Overflow"; break;
#ifdef IMPLEMENTED
    case EDUPLIBNAME: msg="Name Already On Library"; break;
    case ELIBNAME: msg="Name Not On Library"; break;
    case ELIBSPACE: msg="No Space On Library"; break;
    case EINITIALS: msg="Initials Not Allowed"; break;
    case EATTACH: msg="Could Not Attach"; break;
#endif
    case EIO: msg="An I/O Error Occurred"; break;
#ifdef IMPLEMENTED
    case ETTM: msg="A TTM Processing Error Occurred"; break;
    case ESTORAGE: msg="Error In Storage Format"; break;
#endif
    case EPOSITIVE: msg="Only unsigned decimal integers"; break;
    /* messages new to this implementation */
    case ESTACKOVERFLOW: msg="Stack overflow"; break;
    case ESTACKUNDERFLOW: msg="Stack Underflow"; break;
    case EBUFFERSIZE: msg="Buffer overflow"; break;
    case EMANYINCLUDES: msg="Too many includes"; break;
    case EINCLUDE: msg="Cannot read Include file"; break;
    case ERANGE: msg="index out of legal range"; break;
    case EMANYPARMS: msg="Number of parameters greater than MAXARGS"; break;
    case EEOS: msg="Unexpected end of string"; break;
    case EASCII: msg="ASCII characters only"; break;
    case ECHAR8: msg="Illegal utf-8 character set"; break;
    case EUTF32: msg="Illegal utf-32 character set"; break;
    case ETTMCMD: msg="Illegal #<ttm> command"; break;
    case ETIME: msg="Gettimeofday() failed"; break;
    case EOTHER: msg="Unknown Error"; break;
    }
    return msg;
}

/**************************************************/
/* Debug utility functions */

#ifdef DEBUG

static void
dumpnames(TTM* ttm)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
	fprintf(stderr,"[%3d]",i);
        while(entry != NULL) {
	    fprintf(stderr," ");	
	    dbgprint32(entry->name,'|');
            entry = entry->next;
        }
	fprintf(stderr,"\n");	
    }
}

#endif

/**************************************************/
/* Utility functions */


/**
Convert a long long to a utf32 string
*/

static int
int2string(utf32* dst, long long n)
{
    char result[MAXINTCHARS+1];
    int count;

    snprintf(result,sizeof(result),"%lld",n);
    count = toString32(dst,result,TOEOS);
    return count;
}

/**
Convert a string to a signed Long
Use this regular expression:
      [ \t]*[+-]?[0-9]+[MmKk]?[ \t]*
    | [ \t]*0[Xx][0-9a-fA-F]+[ \t]*
In the first case, the value must be in the inclusive range:
    -9223372036854775807..9223372036854775807
Note that this excludes the smallest possible value:
    -9223372036854775808
so that we do can detect overflow.
In the second case, the value must be in the range:
    0..18446744073709551615
but the value will be treated as unsigned when returned; this
allows one to create any arbitrary 8 byte bit pattern.

Return ENOERR if conversion succeeded, err if failed.

*/

static ERR
toInt64(utf32* s, long long* lp)
{
    utf32* p = s;
    utf32 c;
    int negative = 1;
    /* skip white space */
    while((c=*p++)) {if(c != ' ' && c != '\t') break;}
    if(c == NUL) return EDECIMAL; /* nothing but white space or just a sign*/
    if(c == '-') {negative = -1; c=*p++;} else if(c == '+') {c=*p++;}
    if(c == NUL) return EDECIMAL; /* just a +|- */
    if(c == '0' && (*p == 'x' || *p == 'X')) { /* hex case */
        unsigned long ul = 0;
        int i;
        for(i=0;i<16;i++) {/* allow no more than 16 hex digits */
            c=*p++;
            if(!ishex(c)) break;
            ul = (ul<<8) | fromhex(c);
        }
        if(i==0)
            return EDECIMAL; /* it was just "0x" */
        if(i == 16) /* too big */
            return EMANYDIGITS;
        if(lp) {*lp = *(long long*)&ul;} /* return as signed value */
        return ENOERR;
    } else if(c >= '0' && c <= '9') { /* decimal */
        unsigned long long ul;
        ul = (c - '0');
        while((c=*p++)) {
            int digit;
            if(c < '0' || c > '9') break;
            digit = (c-'0');
            ul = (ul * 10) + digit;
            if((ul & 0x8000000000000000ULL) != 0) {
                /* Sign bit was set => overflow */
                return EMANYDIGITS;
            }
        }      
        if(lp) {
            /* convert back to signed */
            long long l;
            l = *(long long*)&ul;
            l *= negative;
            *lp = l;
        }
        return ENOERR;
    } /* else illegal */
    return EDECIMAL;
}

/* Given a char, return its escaped value.
Zero indicates it should be elided
*/
static utf32
convertEscapeChar(utf32 c)
{
    /* de-escape and store */
    switch (c) {
    case 'r': c = '\r'; break;
    case 'n': c = '\n'; break;
    case 't': c = '\t'; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    case '\n': c = 0; break; /* escaped eol is elided */
    default: break; /* no change */
    }
    return c;
}

static void
traceframe(TTM* ttm, Frame* frame, int traceargs)
{
    char tag[4];
    unsigned int i = 0;

    if(frame->argc == 0) {
	fprintf(stderr,"#<empty frame>\n");
	return;
    }
    tag[i++] = (char)ttm->sharpc;
    if(!frame->active)
        tag[i++] = (char)ttm->sharpc;
    tag[i++] = (char)ttm->openc;
    tag[i] = NUL;
    fprintf(stderr,"%s",tag);
    dbgprint32(frame->argv[0],NUL);
    if(traceargs) {
        for(i=1;i<frame->argc;i++) {
            fputc32(ttm->semic,stderr);
            dbgprint32(frame->argv[i],NUL);
        }
    }
    fputc32(ttm->closec,stderr);
    fflush(stderr);
}

static void
trace1(TTM* ttm, int depth, int entering, int tracing)
{
    Frame* frame;

    if(tracing && ttm->stacknext == 0) {
        fprintf(stderr,"trace: no frame to trace\n");
        return;
    }   

    frame = &ttm->stack[depth];
    fprintf(stderr,"[%02d] ",depth);
    if(tracing)
        fprintf(stderr,"%s: ",(entering?"begin":"end"));
    traceframe(ttm,frame,entering);
    /* Dump the contents of result if !entering */
    if(!entering) {
        fprintf(stderr," => ");
        dbgprint32(ttm->result->content,'"');
    } 
    fprintf(stderr,"\n");
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, int entering, int tracing)
{
    trace1(ttm, ttm->stacknext-1, entering, tracing);
}


/**************************************************/
/* Debug Support */
/**
Dump the stack
*/
static void
dumpstack(TTM* ttm)
{
    unsigned int i;
    for(i=1;i<=ttm->stacknext;i++) {
        Frame* frame;
        frame = &ttm->stack[ttm->stacknext-i];
        trace1(ttm,i,1,!TRACING);
    }
    fflush(stderr);
}

static void
dbgprint32c(utf32 c, char quote)
{
    if(ismark(c)) {
        char_t* p;
        char info[16+1];
        if(iscreate(c))
            strcpy(info,"^00");
        else /* segmark */
            snprintf(info,sizeof(info),"^%02d",(int)(c & 0xFF));
        for(p=info;*p;p++) fputc(*p,stderr);
    } else if(iscontrol(c)) {
        fputc32('\\',stderr);
        switch (c) {
        case '\r': fputc('r',stderr); break;
        case '\n': fputc('n',stderr); break;
        case '\t': fputc('t',stderr); break;
        case '\b': fputc('b',stderr); break;
        case '\f': fputc('f',stderr); break;
        default: {
            /* dump as a decimal character */
            char digits[4];
            snprintf(digits,sizeof(digits),"%d",(int)c);
            fprintf(stderr,"%s",digits);
            } break;
        }
    } else {
        if(c == quote) fputc('\\',stderr);
        fputc32(c,stderr);
    }
}

static void
dbgprint32(utf32* s, char quote)
{
    utf32 c;
    if(quote != NUL)
        fputc(quote,stderr);
    while((c=*s++)) {
	dbgprint32c(c,quote);
    }
    if(quote != NUL)
        fputc(quote,stderr);
    fflush(stderr);
}

/**************************************************/
/* Main() Support functions */

static int
getOptionNameLength(char** list)
{
    unsigned int i;
    char** p;
    for(i=0,p=list;*p;i++,p++);
    return i;
}

static int
pushOptionName(char* option, unsigned int max, char** list)
{
    unsigned int i;
    for(i=0;i<max;i++) {
        if(list[i] == NULL) {
            list[i] = (char*)malloc(strlen(option)+1);
            strcpy(list[i],option);
            return 1;
        }
    }
    return 0;
}

static void
initglobals()
{
    memset((void*)includes,0,sizeof(includes));
    memset((void*)eoptions,0,sizeof(eoptions));
    memset((void*)argoptions,0,sizeof(argoptions));
}

static void
usage(const char* msg)
{
    if(msg != NULL)
        fprintf(stderr,"%s\n",msg);
    fprintf(stderr,"ttm [-B integer] [-d string] [-D name=string] [-e string] [-i] [-I directory] [-o file] [-p programfile] [-r rsfile] [-V] [--] [arg...]\n");
    fprintf(stderr,"\tOptions may be repeated\n");
    if(msg != NULL) exit(1); else exit(0);
}

/* Convert option -Dx=y to option -e '#<ds;x;y>' */
static void
convertDtoE(const char* def)
{
    char* macro;
    char* sep;
    macro = (char*)malloc(strlen(def)+strlen("##<ds;;>")+1);
    if(macro == NULL) fail(NOTTM,EMEMORY);
    strcpy(macro,"##<ds;");
    strcat(macro,def);
    sep = strchr(macro,'=');
    if(sep != NULL)
        *sep++ = ';';
    else
        strcat(macro,";");
    strcat(macro,">");
    pushOptionName(macro,MAXEOPTIONS,eoptions);    
    free(macro);
}

static void
readinput(TTM* ttm, const char* filename,Buffer* bb)
{
    utf32* content = NULL;
    FILE* f = NULL;
    int isstdin = 0;
    unsigned int i;
    unsigned int buffersize = bb->alloc;

    if(strcmp(filename,"-") == 0) {
        /* Read from stdinput */
        isstdin = 1;
        f = stdin;      
    } else {
        f = fopen(filename,"r");
        if(f == NULL) {
            fprintf(stderr,"Cannot read file: %s\n",filename);
            exit(1);
        }
    }
    
    resetBuffer(ttm,bb);
    content = bb->content;

    /* Read char_t character by character until EOF */
    for(i=0;i<buffersize-1;i++) {
        utf32 c32;
        c32 = fgetc32(f);
        if(c32 == EOF) break;           
        if(c32 == ttm->escapec) {
            *content++ = c32;
            c32 = fgetc32(f);
        }
        *content++ = c32;
    }
    setBufferLength(ttm,bb,i);
    if(!isstdin) fclose(f);
}

/**
Read from input until the
read characters form a 
balanced string with respect
to the current open/close characters.
Read past the final balancing character
to the next end of line.
Return 0 if the read was terminated
by EOF. 1 otherwise.
*/

static int
readbalanced(TTM* ttm)
{
    Buffer* bb;
    utf32* content;
    utf32 c32;
    unsigned int depth,i;
    unsigned int buffersize;

    bb = ttm->buffer;
    resetBuffer(ttm,bb);
    buffersize = bb->alloc;
    content = bb->content;

    /* Read character by character until EOF; take escapes and open/close
       into account; keep outer <...> */
    for(depth=0,i=0;i<buffersize-1;i++) {
        c32 = fgetc32(stdin);
        if(c32 == EOF) break;
        if(c32 == ttm->escapec) {
            *content++ = c32;
            c32 = fgetc32(stdin);
        }
        *content++ = c32;
        if(c32 == ttm->openc) {
            depth++;
        } else if(c32 == ttm->closec) {
            depth--;
            if(depth == 0) break;
        }
    }
    /* skip to end of line */
    while(c32 != EOF && c32 != '\n') {c32 = fgetc32(stdin);}
    setBufferLength(ttm,bb,i);
    return (i == 0 && c32 == EOF ? 0 : 1);
}

static void
printbuffer(TTM* ttm)
{
    printstring(ttm,ttm->output,ttm->buffer->content,PRINTALL);
}

static int
readfile(TTM* ttm, FILE* file, Buffer* bb)
{
    long filesize;
    utf32* p;
    int count32;

    fseek(file,0,SEEK_SET); /* end of the file */
    filesize = ftell(file); /* get file length */
    rewind(file);
 
    setBufferLength(ttm,bb,filesize); /* temp */
    count32 = 0;
    p = bb->content;
    for(;;) {
        utf32 c = fgetc32(file);
        if(ferror(file)) fail(ttm,EIO);
        if(c == EOF) break;
        *p++ = c;
    }        
    *p = NUL32;
    count32 = (p - bb->content);
    setBufferLength(ttm,bb,count32);
    return count32;
}

static long
tagvalue(const char* p)
{
    unsigned int value;
    int c;
    if(p == NULL || p[0] == NUL)
        return -1;
    value = atol(p);
    c = p[strlen(p)-1];
    switch (c) {
    case 0: break;
    case 'm': case 'M': value *= (1<<20); break;
    case 'k': case 'K': value *= (1<<10); break;
    default: break;
    }
    return value;
}


/**************************************************/
/* Main() */

static char* options = "d:D:e:f:iI:o:r:VX:-";

int
main(int argc, char** argv)
{
    int i,exitcode;
    long buffersize = 0;
    long stacksize = 0;
    long execcount = 0;
    char* debugargs = strdup("");
    int interactive = 0;
    char* outputfilename = NULL;
    char* executefilename = NULL; /* This is the ttm file to execute */
    char* rsfilename = NULL; /* This is data for #<rs> */
    int isstdout = 1;
    FILE* outputfile = NULL;
    int isstdin = 1;
    FILE* rsfile = NULL;
    TTM* ttm = NULL;
    int c;
    char* p;

    if(argc == 1)
        usage(NULL);

    initglobals();

    /* Stash argv[0] */
    pushOptionName(argv[0],MAXARGS,argoptions);

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch(c) {
        case 'X':
            if(optarg == NULL) usage("Illegal -X tag");
            p = optarg;
            c = *p++;
            if(*p++ != '=') usage("Missing -X tag value");
            switch (c) {
            case 'b':
                if(buffersize == 0 && (buffersize = tagvalue(p)) < 0)
                    usage("Illegal buffersize");
                break;
            case 's':
                if(stacksize == 0 && (stacksize = tagvalue(p)) < 0)
                    usage("Illegal stacksize");
                break;
            case 'x':
                if(execcount == 0 && (execcount = tagvalue(p)) < 0)
                    usage("Illegal execcount");
                break;
            default: usage("Illegal -X option");
            }

        case 'd':
            if(debugargs == NULL)
                debugargs = strdup(optarg);
            break;
        case 'D':
            /* Convert the -D to a -e */
            convertDtoE(optarg);
            break;
        case 'e':
            pushOptionName(optarg,MAXEOPTIONS,eoptions);
            break;
        case 'f':
            if(executefilename == NULL)
                executefilename = strdup(optarg);
            break;
        case 'I':
            if(optarg[strlen(optarg)-1] == '/')
                optarg[strlen(optarg)-1] = NUL;
            pushOptionName(optarg,MAXINCLUDES,includes);
            break;
        case 'o':
            if(outputfilename == NULL)
                outputfilename = strdup(optarg);
            break;
        case 'r':
            interactive = 0;
            rsfilename = strdup(optarg);
            break;
        case 'V':
            printf("ttm version: %s\n",VERSION);
            exit(0);
            break;
        case '-':
            break;
        case '?':
        default:
            usage("Illegal option");
        }
    }

    /* Collect any args for #<arg> */
    if(optind < argc) {
        for(;optind < argc;optind++)
            pushOptionName(argv[optind],MAXARGS,argoptions);
    }

    /* Complain if interactive and output file name specified */
    if(outputfilename != NULL && interactive) {
        fprintf(stderr,"Interactive is illegal if output file specified\n");
        exit(1);
    }

    if(buffersize < MINBUFFERSIZE)
        buffersize = MINBUFFERSIZE;         
    if(stacksize < MINSTACKSIZE)
        stacksize = MINSTACKSIZE;           
    if(execcount < MINEXECCOUNT)
        execcount = MINEXECCOUNT;           

    if(outputfilename == NULL) {
        outputfile = stdout;
        isstdout = 1;
    } else {
        outputfile = fopen(outputfilename,"w");
        if(outputfile == NULL) {
            fprintf(stderr,"Output file is not writable: %s\n",outputfilename);
            exit(1);
        }           
        isstdout = 0;
    }

    if(rsfilename == NULL) {
        rsfile = stdin;
        isstdin = 1;
    } else {
        rsfile = fopen(rsfilename,"r");
        if(rsfile == NULL) {
            fprintf(stderr,"-r file is not readable: %s\n",rsfilename);
            exit(1);
        }           
        isstdin = 0;
    }

    /* Create the ttm state */
    ttm = newTTM(buffersize,stacksize,execcount);
    ttm->output = outputfile;
    ttm->isstdout = isstdout;
    ttm->rsinput = rsfile;
    ttm->isstdin = isstdin;    

    defineBuiltinFunctions(ttm);
    predefineNames(ttm);

    /* Execute the -e strings in turn */
    for(i=0;eoptions[i]!=NULL;i++) {
        char* eopt = eoptions[i];
        int count,elen = strlen(eopt);

        resetBuffer(ttm,ttm->buffer);
        setBufferLength(ttm,ttm->buffer,elen); /* temp */
        count = toString32(ttm->buffer->content,eopt,elen);     
        setBufferLength(ttm,ttm->buffer,count);
        scan(ttm);
        if(ttm->flags & FLAG_EXIT)
            goto done;
    }

    /* Now execute the executefile, if any */
    if(executefilename != NULL) {
        readinput(ttm,executefilename,ttm->buffer);
        scan(ttm);
        if(ttm->flags & FLAG_EXIT)
            goto done;
    }    

    /* If interactive, start read-eval loop */
    if(interactive) {
        for(;;) {
            if(!readbalanced(ttm)) break;
            scan(ttm);
            if(ttm->flags & FLAG_EXIT)
                goto done;
        }
    }

done:
    /* Dump any output left in the buffer */
    printbuffer(ttm);

    exitcode = ttm->exitcode;

    /* cleanup */
    if(!ttm->isstdout) fclose(ttm->output);
    if(!ttm->isstdin) fclose(ttm->rsinput);

    freeTTM(ttm);

    exit(exitcode);
}

/* Replacments for strcpy, strcmp ... */
static unsigned int
strlen32(utf32* s)
{
    unsigned int len = 0;
    while(*s++)
        len++;
    return len;
}

static void
strcpy32(utf32* dst, utf32* src)
{
    while((*dst++ = *src++));
}

static void
strncpy32(utf32* dst, utf32* src, unsigned int len)
{
    utf32 c32;
    for(;len>0 && (c32 = *src++);len--){*dst++ = c32;}
    if(c32 != NUL32) *dst = NUL32;
}

#if 0
static void
strcat32(utf32* dst, utf32* src)
{
    /* find end of dst */
    while(*dst++);
    while((*dst++ = *src++));
}
#endif

static utf32*
strdup32(utf32* src)
{
    unsigned int len;
    utf32* dup;

    len = strlen32(src);
    dup = (utf32*)malloc((1+len)*sizeof(utf32));
    if(dup != NULL) {
        utf32* dst = dup;
        while((*dst++ = *src++));
    }
    return dup;
}

static int
strcmp32(utf32* s1, utf32* s2)
{
    while(*s1 && *s2 && *s1 == *s2) {s1++; s2++;}
    /* Look at the last char to decide */
    if(*s1 == *s2) return 0; /* completely identical */
    if(*s1 == NUL32) /*=>*s2!=NUL32*/ return -1; /* s1 is shorter than s2 */
    if(*s2 == NUL32) /*=>*s1!=NUL32*/ return +1; /* s1 is longer than s2 */
    return (*s1 < *s2 ? -1 : +1);
}

static int
strncmp32(utf32* s1, utf32* s2, unsigned int len)
{
    utf32 c1=0;
    utf32 c2=0;
    if(len == 0) return 0; /* null strings always equal */
    for(;len>0;len--) {/* compare thru the last character*/
        c1 = *s1++;
        c2 = *s2++;
        if(c1 != c2) break; /* mismatch before length characters tested*/
    }
    /* Look at the last char to decide */
    if(c1 == c2) return 0; /* completely identical */
    if(c1 == NUL32) /*=>c2!=NUL32*/ return -1; /* s1 is shorter than s2 */
    if(c2 == NUL32) /*=>c1!=NUL32*/ return +1; /* s1 is longer than s2 */
    return (c1 < c2 ? -1 : +1);
}


static void
memcpy32(utf32* dst, utf32* src, int len)
{
    memcpy((void*)dst,(void*)src,len*sizeof(utf32));
}

/**************************************************/
/* Manage utf32 versus char_t */

/* Test equality of char* string to utf32 string */
static int
streq328(utf32* s32, char_t* s8)
{
    utf32 c32;
    int count;
        
    while(*s8 && *s32) {
        count = toChar32(&c32,s8);
        if(count < 0) fail(NOTTM,ECHAR8);
        if(c32 != *s32) break;
        s8 += count;
        s32++;
    }
    if(*s8 && *s32) return 1; /* equal */
    return 0;
}

/**
Convert a string of utf32 characters to a string of char_t
characters.  Stop when len src characters are processed or
end-of-string is encountered, whichever comes first.
WARNING: result is not nul-terminated.
Return: -1 if error, # of dst chars produced otherwise.
*/

static int
toString8(char_t* dst, utf32* src, int len)
{
    utf32* p32;
    char_t* q8;
    int i;

    p32 = src;
    q8 = dst;
    for(i=0;i<len;i++) {
        int count;
        utf32 c=*p32++;
        if(c == NUL32) break;
        count = toChar8(q8,c);
        if(count == 0) return -1;
        q8 += count;
    }
    return (q8 - dst);
}

/**
Convert a string of char_t characters to a string of utf32
characters.  Stop when len src bytes are processed or
end-of-string is encountered, whichever comes first.
WARNING: src length is in bytes, not characters.
WARNING: result is not nul-terminated.
Return: -1 if error, # of dst chars produced otherwise.
*/
static int
toString32(utf32* dst, char_t* src, int len)
{
    utf32* q32;
    char_t* p8;
    int i,count;

    p8 = src;
    q32 = dst;
    for(count=0,i=0;i<len;i+=count) {
        char_t c = *p8;
        if(c == NUL) break;
        count = toChar32(q32, p8);
        if(count == 0) return -1;
        q32++;
        p8 += count;
    }
    return (q32 - dst);
}

#ifndef ISO_8859
/**
Convert a utf32 value to a sequence of char_t bytes.
Return -1 if invalid; # chars in char_t* dst otherwise.
*/
static int
toChar8(char_t* dst, utf32 codepoint)
{
    char_t* p = dst;
    unsigned char uchar;
    /* assert |dst| >= MAXCHARSIZE+1 */
    if(codepoint <= 0x7F) {
        uchar = (unsigned char)codepoint;
       *p++ = (char)uchar;
    } else if(codepoint <= 0x7FF) {
       uchar = (unsigned char) 0xC0 | (codepoint >> 6);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else if(codepoint <= 0xFFFF) {
       uchar = (unsigned char) 0xE0 | (codepoint >> 12);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 6) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else if(codepoint <= 0x10FFFF) {
       uchar = (unsigned char) 0xF0 | (codepoint >> 18);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 12) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 6) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else
        return -1; /*invalid*/
    *p = NUL; /* make sure it is nul terminated. */
    return (p - dst);
}

/**
Convert a char_t multibyte code to utf32;
return -1 if invalid, #bytes processed from src otherwise.
*/
static int
toChar32(utf32* codepointp, char_t* src)
{
    /* assert |src| >= MAXCHARSIZE; not necessarily null terminated */
    char_t* p;
    utf32 codepoint;
    int charsize;
    unsigned int c0;
    static int mask[5] = {0x00,0x7F,0x1F,0x0F,0x07};
    unsigned char bytes[4];
    int i;

    p = src;
    c0 = (unsigned int)*p;
    charsize = utf8count(c0);
    if(charsize == 0) return -1; /* invalid */
    /* Process the 1st char in the char_t codepoint */
    bytes[0] = (c0 & mask[charsize]);
    for(i=1;i<charsize;i++) {
        unsigned char c;
        p++;
        c = *p;
        bytes[i] = (c & 0x3F);
    }
    codepoint = (bytes[0]);
    for(i=1;i<charsize;i++) {
        unsigned char c = bytes[i];
        codepoint <<= 6;
        codepoint |= (c & 0x3F);
    }
    if(codepointp) *codepointp = codepoint;
    return 1;    
}

/* Assumes ismultibyte(c) is true */
static int
utf8count(unsigned int c)
{
    if(((c) & 0x80) == 0x00) return 1; /* us-ascii */
    if(((c) & 0xE0) == 0xC0) return 2;
    if(((c) & 0xF0) == 0xE0) return 3;
    if(((c) & 0xF8) == 0xF0) return 4;
    return 0;
}

#endif /*!ISO_8859*/

#ifdef ISO_8859
static int
toChar8(char_t* dst, utf32 codepoint)
{
    /* Output codepoint as up to 3 non-0 ISO 88659 characters */
    char_t* q = dst;
    int shift = 16;
    char iso[3];
    iso[2] = (codepoint >> shift) & 0xFF; shift -= 8;
    iso[1] = (codepoint >> shift) & 0xFF; shift -= 8;
    iso[0] = (codepoint >> shift) & 0xFF; shift -= 8;
    if(iso[2] != 0) *q++ = iso[2];
    if(iso[1] != 0) *q++ = iso[1];
    if(iso[0] != 0) *q++ = iso[0];
    return (q - dst);
}

static int
toChar32(utf32* codepointp, char_t* src)
{
    *codepointp = (utf32)(*src);
    return 1;
}
#endif

/**************************************************/
/* Character Input/Output */ 

#ifndef ISO_8859
/* Unless you know that you are outputing ASCII,
   all output should go thru this procedure.
*/
static void
fputc32(utf32 c32, FILE* f)
{
    char_t c8[MAXCHARSIZE+1];
    int count,i;
    if((count = toChar8(c8,c32)) < 0) fail(NOTTM,EUTF32);
    for(i=0;i<count;i++) {fputc(c8[i],f);}
}

/* All reading of characters should go thru this procedure. */
static utf32
fgetc32(FILE* f)
{
    int c;
    utf32 c32;
        
    c = fgetc(f);
    if(c == EOF) return EOF;
    if(ismultibyte(c)) {
        char_t c8[MAXCHARSIZE+1];
        int count,i;
        count = utf8count(c);
        i=0; c8[i++] = (char_t)c;
        while(--count > 0) {
            c=fgetc(f);
            if(c == EOF) fail(NOTTM,EEOS);
            c8[i++] = (char_t)c;
        }
        c8[i] = NUL;
        count = toChar32(&c32,c8);
        if(count < 0) fail(NOTTM,ECHAR8);
    } else
        c32 = c;
    return c32;
}
#endif /*!ISO_8859*/

#ifdef ISO_8859
static void
fputc32(utf32 c32, FILE* f)
{
    char_t c8[MAXCHARSIZE+1];
    int count,i;
    count = toChar8(c8,c32);
    for(i=0;i<count;i++) fputc(c8[i],f);
}

/* All reading of characters should go thru this procedure. */
static utf32
fgetc32(FILE* f)
{
    int c = fgetc(f);
    return c;
}

#endif
/**************************************************/
/**
Implement functions that deal with Linux versus Windows
differences.
*/
/**************************************************/

#ifdef MSWINDOWS

static long long
getRunTime(void)
{
    long long runtime;
    FILETIME ftCreation,ftExit,ftKernel,ftUser;

    GetProcessTimes(GetCurrentProcess(),
                    &ftCreation, &ftExit, &ftKernel, &ftUser);

    runtime = ftUser.dwHighDateTime;
    runtime = runtime << 32;
    runtime = runtime | ftUser.dwLowDateTime;
    /* Divide by 10000 to get milliseconds from 100 nanosecond ticks */
    runtime /= 10000;
    return runtime;
}
 
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
 
static int tzflag = 0;

static int
timeofday(struct timeval *tv)
{
    /* Define a structure to receive the current Windows filetime */
    FILETIME ft;
 
    /* Initialize the present time to 0 and the timezone to UTC */
    unsigned long long tmpres = 0;
 
    if(NULL != tv) {
        GetSystemTimeAsFileTime(&ft);

        /*
        The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
        intervals since Jan 1, 1601 in a structure. Copy the high bits to 
        the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
        */
        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;
 
        /* Convert to microseconds by dividing by 10 */
        tmpres /= 10;
 
        /* The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
           in seconds from Jan 1 1601.*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
 
        /* Finally change microseconds to seconds and place in the seconds value.
           The modulus picks up the microseconds. */
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }
    return 0;
}

#else /*!MSWINDOWS */

static long frequency = 0;

static long long
getRunTime(void)
{
    long long runtime;
    struct tms timers;
    clock_t tic;

    if(frequency == 0) 
        frequency = sysconf(_SC_CLK_TCK);

    tic=times(&timers);
    runtime = timers.tms_utime; /* in clock ticks */
    runtime = ((runtime * 1000) / frequency) ; /* runtime in milliseconds */    
    return runtime;
}


static int
timeofday(struct timeval *tv)
{
    return gettimeofday(tv,NULL);
}

#endif /*!MSWINDOWS */


/**************************************************/
/* Getopt */

#ifdef MSWINDOWS
/**
 XGetopt.h  Version 1.2

 Author:  Hans Dietrich
          hdietrich2@hotmail.com

 This software is released into the public domain.
 You are free to use it in any way you like.

 This software is provided "as is" with no expressed
 or implied warranty.  I accept no liability for any
 damage or loss of business that this software may cause.
*/

static int
getopt(int argc, char **argv, char *optstring)
{
    static char *next = NULL;
    char c;
    char *cp = malloc(sizeof(char)*1024);

    if(optind == 0)
        next = NULL;
    optarg = NULL;
    if(next == NULL || *next == ('\0')) {
        if(optind == 0)
            optind++;
        if(optind >= argc
            || argv[optind][0] != ('-')
            || argv[optind][1] == ('\0')) {
            optarg = NULL;
            if(optind < argc)
                optarg = argv[optind];
            return EOF;
        }
        if(strcmp(argv[optind], "--") == 0) {
            optind++;
            optarg = NULL;
            if(optind < argc)
                optarg = argv[optind];
            return EOF;
        }
        next = argv[optind];
        next++;     /* skip past - */
        optind++;
    }
    c = *next++;
    cp = strchr(optstring, c);
    if(cp == NULL || c == (':'))
        return ('?');
    cp++;
    if(*cp == (':')) {
        if(*next != ('\0')) {
            optarg = next;
            next = NULL;
        } else if(optind < argc) {
            optarg = argv[optind];
            optind++;
        } else {
            return ('?');
        }
    }
    return c;
}
#endif /*MSWINDOWS*/


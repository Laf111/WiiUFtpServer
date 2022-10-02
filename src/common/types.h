#ifndef __GCTYPES_H__
#define __GCTYPES_H__

/*! \file gctypes.h 
\brief Data type definitions
*/ 

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*+----------------------------------------------------------------------------------------------+*/
// alias type typedefs
#define FIXED int32_t                    ///< Alias type for sfp32
/*+----------------------------------------------------------------------------------------------+*/
#ifndef NULL
#define NULL    0                    ///< Pointer to 0
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN  3412
#endif /* LITTLE_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BIG_ENDIAN
#define BIG_ENDIAN     1234
#endif /* BIGE_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BYTE_ORDER
#define BYTE_ORDER     BIG_ENDIAN
#endif /* BYTE_ORDER */
/*+----------------------------------------------------------------------------------------------+*/


//!    argv structure
/*!    \struct __argv
    structure used to set up argc/argv
*/
struct __argv {
    int argvMagic;        //!< argv magic number, set to 0x5f617267 ('_arg') if valid 
    char *commandLine;    //!< base address of command line, set of null terminated strings
    int length;//!< total length of command line
    int argc;
    char **argv;
    char **endARGV;
};

//!    Default location for the system argv structure.
extern struct __argv *__system_argv;

// argv struct magic number
#define ARGV_MAGIC 0x5f617267

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif    /* TYPES_H */


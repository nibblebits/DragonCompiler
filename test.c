#define __STD_TYPE typedef
# define __SWORD_TYPE		int
#define __SSIZE_T_TYPE		__SWORD_TYPE

typedef __SSIZE_T_TYPE __ssize_t; /* Type of a byte count, or error.  */

__ssize_t abc;

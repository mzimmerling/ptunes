/* Kernel/i386_linux/config.h.  Generated from config.h.in by configure.  */
/* BEGIN LICENSE BLOCK
 * Version: CMPL 1.1
 *
 * The contents of this file are subject to the Cisco-style Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file except
 * in compliance with the License.  You may obtain a copy of the License
 * at www.eclipse-clp.org/license.
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License. 
 * 
 * The Original Code is  The ECLiPSe Constraint Logic Programming System. 
 * The Initial Developer of the Original Code is  Cisco Systems, Inc. 
 * Portions created by the Initial Developer are
 * Copyright (C) 1994-2006 Cisco Systems, Inc.  All Rights Reserved.
 * 
 * Contributor(s): 
 * 
 * END LICENSE BLOCK */

/*
 * The template header file. We cannot use autoheader because fgrep
 * complains about too many symbols in configure.in
 */


/* This ECLiPSe version */
#define PACKAGE_VERSION "6.0"

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if atof() might not return negative zeros correctly  */
#define ATOF_NEGZERO_BUG 1

/* Define if type char is unsigned and you are not using gcc.  */
/* #undef __CHAR_UNSIGNED__ */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to empty if the keyword does not work.  */
/* #undef volatile */

/* Define to empty if the keyword does not work.  */
/* #undef inline */

/* ignored if not available */
/* #undef finite */

/* Define if system calls automatically restart after interruption
   by a signal.  */
/* #undef HAVE_RESTARTABLE_SYSCALLS */

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

/* Define if you have vfork.h.  */
/* #undef HAVE_VFORK_H */

/* Type sizes.  */
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_CHAR_P 4
#define SIZEOF_LONG_P 4

/* Define if no (void *) type.  */
/* #undef HAVE_NO_VOID_PTR */

/* Define if we have the long long int type.  */
#define HAVE_LONG_LONG 1
/* #undef HAVE___INT64 */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if sprintf() returns the number of characters printed. */
#define SPRINTF_RETURNS_LENGTH 1

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define on System V Release 4.  */
/* #undef SVR4 */

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Check where R_OK etc. are defined, unistd.h or sys/file.h */
#define ACCESS_IN_UNISTD 1

/* On AIX/rs6000 define _BSD to obtain BSD-like features */
/* #undef _BSD */

/* Check if times(3) returns 0 or elapsed time */
/* #undef BSD_TIMES */

/* The right spelling for etext */
#define ETEXT undef

/* Check if the signal action is reset to SIG_DFL before calling the handler */
/* #undef HANDLER_RESET */

/* Look for the alarm function */
#define HAVE_ALARM 1

/* Check if cputime can be limited */
#define HAVE_CPU_LIMIT 1

/* Check for the finite function */
#define HAVE_FINITE 1

/* Check for the isinf and isnan functions */
#define HAVE_ISINF 1
#define HAVE_ISNAN 1

/* Look for the pipes */
#define HAVE_PIPE 1

/* Check if it is possible to push back characters with TIOCSTI */
/* #undef HAVE_PUSHBACK */

/* Look for the putenv function */
#define HAVE_PUTENV 1

/* Look for the rename function */
#define HAVE_RENAME 1

/* Look for the select function */
#define HAVE_SELECT 1

/* Look for the setitimer function */
#define HAVE_SETITIMER 1

/* Look for the sleep function */
#define HAVE_SLEEP 1

/* Look for the strerror function */
#define HAVE_STRERROR 1

/* Look for the localtime_r function */
#define HAVE_LOCALTIME_R 1

/* Check for vsprintf nad if it is there, assume varargs is available */
#define HAVE_VARARGS 1
#define HAVE_VSPRINTF 1
#define HAVE_VSNPRINTF 1

/* Check for ways to control floating point rounding */
#define HAVE_FPU_CONTROL_H 1
/* #undef HAVE_FPSETROUND */
/* #undef HAVE_IEEE_FLAGS */

/* Specify the host architecture as returned by ARCH */
#define HOSTARCH "i386_linux"

/* Look for the PATH_MAX definition in <limits.h> */
/* #undef PATH_IN_LIMITS */

/* Are the tests running on a remote machine? */
/* #undef REMOTE_TESTS */

/* Define if sigio is available */
#define SIGIO_FASYNC 1
/* #undef SIGIO_SETSIG */

/* Check if sys/socket.h exists and if so, assume that sockets are there */
#define SOCKETS 1

/* Check if sys/un.h exists and if so, assume the AF_UNIX address family
 * works
 */
#define HAVE_AF_UNIX 1

/* Check how to obtain page size from sysconf() */
/* #undef SYSCONF_PAGE */

/* Check for System V-style termio */
/* #undef TERMIO_SYS_V_STYLE */

/* Define if you have dirent.h.  */
#define HAVE_DIRENT_H 1

/* Define if you have fstat.  */
#define HAVE_FSTAT 1

/* Define if you have getcwd.  */
#define HAVE_GETCWD 1

/* Define if you have gethostid.  */
#define HAVE_GETHOSTID 1

/* Define if you have gethostname.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have gethrtime (high resolution timer).  */
/* #undef HAVE_GETHRTIME */

/* Define if you don't have dirent.h, but have ndir.h.  */
/* #undef HAVE_NDIR_H */

/* Define if you have netdb.h.  */
#define HAVE_NETDB_H 1

/* Define if you have sgi hardware high resolution timer.  */
/* #undef HAVE_SGIHRCLOCK */

/* Define if sgi hardware high resolution timer is 64 bits  */
/* #undef HAVE_SGI64BITCLOCK */

/* Define if you have getwd.  */
#define HAVE_GETWD 1

/* Define if you have bcopy.  */
#define HAVE_BCOPY 1

/* Define if you have memcpy.  */
#define HAVE_MEMCPY 1

/* Define if you have memmove.  */
#define HAVE_MEMMOVE 1

/* Define if you have a working mmap.  */
#define HAVE_MMAP 1

/* Define if you have sys/mman.h.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have random.  */
#define HAVE_RANDOM 1

/* Define if you have rint.  */
#define HAVE_RINT 1

/* Define if you have trunc.  */
/* #undef HAVE_TRUNC */

/* Define if you have setsid.  */
#define HAVE_SETSID 1

/* Define if you have sigaction.  */
#define HAVE_SIGACTION 1

/* Define if you have sigaltstack.  */
#define HAVE_SIGALTSTACK 1

/* Define if you have siginterrupt.  */
#define HAVE_SIGINTERRUPT 1

/* Define if you have sigprocmask.  */
#define HAVE_SIGPROCMASK 1

/* Define if you have sigstack.  */
#define HAVE_SIGSTACK 1

/* Define if you have sigvec.  */
#define HAVE_SIGVEC 1

/* Define if you have sincos(). Used in ria.  */
#define HAVE_SINCOS 1

/* Define if you don't have dirent.h, but have sys/dir.h.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you don't have dirent.h, but have sys/ndir.h.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have sysconf.  */
#define HAVE_SYSCONF 1

/* Define if you have sysinfo.  */
#define HAVE_SYSINFO 1

/* Define if you have sys/systeminfo.h  */
/* #undef HAVE_SYS_SYSTEMINFO_H */

/* Define if ioctl undestands SIOCGIFHWADDR */
#define HAVE_SIOCGIFHWADDR 1

/* Define if you have tcgetattr.  */
#define HAVE_TCGETATTR 1

/* Define if you have times.  */
#define HAVE_TIMES 1

/* Define if we have <ucontext.h> and can acces registers from signal context.  */
#define HAVE_UCONTEXTGREGS 1

/* Define if you have the <ctype.h> header file.  */
#define HAVE_CTYPE_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/utsname.h> header file.  */
#define HAVE_SYS_UTSNAME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the bsd library (-lbsd).  */
/* #undef HAVE_LIBBSD */

/* Define if you have the m library (-lm).  */
/* #undef HAVE_LIBM */

/* Define is getpagesize() exists */
#define HAVE_GETPAGESIZE 1

/* On some machines, there is not enough space to run bigclause etc. */
/* #undef SMALL_SPACE */

/* On some machines struct nlist uses n_un.n_name, on others it does not */
#define N_NAME n_un.n_name

/* Do we have the GMP multi precision library */
#define HAVE_LIBGMP 1

/* Do we have the FLEXlm licence manager */
/* #undef HAVE_FLEXLM */

/* What suffix is used for dynamic loading */
/* Define if ceil() doesn't correctly return -0.0 for argument >-1.0 <-0.0 */
/* #undef HAVE_CEIL_NEGZERO_BUG */

/*#define O o */
#define OBJECT_SUFFIX_STRING "so"
/* #undef AIX */
#define HAVE_MPROTECT 1
#define HAVE_GETRUSAGE 1
#define UPTIME uptime
#define HAVE_DLOPEN 1
/* #undef HAVE_MACH_O_DYLD_H */
/* #undef HAVE_NLIST */

#define HAVE_TCL 1
#define HAVE_TK 1
/* #undef SBRK_UNDEF */
/* #undef GETHOSTID_UNDEF */
#define STRTOL_UNDEF 1
/* Define if we cannot compute the stack beginning from the sbrk(0) address */
/* #undef STACK_BASE */

#define KB	1024
#define MB	KB*KB

#define VIRTUAL_HEAP_DEFAULT 32*MB
#define VIRTUAL_SHARED_DEFAULT 64*MB
#define VIRTUAL_STACK_DEFAULT 128*MB
#define SHARED_MEM_OFFSET_HEAP 16*MB
#define MEMCPY_STRING 1
/* #undef MEMCPY_MEMORY */
/* #undef HAVE_SYS_SELECT_H */
/* #undef HAVE_VALUES_H */
/* #undef HAVE_INFINITY */

/* on m88k, gettimeofday accepts only one argument */
#ifdef GETTIME1
#define gettimeofday(tp,tzp)	gettimeofday(tp)
#endif

/* define if compiling for Windows NT -- specifies NT version */
/* #undef _WIN32_WINNT */

/* to deal with OS's that define it but don't implement it */
/* #undef HAVE_MAP_NORESERVE */

#define	NREGARG		0
#define	NREGTMP		0


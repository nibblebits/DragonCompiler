/* Copyright (C) 1991-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef __struct_FILE_defined
#define __struct_FILE_defined 1

/* Caution: The contents of this file are not part of the official
   stdio.h API.  However, much of it is part of the official *binary*
   interface, and therefore cannot be changed.  */

#if defined _IO_USE_OLD_IO_FILE && !defined _LIBC
# error "_IO_USE_OLD_IO_FILE should only be defined when building libc itself"
#endif

#if defined _IO_lock_t_defined && !defined _LIBC
# error "_IO_lock_t_defined should only be defined when building libc itself"
#endif

#include <bits/types.h>


typedef union
{
  
} _IO_iconv_t;
/* _IO_seekoff modes */
#define _IOS_INPUT        1
#define _IOS_OUTPUT        2
/* Magic number and bits for the _flags field.  The magic number is
   mostly vestigial, but preserved for compatibility.  It occupies the
   high 16 bits of _flags; the low 16 bits are actual flag bits.  */
#define _IO_MAGIC         0xFBAD0000 /* Magic number */
#define _IO_MAGIC_MASK    0xFFFF0000
#define _IO_USER_BUF          0x0001 /* Don't deallocate buffer on close. */
#define _IO_UNBUFFERED        0x0002
#define _IO_NO_READS          0x0004 /* Reading not allowed.  */
#define _IO_NO_WRITES         0x0008 /* Writing not allowed.  */
#define _IO_EOF_SEEN          0x0010
#define _IO_ERR_SEEN          0x0020
#define _IO_DELETE_DONT_CLOSE 0x0040 /* Don't call close(_fileno) on close.  */
#define _IO_LINKED            0x0080 /* In the list of all open files.  */
#define _IO_IN_BACKUP         0x0100
#define _IO_LINE_BUF          0x0200
#define _IO_TIED_PUT_GET      0x0400 /* Put and get pointer move in unison.  */
#define _IO_CURRENTLY_PUTTING 0x0800
#define _IO_IS_APPENDING      0x1000
#define _IO_IS_FILEBUF        0x2000
                           /* 0x4000  No longer used, reserved for compat.  */
#define _IO_USER_LOCK         0x8000
/* Bits for the _flags2 field.  */
#define _IO_FLAGS2_MMAP 1
#define _IO_FLAGS2_NOTCANCEL 2
#define _IO_FLAGS2_USER_WBUF 8
#define _IO_FLAGS2_NOCLOSE 32
#define _IO_FLAGS2_CLOEXEC 64
#define _IO_FLAGS2_NEED_LOCK 128
/* _IO_pos_BAD is an off64_t value indicating error, unknown, or EOF.  */
#define _IO_pos_BAD ((off64_t) -1)
/* _IO_pos_adjust adjusts an off64_t by some number of bytes.  */
#define _IO_pos_adjust(pos, delta) ((pos) += (delta))
/* _IO_pos_0 is an off64_t value indicating beginning of file.  */
#define _IO_pos_0 ((off64_t) 0)

struct  _IO_codecvt {
     char unused;
};

struct _IO_jump_t;
/* A streammarker remembers a position in a buffer. */
struct _IO_marker {
  struct _IO_marker *_next;
  FILE *_sbuf;
  /* If _pos >= 0
 it points to _buf->Gbase()+_pos. FIXME comment */
  /* if _pos < 0, it points to _buf->eBptr()+_pos. FIXME comment */
  int _pos;
};

struct _IO_FILE;
struct _IO_marker;
struct _IO_codecvt;

#include <bits/types/__mbstate_t.h>

struct _IO_wide_data
{
  wchar_t *_IO_read_ptr;    /* Current read pointer */
  wchar_t *_IO_read_end;    /* End of get area. */
  wchar_t *_IO_read_base;   /* Start of putback+get area. */
  wchar_t *_IO_write_base;  /* Start of put area. */
  wchar_t *_IO_write_ptr;   /* Current put pointer. */
  wchar_t *_IO_write_end;   /* End of put area. */
  wchar_t *_IO_buf_base;    /* Start of reserve area. */
  wchar_t *_IO_buf_end;     /* End of reserve area. */
  /* The following fields are used to support backing up and undo. */
  wchar_t *_IO_save_base;   /* Pointer to start of non-current get area. */
  wchar_t *_IO_backup_base; /* Pointer to first valid character of
                   backup area */
  wchar_t *_IO_save_end;    /* Pointer to end of non-current get area. */
 
  __mbstate_t _IO_state;
  __mbstate_t _IO_last_state;
  struct _IO_codecvt _codecvt;
 
  wchar_t _shortbuf[1];
 
  const struct _IO_jump_t *_wide_vtable;
};

/* The tag name of this struct is _IO_FILE to preserve historic
   C++ mangled names for functions taking FILE* arguments.
   That name should not be used in new code.  */
struct _IO_FILE
{
  int _flags;		/* High-order word is _IO_MAGIC; rest is flags. */

  /* The following pointers correspond to the C++ streambuf protocol. */
  char *_IO_read_ptr;	/* Current read pointer */
  char *_IO_read_end;	/* End of get area. */
  char *_IO_read_base;	/* Start of putback+get area. */
  char *_IO_write_base;	/* Start of put area. */
  char *_IO_write_ptr;	/* Current put pointer. */
  char *_IO_write_end;	/* End of put area. */
  char *_IO_buf_base;	/* Start of reserve area. */
  char *_IO_buf_end;	/* End of reserve area. */

  /* The following fields are used to support backing up and undo. */
  char *_IO_save_base; /* Pointer to start of non-current get area. */
  char *_IO_backup_base;  /* Pointer to first valid character of backup area */
  char *_IO_save_end; /* Pointer to end of non-current get area. */

  struct _IO_marker *_markers;

  struct _IO_FILE *_chain;

  int _fileno;
  int _flags2;
  __off_t _old_offset; /* This used to be _offset but it's too small.  */

  /* 1+column number of pbase(); 0 is unknown. */
  unsigned short _cur_column;
  signed char _vtable_offset;
  char _shortbuf[1];

  _IO_lock_t *_lock;
#ifdef _IO_USE_OLD_IO_FILE
};

struct _IO_FILE_complete
{
  struct _IO_FILE _file;
#endif
  __off64_t _offset;
  /* Wide character stream stuff.  */
  struct _IO_codecvt *_codecvt;
  struct _IO_wide_data *_wide_data;
  struct _IO_FILE *_freeres_list;
  void *_freeres_buf;
  size_t __pad5;
  int _mode;
  /* Make sure we don't get into trouble again.  */
  char _unused2[15 * sizeof (int) - 4 * sizeof (void *) - sizeof (size_t)];
};

/* These macros are used by bits/stdio.h and internal headers.  */
#define __getc_unlocked_body(_fp)					\
  (__glibc_unlikely ((_fp)->_IO_read_ptr >= (_fp)->_IO_read_end)	\
   ? __uflow (_fp) : *(unsigned char *) (_fp)->_IO_read_ptr++)

#define __putc_unlocked_body(_ch, _fp)					\
  (__glibc_unlikely ((_fp)->_IO_write_ptr >= (_fp)->_IO_write_end)	\
   ? __overflow (_fp, (unsigned char) (_ch))				\
   : (unsigned char) (*(_fp)->_IO_write_ptr++ = (_ch)))

#define _IO_EOF_SEEN 0x0010
#define __feof_unlocked_body(_fp) (((_fp)->_flags & _IO_EOF_SEEN) != 0)

#define _IO_ERR_SEEN 0x0020
#define __ferror_unlocked_body(_fp) (((_fp)->_flags & _IO_ERR_SEEN) != 0)

#define _IO_USER_LOCK 0x8000
/* Many more flag bits are defined internally.  */

#endif

/* plist.c -- plist module.
   Copyright (C) 2003, 2004
     National Institute of Advanced Industrial Science and Technology (AIST)
     Registration Number H15PRO112

   This file is part of the m17n library.

   The m17n library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   The m17n library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the m17n library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.  */

/***en
    @addtogroup m17nPlist

    @brief Property List objects and API for them.

    A @e property @e list (or @e plist for short) is a list of zero or
    more properties.  A property consists of a @e key and a @e value,
    where key is a symbol and value is anything that can be cast to
    <tt>(void *)</tt>.

    If the key of a property is a @e managing @e key, its @e value is
    a @e managed @e object.  A property list itself is a managed
    objects.  */

/*=*/

#if !defined (FOR_DOXYGEN) || defined (DOXYGEN_INTERNAL_MODULE)
/*** @addtogroup m17nInternal
     @{ */

#include <stdio.h>
#include <string.h>

#include "m17n.h"
#include "m17n-misc.h"
#include "internal.h"
#include "character.h"
#include "mtext.h"
#include "symbol.h"
#include "plist.h"

static M17NObjectArray plist_table;

/** Set PLIST to a newly allocated plist object.  */

#define MPLIST_NEW(plist)				\
  do {							\
    M17N_OBJECT (plist, free_plist, MERROR_PLIST);	\
    M17N_OBJECT_REGISTER (plist_table, plist);		\
  } while (0)


/** Set the element of PLIST to KEY and VAL.  If PLIST is an anchor,
    append a new anchor.  */

#define MPLIST_SET(plist, key, val)	\
  do {					\
    MPLIST_KEY (plist) = (key);		\
    MPLIST_VAL (plist) = (val);		\
    if (! (plist)->next)		\
      MPLIST_NEW ((plist)->next);	\
  } while (0)


/** Set the element of PLIST to KEY and VAL.  PLIST must be an anchor.
    Append a new anchor and set PLIST to that anchor.  */

#define MPLIST_SET_ADVANCE(plist, key, val)	\
  do {						\
    MPLIST_KEY (plist) = (key);			\
    MPLIST_VAL (plist) = (val);			\
    MPLIST_NEW ((plist)->next);			\
    plist = (plist)->next;			\
  } while (0)


static void
free_plist (void *object)
{
  MPlist *plist = (MPlist *) object;

  do {
    MPlist *next = plist->next;

    if (MPLIST_KEY (plist) != Mnil && MPLIST_KEY (plist)->managing_key)
      M17N_OBJECT_UNREF (MPLIST_VAL (plist));
    M17N_OBJECT_UNREGISTER (plist_table, plist);
    free (plist);
    plist = next;
  } while (plist && plist->control.ref_count == 1);
  M17N_OBJECT_UNREF (plist);
}



/* Load a plist from a string.  */

#define READ_CHUNK 0x10000

typedef struct
{
  /* File pointer if the stream is associated with a file.  Otherwise
     NULL.  */
  FILE *fp;
  int eof;
  unsigned char buffer[READ_CHUNK];
  unsigned char *p, *pend;
} MStream;

static int
get_byte (MStream *st)
{
  int n;

  if (! st->fp || st->eof)
    return EOF;
  n = fread (st->buffer, 1, READ_CHUNK, st->fp);
  if (n <= 0)
    {
      st->eof = 1;
      return EOF;
    }
  st->p = st->buffer + 1;
  st->pend = st->buffer + n;
  return st->buffer[0];
}

#define GETC(st)	\
  ((st)->p < (st)->pend ? *(st)->p++ : get_byte (st))


#define UNGETC(c, st)	\
  (*--(st)->p = (c))

/** Mapping table for reading a number.  Hexadecimal chars
    (0..9,A..F,a..F) are mapped to the corresponding numbers.
    Apostrophe (code 39) is mapped to 254.  All the other bytes are
    mapped to 255.  */
unsigned char hex_mnemonic[256];

/** Mapping table for escaped characters.  Mnemonic characters (e, b,
    f, n, r, or t) that follows '\' are mapped to the corresponding
    character code.  All the other bytes are mapped to themselves.  */
unsigned char escape_mnemonic[256];


/** Read an integer from the stream ST.  It is assumed that we have
    already read one character C.  */

static int
read_decimal (MStream *st, int c)
{
  int num = 0;

  while (c >= '0' && c <= '9')
    {
      num = (num * 10) + (c - '0');
      c = GETC (st);
    }

  if (c != EOF)
    UNGETC (c, st);
  return num;
}

/** Read an unsigned from the stream ST.  */

static unsigned
read_hexadesimal (MStream *st)
{
  int c;
  unsigned num = 0, n;

  while ((c = GETC (st)) != EOF
	 && (n = hex_mnemonic[c]) < 16)
    num = (num << 4) | n;
  if (c != EOF)
    UNGETC (c, st);
  return num;
}


/** Read an M-text element from ST, and add it to LIST.  Return a list
    for the next element.  */

static MPlist *
read_mtext_element (MPlist *plist, MStream *st)
{
  unsigned char buffer[1024];
  int bufsize = 1024;
  unsigned char *buf = buffer;
  int c, i;

  i = 0;
  while ((c = GETC (st)) != EOF && c != '"')
    {
      if (i + MAX_UTF8_CHAR_BYTES >= bufsize)
	{
	  bufsize *= 2;
	  if (buf == buffer)
	    {
	      MTABLE_MALLOC (buf, bufsize, MERROR_PLIST);
	      memcpy (buf, buffer, i);
	    }
	  else
	    MTABLE_REALLOC (buf, bufsize, MERROR_PLIST);
	}

      if (c == '\\')
	{
	  c = GETC (st);
	  if (c == EOF)
	    break;
	  if (c == 'x')
	    {
	      int next_c;

	      c = read_hexadesimal (st);
	      next_c = GETC (st);
	      if (next_c != ' ')
		UNGETC (next_c, st);
	    }
	  else
	    c = escape_mnemonic[c];
	}

      buf[i++] = c;
    }

  MPLIST_SET_ADVANCE (plist, Mtext,
		      mtext__from_data (buf, i, MTEXT_FORMAT_UTF_8, 1));
  if (buf != buffer)
    free (buf);
  return plist;
}

static int
read_character (MStream *st, int c)
{
  unsigned char buf[MAX_UTF8_CHAR_BYTES + 1];
  int len = CHAR_BYTES_BY_HEAD (c);
  int i;

  buf[0] = c;
  for (i = 1; i < len; i++)
    {
      c = GETC (st);
      if (c == EOF
	  || (c & 0xC0) != 0x80)
	break;
      buf[i] = c;
    }
  if (i == len)
    c = STRING_CHAR_UTF8 (buf);
  else
    c = buf[0];
  return c;
}


/** Read an integer element from ST, and add it to LIST.  Return a
    list for the next element.  It is assumed that we have already
    read the character C. */

static MPlist *
read_integer_element (MPlist *plist, MStream *st, int c)
{
  int num;

  if (c == '0' || c == '#')
    {
      c = GETC (st);
      if (c == 'x')
	num = read_hexadesimal (st);
      else
	num = read_decimal (st, c);
    }
  else if (c == '?')
    {
      c = GETC (st);
      if (c == EOF)
	num = 0;
      else if (c != '\\')
	{
	  if (c < 128 || ! CHAR_UNITS_BY_HEAD_UTF8 (c))
	    num = c;
	  else
	    num = read_character (st, c);
	}
      else
	{
	  c = GETC (st);
	  if (c == EOF)
	    num = '\\';
	  else if (c < 128 || ! CHAR_UNITS_BY_HEAD_UTF8 (c))
	    num = escape_mnemonic[c];
	  else
	    num = read_character (st, c);
	}
    }
  else if (c == '-')
    num = - read_decimal (st, GETC (st));
  else
    num = read_decimal (st, c);

  MPLIST_SET_ADVANCE (plist, Minteger, (void *) num);
  return plist;
}

/** Read a symbol element from ST, and add it to LIST.  Return a list
    for the next element.  */

static MPlist *
read_symbol_element (MPlist *plist, MStream *st)
{
  unsigned char buffer[1024];
  int bufsize = 1024;
  unsigned char *buf = buffer;
  int c, i;

  i = 0;
  while ((c = GETC (st)) != EOF
	 && c > ' '
	 && c != ')' && c != '(' && c != '"')
    {
      if (i >= bufsize)
	{
	  bufsize *= 2;
	  if (buf == buffer)
	    {
	      MTABLE_MALLOC (buf, bufsize, MERROR_PLIST);
	      memcpy (buf, buffer, i);
	    }
	  else
	    MTABLE_REALLOC (buf, bufsize, MERROR_PLIST);
	}
      if (c == '\\')
	{
	  c = GETC (st);
	  if (c == EOF)
	    break;
	  c = escape_mnemonic[c];
	}
      buf[i++] = c;
    }

  buf[i] = 0;
  MPLIST_SET_ADVANCE (plist, Msymbol, msymbol ((char *) buf));
  if (buf != buffer)
    free (buf);
  if (c > ' ')
    UNGETC (c, st);
  return plist;
}

/* Read an element of various type from stream ST, and add it to LIST.
   Return a list for the next element.  The element type is decided by
   the first token character found as below:
	'(': plist
	'"': mtext
	'0'..'9', '-': integer
	'?': integer representing character code
	the other ASCII letters: symbol
*/

static MPlist *
read_element (MPlist *plist, MStream *st)
{
  int c;

  /* Skip separators and comments.  */
  while (1)
    {
      while ((c = GETC (st)) != EOF && c <= ' ');
      if (c != ';')
	break;
      while ((c = GETC (st)) != EOF && c != '\n');
      if (c == EOF)
	break;
    }

  if (c == '(')
    {
      MPlist *pl, *p;

      MPLIST_NEW (pl);
      p = pl;
      while ((p = read_element (p, st)));
      MPLIST_SET_ADVANCE (plist, Mplist, pl);
      return plist;
    }
  if (c == '"')
    return read_mtext_element (plist, st);
  if ((c >= '0' && c <= '9') || c == '-' || c == '?' || c == '#')
    return read_integer_element (plist, st, c);
  if (c == EOF || c == ')')
    return NULL;
  UNGETC (c, st);
  return read_symbol_element (plist, st);
}

void
write_element (MText *mt, MPlist *plist)
{
  if (MPLIST_SYMBOL_P (plist))
    {
      MSymbol sym = MPLIST_SYMBOL (plist);

      if (sym == Mnil)
	{
	  MTEXT_CAT_ASCII (mt, "nil");
	}
      else
	{
	  char *name = MSYMBOL_NAME (sym);
	  char *buf = alloca (MSYMBOL_NAMELEN (sym) * 2 + 1), *p = buf;

	  while (*name)
	    {
	      if (*name <= ' ' || *name == '"' || *name == ')' || *name == ')')
		*p++ = '\\';
	      *p++ = *name++;
	    }
	  *p = '\0';
	  MTEXT_CAT_ASCII (mt, buf);
	}
    }
  else if (MPLIST_INTEGER_P (plist))
    {
      int num = MPLIST_INTEGER (plist);
      char buf[128];

      sprintf (buf, "%d", num);
      MTEXT_CAT_ASCII (mt, buf);
    }
  else if (MPLIST_PLIST_P (plist))
    {
      MPlist *pl;

      plist = MPLIST_PLIST (plist);
      mtext_cat_char (mt, '(');
      MPLIST_DO (pl, plist)
	{
	  if (pl != plist)
	    mtext_cat_char (mt, ' ');
	  write_element (mt, pl);
	}
      mtext_cat_char (mt, ')');
    }
  else if (MPLIST_MTEXT_P (plist))
    {
      mtext_cat_char (mt, '"');
      /* Not yet implemnted */
      mtext_cat_char (mt, '"');
    }
}

/* Support functions for mdebug_dump_plist.  */

static void
dump_string (char *str)
{
  char *p = str, *pend = p + strlen (p), *new, *p1;

  new = p1 = alloca ((pend - p) * 4 + 1);
  while (p < pend)
    {
      if (*p < 0)
	{
	  sprintf (p1, "\\x%02X", (unsigned char) *p);
	  p1 += 4;
	}
      else if (*p < ' ')
	{
	  *p1++ = '^';
	  *p1++ = *p + '@';
	}
      else if (*p == ' ')
	{
	  *p1++ = '\\';
	  *p1++ = ' ';
	}
      else
	*p1++ = *p;
      p++;
    }
  *p1 = '\0';
  fprintf (stderr, "%s", new);
}

static void
dump_plist_element (MPlist *plist, int indent)
{
  char *prefix = (char *) alloca (indent + 1);
  MSymbol key;

  memset (prefix, 32, indent);
  prefix[indent] = 0;

  key = MPLIST_KEY (plist);
  fprintf (stderr, "(%s(#%d) ", msymbol_name (MPLIST_KEY (plist)),
	   plist->control.ref_count);
  if (key == Msymbol)
    dump_string (msymbol_name (MPLIST_SYMBOL (plist)));
  else if (key == Mtext)
    mdebug_dump_mtext (MPLIST_MTEXT (plist), indent, 0);
  else if (key == Minteger)
    fprintf (stderr, "%x", MPLIST_INTEGER (plist));
  else if (key == Mstring) 
    fprintf (stderr, "\"%s\"", MPLIST_STRING (plist));
  else if (key == Mplist)
    {
      fprintf (stderr, "\n%s", prefix);
      mdebug_dump_plist (MPLIST_PLIST (plist), indent);
    }
  else
    fprintf (stderr, "0x%X", (unsigned) MPLIST_VAL (plist));
  fprintf (stderr, ")");
}


/* Internal API */
int
mplist__init ()
{
  int i;

  plist_table.count = 0;

  Minteger = msymbol ("integer");
  Mplist = msymbol_as_managing_key ("plist");
  Mtext = msymbol_as_managing_key ("mtext");

  for (i = 0; i < 256; i++)
    hex_mnemonic[i] = 255;
  for (i = '0'; i <= '9'; i++)
    hex_mnemonic[i] = i - '0';
  for (i = 'A'; i <= 'F'; i++)
    hex_mnemonic[i] = i - 'A' + 10;
  for (i = 'a'; i <= 'f'; i++)
    hex_mnemonic[i] = i - 'a' + 10;
  for (i = 0; i < 256; i++)
    escape_mnemonic[i] = i;
  escape_mnemonic['e'] = 27;
  escape_mnemonic['b'] = '\b';
  escape_mnemonic['f'] = '\f';
  escape_mnemonic['n'] = '\n';
  escape_mnemonic['r'] = '\r';
  escape_mnemonic['t'] = '\t';
  escape_mnemonic['\\'] = '\\';

  return 0;
}

void
mplist__fini (void)
{
  mdebug__report_object ("Plist", &plist_table);
}


/* Parse this form of PLIST:
      (symbol:KEY1 TYPE1:VAL1 symbol:KEY2 TYPE2:VAL2 ...)
   and return a newly created plist of this form:
      (KEY1:VAL1 KEY2:VAL2 ...)  */

MPlist *
mplist__from_plist (MPlist *plist)
{
  MPlist *pl, *p;

  MPLIST_NEW (pl);
  p = pl;
  while (! MPLIST_TAIL_P (plist))
    {
      MSymbol key, type;

      if (! MPLIST_SYMBOL_P (plist))
	MERROR (MERROR_PLIST, NULL);
      key = MPLIST_SYMBOL (plist);
      plist = MPLIST_NEXT (plist);
      type = MPLIST_KEY (plist);
      if (type->managing_key)
	M17N_OBJECT_REF (MPLIST_VAL (plist));
      MPLIST_SET_ADVANCE (p, key, MPLIST_VAL (plist));
      plist = MPLIST_NEXT (plist);
    }
  return pl;
}

/** Parse this form of PLIST:
      ((symbol:KEY1 ANY:VAL1 ... ) (symbol:KEY2 ANY:VAL2 ...) ...)
    and return a newly created plist of this form:
      (KEY1:(ANY:VAL1 ...) KEY2:(ANY:VAL2 ...) ...)
    ANY can be any type.  */

MPlist *
mplist__from_alist (MPlist *plist)
{
  MPlist *pl, *p;

  MPLIST_NEW (pl);
  p = pl;
  MPLIST_DO (plist, plist)
    {
      MPlist *elt;

      if (! MPLIST_PLIST_P (plist))
	MERROR (MERROR_PLIST, NULL);
      elt = MPLIST_PLIST (plist);
      if (! MPLIST_SYMBOL_P (elt))
	MERROR (MERROR_PLIST, NULL);
      MPLIST_SET_ADVANCE (p, MPLIST_SYMBOL (elt), MPLIST_NEXT (elt));
      M17N_OBJECT_REF (MPLIST_NEXT (elt));
    }
  return pl;
}


MPlist *
mplist__from_file (FILE *fp)
{
  MPlist *plist, *pl;
  MStream st;

  st.fp = fp;
  st.eof = 0;
  st.p = st.pend = st.buffer;
  MPLIST_NEW (plist);
  pl = plist;
  while ((pl = read_element (pl, &st)));
  return plist;
}


/** Parse $STR of $N bytes and return a property list object.  $FORMAT
    must be either @c MTEXT_FORMAT_US_ASCII or @c MTEXT_FORMAT_UTF_8,
    and controls how to produce @c STRING or @c M-TEXT in the
    following definition.

    The syntax of $STR is as follows.

    PLIST ::= '(' ELEMENT * ')'

    ELEMENT ::= SYMBOL | INTEGER | UNSIGNED | STRING | M-TEXT | PLIST

    SYMBOL ::= ascii-character-sequence

    INTEGER ::= '-' ? [ '0' | .. | '9' ]+

    UNSIGNED ::= '0x' [ '0' | .. | '9' | 'A' | .. | 'F' | 'a' | .. | 'f' ]+

    M-TEXT ::= '"' byte-sequence '"'

    Each kind of @c ELEMENT is assigned one of these keys:
	@c Msymbol, @c Mint, @c Munsigned, @c Mtext, @c Mplist

    In an ascii-character-sequence, a backslush (\) is used as the escape
    character, which means that, for instance, <tt>"abc\ def"</tt>
    produces a symbol whose name is of length seven with the fourth
    character being a space.

    In a byte-sequence, "\r", "\n", "\e", and "\t" are replaced by CR,
    NL, ESC, and TAB character respectively, "\xXX" are replaced by
    byte 0xXX.  After this replacement, the byte-sequence is decoded
    into M-TEXT by $CODING.  */

MPlist *
mplist__from_string (unsigned char *str, int n)
{
  MPlist *plist, *pl;
  MStream st;

  st.fp = NULL;
  st.eof = 0;
  st.p = str;
  st.pend = str + n;
  MPLIST_NEW (plist);
  pl = plist;
  while ((pl = read_element (pl, &st)));
  return plist;
}

int
mplist__serialize (MText *mt, MPlist *plist)
{
  MPlist *pl;

  MPLIST_DO (pl, plist)
    {
      if (pl != plist)
	mtext_cat_char (mt, ' ');
      write_element (mt, pl);
    }
  return 0;
}

/*** @} */
#endif /* !FOR_DOXYGEN || DOXYGEN_INTERNAL_MODULE */


/* External API */

/*** @addtogroup m17nPlist */
/*** @{ */
/*=*/

/***en
    @brief Symbol whose name is "integer".

    The symbol @c Minteger has the name <tt>"integer"</tt>.  A value
    of a plist whose key is @c Minteger must be an integer.  */

MSymbol Minteger;
/*=*/

/***en
    @brief Symbol whose name is "plist".

    The symbol @c Mplist has the name <tt>"plist"</tt>.  It is a
    managing key.  A value of a plist whose key is @c Mplist must be a
    plist.  */

MSymbol Mplist;
/*=*/

/***en
    @brief Symbol whose name is "mtext".

    The symbol @c Mtext has the name <tt>"mtext"</tt>.  It is a
    managing key.  A value of a plist whose key is @c Mtext must be an
    M-text.  */

/***oldja
    @brief "text" を名前として持つシンボル

    定義済みシンボル @c Mtext は <tt>"text"</tt> という名前を持つ管理
    キーである。 */

MSymbol Mtext;


/*=*/
/***en
    @brief Create a property list object.

    The mplist () function returns a newly created property list
    object of length zero.

    @returns
    This function returns a newly created property list.

    @errors
    This function never fails.  */

MPlist *
mplist ()
{
  MPlist *plist;

  MPLIST_NEW (plist);
  return plist;
}  

/*=*/
/***en
    @brief Copy a plist.

    The mplist_copy () function copies $PLIST.  In the copy, the
    values are the same as those of $PLIST.

    @return
    This function returns a newly created plist which is a copy of
    $PLIST.  */
/***
    @errors
    This function never fails.  */ 

MPlist *
mplist_copy (MPlist *plist)
{
  MPlist *copy = mplist (), *pl = copy;

  MPLIST_DO (plist, plist)
    pl = mplist_add (pl, MPLIST_KEY (plist), MPLIST_VAL (plist));
  return copy;
}

/*=*/

/***en
    @brief Set the value of a property in a property list object.

    The mplist_put () function searches property list object $PLIST
    from the beginning for a property whose key is $KEY.  If such a
    property is found, its value is changed to $VALUE.  Otherwise, a
    new property whose key is $KEY and value is $VALUE is appended at
    the end of $PLIST.  See the documentation of mplist_add () for
    the restriction on $KEY and $VAL.

    If $KEY is a managing key, $VAL must be a managed object.  In this
    case, the reference count of the old value, if not @c NULL, is
    decremented by one, and that of $VAL is incremented by one.

    @return
    If the operation was successful, mplist_put () returns a sublist of
    $PLIST whose first element is the just modified or added one.
    Otherwise, it returns @c NULL.  */

MPlist *
mplist_put (MPlist *plist, MSymbol key, void *val)
{
  if (key == Mnil)
    MERROR (MERROR_PLIST, NULL);
  MPLIST_FIND (plist, key);
  if (key->managing_key)
    {
      if (! MPLIST_TAIL_P (plist))
	M17N_OBJECT_UNREF (MPLIST_VAL (plist));
      M17N_OBJECT_REF (val);
    }
  MPLIST_SET (plist, key, val);
  return plist;
}

/*=*/

/***en
    @brief Get the value of a property in a property list object.

    The mplist_get () function searches property list object $PLIST
    from the beginning for a property whose key is $KEY.  If such a
    property is found, a pointer to its value is returned as the type
    of <tt>(void *)</tt>.  If not found, @c NULL is returned.

    When @c NULL is returned, there are two possibilities: one is the
    case where no property is found (see above); the other is the case
    where a property is found and its value is @c NULL.  In case that
    these two cases must be distinguished, use the mplist_find_by_key ()
    function.  */

/***
    @seealso
    mplist_find_by_key () */

void *
mplist_get (MPlist *plist, MSymbol key)
{
  MPLIST_FIND (plist, key);
  return (MPLIST_TAIL_P (plist) ? NULL : MPLIST_VAL (plist));
}

/*=*/

/***en
    @brief Add a property at the end of a property list object.

    The mplist_add () function appends at the end of $PLIST a property
    whose key is $KEY and value is $VAL.  $KEY can be any symbol
    other than @c Mnil.

    If $KEY is a managing key, $VAL must be a managed object.  In this
    case, the reference count of $VAL is incremented by one.

    @return
    If the operation was successful, mplist_add () returns a sublist of
    $PLIST whose first element is the just added one.  Otherwise, it
    returns @c NULL.  */

MPlist *
mplist_add (MPlist *plist, MSymbol key, void *val)
{
  if (key == Mnil)
    MERROR (MERROR_PLIST, NULL);
  MPLIST_FIND (plist, Mnil);
  if (key->managing_key)
    M17N_OBJECT_REF (val);
  MPLIST_KEY (plist) = key;
  MPLIST_VAL (plist) = val;
  MPLIST_NEW (plist->next);
  return plist;
}

/*=*/

/***en
    @brief Push a property to a property list object.

    The mplist_push () function pushes at the top of $PLIST a
    property whose key is $KEY and value si $VAL.

    If $KEY is a managing key, $VAL must be a managed object.  In this
    case, the reference count of $VAL is incremented by one.

    @return
    If the operation was successful, this function returns $PLIST.
    Otherwise, it returns @c NULL.  */

MPlist *
mplist_push (MPlist *plist, MSymbol key, void *val)
{
  MPlist *pl;

  if (key == Mnil)
    MERROR (MERROR_PLIST, NULL);
  MPLIST_NEW (pl);
  MPLIST_KEY (pl) = MPLIST_KEY (plist);
  MPLIST_VAL (pl) = MPLIST_VAL (plist);
  pl->next = plist->next;
  plist->next = pl;
  if (key->managing_key)
    M17N_OBJECT_REF (val);
  MPLIST_KEY (plist) = key;
  MPLIST_VAL (plist) = val;
  return plist;
}

/*=*/

/***en
    @brief Pop a property from a property list object.

    The mplist_pop () function pops the topmost property from $PLIST.
    As a result, the key and value of $PLIST becomes those of the next
    of $PLIST.

    @return
    If the operation was successful, this function return the value of
    the just popped property.  Otherwise, it returns @c NULL.  */

void *
mplist_pop (MPlist *plist)
{
  void *val;
  MPlist *next;

  if (MPLIST_TAIL_P (plist))
    return NULL;
  val = MPLIST_VAL (plist);
  next = plist->next;
  MPLIST_KEY (plist) = MPLIST_KEY (next);
  MPLIST_VAL (plist) = MPLIST_VAL (next);
  if (MPLIST_KEY (plist) != Mnil
      && MPLIST_KEY (plist)->managing_key
      && MPLIST_VAL (plist))
    M17N_OBJECT_REF (MPLIST_VAL (plist));
  plist->next = next->next;
  if (plist->next)
    M17N_OBJECT_REF (plist->next);
  M17N_OBJECT_UNREF (next);
  return val;
}

/*=*/
/***en
    @brief Find a property of a specific key in a property list object.

    The mplist_find_by_key () function searches property list object
    $PLIST from the beginning for a property whose key is $KEY.  If
    such a property is found, a sublist of $PLIST whose first element
    is the found one is returned.  Otherwise, @c NULL is returned.

    If $KEY is Mnil, it returns the last a sublist of $PLIST whose
    first element is the last one of $PLIST.  */

MPlist *
mplist_find_by_key (MPlist *plist, MSymbol key)
{
  MPLIST_FIND (plist, key);
  return (MPLIST_TAIL_P (plist)
	  ? (key == Mnil ? plist : NULL)
	  : plist);
}

/*=*/
/***en
    @brief Find a property of a specific value in a property list object.

    The mplist_find_by_value () function searches property list object
    $PLIST from the beginning for a property whose value is $VAL.  If
    such a property is found, a sublist of $PLIST whose first element
    is the found one is returned.  Otherwise, @c NULL is returned.  */

MPlist *
mplist_find_by_value (MPlist *plist, void *val)
{
  MPLIST_DO (plist, plist)
    {
      if (MPLIST_VAL (plist) == val)
	return plist;
    }
  return NULL;
}

/*=*/

/***en
    @brief Return the next sublist of a plist.

    The mplist_next () function returns a pointer to the sublist of
    $PLIST, which begins at the second element in $PLIST.  If the
    length of $PLIST is zero, it returns @c NULL.  */

MPlist *
mplist_next (MPlist *plist)
{
  return (MPLIST_TAIL_P (plist) ? NULL : plist->next);
}

/*=*/

/***en
    @brief Set the first property in a property list object.

    The mplist_set () function sets the key and value of the first
    property in property list object $PLIST to $KEY and $VALUE,
    respectively.  See the documentation of mplist_add () for the
    restriction on $KEY and $VAL.

    @return
    If the operation was successful, mplist_set () returns $PLIST.
    Otherwise, it returns @c NULL.  */

MPlist *
mplist_set (MPlist *plist, MSymbol key, void * val)
{
  if (key == Mnil)
    {
      if (! MPLIST_TAIL_P (plist))
	{
	  key = MPLIST_KEY (plist);
	  M17N_OBJECT_UNREF (MPLIST_NEXT (plist));
	  MPLIST_KEY (plist) = Mnil;
	  if (key->managing_key && MPLIST_VAL (plist))
	    M17N_OBJECT_UNREF (MPLIST_VAL (plist));
	  plist->next = NULL;
	}
    }
  else
    {
      if (! MPLIST_TAIL_P (plist)
	  && MPLIST_KEY (plist)->managing_key
	  && MPLIST_VAL (plist))
	M17N_OBJECT_UNREF (MPLIST_VAL (plist));
      if (key->managing_key)
	M17N_OBJECT_REF (val);
      MPLIST_SET (plist, key, val);
    }
  return plist;
}

/*=*/

/***en
    @brief Return the length of a plist.

    The mplist_length () function returns the number of properties in
    property list object $PLIST.  */

int
mplist_length (MPlist *plist)
{
  int n;

  for (n = 0; ! (MPLIST_TAIL_P (plist)); n++, plist = plist->next);
  return n;
}

/*=*/

/***en
    @brief Return the key of the first property in a property list object.

    The mplist_key () function returns the key of the first property
    in property list object $PLIST.  If the length of $PLIST is zero,
    it returns @c Mnil.  */

MSymbol
mplist_key (MPlist *plist)
{
  return MPLIST_KEY (plist);
}

/*=*/

/***en
    @brief Return the value of the first property in a property list object.

    The mplist_value () function returns the value of the first
    property in property list object $PLIST.  If the length of $PLIST
    is zero, it returns @c NULL.  */

void *
mplist_value (MPlist *plist)
{
  return MPLIST_VAL (plist);
}

/***en
    @brief Generate a plist by deserializaing an M-text.

    The mplist_deserialize () function parses M-text $MT and returns a
    property list.

    The syntax of $MT is as follows.

    MT ::= '(' ELEMENT * ')'

    ELEMENT ::= SYMBOL | INTEGER | M-TEXT | PLIST

    SYMBOL ::= ascii-character-sequence

    INTEGER ::= '-' ? [ '0' | .. | '9' ]+
		| '0x' [ '0' | .. | '9' | 'A' | .. | 'F' | 'a' | .. | 'f' ]+

    M-TEXT ::= '"' character-sequence '"'

    Each kind of @c ELEMENT is assigned one of these keys:
	@c Msymbol, @c Minteger, @c Mtext, @c Mplist

    In an ascii-character-sequence, a backslush (\) is used as the escape
    character, which means that, for instance, <tt>"abc\ def"</tt>
    produces a symbol whose name is of length seven with the fourth
    character being a space.  */

MPlist *
mplist_deserialize (MText *mt)
{
  if (mt->format > MTEXT_FORMAT_UTF_8)
    {
      if (mtext__adjust_format (mt, MTEXT_FORMAT_UTF_8) < 0)
	MERROR (MERROR_PLIST, NULL);
    }
  return mplist__from_string (MTEXT_DATA (mt), mtext_nbytes (mt));
}

/*** @}  */

/*** @addtogroup m17nDebug */
/*=*/
/*** @{  */

/***en
    @brief Dump a plist.

    The mdebug_dump_plist () function prints $PLIST in a human
    readable way to the stderr.  $INDENT specifies how many columns to
    indent the lines but the first one.

    @return
    This function returns $PLIST.  */

MPlist *
mdebug_dump_plist (MPlist *plist, int indent)
{
  char *prefix = (char *) alloca (indent + 1);
  MPlist *pl;
  int first = 1;

  memset (prefix, 32, indent);
  prefix[indent] = 0;

  fprintf (stderr, "(");
  MPLIST_DO (pl, plist)
    {
      if (first)
	first = 0;
      else
	fprintf (stderr, "\n%s ", prefix);
      dump_plist_element (pl, indent + 2);
    }
  fprintf (stderr, ")");
  return plist;
}

/*** @} */

/*
  Local Variables:
  coding: euc-japan
  End:
*/

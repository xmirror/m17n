/* m17n-core.h -- header file for the CORE API of the m17n library.
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

#ifndef _M17N_CORE_H_
#define _M17N_CORE_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Header file for m17n library.
 */

/* (C1) Introduction */

/***en @defgroup m17nIntro Introduction  */
/***ja @defgroup m17nIntro はじめに  */
/*=*/

#define M17NLIB_MAJOR_VERSION 1
#define M17NLIB_MINOR_VERSION 0
#define M17NLIB_PATCH_LEVEL 1
#define M17NLIB_VERSION_NAME "1.0.1"

extern void m17n_init_core (void);
#define M17N_INIT() m17n_init_core ()
extern void m17n_fini_core (void);
#define M17N_FINI() m17n_fini_core ()

/***en @defgroup m17nCore CORE API  */
/***ja @defgroup m17nCore CORE API */
/*=*/
/*** @ingroup m17nCore */
/***en @defgroup m17nObject Managed Object */
/***ja @defgroup m17nObject 管理下オブジェクト */
/*=*/

/*** @ingroup m17nObject  */
/***en
    @brief The first member of a managed object.

    When an application program defines a new structure for managed
    objects, its first member must be of the type @c struct
    #M17NObjectHead.  Its contents are used by the m17n library, and
    application programs should never touch them.  */

typedef struct
{
  void *filler[2];
} M17NObjectHead;

/*=*/

/* Return a newly allocated managed object.  */
extern void *m17n_object_setup (int size, void (*freer) (void *));

/* Increment the reference count of managed object OBJECT.  */
extern int m17n_object_ref (void *object);

/* Decrement the reference count of managed object OBJECT.  */
extern int m17n_object_unref (void *object);

/*=*/

/* (C2) Symbol handling */

/*** @ingroup m17nCore */
/***en @defgroup m17nSymbol Symbol  */
/***ja @defgroup m17nSymbol シンボル */
/*=*/

/***
    @ingroup m17nSymbol */
/***en
    @brief Type of symbols.

    The type #MSymbol is for a @e symbol object.  Its internal
    structure is concealed from application programs.  */

/***ja
    @brief シンボルの型

    #MSymbol はシンボルオブジェクトの型へのポインタである。 */

typedef struct MSymbol *MSymbol;

/*=*/

/* Predefined symbols. */ 
extern MSymbol Mnil;
extern MSymbol Mt;
extern MSymbol Mstring;
extern MSymbol Msymbol;
extern MSymbol Mtext;

/* Return a symbol of name NAME.  */
extern MSymbol msymbol (const char *name);

/* Return a managing key of name NAME.  */
extern MSymbol msymbol_as_managing_key (const char *name);

/* Return a symbol of name NAME if it already exists.  */
extern MSymbol msymbol_exist (const char *name);

/* Return the name of SYMBOL.  */
extern char *msymbol_name (MSymbol symbol);

/* Give SYMBOL KEY property with value VALUE.  */
extern int msymbol_put (MSymbol symbol, MSymbol key, void *val);

/*** Return KEY property value of SYMBOL.  */
extern void *msymbol_get (MSymbol symbol, MSymbol key);

/* 
 *  (2-1) Property List
 */

/*** @ingroup m17nCore */
/***en @defgroup m17nPlist Property List */
/***ja @defgroup m17nPlist プロパティリスト・オブジェクト */
/*=*/

/***
    @ingroup m17nPlist */ 
/***en
    @brief Type of property list objects.

    The type #MPlist is for a property list object.  Its internal
    structure is concealed from application programs.  */

typedef struct MPlist MPlist;

/*=*/

extern MSymbol Mplist, Minteger;

extern MPlist *mplist ();

extern MPlist *mplist_copy (MPlist *plist);

extern MPlist *mplist_add (MPlist *plist, MSymbol key, void *val);

extern MPlist *mplist_push (MPlist *plist, MSymbol key, void *val);

extern void *mplist_pop (MPlist *plist);

extern MPlist *mplist_put (MPlist *plist, MSymbol key, void *val);

extern void *mplist_get (MPlist *plist, MSymbol key);

extern MPlist *mplist_find_by_key (MPlist *plist, MSymbol key);

extern MPlist *mplist_find_by_value (MPlist *plist, void *val);

extern MPlist *mplist_next (MPlist *plist);

extern MPlist *mplist_set (MPlist *plist, MSymbol key, void *val);

extern int mplist_length (MPlist *plist);

extern MSymbol mplist_key (MPlist *plist);

extern void *mplist_value (MPlist *plist);

/* (S1) Characters */

/*=*/
/*** @ingroup m17nCore */
/***en @defgroup m17nCharacter Character */
/***ja @defgroup m17nCharacter 文字 */
/*=*/

#define MCHAR_MAX 0x3FFFFF
/*#define MCHAR_MAX 0x7FFFFFFF*/

extern MSymbol Mscript;
extern MSymbol Mname;
extern MSymbol Mcategory;
extern MSymbol Mcombining_class;
extern MSymbol Mbidi_category;
extern MSymbol Msimple_case_folding;
extern MSymbol Mcomplicated_case_folding;

extern MSymbol mchar_define_property (char *name, MSymbol type);

extern void *mchar_get_prop (int c, MSymbol key);

extern int mchar_put_prop (int c, MSymbol key, void *val);

/* (C3) Handling chartable */

/*** @ingroup m17nCore */
/***en @defgroup m17nChartable Chartable */
/***ja @defgroup m17nChartable 文字テーブル */
/*=*/
extern MSymbol Mchar_table;

/***
    @ingroup m17nChartable */
/***en
    @brief Type of chartables.

    The type #MCharTable is for a @e chartable objects.  Its
    internal structure is concealed from application programs.  */

/***ja
    @brief 文字テーブルの型

    #MCharTable 型は @e 文字テーブル オブジェクト用の構造体である。
    内部構造はアプリケーションプログラムからは見えない。  */

typedef struct MCharTable MCharTable;
/*=*/

extern MCharTable *mchartable (MSymbol key, void *default_value);

extern void *mchartable_lookup (MCharTable *table, int c);

extern int mchartable_set (MCharTable *table, int c, void *val);

extern int mchartable_set_range (MCharTable *table, int from, int to,
				 void *val);

extern int mchartable_map (MCharTable *table, void *ignore,
			   void (*func) (int from, int to,
					 void *val, void *arg), 
			   void *func_arg);

extern void mchartable_range (MCharTable *table, int *from, int *to);

/*
 *  (5) Handling M-text.
 *	"M" of M-text stands for:
 *	o Multilingual
 *	o Metamorphic
 *	o More than string
 */

/*** @ingroup m17nCore */
/***en @defgroup m17nMtext M-text */
/***ja @defgroup m17nMtext M-text */
/*=*/

/*
 * (5-1) M-text basics
 */

/*** @ingroup m17nMtext */
/***en
    @brief Type of @e M-texts.

    The type #MText is for an @e M-text object.  Its internal
    structure is concealed from application programs.  */

/***ja
    @brief @e MText 用構造体 

    #Mtext 構造体は @e M-text オブジェクトに用いられる。内部構造はア
    プリケーションプログラムからは見えない。

    @latexonly \IPAlabel{MText} @endlatexonly
    @latexonly \IPAlabel{MText->MPlist} @endlatexonly  */

typedef struct MText MText;

/*=*/

extern MText *mtext ();

/*=*/

/***en
    @brief Enumeration for specifying the format of an M-text.

    The enum #MTextFormat is used as an argument of the
    mtext_from_data () function to specify the format of data from
    which an M-text is created.  */

enum MTextFormat
  {
    MTEXT_FORMAT_US_ASCII,
    MTEXT_FORMAT_UTF_8,
    MTEXT_FORMAT_UTF_16LE,
    MTEXT_FORMAT_UTF_16BE,
    MTEXT_FORMAT_UTF_32LE,
    MTEXT_FORMAT_UTF_32BE,
    MTEXT_FORMAT_MAX
  };
/*=*/

extern MText *mtext_from_data (void *data, int nitems,
			       enum MTextFormat format);


/*=*/

/*
 *  (5-2) Functions to manipulate M-texts.  They correspond to string
 *   manipulating functions in libc.
 *   In the following functions, mtext_XXX() corresponds to strXXX().
 */

extern int mtext_len (MText *mt);

extern int mtext_ref_char (MText *mt, int pos);

extern int mtext_set_char (MText *mt, int pos, int c);

extern MText *mtext_copy (MText *mt1, int pos, MText *mt2, int from, int to);

extern int mtext_compare (MText *mt1, int from1, int to1,
			  MText *mt2, int from2, int to2);

extern int mtext_case_compare (MText *mt1, int from1, int to1,
			       MText *mt2, int from2, int to2);

extern int mtext_character (MText *mt, int from, int to, int c);

extern int mtext_del (MText *mt, int from, int to);

extern int mtext_ins (MText *mt1, int pos, MText *mt2);

extern int mtext_ins_char (MText *mt, int pos, int c, int n);

extern MText *mtext_cat_char (MText *mt, int c);

extern MText *mtext_duplicate (MText *mt, int from, int to);

extern MText *mtext_dup (MText *mt);

extern MText *mtext_cat (MText *mt1, MText *mt2);

extern MText *mtext_ncat (MText *mt1, MText *mt2, int n);

extern MText *mtext_cpy (MText *mt1, MText *mt2);

extern MText *mtext_ncpy (MText *mt1, MText *mt2, int n);

extern int mtext_chr (MText *mt, int c);

extern int mtext_rchr (MText *mt, int c);

extern int mtext_cmp (MText *mt1, MText *mt2);

extern int mtext_ncmp (MText *mt1, MText *mt2, int n);

extern int mtext_spn (MText *mt1, MText *mt2);

extern int mtext_cspn (MText *mt1, MText *mt2);

extern int mtext_pbrk (MText *mt1, MText *mt2);

extern int mtext_text (MText *mt1, int pos, MText *mt2);

extern int mtext_search (MText *mt1, int from, int to, MText *mt2);

extern MText *mtext_tok (MText *mt, MText *delim, int *pos);

extern int mtext_casecmp (MText *mt1, MText *mt2);

extern int mtext_ncasecmp (MText *mt1, MText *mt2, int n);

/*** @ingroup m17nPlist */
extern MPlist *mplist_deserialize (MText *mt);

/*
 * (5-3) Text properties
 */

/*** @ingroup m17nCore */
/***en @defgroup m17nTextProperty Text Property */
/***ja @defgroup m17nTextProperty テキストプロパティ */
/*=*/
/*** @ingroup m17nTextProperty */
/***en
    @brief Flag bits to control text property.

    The mtext_property () funciton accepts logical OR of these flag
    bits as an argument.  They control the behaviour of the created
    text property as described in the documentation of each flag
    bit.  */

enum MTextPropertyControl
  {
    /***en If this flag bit is on, an M-text inserted at the start
	position or at the middle of the text property inherits the
	text property.  */
    MTEXTPROP_FRONT_STICKY = 0x01,

    /***en If this flag bit is on, an M-text inserted at the end
	position or at the middle of the text property inherits the
	text property.  */
    MTEXTPROP_REAR_STICKY = 0x02,

    /***en If this flag bit is on, the text property is removed if a
	text in its region is modified.  */
    MTEXTPROP_VOLATILE_WEAK = 0x04,

    /***en If this flag bit is on, the text property is removed if a
	text or the other text property in its region is modified.  */
    MTEXTPROP_VOLATILE_STRONG = 0x08,

    /***en If this flag bit is on, the text property is not
	automatically merged with the others.  */
    MTEXTPROP_NO_MERGE = 0x10,

    MTEXTPROP_CONTROL_MAX = 0x1F
  };

/*=*/
extern MSymbol Mtext_prop_serializer;
extern MSymbol Mtext_prop_deserializer;


/*** @ingroup m17nTextProperty */
/***en
    @brief Type of serializer functions.

    This is the type of serializer functions.  If the key of a symbol
    property is #Msymbol_prop_serializer, the value must be of this
    type.

    @seealso Mtext_prop_serialize (), Mtext_prop_serializer
*/

typedef MPlist *(*MTextPropSerializeFunc) (void *val);

/*** @ingroup m17nTextProperty */
/***en
    @brief Type of deserializer functions.

    This is the type of deserializer functions.  If the key of a
    symbol property is #Msymbol_prop_deserializer, the value must be
    of this type.

    @seealso Mtext_prop_deserialize (), Mtext_prop_deserializer
*/
typedef void *(*MTextPropDeserializeFunc) (MPlist *plist);

extern void *mtext_get_prop (MText *mt, int pos, MSymbol key);

extern int mtext_get_prop_values (MText *mt, int pos, MSymbol key,
				  void **values, int num);

extern int mtext_get_prop_keys (MText *mt, int pos, MSymbol **keys);

extern int mtext_put_prop (MText *mt, int from, int to,
			   MSymbol key, void *val);

extern int mtext_put_prop_values (MText *mt, int from, int to,
				  MSymbol key, void **values, int num);

extern int mtext_push_prop (MText *mt, int from, int to,
			    MSymbol key, void *val);

extern int mtext_pop_prop (MText *mt, int from, int to,
			   MSymbol key);

extern int mtext_change_prop (MText *mt, int from, int to,
			      MSymbol key,
			      int (*func) (int, void ***, int *));

extern int mtext_prop_range (MText *mt, MSymbol key, int pos,
			     int *from, int *to, int deeper);

/*=*/
typedef struct MTextProperty MTextProperty;

/*=*/

extern MTextProperty *mtext_property (MSymbol key, void *val,
				      int control_bits);

extern MText *mtext_property_mtext (MTextProperty *prop);

extern MSymbol mtext_property_key (MTextProperty *prop);

extern void *mtext_property_value (MTextProperty *prop);

extern int mtext_property_start (MTextProperty *prop);

extern int mtext_property_end (MTextProperty *prop);

extern MTextProperty *mtext_get_property (MText *mt, int pos, MSymbol key);

extern int mtext_get_properties (MText *mt, int pos, MSymbol key,
				 MTextProperty **props, int num);

extern int mtext_attach_property (MText *mt, int from, int to,
				  MTextProperty *prop);

extern int mtext_detach_property (MTextProperty *prop);

extern int mtext_push_property (MText *mt, int from, int to,
				MTextProperty *prop);

extern MText *mtext_serialize (MText *mt, int from, int to,
			       MPlist *property_list);

extern MText *mtext_deserialize (MText *mt);

#ifdef __cplusplus
}
#endif

#endif /* _M17N_CORE_H_ */

/*
  Local Variables:
  coding: euc-japan
  End:
*/

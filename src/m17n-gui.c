/* m17n-gui.c -- body of the GUI API.
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
    @addtogroup m17nGUI
    @brief GUI support for a window system.

    This section defines the m17n GUI API concerning M-text drawing
    and inputting under a window system.

    All the definitions here are independent of window systems.  An
    actual library file, however, can depend on a specific window
    system.  For instance, the library file m17n-X.so is an example of
    implementation of the m17n GUI API for the X Window System.

    Actually the GUI API is mainly for toolkit libraries or to
    implement XOM, not for direct use from application programs.
*/

/***ja
    @addtogroup m17nGUI
    @brief ウィンドウシステム上の GUI サポート.

    このセクションはウィンドウシステムのもとでの M-text の表示と入力に
    かかわる m17n GUI API を定義する。

    ここでのすべての定義はウィンドウシステムとは独立である。しかし、実
    際のライブラリファイルは個別のウィンドウシステムに依存する場合があ
    る。たとえばライブラリファイル m17n-X.so は、m17n GUI API の X ウィ
    ンドウ用の実装例である。

    現実には、GUI API は主にツールキットライブラリ向けであるか、または 
    XOM を実装するために用いられており、アプリケーションプログラムから
    の直接の利用を念頭においたものではない。
*/

/*=*/

#if !defined (FOR_DOXYGEN) || defined (DOXYGEN_INTERNAL_MODULE)
/*** @addtogroup m17nInternal
     @{ */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include "m17n-gui.h"
#include "m17n-misc.h"
#include "internal.h"
#include "plist.h"
#include "internal-gui.h"
#include "font.h"
#include "fontset.h"
#include "face.h"

static int win_initialized;

#ifndef DLOPEN_SHLIB_EXT
#define DLOPEN_SHLIB_EXT ".so"
#endif

/** Close MFrame and free it.  */

static void
free_frame (void *object)
{
  MFrame *frame = (MFrame *) object;

  (*frame->driver->close) (frame);
  M17N_OBJECT_UNREF (frame->face);
  free (frame->font);
  M17N_OBJECT_UNREF (frame->font_driver_list);
  free (object);
}


#ifdef HAVE_FREETYPE
/** Null device support.  */

static struct {
  MPlist *realized_fontset_list;
  MPlist *realized_font_list;
  MPlist *realized_face_list;
} null_device;

static int
null_device_init ()
{
  null_device.realized_fontset_list = mplist ();
  null_device.realized_font_list = mplist ();
  null_device.realized_face_list = mplist ();  
  return 0;
}

static int
null_device_fini ()
{
  MPlist *plist;

  MPLIST_DO (plist, null_device.realized_fontset_list)
    mfont__free_realized_fontset ((MRealizedFontset *) MPLIST_VAL (plist));
  M17N_OBJECT_UNREF (null_device.realized_fontset_list);

  MPLIST_DO (plist, null_device.realized_face_list)
    mface__free_realized ((MRealizedFace *) MPLIST_VAL (plist));
  M17N_OBJECT_UNREF (null_device.realized_face_list);

  MPLIST_DO (plist, null_device.realized_font_list)
    mfont__free_realized ((MRealizedFont *) MPLIST_VAL (plist));
  M17N_OBJECT_UNREF (null_device.realized_font_list);
  return 0;
}

static int
null_device_open (MFrame *frame, MPlist *param)
{
  MFace *face;

  frame->device = NULL;
  frame->device_type = 0;
  frame->font_driver_list = mplist ();
  mplist_add (frame->font_driver_list, Mfreetype, &mfont__ft_driver);
  frame->realized_font_list = null_device.realized_font_list;
  frame->realized_face_list = null_device.realized_face_list;
  frame->realized_fontset_list = null_device.realized_fontset_list;
  face = mface_copy (mface__default);
  mplist_push (param, Mface, face);
  M17N_OBJECT_UNREF (face);
  return 0;
}

static void
null_device_close (MFrame *frame)
{
}

static void *
null_device_get_prop (MFrame *frame, MSymbol key)
{
  return NULL;
}

static void
null_device_realize_face (MRealizedFace *rface)
{
  rface->info = NULL;
}

static void
null_device_free_realized_face (MRealizedFace *rface)
{
}

static MDeviceDriver null_driver =
  {
    0,
    null_device_init,
    null_device_fini,
    null_device_open,
    null_device_close,
    null_device_get_prop,
    null_device_realize_face,
    null_device_free_realized_face
  };

#endif

/* Internal API */

MSymbol Mfreetype;

/** Plist of device symbol vs functions to initialized the device
    library.  */
MPlist *m17n__device_library_list;


/*** @} */ 
#endif /* !FOR_DOXYGEN || DOXYGEN_INTERNAL_MODULE */


/* External API */

int
m17n_init_win (void)
{
  int mdebug_mask = MDEBUG_INIT;

  if (win_initialized++)
    return 0;
  m17n_init ();
  if (merror_code != MERROR_NONE)
    return -1;

  MDEBUG_PUSH_TIME ();

  Mx = msymbol ("x");
  Mgd = msymbol ("gd");

  Mfreetype = msymbol ("freetype");
  Mfont = msymbol ("font");
  Mfont_width = msymbol ("font-width");
  Mfont_ascent = msymbol ("font-ascent");
  Mfont_descent = msymbol ("font-descent");
  Mdevice = msymbol ("device");

  Mdisplay = msymbol ("display");
  Mscreen = msymbol ("screen");
  Mdrawable = msymbol ("drawable");
  Mdepth = msymbol ("depth");
  Mwidget = msymbol ("widget");

  MDEBUG_PUSH_TIME ();
  if (mfont__init () < 0)
    goto err;
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize font module."));
  if (mfont__fontset_init () < 0)
    goto err;
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize fontset module."));
  if (mface__init () < 0)
    goto err;
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize face module."));
  if (mdraw__init () < 0)
    goto err;
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize draw module."));
  if (minput__win_init () < 0)
    goto err;
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize input-win module."));
  mframe_default = NULL;

  m17n__device_library_list = mplist ();
#ifdef HAVE_FREETYPE
  null_driver.initialized = 0;
  mplist_put (m17n__device_library_list, Mt, &null_driver);
#endif

  return 0;

 err:
  MDEBUG_POP_TIME ();
  MDEBUG_PRINT_TIME ("INIT", (stderr, " to initialize the m17n GUI module."));
  MDEBUG_POP_TIME ();
  return -1;
}

void
m17n_fini_win (void)
{
  int mdebug_mask = MDEBUG_FINI;

  if (win_initialized > 1)
    win_initialized--;
  else
    {
      MPlist *plist;

      win_initialized = 0;
      MDEBUG_PUSH_TIME ();
      MDEBUG_PUSH_TIME ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize device modules."));
      MPLIST_DO (plist, m17n__device_library_list)
	{
	  MDeviceDriver *driver = MPLIST_VAL (plist);

	  if (driver->initialized)
	    {
	      (*driver->fini) ();
	      driver->initialized = 0;
	    }
	}
      M17N_OBJECT_UNREF (m17n__device_library_list);
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize input-gui module."));
      minput__win_fini ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize draw module."));
      mdraw__fini ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize face module."));
      mface__fini ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize fontset module."));
      mfont__fontset_fini ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize font module."));
      mfont__fini ();
      mframe_default = NULL;
      MDEBUG_POP_TIME ();
      MDEBUG_PRINT_TIME ("FINI", (stderr, " to finalize the gui modules."));
      MDEBUG_POP_TIME ();
    }
  m17n_fini ();
}

/*** @addtogroup m17nFrame */
/***en
    @brief A @e frame is an object corresponding to the graphic device.

    A @e frame is an object of the type #MFrame to hold various
    information about each display/input device.  Almost all m17n GUI
    functions require a pointer to a frame as an argument.  */

/***ja
    @brief @e フレーム とはグラフィックデバイスに対応するオブジェクトである.

    @e フレーム とは #MFrame 型のオブジェクトであり、個々の表示／入力デ
    バイスの情報を格納するために用いられる。ほとんどすべての m17n GUI
    関数は、引数としてフレームへのポインタを要求する。  */

/*** @{ */
/*=*/

/***en
    @name Variables: Keys of frame parameter

    These are the symbols to use in a parameter to creat a frame.  See
    the function mframe () for details.

    #Mdevice, #Mdisplay, #Mscreen, #Mdrawable, #Mdepth, and #Mcolormap
    are also keys of a frame property.  */

/***ja
    @name 変数： フレームパラメータ用キー

    フレームを生成する際のパラメータに用いるシンボル。詳しくは関数
    mframe () の説明参照。

    #Mdevice、 #Mdisplay、 #Mscreen、 #Mdrawable、 #Mdepth、#Mcolormap
    はフレームプロパティのキーでもある。  */

/*=*/

MSymbol Mdevice, Mdisplay, Mscreen, Mdrawable, Mdepth, Mcolormap, Mwidget; 

MSymbol Mx, Mgd;

/*=*/

/***en
    @name Variables: Keys of frame property

    These are the symbols to use as an argument to the function
    mframe_get_prop ().  */
/***ja
    @name 変数： フレームプロパティのキー

    関数 mframe_get_prop () の引数に用いられるシンボル。  */ 
/*** @{ */ 
/*=*/
MSymbol Mfont;
MSymbol Mfont_width;
MSymbol Mfont_ascent;
MSymbol Mfont_descent;

/*=*/

/*** @} */ 
/*=*/

/***en
    @brief Create a new frame.

    The mframe () function creates a new frame with parameters listed
    in $PLIST which may be @c NULL.

    The recognized keys in $PLIST are window system dependent.

    The following key is always recognized.

    <ul>

    <li> #Mdevice, the value must be one of #Mx, #Mgd, and #Mnil.

    If the value is #Mx, the frame is for X Window System.  The
    argument #MDrawWindow specified together with the frame must be of
    type @c Window.  The frame is both readable and writable, thus all
    GUI functions can be used.

    If the value is #Mgd, the frame is for an image object of GD
    library.  The argument #MDrawWindow specified together with the
    frame must be of type @c gdImagePtr.  The frame is writable
    only, thus functions minput_XXX can't be used for the frame.

    If the value is #Mnil, the frame is for a null device.  The frame
    is not writable nor readable, thus functions mdraw_XXX that
    require the argument #MDrawWindow and functions minput_XXX can't
    be used for the frame.

    <li> #Mface, the value must be a pointer to #MFace.

    The value is used as the default face of the frame.

    </ul>

    In addition, if the value of the key #Mdevice is #Mx, the
    following keys are recognized.  They are to specify the root
    window and the depth of drawables that can be used with the frame.

    <ul>

    <li> #Mdrawable, the value type must be <tt>Drawable</tt>.

    A parameter of key #Mdisplay must also be specified.  The
    created frame can be used for drawables whose root window and
    depth are the same as those of the specified drawable on the
    specified display.

    When this parameter is specified, the parameter of key #Mscreen
    is ignored.

    <li> #Mwidget, the value type must be <tt>Widget</tt>.

    The created frame can be used for drawables whose root window and
    depth are the same as those of the specified widget.

    If a parameter of key #Mface is not specified, the default face
    is created from the resources of the widget.

    When this parameter is specified, the parameters of key #Mdisplay,
    #Mscreen, #Mdrawable, #Mdepth are ignored.

    <li> #Mdepth, the value type must be <tt>unsigned</tt>.

    The created frame can be used for drawables of the specified
    depth.

    <li> #Mscreen, the value type must be <tt>(Screen *)</tt>.

    The created frame can be used for drawables whose root window is
    the same as the root window of the specified screen, and depth is
    the same at the default depth of the screen.

    When this parameter is specified, parameter of key #Mdisplay is
    ignored.

    <li> #Mdisplay, the value type must be <tt>(Display *)</tt>.

    The created frame can be used for drawables whose root window is
    the same as the root window for the default screen of the display,
    and depth is the same as the default depth of the screen.

    <li> #Mcolormap, the value type must be <tt>(Colormap)</tt>.

    The created frame uses the specified colormap.

    </ul>

    @return
    If the operation was successful, mframe () returns a pointer to a
    newly created frame.  Otherwise, it returns @c NULL.  */

/***ja
    @brief 新しいフレームを作る.

    関数 mframe () は $PLIST 中のパラメータを持つ新しいフレームを作る。
    $PLIST は @c NULL でも良い。

    $PLIST に現われるキーのうちどれが認識されるかはウィンドウシステム
    に依存する。

    以下のキーは常に認識される。

    <ul>

    <li> #Mdevice. 値は #Mx, #Mgd, #Mnil のいずれかでなくてはならない。

    値が #Mx ならば、新しいフレームは X ウィンドウシステム用である。こ
    のフレームと共に指定された引数 #MDrawWindow は、 @c Window 型でな
    くてはならない。フレームは読み書きともに可能であり、すべてのGUI 関
    数が使用できる。

    値が #Mgd ならば、新しいフレームは GD ライブラリのイメージオブジェ
    クト用である。このフレームと共に指定された引数 #MDrawWindow は、 
    @c gdImagePtr 型でなくてはならない。フレームは書き出し専用であり、
    minput_ で始まる名前の関数は使用できない。

    値が #Mnil ならば、新しいフレームは, null デバイス用である。このフ
    レームは読み書きできないので、引数 #MDrawWindow を必要とするmdraw_ 
    で始まる名前の関数や、minput_ で始まる名前の関数は使用できない。

    <li> #Mface. 値は #MFace へのポインタでなくてはならない。

    この値はフレームのデフォルトのフェースとして用いられる。

    </ul>

    これらのキーに加え、#Mdevice のキーが #Mx である場合に限り以下のキー
    も認識される。以下のキーはルートウィンドウと、フレームで利用できる 
    drawable の深さを指定する。

    <ul>

    <li> #Mdrawable. 値は <tt>Drawable</tt> 型でなくてはならない。

    キー #Mdisplay を持つパラメータも指定されている必要がある。生成さ
    れたフレームは、指定されたディスプレイ上の指定された drawable と同
    じルートウィンドウと深さを持つ drawable に用いられる。

    このパラメータがある場合には、#Mscreen をキーとするパラメータは無
    視される。

    <li> #Mwidget. 値は <tt>Widget</tt> 型でなくてはならない。

    生成されたフレームは、指定したウィジェットと同じルートウィンドウと
    深さを持つ drawable に用いられる。

    キー #Mface を持つパラメータがなければ、デフォルトのフェースはこの
    ウィジェットのリソースから作られる。

    このパラメータがある場合には、#Mdisplay, #Mscreen, #Mdrawable,
    #Mdepth をキーとするパラメータは無視される。

    <li> #Mdepth. 値は <tt>unsigned</tt>  型でなくてはならない。

    生成されたフレームは、指定した深さの drawable に用いられる。

    <li> #Mscreen. 値は <tt>(Screen *)</tt> 型でなくてはならない。

    生成したフレームは、指定したスクリーンと同じルートウィンドウを持ち、
    スクリーンのデフォルトの深さと同じ深さを持つ drawable に用いられる。

    このパラメータがある場合には、#Mdisplay をキーとするパラメータは無
    視される。

    <li> #Mdisplay. 値は <tt>(Display *)</tt> 型でなくてはならない。

    生成されたフレームは、指定したディスプレイのデフォルトスクリーンと
    同じルートウィンドウと同じ深さを持つdrawables に用いられる。

    <li> #Mcolormap. 値は <tt>(Colormap)</tt> 型でなくてはならない。

    生成されたフレームは、指定したカラーマップを使用する。

    </ul>

    @return
    成功すれば mframe() は新しいフレームへのポインタを返す。そうでなけ
    れば @c NULL を返す。  */

MFrame *
mframe (MPlist *plist)
{
  MFrame *frame;
  int plist_created = 0;
  MPlist *pl;
  MSymbol device;
  MDeviceDriver *driver;

  if (plist)
    {
      pl = mplist_find_by_key (plist, Mdevice);
      if (pl)
	{
	  device = MPLIST_VAL (pl);
	  if (device == Mt)
	    MERROR (MERROR_WIN, NULL);
	  if (device == Mnil)
	    device = Mt;
	}
      else
	device = Mx;
    }
  else
    {
      plist = mplist ();
      plist_created = 1;
      device = Mx;
    }

  driver = mplist_get (m17n__device_library_list, device);
  if (! driver)
    MERROR (MERROR_WIN, NULL);
  if (! driver->initialized)
    {
      if ((*driver->init) () < 0)
	MERROR (MERROR_WIN, NULL);
      driver->initialized = 1;
    }      

  M17N_OBJECT (frame, free_frame, MERROR_FRAME);
  if ((*driver->open) (frame, plist) < 0)
    {
      free (frame);
      MERROR (MERROR_WIN, NULL);
    }
  frame->driver = driver;

  if (! mframe_default)
    mframe_default = frame;

  frame->face = mface ();
  MPLIST_DO (pl, plist)
    if (MPLIST_KEY (pl) == Mface)
      mface_merge (frame->face, (MFace *) MPLIST_VAL (pl));
  mface__update_frame_face (frame);

  if (plist_created)
    M17N_OBJECT_UNREF (plist);
  return frame;
}

/*=*/

/***en
    @brief Return property value of frame.

    The mframe_get_prop () function returns a value of property $KEY
    of frame $FRAME.  The valid keys and the corresponding return
    values are as follows.

@verbatim

        key             type of value   meaning of value
        ---             -------------   ----------------
        Mface           MFace *         The default face.

        Mfont           MFont *         The default font.

        Mfont_width     int             Width of the default font.

        Mfont_ascent    int             Ascent of the default font.

        Mfont_descent   int             Descent of the default font.

@endverbatim

    In the m17n-X library, the followings are also accepted.

@verbatim

        key             type of value   meaning of value
        ---             -------------   ----------------
        Mdisplay        Display *       Display associated with the frame.

        Mscreen         int             Screen number of a screen associated
                                        with the frame.

        Mcolormap       Colormap        Colormap of the frame.

        Mdepth          unsigned        Depth of the frame.
@endverbatim
*/

/***ja
    @brief フレームのプロパティの値を返す.

    関数 mframe_get_prop () はフレーム $FRAME のキー $KEY を持つプロパ
    ティの値を返す。有効なキーとその値は以下の通り。

@verbatim

        キー            値の型          値の意味
        ---             -------------   ----------------
        Mface           MFace *         デフォルトのフェース

        Mfont           MFont *         デフォルトのフォント

        Mfont_width     int             デフォルトのフォントの幅

        Mfont_ascent    int             デフォルトのフォントの ascent

        Mfont_descent   int             デフォルトのフォントの descent

@endverbatim

     m17n-X ライブラリでは、以下のキーも使用できる。

@verbatim

        キー            値の型          値の意味
        ---             -------------   ----------------
        Mdisplay        Display *       フレームと関連付けられたディスプレイ

        Mscreen         int             フレームと関連付けられたスクリーン
                                        のスクリーンナンバ

        Mcolormap       Colormap        フレームのカラーマップ

        Mdepth          unsigned        フレームの深さ
@endverbatim

*/

void *
mframe_get_prop (MFrame *frame, MSymbol key)
{
  if (key == Mface)
    return frame->face;
  if (key == Mfont)
    return &frame->rface->rfont->font;
  if (key == Mfont_width)
    return (void *) (frame->space_width);
  if (key == Mfont_ascent)
    return (void *) (frame->ascent);
  if (key == Mfont_descent)
    return (void *) (frame->descent);
  return (*frame->driver->get_prop) (frame, key);
}

/*=*/

/***en
    @brief The default frame.

    The external variable #mframe_default contains a pointer to the
    default frame that is created by the first call of mframe ().  */

/***ja
    @brief デフォルトのフレーム.

    外部変数 #mframe_default は、デフォルトのフレームへのポインタを持
    つ。デフォルトのフレームは、最初に mframe () が呼び出されたときに
    作られる。  */

MFrame *mframe_default;

/*** @} */

/*
  Local Variables:
  coding: euc-japan
  End:
*/

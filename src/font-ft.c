/* font-ft.c -- FreeType interface sub-module.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include "m17n-gui.h"
#include "m17n-misc.h"
#include "internal.h"
#include "plist.h"
#include "internal-gui.h"
#include "font.h"
#include "face.h"

#ifdef HAVE_FREETYPE

#include <freetype/ftbdf.h>

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>

int fontconfig_initialized = 0;
FcConfig *fc_config;
#endif	/* not HAVE_FONTCONFIG */

static MSymbol Municode_bmp, Municode_full, Miso10646_1, Miso8859_1;

static FT_Library ft_library;

typedef struct
{
  MSymbol ft_style;
  MSymbol weight, style, stretch;
} MFTtoProp;

static int ft_to_prop_size;
static MFTtoProp *ft_to_prop;

/** List of FreeType fonts.  Keys are family names, values are plists
    contains fonts of the corresponding family.  In the deeper plist,
    keys are Mt, values are (MFTInfo *).  */
static MPlist *ft_font_list;

static int all_fonts_scaned;

/** Return 0 if NAME implies TrueType or OpenType fonts.  Othersize
    return -1.  */

static int
check_otf_filename (const char *name)
{
  int len = strlen (name);
  const char *ext = name + (len - 4);

  if (len < 5
      || (memcmp (ext, ".ttf", 4)
	  && memcmp (ext, ".TTF", 4)
	  && memcmp (ext, ".otf", 4)
	  && memcmp (ext, ".OTF", 4)))
    return -1;
  return 0;
}

/** Setup members of FT_INFO from FT_FACE.  Return the family name.  */

static MSymbol
set_font_info (FT_Face ft_face, MFTInfo *ft_info, MSymbol family)
{
  MFont *font = &ft_info->font;
  MSymbol style;
  int len;
  char *buf, *p;
  MPlist *charmap_list;
  int unicode_bmp = -1, unicode_full = -1, unicode = -1;
  int i;

  MFONT_INIT (font);

  if (family == Mnil)
    {
      len = strlen (ft_face->family_name) + 1;
      buf = (char *) alloca (len);
      memcpy (buf, ft_face->family_name, len);
      for (p = buf; *p; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p += 'a' - 'A';
      family = msymbol (buf);
    }
  mfont__set_property (font, MFONT_FAMILY, family);

  if (ft_face->style_name)
    {
      len = strlen (ft_face->style_name) + 1;
      buf = (char *) alloca (len);
      memcpy (buf, ft_face->style_name, len);
      for (p = buf; *p; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p += 'a' - 'A';
      style = msymbol (buf);
      for (i = 0; i < ft_to_prop_size; i++)
	if (ft_to_prop[i].ft_style == style)
	  {
	    mfont__set_property (font, MFONT_WEIGHT, ft_to_prop[i].weight);
	    mfont__set_property (font, MFONT_STYLE, ft_to_prop[i].style);
	    mfont__set_property (font, MFONT_STRETCH, ft_to_prop[i].stretch);
	    break;
	  }
    }
  else
    i = ft_to_prop_size;

  if (i == ft_to_prop_size)
    {
      mfont__set_property (font, MFONT_WEIGHT, msymbol ("medium"));
      mfont__set_property (font, MFONT_STYLE, msymbol ("r"));
      mfont__set_property (font, MFONT_STRETCH, msymbol ("normal"));
    }

  mfont__set_property (font, MFONT_ADSTYLE, msymbol (""));

  charmap_list = mplist ();
  mplist_add (charmap_list, Mt, (void *) -1);
  for (i = 0; i < ft_face->num_charmaps; i++)
    {
      char registry_buf[16];
      MSymbol registry;

      sprintf (registry_buf, "%d-%d",
	       ft_face->charmaps[i]->platform_id,
	       ft_face->charmaps[i]->encoding_id);
      registry = msymbol (registry_buf);
      mplist_add (charmap_list, registry, (void *) i);
      
      if (ft_face->charmaps[i]->platform_id == 0)
	{
	  if (ft_face->charmaps[i]->encoding_id == 3)
	    unicode_bmp = i;
	  else if (ft_face->charmaps[i]->encoding_id == 4)
	    unicode_full = i;
	}
      else if (ft_face->charmaps[i]->platform_id == 3)
	{
	  if (ft_face->charmaps[i]->encoding_id == 1)
	    unicode_bmp = i;
	  else if (ft_face->charmaps[i]->encoding_id == 10)
	    unicode_full = i;
	}
      else if (ft_face->charmaps[i]->platform_id == 1
	       && ft_face->charmaps[i]->encoding_id == 0)
	mplist_add (charmap_list, msymbol ("apple-roman"), (void *) i);
    }
  if (unicode_full >= 0)
    {
      mplist_add (charmap_list, Municode_full, (void *) unicode_full);
      mplist_add (charmap_list, Municode_bmp, (void *) unicode_full);
      mplist_add (charmap_list, Miso10646_1, (void *) unicode_full);
      unicode = unicode_full;
    }
  else if (unicode_bmp >= 0)
    {
      mplist_add (charmap_list, Municode_bmp, (void *) unicode_bmp);
      mplist_add (charmap_list, Miso10646_1, (void *) unicode_bmp);
      unicode = unicode_bmp;
    }
  if (unicode >= 0)
    {
      FT_Set_Charmap (ft_face, ft_face->charmaps[unicode]);
      for (i = 255; i >= 32; i--)
	{
	  if (i == 160)
	    i = 126;
	  if (FT_Get_Char_Index (ft_face, (FT_ULong) i) == 0)
	    break;
	}
      if (i == 31)
	mplist_add (charmap_list, Miso8859_1, (void *) unicode);
    }

  ft_info->charmap_list = charmap_list;

  if (! FT_IS_SCALABLE (ft_face))
    {
      BDF_PropertyRec prop;
      
      FT_Get_BDF_Property (ft_face, "PIXEL_SIZE", &prop);
      font->property[MFONT_SIZE] = prop.u.integer * 10;
      FT_Get_BDF_Property (ft_face, "RESOLUTION_Y", &prop);
      font->property[MFONT_RESY] = prop.u.integer;
    }

  return family;
}


static void
close_ft (void *object)
{
  MFTInfo *ft_info = object;

  if (ft_info->ft_face)
    {
      if (ft_info->extra_info)
	M17N_OBJECT_UNREF (ft_info->extra_info);
      FT_Done_Face (ft_info->ft_face);
#ifdef HAVE_OTF
      if (ft_info->otf)
	OTF_close (ft_info->otf);
#endif /* HAVE_OTF */
    }
  else
    {
      free (ft_info->filename);
      M17N_OBJECT_UNREF (ft_info->charmap_list);
    }
  free (ft_info);
}

static void
add_font_info (char *filename, MSymbol family)
{
  FT_Face ft_face;
  BDF_PropertyRec prop;
  MFTInfo *ft_info = NULL;

  if (FT_New_Face (ft_library, filename, 0, &ft_face) == 0)
    {
      if (FT_IS_SCALABLE (ft_face)
	  || FT_Get_BDF_Property (ft_face, "PIXEL_SIZE", &prop) == 0)
	{
	  MSymbol fam;
	  MPlist *plist;

	  M17N_OBJECT (ft_info, close_ft, MERROR_FONT_FT);
	  ft_info->filename = strdup (filename);
	  ft_info->otf_flag = check_otf_filename (filename);
	  fam = set_font_info (ft_face, ft_info, family);
	  plist = mplist_get (ft_font_list, fam);
	  if (! plist)
	    {
	      plist = mplist ();
	      mplist_add (ft_font_list, fam, plist);
	    }
	  mplist_add (plist, fam, ft_info);
	}
      FT_Done_Face (ft_face);
    }
}

#ifdef HAVE_FONTCONFIG

static void
fc_list (MSymbol family)
{
  FcPattern *pattern;
  FcObjectSet *os;
  FcFontSet *fs;
  int i;

  if (! fc_config)
    {
      char *pathname;
      struct stat buf;
      MPlist *plist;

      FcInit ();
      fc_config = FcConfigGetCurrent ();
      MPLIST_DO (plist, mfont_freetype_path)
	if (MPLIST_STRING_P (plist)
	    && (pathname = MPLIST_STRING (plist))
	    && stat (pathname, &buf) == 0)
	  FcConfigAppFontAddDir (fc_config, (FcChar8 *) pathname);
    }

  pattern = FcPatternCreate ();
  if (family)
    FcPatternAddString (pattern, FC_FAMILY,
			(FcChar8 *) (msymbol_name (family)));
  os = FcObjectSetBuild (FC_FILE, FC_FOUNDRY, FC_FAMILY, FC_STYLE, FC_PIXEL_SIZE, NULL);
  fs = FcFontList (fc_config, pattern, os);
  if (fs)
    {
      for (i = 0; i < fs->nfont; i++)
	{
	  char *filename;

	  FcPatternGetString (fs->fonts[i], FC_FILE, 0, (FcChar8 **) &filename);
	  add_font_info (filename, family);
	}
      FcFontSetDestroy (fs);
    }
  FcObjectSetDestroy (os);
  FcPatternDestroy (pattern);
}

#else  /* not HAVE_FONTCONFIG */

static void
ft_list ()
{
  MPlist *plist;
  struct stat buf;
  char *pathname;

  MPLIST_DO (plist, mfont_freetype_path)
    if (MPLIST_STRING_P (plist)
	&& (pathname = MPLIST_STRING (plist))
	&& stat (pathname, &buf) == 0)
      {
	if (S_ISREG (buf.st_mode))
	  add_font_info (pathname, Mnil);
	else if (S_ISDIR (buf.st_mode))
	  {
	    int len = strlen (pathname);
	    char path[PATH_MAX];
	    DIR *dir = opendir (pathname);
	    struct dirent *dp;

	    if (dir)
	      {
		strcpy (path, pathname);
		strcpy (path + len, "/");
		len++;
		while ((dp = readdir (dir)) != NULL)
		  {
		    strcpy (path + len, dp->d_name);
		    add_font_info (path, Mnil);
		  }
		closedir (dir);
	      }
	  }
      }
}

#endif	/* not HAVE_FONTCONFIG */

static MRealizedFont *ft_select (MFrame *, MFont *, MFont *, int);
static int ft_open (MRealizedFont *);
static void ft_find_metric (MRealizedFont *, MGlyphString *, int, int);
static unsigned ft_encode_char (MRealizedFont *, int, unsigned);
static void ft_render (MDrawWindow, int, int, MGlyphString *,
		       MGlyph *, MGlyph *, int, MDrawRegion);

MFontDriver mfont__ft_driver =
  { ft_select, ft_open, ft_find_metric, ft_encode_char, ft_render };

/* The FreeType font driver function LIST.  */

static MRealizedFont *
ft_select (MFrame *frame, MFont *spec, MFont *request, int limited_size)
{
  MPlist *plist, *pl;
  MFTInfo *best_font;
  int best_score;
  MRealizedFont *rfont;
  MSymbol family, registry;

  best_font = NULL;
  best_score = 0;
  family = FONT_PROPERTY (spec, MFONT_FAMILY);
  if (family == Mnil)
    family = FONT_PROPERTY (request, MFONT_FAMILY);
  registry = FONT_PROPERTY (spec, MFONT_REGISTRY);
  if (registry == Mnil)
    registry = Mt;

  if (! ft_font_list)
    ft_font_list = mplist ();
#ifdef HAVE_FONTCONFIG
  if (family != Mnil)
    {
      plist = mplist_get (ft_font_list, family);
      if (! plist)
	{
	  fc_list (family);
	  plist = mplist_get (ft_font_list, family);
	  if (! plist)
	    {
	      mplist_add (ft_font_list, family, plist = mplist ());
	      return NULL;
	    }
	}
    }
  else
    {
      if (! all_fonts_scaned)
	{
	  fc_list (Mnil);
	  all_fonts_scaned = 1;
	}
    }
#else  /* not HAVE_FONTCONFIG */
  if (! all_fonts_scaned)
    {
      ft_list ();
      all_fonts_scaned = 1;
    }
  if (family != Mnil)
    {
      plist = mplist_get (ft_font_list, family);
      if (! plist)
	return NULL;
    }
#endif	/* not HAVE_FONTCONFIG */

  if (family == Mnil)
    plist = MPLIST_VAL (ft_font_list);

 retry:
  MPLIST_DO (pl, plist)
    {
      MFTInfo *ft_info = MPLIST_VAL (pl);
      int score;

      if (! mplist_find_by_key (ft_info->charmap_list, registry))
	continue;
      /* We always ignore FOUNDRY.  */
      ft_info->font.property[MFONT_FOUNDRY] = spec->property[MFONT_FOUNDRY];
      score = mfont__score (&ft_info->font, spec, request, limited_size);
      if (score >= 0
	  && (! best_font
	      || best_score > score))
	{
	  best_font = ft_info;
	  best_score = score;
	  if (score == 0)
	    break;
	}
    }
  if (family == Mnil)
    {
      plist = MPLIST_NEXT (plist);
      if (! MPLIST_TAIL_P (plist))
	goto retry;
    }
  if (! best_font)
    return NULL;

  MSTRUCT_CALLOC (rfont, MERROR_FONT_FT);
  rfont->frame = frame;
  rfont->spec = *spec;
  rfont->request = *request;
  rfont->font = best_font->font;
  rfont->font.property[MFONT_SIZE] = request->property[MFONT_SIZE];
  rfont->font.property[MFONT_REGISTRY] = spec->property[MFONT_REGISTRY];
  rfont->score = best_score;
  rfont->info = best_font;
  M17N_OBJECT_REF (best_font);
  return rfont;
}


/* The FreeType font driver function OPEN.  */

static int
ft_open (MRealizedFont *rfont)
{
  MFTInfo *base = rfont->info, *ft_info;
  MSymbol registry = FONT_PROPERTY (&rfont->font, MFONT_REGISTRY);
  int mdebug_mask = MDEBUG_FONT;
  int size;

  M17N_OBJECT (ft_info, close_ft, MERROR_FONT_FT);
  ft_info->font = base->font;
  ft_info->filename = base->filename;
  ft_info->otf_flag = base->otf_flag;
  ft_info->charmap_list = base->charmap_list;
  M17N_OBJECT_UNREF (base);
  rfont->info = ft_info;

  rfont->status = -1;
  ft_info->ft_face = NULL;
  if (FT_New_Face (ft_library, ft_info->filename, 0, &ft_info->ft_face))
    goto err;
  if (registry == Mnil)
    registry = Mt;
  ft_info->charmap_index
    = (int) mplist_get (((MFTInfo *) rfont->info)->charmap_list, registry);
  if (ft_info->charmap_index >= 0
      && FT_Set_Charmap (ft_info->ft_face, 
			 ft_info->ft_face->charmaps[ft_info->charmap_index]))
    goto err;
  size = rfont->font.property[MFONT_SIZE] / 10;
  if (FT_Set_Pixel_Sizes (ft_info->ft_face, 0, size))
    goto err;

  MDEBUG_PRINT1 (" [FT-FONT] o %s\n", ft_info->filename);
  rfont->status = 1;
  rfont->ascent = ft_info->ft_face->ascender >> 6;
  rfont->descent = ft_info->ft_face->descender >> 6;
  return 0;

 err:
  if (ft_info->ft_face)
    FT_Done_Face (ft_info->ft_face);
  free (ft_info);
  MDEBUG_PRINT1 (" [FT-FONT] x %s\n", ft_info->filename);
  return -1;
}

/* The FreeType font driver function FIND_METRIC.  */

static void
ft_find_metric (MRealizedFont *rfont, MGlyphString *gstring,
		int from, int to)
{
  MFTInfo *ft_info = (MFTInfo *) rfont->info;
  FT_Face ft_face = ft_info->ft_face;
  MGlyph *g = MGLYPH (from), *gend = MGLYPH (to);
  FT_Int32 load_flags = FT_LOAD_RENDER;

#ifdef FT_LOAD_TARGET_MONO
  load_flags |= FT_LOAD_TARGET_MONO;
#else
  load_flags |= FT_LOAD_MONOCHROME;
#endif

  for (; g != gend; g++)
    {
      if (g->code == MCHAR_INVALID_CODE)
	{
	  if (FT_IS_SCALABLE (ft_face))
	    {
	      unsigned unitsPerEm = ft_face->units_per_EM;
	      int size = rfont->font.property[MFONT_SIZE] / 10;

	      g->lbearing = 0;
	      g->rbearing = ft_face->max_advance_width * size / unitsPerEm;
	      g->width = ft_face->max_advance_width * size / unitsPerEm;
	      g->ascent = ft_face->ascender * size / unitsPerEm;
	      g->descent = (- ft_face->descender) * size / unitsPerEm;
	    }
	  else
	    {
	      BDF_PropertyRec prop;

	      g->lbearing = 0;
	      g->rbearing = g->width = ft_face->available_sizes->width;
	      if (FT_Get_BDF_Property (ft_face, "ASCENT", &prop) == 0)
		{
		  g->ascent = prop.u.integer;
		  FT_Get_BDF_Property (ft_face, "DESCENT", &prop);
		  g->descent = prop.u.integer;
		}
	      else
		{
		  g->ascent = ft_face->available_sizes->height;
		  g->descent = 0;
		}
	    }
	}
      else
	{
	  FT_Glyph_Metrics *metrics;
	  FT_UInt code;

	  if (g->otf_encoded)
	    code = g->code;
	  else
	    code = FT_Get_Char_Index (ft_face, (FT_ULong) g->code);

	  FT_Load_Glyph (ft_face, code, FT_LOAD_RENDER);
	  metrics = &ft_face->glyph->metrics;
	  g->lbearing = (metrics->horiBearingX >> 6);
	  g->rbearing = (metrics->horiBearingX + metrics->width) >> 6;
	  g->width = metrics->horiAdvance >> 6;
	  g->ascent = metrics->horiBearingY >> 6;
	  g->descent = (metrics->height - metrics->horiBearingY) >> 6;
	}
    }
}

/* The FreeType font driver function ENCODE_CHAR.  */

static unsigned
ft_encode_char (MRealizedFont *rfont, int c, unsigned ignored)
{
  MFTInfo *ft_info;
  FT_UInt code;

  if (rfont->status == 0)
    {
      if ((rfont->driver->open) (rfont) < 0)
	return -1;
    }
  ft_info = (MFTInfo *) rfont->info;
  code = FT_Get_Char_Index (ft_info->ft_face, (FT_ULong) c);
  if (! code)
    return MCHAR_INVALID_CODE;
  return ((unsigned) c);

}

/* The FreeType font driver function RENDER.  */

#define NUM_POINTS 0x1000

typedef struct {
  MDrawPoint points[NUM_POINTS];
  MDrawPoint *p;
} MPointTable;

static void
ft_render (MDrawWindow win, int x, int y,
	   MGlyphString *gstring, MGlyph *from, MGlyph *to,
	   int reverse, MDrawRegion region)
{
  MFTInfo *ft_info;
  FT_Face ft_face;
  MRealizedFace *rface = from->rface;
  MFrame *frame = rface->frame;
  FT_Int32 load_flags = FT_LOAD_RENDER;
  MGlyph *g;
  int i, j;
  MPointTable point_table[8];

  if (from == to)
    return;

  /* It is assured that the all glyphs in the current range use the
     same realized face.  */
  ft_info = (MFTInfo *) rface->rfont->info;
  ft_face = ft_info->ft_face;

  if (! gstring->anti_alias)
    {
#ifdef FT_LOAD_TARGET_MONO
      load_flags |= FT_LOAD_TARGET_MONO;
#else
      load_flags |= FT_LOAD_MONOCHROME;
#endif
    }

  for (i = 0; i < 8; i++)
    point_table[i].p = point_table[i].points;

  for (g = from; g < to; x += g++->width)
    {
      FT_UInt code;
      unsigned char *bmp;
      int intensity;
      MPointTable *ptable;
      int xoff, yoff;
      int width, pitch;

      if (g->otf_encoded)
	code = g->code;
      else
	code = FT_Get_Char_Index (ft_face, (FT_ULong) g->code);
      FT_Load_Glyph (ft_face, code, load_flags);
      yoff = y - ft_face->glyph->bitmap_top + g->yoff;
      bmp = ft_face->glyph->bitmap.buffer;
      width = ft_face->glyph->bitmap.width;
      pitch = ft_face->glyph->bitmap.pitch;
      if (! gstring->anti_alias)
	pitch *= 8;
      if (width > pitch)
	width = pitch;

      if (gstring->anti_alias)
	for (i = 0; i < ft_face->glyph->bitmap.rows;
	     i++, bmp += ft_face->glyph->bitmap.pitch, yoff++)
	  {
	    xoff = x + ft_face->glyph->bitmap_left + g->xoff;
	    for (j = 0; j < width; j++, xoff++)
	      {
		intensity = bmp[j] >> 5;
		if (intensity)
		  {
		    ptable = point_table + intensity;
		    ptable->p->x = xoff;
		    ptable->p->y = yoff;
		    ptable->p++;
		    if (ptable->p - ptable->points == NUM_POINTS)
		      {
			(*frame->driver->draw_points)
			  (frame, win, rface,
			   reverse ? 7 - intensity : intensity,
			   ptable->points, NUM_POINTS, region);
			ptable->p = ptable->points;
		      }
		  }
	      }
	  }
      else
	for (i = 0; i < ft_face->glyph->bitmap.rows;
	     i++, bmp += ft_face->glyph->bitmap.pitch, yoff++)
	  {
	    xoff = x + ft_face->glyph->bitmap_left + g->xoff;
	    for (j = 0; j < width; j++, xoff++)
	      {
		intensity = bmp[j / 8] & (1 << (7 - (j % 8)));
		if (intensity)
		  {
		    ptable = point_table;
		    ptable->p->x = xoff;
		    ptable->p->y = yoff;
		    ptable->p++;
		    if (ptable->p - ptable->points == NUM_POINTS)
		      {
			(*frame->driver->draw_points) (frame, win, rface,
					   reverse ? 0 : 7,
					   ptable->points, NUM_POINTS, region);
			ptable->p = ptable->points;
		      }		    
		  }
	      }
	}
    }

  if (gstring->anti_alias)
    {
      for (i = 1; i < 8; i++)
	if (point_table[i].p != point_table[i].points)
	  (*frame->driver->draw_points) (frame, win, rface, reverse ? 7 - i : i,
			     point_table[i].points,
			     point_table[i].p - point_table[i].points, region);
    }
  else
    {
      if (point_table[0].p != point_table[0].points)
	(*frame->driver->draw_points) (frame, win, rface, reverse ? 0 : 7,
			   point_table[0].points,
			   point_table[0].p - point_table[0].points, region);
    }
}



int
mfont__ft_init ()
{
  struct {
    char *ft_style;
    char *weight, *style, *stretch;
  } ft_to_prop_name[] =
    { { "regular", "medium", "r", "normal" },
      { "italic", "medium", "i", "normal" },
      { "bold", "bold", "r", "normal" },
      { "bold italic", "bold", "i", "normal" },
      { "narrow", "medium", "r", "condensed" },
      { "narrow italic", "medium", "i", "condensed" },
      { "narrow bold", "bold", "r", "condensed" },
      { "narrow bold italic", "bold", "i", "condensed" },
      { "black", "black", "r", "normal" },
      { "black italic", "black", "i", "normal" },
      { "oblique", "medium", "o", "normal" },
      { "boldoblique", "bold", "o", "normal" } };
  int i;

  if (FT_Init_FreeType (&ft_library) != 0)
    MERROR (MERROR_FONT_FT, -1);

  ft_to_prop_size = sizeof (ft_to_prop_name) / sizeof (ft_to_prop_name[0]);
  MTABLE_MALLOC (ft_to_prop, ft_to_prop_size, MERROR_FONT_FT);
  for (i = 0; i < ft_to_prop_size; i++)
    {
      ft_to_prop[i].ft_style = msymbol (ft_to_prop_name[i].ft_style);
      ft_to_prop[i].weight = msymbol (ft_to_prop_name[i].weight);
      ft_to_prop[i].style = msymbol (ft_to_prop_name[i].style);
      ft_to_prop[i].stretch = msymbol (ft_to_prop_name[i].stretch);
    }

  Municode_bmp = msymbol ("unicode-bmp");
  Municode_full = msymbol ("unicode-full");
  Miso10646_1 = msymbol ("iso10646-1");
  Miso8859_1 = msymbol ("iso8859-1");

  return 0;
}

void
mfont__ft_fini ()
{
  MPlist *plist, *p;

  if (ft_font_list)
    {
      MPLIST_DO (plist, ft_font_list)
	{
	  MPLIST_DO (p, MPLIST_VAL (plist))
	    {
	      MFTInfo *ft_info = MPLIST_VAL (p);

	      M17N_OBJECT_UNREF (ft_info);
	    }
	  M17N_OBJECT_UNREF (MPLIST_VAL (plist));
	}
      M17N_OBJECT_UNREF (ft_font_list);
      ft_font_list = NULL;
    }
  free (ft_to_prop);
  FT_Done_FreeType (ft_library);
  all_fonts_scaned = 0;
}


int
mfont__ft_drive_otf (MGlyphString *gstring, int from, int to,
		     MRealizedFont *rfont,
		     MSymbol script, MSymbol langsys, 
		     MSymbol gsub_features, MSymbol gpos_features)
{
  int len = to - from;
  MGlyph g;
  int i;
#ifdef HAVE_OTF
  MFTInfo *ft_info;
  OTF *otf;
  OTF_GlyphString otf_gstring;
  OTF_Glyph *otfg;
  char *script_name, *language_name;
  char *gsub_feature_names, *gpos_feature_names;
  int from_pos, to_pos;
  int unitsPerEm;

  if (len == 0)
    return from;

  ft_info = (MFTInfo *) rfont->info;
  if (ft_info->otf_flag < 0)
    goto simple_copy;
  otf = ft_info->otf;
  if (! otf && (otf = OTF_open (ft_info->filename)))
    {
      if (OTF_get_table (otf, "head") < 0
	  || (OTF_check_table (otf, "GSUB") < 0
	      && OTF_check_table (otf, "GPOS") < 0))
	{
	  OTF_close (otf);
	  ft_info->otf_flag = -1;
	  ft_info->otf = NULL;
	  goto simple_copy;
	}
      ft_info->otf = otf;
    }

  script_name = msymbol_name (script);
  language_name = langsys != Mnil ? msymbol_name (langsys) : NULL;
  gsub_feature_names
    = (gsub_features == Mt ? "*"
       : gsub_features == Mnil ? NULL
       : msymbol_name (gsub_features));
  gpos_feature_names
    = (gpos_features == Mt ? "*"
       : gpos_features == Mnil ? NULL
       : msymbol_name (gpos_features));

  g = gstring->glyphs[from];
  from_pos = g.pos;
  to_pos = g.to;
  for (i = from + 1; i < to; i++)
    {
      if (from_pos > gstring->glyphs[i].pos)
	from_pos = gstring->glyphs[i].pos;
      if (to_pos < gstring->glyphs[i].to)
	to_pos = gstring->glyphs[i].to;
    }

  unitsPerEm = otf->head->unitsPerEm;
  otf_gstring.size = otf_gstring.used = len;
  otf_gstring.glyphs = (OTF_Glyph *) alloca (sizeof (OTF_Glyph) * len);
  memset (otf_gstring.glyphs, 0, sizeof (OTF_Glyph) * len);
  for (i = 0; i < len; i++)
    {
      if (gstring->glyphs[from + i].otf_encoded)
	{
	  otf_gstring.glyphs[i].c = gstring->glyphs[from + i].c;
	  otf_gstring.glyphs[i].glyph_id = gstring->glyphs[from + i].code;
	}
      else
	{
	  otf_gstring.glyphs[i].c = gstring->glyphs[from + i].code;
	}
    }
      
  if (OTF_drive_tables (otf, &otf_gstring, script_name, language_name,
			gsub_feature_names, gpos_feature_names) < 0)
    goto simple_copy;
  g.pos = from_pos;
  g.to = to_pos;
  for (i = 0, otfg = otf_gstring.glyphs; i < otf_gstring.used; i++, otfg++)
    {
      g.combining_code = 0;
      g.c = otfg->c;
      if (otfg->glyph_id)
	{
	  g.code = otfg->glyph_id;
	  switch (otfg->positioning_type)
	    {
	    case 1: case 2:
	      {
		int off_x = 128, off_y = 128;

		if (otfg->f.f1.format & OTF_XPlacement)
		  off_x = ((double) (otfg->f.f1.value->XPlacement)
			   * 100 / unitsPerEm + 128);
		if (otfg->f.f1.format & OTF_YPlacement)
		  off_y = ((double) (otfg->f.f1.value->YPlacement)
			   * 100 / unitsPerEm + 128);
		g.combining_code
		  = MAKE_COMBINING_CODE (3, 2, 3, 0, off_y, off_x);
		if ((otfg->f.f1.format & OTF_XAdvance)
		    || (otfg->f.f1.format & OTF_YAdvance))
		  off_y--;
	      }
	      break;
	    case 3:
	      /* Not yet supported.  */
	      break;
	    case 4:
	      {
		int off_x, off_y;

		off_x = ((double) (otfg->f.f4.base_anchor->XCoordinate
				   - otfg->f.f4.mark_anchor->XCoordinate)
			 * 100 / unitsPerEm + 128);
		off_y = ((double) (otfg->f.f4.base_anchor->YCoordinate
				   - otfg->f.f4.mark_anchor->YCoordinate)
			 * 100 / unitsPerEm + 128);
		g.combining_code
		  = MAKE_COMBINING_CODE (3, 0, 3, 0, off_y, off_x);
	      }
	      break;
	    case 5:
	      /* Not yet supported.  */
	      break;
	    default:		/* i.e case 6 */
	      /* Not yet supported.  */
	      break;
	    }
	  g.otf_encoded = 1;
	}
      else
	{
	  g.code = otfg->c;
	  g.otf_encoded = 0;
	}
      MLIST_APPEND1 (gstring, glyphs, g, MERROR_FONT_OTF);
    }
  return to;

 simple_copy:
#endif	/* HAVE_OTF */
  for (i = 0; i < len; i++)
    {
      g = gstring->glyphs[from + i];
      MLIST_APPEND1 (gstring, glyphs, g, MERROR_FONT_OTF);
    }
  return to;
}

int
mfont__ft_decode_otf (MGlyph *g)
{
#ifdef HAVE_OTF
  MFTInfo *ft_info = (MFTInfo *) g->rface->rfont->info;
  int c = OTF_get_unicode (ft_info->otf, (OTF_GlyphID) g->code);

  return (c ? c : -1);
#else  /* not HAVE_OTF */
  return -1;
#endif	/* not HAVE_OTF */
}

#endif /* HAVE_FREETYPE */

/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 2000,2001 Darwin O'Connor

         This program is free software; you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
         (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
         GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    url_mmio.c: url interface to OS/2's MMIO file services
*/
#define INCL_OS2MM
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <os2.h>
#include <os2me.h>
#include "url.h"

typedef struct _URL_mmio
{
    char common[sizeof(struct _URL)];
    HMMIO fp;
} URL_mmio;

static long url_mmio_read(URL url, void *buff, long n);
static int  url_mmio_fgetc(URL url);
static long url_mmio_tell(URL url);
static long url_mmio_seek(URL url, long offset, int whence);
static void url_mmio_close(URL url);

URL url_mmio_open(int fp)
{
    URL_mmio *url;

    url = (URL_mmio *)alloc_url(sizeof(URL_mmio));

    /* common members */
    URLm(url, type)      = URL_mmio_t;
    URLm(url, url_read)  = url_mmio_read;
    URLm(url, url_gets)  = NULL;
    URLm(url, url_fgetc) = url_mmio_fgetc;
    URLm(url, url_seek)  = url_mmio_seek;
    URLm(url, url_tell)  = url_mmio_tell;
    URLm(url, url_close) = url_mmio_close;

    /* private members */
    url->fp=(HMMIO)fp;
    return (URL)url;
}

static long url_mmio_read(URL url, void *buff, long size)
{
   return mmioRead(((URL_mmio *)url)->fp,buff,size);
}

static int url_mmio_fgetc(URL url)
{
   unsigned char c;
   int r;
      
   if (mmioRead(((URL_mmio *)url)->fp,&c,1)==0) return EOF;
   else return c;
}

static long url_mmio_tell(URL url)
{
    MMIOINFO mmioinfo;

    mmioGetInfo(((URL_mmio *)url)->fp,&mmioinfo,0);
    return mmioinfo.lDiskOffset;
}

static void url_mmio_close(URL url)
{
    mmioClose(((URL_mmio *)url)->fp,0);
}

static long url_mmio_seek(URL url, long offset, int whence)
{
    int ret;

    ret=url_mmio_tell(url);
    mmioSeek(((URL_mmio *)url)->fp,offset,whence);
    return ret;
}
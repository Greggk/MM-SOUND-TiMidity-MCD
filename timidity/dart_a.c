/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

    dart_c.c: This is the output driver for OS/2 DART output. 

    Initial version by Darwin O'Connor <doconnor@autobahn.mb.ca>
    Modified for Timidity++ by Lesha Bogdanow <boga@fly.triniti.troitsk.ru>

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/*#define USE_OS2_TOOLKIT_HEADERS*/
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_NOPMAPI
#define INCL_OS2MM

#include <os2.h>
#include <os2me.h>
#include <string.h>
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"
#include "mcd_c.h"


static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 count);
static int acntl(int request, void *arg);

/* export the playback mode */
#define dpm dart_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_BUFF_FRAGM_OPT, 
  -1,
  {40/*buffers*/,1/*dynamic priority*/,0,0,0},
  "OS/2 DART output", 'd',
  "",
  open_output,
  close_output,
  output_data,
  acntl
};

#define PRIORTY_DELTA 30
int max_buffers,
    gohigh,
    offhigh;
#ifdef PM
extern HWND Frame;
#endif
#if defined(PM) || defined(MCD)
extern unsigned int currtime;
extern HEV pausesem;
extern int paused;
#endif
#ifdef MCD
extern tCuePoint CuePoints[50];
extern int CuePointCount;
extern tCuePoint PosAdvise;
#endif
LONG MyEvent (ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags);
MCI_MIX_BUFFER Buffers[100];
MCI_MIXSETUP_PARMS MixSetupParms = {NULLHANDLE,0,MCI_WAVE_FORMAT_PCM,0,0,MCI_PLAY,MCI_DEVTYPE_WAVEFORM_AUDIO,0,NULL,NULL,MyEvent,NULL,0,0};
MCI_AMP_OPEN_PARMS AmpOpenParms = {NULLHANDLE,0,0,(PSZ)MCI_DEVTYPE_AUDIO_AMPMIX,NULL,NULL,NULL};
MCI_BUFFER_PARMS BufferParms = {NULLHANDLE,sizeof(MCI_BUFFER_PARMS),0,
				1 << (AUDIO_BUFFER_BITS),0,0,0,Buffers};
int buffcount=0,
    numout=0,
    blocking=0,
    flushing=0,
    hipri=0,
    isplaying=0;
HEV sem;
ULONG threadID;

LONG MyEvent (ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags) {
   if (ulFlags & MIX_STREAM_ERROR && ulStatus==ERROR_DEVICE_UNDERRUN) {
      if (!hipri && !flushing && dpm.extra_param[1]) {
	 hipri=1;
	 DosSetPriority(PRTYS_THREAD,PRTYC_NOCHANGE,PRIORTY_DELTA,threadID);
      }
      if (flushing) {
	 DosPostEventSem(sem);
	 blocking=0;
      }
   }
   if (ulFlags & MIX_WRITE_COMPLETE) {
      numout--;
#ifdef MCD
      while (CuePointCount>0 && currtime+pBuffer->ulBufferLength>CuePoints[0].CuePoint) {
         SendCue(MM_MCICUEPOINT,CuePoints[0].CuePoint);
         DeleteCuePoint(CuePoints[0].CuePoint);
      }
      if (PosAdvise.CuePoint>0 && PosAdvise.CuePoint-(currtime % PosAdvise.CuePoint)<pBuffer->ulBufferLength) {
         SendCue(MM_MCIPOSITIONCHANGE,((currtime/PosAdvise.CuePoint)+1)*PosAdvise.CuePoint);
      }
#endif
#if defined(PM) || defined(MCD)
      /*      currtime=pBuffer->ulTime+timeoffset;*/
      currtime+=pBuffer->ulBufferLength;
#endif
      if (numout<gohigh && !hipri && !flushing && dpm.extra_param[1]) {
         hipri=1;
	 DosSetPriority(PRTYS_THREAD,PRTYC_NOCHANGE,PRIORTY_DELTA,threadID);
      }
      if (blocking || (flushing && !numout)) {
         DosPostEventSem(sem);
         blocking=0;
      }
   }
   return 0;
}

void error(ULONG rc) {
   char errstr[80];

   mciGetErrorString(rc,errstr,80);
   ctl->cmsg(CMSG_ERROR,VERB_NORMAL,"MMOS/2 error: %s",errstr);
}

PTIB ptib=NULL;

static int open_output(void) {
   ULONG rc;
   PPIB ppib=NULL;

    buffcount=0;
    numout=0;
    blocking=0;
    flushing=0;
    hipri=0;
    isplaying=0;
#ifdef PM
   AmpOpenParms.hwndCallback=Frame;
#endif
   rc = mciSendCommand(0,MCI_OPEN,MCI_WAIT | MCI_OPEN_TYPE_ID
/*#ifdef PM*/
                       | MCI_OPEN_SHAREABLE
/*#endif*/
                       ,&AmpOpenParms,0);
   if (ULONG_LOWD(rc)!=MCIERR_SUCCESS) { 
      error(rc);
      return -1;
   }
   /* They can't mean these */
   dpm.encoding&=~PE_BYTESWAP;
   dpm.encoding&=~PE_ULAW;
   if (dpm.encoding & PE_16BIT) dpm.encoding|=PE_SIGNED;
   else dpm.encoding&=~PE_SIGNED;
   MixSetupParms.ulBitsPerSample=dpm.encoding & PE_16BIT?16:8;
   MixSetupParms.ulSamplesPerSec=dpm.rate;
   MixSetupParms.ulChannels=dpm.encoding & PE_MONO?1:2;
   rc=mciSendCommand(AmpOpenParms.usDeviceID,MCI_MIXSETUP,
		     MCI_WAIT | MCI_MIXSETUP_INIT,&MixSetupParms,0);
   if (ULONG_LOWD(rc)!=MCIERR_SUCCESS) error(rc);
   if (!(dpm.encoding & PE_MONO)) BufferParms.ulBufferSize*=2;
   if (dpm.encoding & PE_16BIT) BufferParms.ulBufferSize*=2;
   max_buffers=dpm.extra_param[0];
   BufferParms.ulNumBuffers=max_buffers;
   if (max_buffers<10) dpm.extra_param[1]=0;
   else {
      gohigh=max_buffers/4;
      offhigh=max_buffers/2;
   }
   rc=mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,
                     MCI_WAIT | MCI_ALLOCATE_MEMORY,&BufferParms,0);
   if (ULONG_LOWD(rc)!=MCIERR_SUCCESS) error(rc);
   DosCreateEventSem(NULL,&sem,0,TRUE);
   DosGetInfoBlocks(&ptib,&ppib);
   threadID=ptib->tib_ptib2->tib2_ultid;

   return 0;
}

static int output_data(char *buf, int32 count) {
   static ULONG numposted;
   static MCI_PLAY_PARMS PlayParms = {NULLHANDLE,0,0};

   if (numout>=max_buffers) {
      DosResetEventSem(sem,&numposted);
      blocking=1;
      DosWaitEventSem(sem,-1);
   }
   if (numout>offhigh && hipri) {
      hipri=0;
      DosSetPriority(PRTYS_THREAD,PRTYC_NOCHANGE,-PRIORTY_DELTA,threadID);
   }
   memcpy(Buffers[buffcount].pBuffer,buf,count);

   Buffers[buffcount].ulBufferLength=count;
#if defined(PM) || defined(MCD)
   if (paused) DosWaitEventSem(pausesem,-1); /*Doing a mixWrite when paused causes a resume*/
#endif
   MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle,&Buffers[buffcount++],1);
   if (buffcount>=max_buffers) buffcount=0;
   numout++;
   if (!isplaying) {
      mciSendCommand(AmpOpenParms.usDeviceID,MCI_PLAY,0,&PlayParms,0);
      isplaying=1;
   }

   return 0;
}

static void close_output(void) {
   MCI_GENERIC_PARMS GenericParms;
   ULONG rc;

   DosCloseEventSem(sem);
   rc=mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,
                     MCI_WAIT | MCI_DEALLOCATE_MEMORY,&BufferParms,0);
   if (ULONG_LOWD(rc)!=MCIERR_SUCCESS) error(rc);
   rc=mciSendCommand(AmpOpenParms.usDeviceID,MCI_CLOSE,MCI_WAIT,&GenericParms,0);
   if (ULONG_LOWD(rc)!=MCIERR_SUCCESS) error(rc);
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_DISCARD: {
        MCI_GENERIC_PARMS GenericParms;

        mciSendCommand(AmpOpenParms.usDeviceID,MCI_STOP,MCI_WAIT,&GenericParms,0);
        isplaying=0;
        if (hipri) {
           hipri=0;
           DosSetPriority(PRTYS_THREAD,PRTYC_NOCHANGE,-PRIORTY_DELTA,threadID);
           }
        numout=0;
        return 0;
        }
      case PM_REQ_FLUSH: {
         MCI_GENERIC_PARMS GenericParms;
         ULONG numposted;

         DosResetEventSem(sem,&numposted);
         flushing=1;
         isplaying=0;
         if (!numout) DosPostEventSem(sem); /*Don't stop if already done*/
         else if (DosWaitEventSem(sem,6000)==640)
            ctl->cmsg(CMSG_WARNING,VERB_NORMAL,"Output Flush timed out");
         flushing=0;
         if (hipri) {
            hipri=0;
            DosSetPriority(PRTYS_THREAD,PRTYC_NOCHANGE,-PRIORTY_DELTA,threadID);
            }
         mciSendCommand(AmpOpenParms.usDeviceID,MCI_STOP,MCI_WAIT,&GenericParms,0);
         numout=0;
         return 0;
         }
    }
    return -1;
}

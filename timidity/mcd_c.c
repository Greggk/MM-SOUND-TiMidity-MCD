
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

    mcd_c.c: Control driver to allow Timidity to act as an OS/2 Media Control Device DLL
*/
/*#define INCL_MCIOS2*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#if 1
#define INCL_DOSRAS        //INCL_DOSMISC
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS			// For exceptq
#define INCL_DOSMODULEMGR               // For exceptq
#endif
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#define INCL_OS2MM
#include <os2.h>
#include <os2me.h>
#include <mmio.h>
#include "config.h"
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "output.h"
#include "tables.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"
#include "recache.h"
#include "mcd_c.h"
#include "aq.h"
#include "playmidi.h"

#if 0
#define  _PMPRINTF_
#include "PMPRINTF.H"
#endif
#if 0
// 2023-03-02 SHL For exceptq and DosDumpProcess and DosRaiseException

// 2023-02-02 SHL Enable exceptq support
#ifdef EXCEPTQ_H_INCLUDED
#error exceptq.h already included
#endif
EXCEPTIONREGISTRATIONRECORD reg = {0};
// Assume exceptq.dll already loaded by libc
#define INCL_LOADEXCEPTQ
#include "exceptq.h"
#endif
#if 1
#define MAIN_INTERFACE
MAIN_INTERFACE void timidity_start_initialize(void);
MAIN_INTERFACE int timidity_pre_load_configuration(void);
MAIN_INTERFACE int timidity_post_load_configuration(void);
MAIN_INTERFACE void timidity_init_player(void);
MAIN_INTERFACE int timidity_play_main(int nfiles, char **files);
void timidity_init_aq_buff(void);
USHORT APIENTRY mmioGetData( HMMIO hmmio, PMMIOINFO pmmioinfo, USHORT usFlags );
int play_midi(MidiEvent *eventlist, int32 samples);
#endif
int read_config_file(char *name, int self);
extern char def_instr_name[256];

static void ctl_refresh(void) {}
static void ctl_total_time(int tt);
static void ctl_master_volume(int mv) {}
static void ctl_file_name(char *name);
static void ctl_current_time(int ct) {}
static void ctl_note(int v) {}
static void ctl_program(int ch, int val) {}
static void ctl_volume(int channel, int val) {}
static void ctl_expression(int channel, int val) {}
static void ctl_panning(int channel, int val) {}
static void ctl_sustain(int channel, int val) {}
static void ctl_pitch_bend(int channel, int val) {}
static void ctl_reset(void) {}
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void) {}
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_event(CtlEvent *ev) {}


/**********************************/
/* export the interface functions */

/*#define ctl mcd_control_mode*/

ControlMode mcd_control_mode=
{
  "OS/2 MCD interface", 'm',
  1,0,0,0,
  ctl_open, ctl_close,dumb_pass_playing_list, ctl_read, cmsg,
  ctl_event
};

static FILE *infp=0, *outfp=0; /* infp isn't actually used yet */

typedef struct {
   ULONG param1;
   USHORT userparm;
   HEV finishSEM;
   USHORT message;
   USHORT Notifycode;
   ULONG Returncode;
   VOID *param2copy;
} tmsginfo;

typedef struct stinstance {
   int32 events, samples;
   MidiEvent *event;
   HMMIO fp;
   USHORT DeviceID;
   ULONG Openparam1;
   HWND Opencallback;
   USHORT OpenUserparm;
   tmsginfo *queue[20];
   int queuesize;
   HMTX queuedoor;
   HEV waitSEM;
   tmsginfo *deleyedmsg;
   TID threadid;
   int timeformat;
   unsigned int maxtime,
      sectime;
   int status;
   USHORT PlayNotifycode;
   int SeekTo;
} tinstance;

tCuePoint CuePoints[50];
int CuePointCount = 0;
tCuePoint PosAdvise;

tinstance myinstance;
FILE *debugfile = NULL;
int debugging =0;
unsigned int timesize[3] = {0,1000,3000};
MCI_GENERIC_PARMS GenericParms = {0};
unsigned int currtime;
extern MCI_AMP_OPEN_PARMS AmpOpenParms;
extern int32 midi_restart_time;
HEV pausesem;
int paused;
char currfile[1024];
int instancerunning=0;
extern VOLATILE int intr;

unsigned int muldiv(unsigned int a, unsigned int b, unsigned int c);

int timetommtime (unsigned int currtime) {
   if (debugging) fprintf(debugfile,"CurrTime: %d, convert %d, sectime: %d, result: %d\n",currtime,timesize[myinstance.timeformat],myinstance.sectime,muldiv(currtime,timesize[myinstance.timeformat],myinstance.sectime));
   return muldiv(currtime,timesize[myinstance.timeformat],myinstance.sectime);
}

int mmtimetotime (unsigned int currtime) {
   return muldiv(currtime,myinstance.sectime,timesize[myinstance.timeformat]);
}

ULONG Waitfor(tmsginfo *msginfo) {
   ULONG result;

   if (msginfo->finishSEM) {
      DosWaitEventSem(msginfo->finishSEM,SEM_INDEFINITE_WAIT);
      DosCloseEventSem(msginfo->finishSEM);
#ifdef _PMPRINTF_
      DebugHereIAm();
#endif
      if (debugging) fprintf(debugfile,"The waiting is over\n");
      if (msginfo->param2copy!=NULL) free(msginfo->param2copy); /*Freed by other thread if not wait*/
      result=msginfo->Returncode;
      free(msginfo);
   } else result=MCIERR_SUCCESS;
#ifdef EXCEPTQ_H_INCLUDED
   UninstallExceptq(&reg);
#endif
   return result;
}

void removemsg(tinstance *pInstance) {
   int loop=1;

   DosRequestMutexSem(pInstance->queuedoor,SEM_INDEFINITE_WAIT);
   while (loop<pInstance->queuesize) {
      pInstance->queue[loop-1]=pInstance->queue[loop];
      loop++;
   }
   pInstance->queuesize--;
   DosReleaseMutexSem(pInstance->queuedoor);
}

int InsertCuePoint(tCuePoint *NewCuePoint) {
   ULONG CuePoint;
   int loop, loop2;

   if (CuePointCount>=50) return MCIERR_CUEPOINT_LIMIT_REACHED;
   CuePoint=NewCuePoint->CuePoint;
   loop=0;
#ifdef _PMPRINTF_
   Pmpf(("CuePoint %u CuePointCount %i", CuePoint, CuePointCount));
#endif
   while (loop<CuePointCount && CuePoints[loop].CuePoint<CuePoint) loop++;
#ifdef _PMPRINTF_
   Pmpf(("CuePoint %u CuePoints %u CuePointCount %i", CuePoint, CuePoints[loop].CuePoint, CuePointCount));
#endif
   if (CuePointCount > 0 && CuePoints[loop].CuePoint==CuePoint) return MCIERR_DUPLICATE_CUEPOINT;
   if (CuePointCount > 0) {
   loop2=CuePointCount-1;
   while (loop2>=loop) {CuePoints[loop2+1]=CuePoints[loop2]; loop2--;}
   }
#ifdef _PMPRINTF_
   Pmpf(("CuePoint %u CuePoints %u", CuePoint, CuePoints[loop].CuePoint));
#endif
   CuePoints[loop]=*NewCuePoint;
   CuePointCount++;
   return MCIERR_SUCCESS;
}

int DeleteCuePoint(ULONG CueTime) {
   int loop;

   loop=0;
   while (loop<CuePointCount && CuePoints[loop].CuePoint!=CueTime) loop++;
   if (loop<CuePointCount) {
      while (loop<=CuePointCount) {
         CuePoints[loop]=CuePoints[loop+1];
         loop++;
      }
      CuePointCount--;
      return MCIERR_SUCCESS;
   } else return MCIERR_INVALID_CUEPOINT;
}

void SendCue(USHORT MsgType, ULONG CueTime) {
   tCuePoint *CuePoint;
   ULONG rc;

   if (MsgType==MM_MCICUEPOINT) CuePoint=&CuePoints[0];
   else CuePoint=&PosAdvise;

   rc=mdmDriverNotify(myinstance.DeviceID,CuePoint->Callback,MsgType,CuePoint->UserParm,
                      muldiv(CueTime,timesize[MCI_FORMAT_MMTIME],myinstance.sectime));
   if (debugging) fprintf(debugfile,"Notify Return:%d Window: %X CuePoint: %X MsgType %d Callback %X\n PosAdvise: %X",muldiv(CueTime,timesize[MCI_FORMAT_MMTIME],myinstance.sectime),CuePoint->Callback,
           CuePoint,MsgType,PosAdvise.Callback,&PosAdvise);
}

void Finished(tmsginfo *msginfo, HWND handle) {
   if (msginfo->param1 & MCI_WAIT) {
      if (debugging) fprintf(debugfile,"Post finished:\n");
      DosPostEventSem(msginfo->finishSEM);
   }
   else if (msginfo->param1 & MCI_NOTIFY) {
      mdmDriverNotify(myinstance.DeviceID,handle,MM_MCINOTIFY,
                      msginfo->userparm,
                      MAKEULONG(msginfo->message,msginfo->Notifycode));
      if (msginfo->param2copy!=NULL) free(msginfo->param2copy); /*Freed by other thread if wait*/
      free(msginfo);
   }
}

void loadfile(HMMIO handle) {
   struct timidity_file tf;
   MMIOINFO mmioinfo={0,0,NULL,0,16384,NULL,NULL,NULL,NULL,0,0,{0,0,0,0},0,
                             MMIO_NOTRANSLATE,0,NULL,NULLHANDLE};

    if (myinstance.status!=MCI_MODE_NOT_READY) {
       free_instruments(1);
       free(myinstance.event);
    }
    CuePointCount=0;
    PosAdvise.CuePoint=0;
/*   if (current_file_info!=NULL) free(current_file_info);
      current_file_info=new_midi_file_info(NULL);*/
    /* from play_midi_file*/
    /* Reset key & speed each files */
    note_key_offset = 0;
    midi_time_ratio = 1.0;

    /* Reset restart offset 
    midi_restart_time = 0;  */

#ifdef REDUCE_VOICE_TIME_TUNING
    /* Reset voice reduction stuff */
    min_bad_nv = 256;
    max_good_nv = 1;
    ok_nv_total = 32;
    ok_nv_counts = 1;
    ok_nv = 32;
    ok_nv_sample = 0;
    old_rate = -1;
#if defined(CSPLINE_INTERPOLATION) || defined(LAGRANGE_INTERPOLATION)
    reduce_quality_flag = no_4point_interpolation;
#endif
    restore_voices(0);
#endif

      tf.url=url_mmio_open((int)handle);
      tf.tmpname=NULL;
      strcpy(current_filename,currfile);
      myinstance.event=read_midi_file(&tf,&(myinstance.events),
                                      &(myinstance.samples),current_filename);
      if (myinstance.event) {
         ctl_total_time(myinstance.samples);
         myinstance.status=MCI_MODE_STOP;
         load_missing_instruments(NULL);
      } else {
         if (debugging) {
            fprintf(debugfile,"mmio error: %d GetData: %d\n",mmioGetLastError(handle),mmioGetData(handle,&mmioinfo,0));
            fprintf(debugfile,"Size %d Start %d Next %d End %d bufoffset %d diskoffset %d LogicPos %d\n",
            mmioinfo.cchBuffer,mmioinfo.pchBuffer,mmioinfo.pchNext,mmioinfo.pchEndRead,mmioinfo.lBufOffset,mmioinfo.lDiskOffset,mmioinfo.lLogicalFilePos);
         }
      }
      url_close(tf.url);
}

void *OpenTimidity(tmsginfo *msginfo) {
   ULONG postcount;
   int loop;
   int TranslateChange;
   PSZ mmbase;
   char path[256];
   char *end;
   USHORT rc;
   HMMIO handle;

   myinstance.deleyedmsg=NULL;
   if (DosScanEnv("TIMIDITYDIR",&mmbase)==NO_ERROR) {
      strcpy(path,mmbase);
      end=(char *)index(path,';'); /*Why do I need to cast here?*/
      if (end) *end='\0';
      add_to_pathlist(path);

   }
   /*from main*/
   timidity_start_initialize();
   timidity_pre_load_configuration();
   timidity_post_load_configuration();
   timidity_init_player();
   /*from timidty_play_main*/
   if (play_mode->open_output()==-1) {
      msginfo->Returncode=MCIERR_DEVICE_LOCKED;
      Finished(msginfo,myinstance.Opencallback);
      return NULL;
   }
   ctl->verbosity=2;
   ctl->open(0,0);
   if (!control_ratio) {
      control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
      if(control_ratio<1)
         control_ratio=1;
      else if (control_ratio > MAX_CONTROL_RATIO)
         control_ratio=MAX_CONTROL_RATIO;
   }
   init_load_soundfont();
   aq_setup();
   timidity_init_aq_buff();
   if(allocate_cache_size > 0)
      resamp_cache_reset();
   if (*def_instr_name) set_default_instrument(def_instr_name);
   current_file_info=NULL;
   if (myinstance.fp!=NULLHANDLE) {
      loadfile(myinstance.fp);
      if (myinstance.event==NULL) msginfo->Returncode=MCIERR_INVALID_MEDIA_TYPE;
   }
   Finished(msginfo,myinstance.Opencallback);
   while (TRUE) {
      DosResetEventSem(myinstance.waitSEM,&postcount); /*Reset count*/
      if (debugging) fprintf(debugfile,"Reset1 %ld\n",postcount);
      if (myinstance.queuesize==0) {
         DosQueryEventSem(myinstance.waitSEM,&postcount); /*Check for activity since check queuesize*/
         if (debugging) fprintf(debugfile,"Reset2 %ld\n",postcount);
         if (postcount==0) {
            if (debugging) fprintf(debugfile,"Waiting Wait\n");
            DosWaitEventSem(myinstance.waitSEM,SEM_INDEFINITE_WAIT);
            if (debugging) fprintf(debugfile,"Finished Wait\n");
         }
      }
      /*      pInstance=(tinstance *)PVOIDFROMMP(qmsg.mp2);*/
      msginfo=myinstance.queue[0];
      if (debugging) fprintf(debugfile,"Message recieved: %d,%d\n",msginfo->message,myinstance.queuesize);
      removemsg(&myinstance);
      switch(msginfo->message) {
      case MCI_PLAY:
         if (debugging) fprintf(debugfile,"Play Now!\n");
         myinstance.status=MCI_MODE_PLAY;
         myinstance.PlayNotifycode=MCI_NOTIFY_SUCCESSFUL;
if (debugging) fprintf(debugfile,"currtime: %d SeekTo: %d\n",currtime,myinstance.SeekTo);
         if (myinstance.SeekTo!=-1L) {
            currtime=myinstance.SeekTo;
            myinstance.SeekTo=-1L;
         }
         midi_restart_time=muldiv(currtime,play_mode->rate,myinstance.sectime);
         play_midi(myinstance.event, myinstance.events);
         //, myinstance.samples, muldiv(currtime,play_mode->rate,myinstance.sectime));
         if (debugging) fprintf(debugfile,"Post Play Now!\n");
         msginfo->Notifycode=myinstance.PlayNotifycode;
         myinstance.status=MCI_MODE_STOP;
         break;
      case MCI_LOAD:
         if (debugging) fprintf(debugfile,"Load Now!\n");
         loadfile((HMMIO)((MCI_LOAD_PARMS *)(msginfo->param2copy))->pszElementName);
         if (myinstance.event==NULL) msginfo->Returncode=MCIERR_INVALID_MEDIA_TYPE;
         else {
            currtime=0;
            myinstance.status=MCI_MODE_STOP;
         }
         break;
      case MCI_CLOSE:
         if (debugging) fprintf(debugfile,"Close Now!\n");
         if (myinstance.status!=MCI_MODE_NOT_READY) {
            free_instruments(0);
            if (debugging) fprintf(debugfile,"after free intruments\n");
            free(myinstance.event);
            myinstance.status=MCI_MODE_NOT_READY;
         }
         if(intr)
            aq_flush(1);
         ctl->close();
         if (debugging) fprintf(debugfile,"Before Close SEM\n");
         DosCloseMutexSem(myinstance.queuedoor);
         DosCloseEventSem(myinstance.waitSEM);
         DosCloseEventSem(pausesem);
         if (debugging) fprintf(debugfile,"Past Close SEM\n");
         loop=1; /*Remove any addition queued messages*/
         while (loop<myinstance.queuesize) {
            if (myinstance.queue[loop]->param2copy!=NULL)
               free(myinstance.queue[loop]->param2copy);
            free(myinstance.queue[loop++]);
         }
         if (debugging) fprintf(debugfile,"Past Free msgs\n");
         Finished(msginfo,((PMCI_GENERIC_PARMS)msginfo->param2copy)->hwndCallback);
         play_mode->close_output();
         if (debugging) fprintf(debugfile,"after play close\n");
         instancerunning=0;
         if (debugging) {
            fprintf(debugfile,"Ending thread\n");
            fflush(debugfile);
         }
         return NULL;
      }
      Finished(msginfo,((PMCI_GENERIC_PARMS)msginfo->param2copy)->hwndCallback);
      if (myinstance.deleyedmsg!=NULL) {
         Finished(myinstance.deleyedmsg,((PMCI_GENERIC_PARMS)myinstance.deleyedmsg->param2copy)->hwndCallback);
         myinstance.deleyedmsg=NULL;
      }
   }
}

static int ctl_read(int32 *valp) {
   tmsginfo *msginfo;

   while (myinstance.queuesize>0) {
      msginfo=myinstance.queue[0];
      if (debugging) fprintf(debugfile,"Message recieved (running): %d,%d\n",msginfo->message,myinstance.queuesize);
      switch(msginfo->message) {
      case MCI_STOP:
      case MCI_SEEK:
         removemsg(&myinstance);
         myinstance.deleyedmsg=msginfo;
      case MCI_CLOSE:
      case MCI_LOAD:
         myinstance.PlayNotifycode=MCI_NOTIFY_ABORTED;
         return RC_QUIT;
      case MCI_PLAY:
         removemsg(&myinstance);
      }
   }
  return RC_NONE;
}

ULONG mciDriverEntry(tinstance *pInstance, USHORT usMessage, ULONG ulParam1,
                     ULONG ulParam2, USHORT usUserParm) {
   MMDRV_OPEN_PARMS *open_parm;
   MMIOINFO mmioinfo={0,0,NULL,0,16384,NULL,NULL,NULL,NULL,0,0,{0,0,0,0},0,
                             MMIO_NOTRANSLATE,0,NULL,NULLHANDLE};
   tmsginfo *msginfo;
   int param2size;
   ULONG SeekTo;
   tCuePoint NewCuePoint;
   PSZ debugresult;
#ifdef EXCEPTQ_H_INCLUDED
   
   EXCEPTIONREPORTRECORD err = {0};
#endif
#ifdef EXCEPTQ_H_INCLUDED
         LoadExceptq(&reg, "DI", "exceptq loaded by Timidity MCD mciDriverEntry");
#endif
   if (!debugfile) {
      if (DosScanEnv("TIMIDITYDEBUG",&debugresult)==NO_ERROR) {

         debugfile=fopen(debugresult,"w");
         setvbuf(debugfile,NULL,_IOLBF,0);
         debugging=1;
      } else {
         debugfile=(FILE *)1;
         debugging=0;
      }
   }

   if (debugging) fprintf(debugfile,"Message Sent:%d\n",usMessage);
   switch (usMessage) {
   case MCI_OPEN:
      if (debugging) fprintf(debugfile,"Open File\n");
      if (instancerunning==1) DosSleep(200); /*It to finish shutting down*/
      if (instancerunning==1) return MCIERR_DEVICE_LOCKED;
      instancerunning=1;
      open_parm=(MMDRV_OPEN_PARMS *)ulParam2;
      /*      pInstance=malloc(sizeof(tinstance));*/
      open_parm->pInstance=&myinstance;
      open_parm->usResourceUnitsRequired=1;
      myinstance.Openparam1=ulParam1;
      myinstance.DeviceID=open_parm->usDeviceID;
      myinstance.Opencallback=open_parm->hwndCallback;
      myinstance.OpenUserparm=usUserParm;
      myinstance.fp=NULLHANDLE;
      if (ulParam1 & MCI_OPEN_MMIO) {
         myinstance.fp=(HMMIO)open_parm->pszElementName;
         mmioGetData(myinstance.fp,&mmioinfo,0);
         currfile[0]='\0';
         if (mmioinfo.ulTranslate & MMIO_TRANSLATEDATA) return MCIERR_UNSUPP_FORMAT_MODE;
      } else {
         if (ulParam1 & MCI_OPEN_ELEMENT) {
if (debugging) fprintf(debugfile,"Enter place A: %s\n",open_parm->pszElementName);
            strcpy(currfile,open_parm->pszElementName);
            mmioinfo.fccIOProc=mmioStringToFOURCC("MIDI",0);
            myinstance.fp=mmioOpen(open_parm->pszElementName,&mmioinfo,
                                   MMIO_READ | MMIO_DENYWRITE | MMIO_ALLOCBUF);
            if (!myinstance.fp)
               return MCIERR_FILE_NOT_FOUND;
         }
      }
      myinstance.queuesize=0;
      myinstance.timeformat=MCI_FORMAT_MMTIME;
      myinstance.status=MCI_MODE_NOT_READY;
      myinstance.SeekTo=-1L;
      DosCreateMutexSem(NULL,&(myinstance.queuedoor),0,FALSE);
      DosCreateEventSem(NULL,&(myinstance.waitSEM),0,FALSE);
      DosCreateEventSem(NULL,&pausesem,0,FALSE);
      paused=0;
      msginfo=malloc(sizeof(tmsginfo));
      msginfo->param1=ulParam1;
      msginfo->userparm=usUserParm;
      msginfo->message=usMessage;
      msginfo->param2copy=NULL;
      msginfo->Notifycode=MCI_NOTIFY_SUCCESSFUL;
      msginfo->Returncode=MCIERR_SUCCESS;
      if (ulParam1 & MCI_WAIT) DosCreateEventSem(NULL,&(msginfo->finishSEM),0,FALSE);
      else msginfo->finishSEM=NULLHANDLE;
      myinstance.threadid=_beginthread(OpenTimidity,NULL,65536 * 2,msginfo);
      return Waitfor(msginfo);
   case MCI_GETDEVCAPS:
      if (ulParam1 & MCI_GETDEVCAPS_MESSAGE) {
         switch (((MCI_GETDEVCAPS_PARMS *)(ulParam2))->usMessage) {
         case MCI_OPEN:
         case MCI_GETDEVCAPS:
         case MCI_PLAY:
         case MCI_CLOSE:
         case MCI_STOP:
         case MCI_LOAD:
         case MCI_STATUS:
         case MCI_SET:
         case MCI_PAUSE:
         case MCI_RESUME:
         case MCI_SEEK:
         case MCI_SET_POSITION_ADVISE:
         case MCI_SET_CUEPOINT:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_TRUE;
         default:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_FALSE;
         }
         return (MCI_TRUE_FALSE_RETURN << 16) | MCIERR_SUCCESS;
      }
      if (ulParam1 & MCI_GETDEVCAPS_ITEM) {
         if (debugging) fprintf(debugfile,"Dev Caps: %d\n",(((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulItem));
         switch (((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulItem) {
         case MCI_GETDEVCAPS_CAN_PLAY:
         case MCI_GETDEVCAPS_CAN_SETVOLUME:
         case MCI_GETDEVCAPS_HAS_AUDIO:
         case MCI_GETDEVCAPS_CAN_STREAM:
         case MCI_GETDEVCAPS_USES_FILES:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_TRUE;
            return (MCI_TRUE_FALSE_RETURN << 16) | MCIERR_SUCCESS;
         case MCI_GETDEVCAPS_CAN_EJECT:
         case MCI_GETDEVCAPS_CAN_LOCKEJECT:
         case MCI_GETDEVCAPS_CAN_PROCESS_INTERNAL:
         case MCI_GETDEVCAPS_CAN_RECORD:
         case MCI_GETDEVCAPS_CAN_RECORD_INSERT:
         case MCI_GETDEVCAPS_CAN_SAVE:
         case MCI_GETDEVCAPS_HAS_IMAGE:
         case MCI_GETDEVCAPS_HAS_VIDEO:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_FALSE;
            return (MCI_TRUE_FALSE_RETURN << 16) | MCIERR_SUCCESS;
         case MCI_GETDEVCAPS_DEVICE_TYPE:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_DEVTYPE_SEQUENCER;
            return (MCI_DEVICENAME_RETURN << 16) | MCIERR_SUCCESS;
         case MCI_GETDEVCAPS_PREROLL_TYPE:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=MCI_PREROLL_NOTIFIED;
            return (MCI_PREROLL_TYPE_RETURN << 16) | MCIERR_SUCCESS;
         case MCI_GETDEVCAPS_PREROLL_TIME:
            ((MCI_GETDEVCAPS_PARMS *)(ulParam2))->ulReturn=0;
            return (MCI_INTEGER_RETURNED << 16) | MCIERR_SUCCESS;
         default:
            return MCIERR_UNSUPPORTED_FLAG;
         }
      }
      break;
   case MCI_STATUS:
      if (ulParam1!=MCI_WAIT & MCI_STATUS_ITEM) return MCIERR_UNSUPPORTED_FLAG;
if (debugging) fprintf(debugfile,"Status Message: %d\n",((MCI_STATUS_PARMS *)(ulParam2))->ulItem);
      switch (((MCI_STATUS_PARMS *)(ulParam2))->ulItem) {
      case MCI_STATUS_MEDIA_PRESENT:
      case MCI_STATUS_READY:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=MCI_TRUE;
if (debugging) fprintf(debugfile,"Return true\n");
            return (MCI_TRUE_FALSE_RETURN << 16) | MCIERR_SUCCESS;
         break;
/*      case MCI_STATUS_CAN_PASTE:
      case MCI_STATUS_CAN_REDO:
      case MCI_STATUS_CAN_UNDO:*/
      case MCI_STATUS_VIDEO:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=MCI_FALSE;
            return (MCI_TRUE_FALSE_RETURN << 16) | MCIERR_SUCCESS;
         break;
      case MCI_STATUS_LENGTH:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=timetommtime(myinstance.maxtime);
            return (MCI_INTEGER_RETURNED << 16) | MCIERR_SUCCESS;
         break;
      case MCI_STATUS_POSITION:
      case MCI_STATUS_POSITION_IN_TRACK:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=timetommtime(currtime);
            return (MCI_INTEGER_RETURNED << 16) | MCIERR_SUCCESS;
         break;
      case MCI_STATUS_MODE:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=myinstance.status;
if (debugging) fprintf(debugfile,"Status: %d\n",myinstance.status);
            return (MCI_MODE_RETURN << 16) | MCIERR_SUCCESS;
         break;
      case MCI_STATUS_TIME_FORMAT:
            ((MCI_STATUS_PARMS *)(ulParam2))->ulReturn=myinstance.timeformat;
            return (MCI_TIME_FORMAT_RETURN << 16) | MCIERR_SUCCESS;
         break;
      case MCI_STATUS_AUDIO:
      case MCI_STATUS_VOLUME:
         return mciSendCommand(AmpOpenParms.usDeviceID,MCI_STATUS,ulParam1,(PVOID)ulParam2,usUserParm);
         break;
      default:
if (debugging) fprintf(debugfile,"Return unsupported\n");
         return MCIERR_UNSUPPORTED_FLAG;
      }
      break;
   case MCI_SET:
      if (debugging) fprintf(debugfile,"Set: %X\n",ulParam1);
      if (ulParam1 & MCI_SET_TIME_FORMAT) {
         if (debugging) fprintf(debugfile,"Set TimeFormat: %d\n",((MCI_SET_PARMS *)(ulParam2))->ulTimeFormat);
         myinstance.timeformat=((MCI_SET_PARMS *)(ulParam2))->ulTimeFormat;
      } else if (ulParam1 & MCI_SET_AUDIO) {
         return mciSendCommand(AmpOpenParms.usDeviceID,MCI_SET,ulParam1,(PVOID)ulParam2,usUserParm);
      }
      break;
   case MCI_PAUSE:
      if (myinstance.status==MCI_MODE_NOT_READY || myinstance.status==MCI_MODE_STOP) 
         return MCIERR_SUCCESS;
      paused=1;
      DosResetEventSem(pausesem,&(SeekTo));
      mciSendCommand(AmpOpenParms.usDeviceID,MCI_PAUSE,MCI_WAIT,&GenericParms,0);
      myinstance.status=MCI_MODE_PAUSE;
      break;
   case MCI_RESUME:
      if (myinstance.status==MCI_MODE_NOT_READY || myinstance.status==MCI_MODE_STOP) 
         return MCIERR_SUCCESS;
      paused=0;
      DosPostEventSem(pausesem);
      mciSendCommand(AmpOpenParms.usDeviceID,MCI_RESUME,MCI_WAIT,&GenericParms,0);
      myinstance.status=MCI_MODE_PLAY;
      break;
   case MCI_SEEK:
      if (myinstance.status==MCI_MODE_NOT_READY /*|| myinstance.status==MCI_MODE_STOP*/) 
         return MCIERR_SUCCESS;
      if (ulParam1 & MCI_TO) SeekTo=mmtimetotime(((PMCI_SEEK_PARMS)ulParam2)->ulTo);
      else if (ulParam1 & MCI_TO_START) SeekTo=0;
      else if (ulParam1 & MCI_TO_END) SeekTo=myinstance.maxtime;
if (debugging) fprintf(debugfile,"Mode: %d SeekTo: %d\n",myinstance.status,SeekTo);
      if (myinstance.status==MCI_MODE_PLAY) {
         myinstance.SeekTo=SeekTo;
      } else {
         currtime=SeekTo;
         break;
      }
   case MCI_PLAY:
   case MCI_CLOSE:
   case MCI_STOP:
   case MCI_LOAD:
      msginfo=malloc(sizeof(tmsginfo));
      msginfo->param1=ulParam1;
      msginfo->userparm=usUserParm;
      msginfo->message=usMessage;
      msginfo->Returncode=MCIERR_SUCCESS;
      switch(usMessage) {
      case MCI_PLAY:
         if (myinstance.status==MCI_MODE_NOT_READY) {
            free(msginfo);
            return MCIERR_DEVICE_NOT_READY;
         }
         param2size=sizeof(MCI_PLAY_PARMS);
         break;
      case MCI_LOAD:
         param2size=sizeof(MCI_LOAD_PARMS);
         if (ulParam1 & MCI_OPEN_ELEMENT) {
if (debugging) fprintf(debugfile,"Filename: %s\n",((MCI_LOAD_PARMS *)(ulParam2))->pszElementName);
            mmioinfo.fccIOProc=mmioStringToFOURCC("MIDI",0);
            strcpy(currfile,((MCI_LOAD_PARMS *)(ulParam2))->pszElementName);
            *(PHMMIO)((MCI_LOAD_PARMS *)(ulParam2))->pszElementName=mmioOpen(((MCI_LOAD_PARMS *)(ulParam2))->pszElementName,&mmioinfo,MMIO_READ | MMIO_DENYWRITE | MMIO_ALLOCBUF);
            if (!((HMMIO)((MCI_LOAD_PARMS *)(ulParam2))->pszElementName))
               return MCIERR_FILE_NOT_FOUND;
         } else {
            mmioGetData((HMMIO)((MCI_LOAD_PARMS *)(ulParam2))->pszElementName,&mmioinfo,0);
            if (mmioinfo.ulTranslate & MMIO_TRANSLATEDATA) return MCIERR_UNSUPP_FORMAT_MODE;
            currfile[0]='\0';
         }
         break;
      case MCI_STOP:
#ifdef _PMPRINTF_
        DebugHereIAm();
#endif
         if (myinstance.status==MCI_MODE_NOT_READY || myinstance.status==MCI_MODE_STOP) {
            free(msginfo);
            return MCIERR_SUCCESS;
         }
      case MCI_SEEK:
         if (myinstance.status==MCI_MODE_NOT_READY) {
            free(msginfo);
            return MCIERR_FILE_NOT_FOUND;
         }
      case MCI_CLOSE:
#ifdef _PMPRINTF_
        DebugHereIAm();
#endif
        if (ulParam1 & MCI_CLOSE_EXIT) {
#ifdef EXCEPTQ_H_INCLUDED
            UninstallExceptq(&reg);
#endif
            return MCIERR_SUCCESS;
        }
        param2size=sizeof(MCI_GENERIC_PARMS);
      }
      msginfo->param2copy=memcpy(malloc(param2size),(void *)ulParam2,param2size);
      if ((ulParam1 & MCI_WAIT) /*&& usMessage!=MCI_CLOSE*/) /*Don't wait with close*/
         DosCreateEventSem(NULL,&(msginfo->finishSEM),0,0);
      else msginfo->finishSEM=NULLHANDLE;
      if (debugging) fprintf(debugfile,"Queueing:\n");
      DosRequestMutexSem(myinstance.queuedoor,SEM_INDEFINITE_WAIT);
      myinstance.queue[(myinstance.queuesize)++]=msginfo;
      DosReleaseMutexSem(myinstance.queuedoor);
      if (debugging) fprintf(debugfile,"Queue, will post\n");
      DosPostEventSem(myinstance.waitSEM);
      return Waitfor(msginfo);
   case MCI_SET_POSITION_ADVISE:
      if (myinstance.status==MCI_MODE_NOT_READY) {
         return MCIERR_FILE_NOT_FOUND;
      }
      if (ulParam1 & MCI_SET_POSITION_ADVISE_ON) {
         PosAdvise.CuePoint=mmtimetotime(((PMCI_POSITION_PARMS)ulParam2)->ulUnits);
         PosAdvise.Callback=((PMCI_POSITION_PARMS)ulParam2)->hwndCallback;
         PosAdvise.UserParm=((PMCI_POSITION_PARMS)ulParam2)->usUserParm;
if (debugging) fprintf(debugfile,"Callback=%d\n",PosAdvise.Callback);
      } else PosAdvise.CuePoint=0;
      break;
   case MCI_SET_CUEPOINT:
      if (myinstance.status==MCI_MODE_NOT_READY) {
         return MCIERR_FILE_NOT_FOUND;
      }
      if (ulParam1 & MCI_SET_CUEPOINT_ON) {
         NewCuePoint.CuePoint=mmtimetotime(((PMCI_CUEPOINT_PARMS)ulParam2)->ulCuepoint);
         NewCuePoint.Callback=((PMCI_CUEPOINT_PARMS)ulParam2)->hwndCallback;
         NewCuePoint.UserParm=((PMCI_CUEPOINT_PARMS)ulParam2)->usUserParm;
#ifdef _PMPRINTF_
         Pmpf(("CuePoint %u ", NewCuePoint.CuePoint));
#endif
         return InsertCuePoint(&NewCuePoint);
      } else return DeleteCuePoint(mmtimetotime(((PMCI_CUEPOINT_PARMS)ulParam2)->ulCuepoint));
   case MCI_ACQUIREDEVICE:
   case MCI_RELEASEDEVICE:
   case MCIDRV_RESTORE:
   case MCIDRV_SAVE:
      break;
   default:
      return MCIERR_UNRECOGNIZED_COMMAND;
  }
   return MCIERR_SUCCESS;
}

static int ctl_open(int using_stdin, int using_stdout)
{
  if (using_stdin && using_stdout)
    infp=outfp=stderr;
  else if (using_stdout)
    outfp=stderr;
  else if (using_stdin)
    infp=stdout;

  mcd_control_mode.opened=1;
  return 0;
}

static void ctl_total_time(int tt) {
      currtime=0;
      myinstance.maxtime=tt;
      myinstance.sectime=play_mode->rate;
      if (!(play_mode->encoding & PE_MONO))
         {myinstance.sectime*=2; myinstance.maxtime*=2;}
      if (play_mode->encoding & PE_16BIT)
         {myinstance.sectime*=2; myinstance.maxtime*=2;}
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
  va_list ap;
  if (!debugging) return 0;
  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      mcd_control_mode.verbosity<verbosity_level)
    return 0;
  va_start(ap, fmt);
  if (!mcd_control_mode.opened)
    {
      vfprintf(debugfile, fmt, ap);
      fprintf(debugfile, "\n");
    }
  else
    {
      vfprintf(debugfile, fmt, ap);
      fprintf(debugfile, "\n");
    }
  va_end(ap);
  return 0;
}

static void ctl_file_name(char *name)
{
  if ((mcd_control_mode.verbosity>=0 || mcd_control_mode.trace_playing) && debugging)
    fprintf(debugfile, "Playing %s\n", name);
}
/*
int main(int argc, char *argv[]) {
   PVOID instance;
   MMDRV_OPEN_PARMS open_parm;
   MCI_PLAY_PARMS play_parm;
   MCI_GENERIC_PARMS generic_parm;
   char junk[10];

   open_parm.pszElementName="C:/SOUND/PLAY/STARWARS.MID";
   mciDriverEntry(NULL,MCI_OPEN,MCI_WAIT,(ULONG)&open_parm,0);
   instance=open_parm.pInstance;
   mciDriverEntry(instance,MCI_PLAY,0,(ULONG)&play_parm,0);
   printf("Type something:");
   scanf("%s\n",junk);
   mciDriverEntry(instance,MCI_STOP,MCI_WAIT,(ULONG)&generic_parm,0);
   mciDriverEntry(instance,MCI_CLOSE,MCI_WAIT,(ULONG)&generic_parm,0);
   }
*/

#include <stdio.h>
#define INCL_NOPMAPI
#define INCL_OS2MM
#include <os2.h>
#include <os2me.h>


void main() {
/*   MCI_AMP_OPEN_PARMS mmos2openparms = {NULLHANDLE,0,0,NULL,NULL,NULL,NULL};
   MCI_PLAY_PARMS playparms = {NULLHANDLE,0,0};
   MCI_GENERIC_PARMS genericparms = {NULLHANDLE};

   mmos2openparms.pszElementName="\\sound\\play\\wkrp-cin.mid";
   mmos2openparms.pszDeviceType="timidity";
   mciSendCommand(0,MCI_OPEN,MCI_WAIT | MCI_OPEN_ELEMENT | MCI_READONLY,&mmos2openparms,0);
   mciSendCommand(0,MCI_PLAY,MCI_WAIT,&playparms,0);
   mciSendCommand(0,MCI_CLOSE,MCI_WAIT,&genericparms,0);
   */
   char command[200];
   char result[200];
   ULONG rc;
   
   while (TRUE) {
      printf("MCI>");
      gets(command);
      strcpy(result,"Unchanged");
      rc=mciSendString(command,result,200,NULLHANDLE,0);
      puts(result);
      mciGetErrorString(rc,result,200);
      printf("Error <%s>\n",result);
   }
}

/*gcc testmci.c -los2me*/

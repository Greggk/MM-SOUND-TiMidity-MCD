/*
        Build_TimidityMCD.cmd:
        this uses WIC.EXE to create an archive called
        TimidityMCD.WPI, containing the TimidityMCD program
        (C) VOICE 
*/

/* Set DEBUG=1 to output diagnostic information, and pause, while building */
DEBUG=0

/* Load the REXX Funcs library */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

/* Big screen, so you can see what happened */
"@MODE Co80,43"

/* Find out where warpIn is. */
origdir = directory();
warpindir=SysIni('USER', 'WarpIN', 'Path')
warpindir=left(warpindir,length(warpindir)-1)

/* set the ENDLIBPATH */
"@SET ENDLIBPATH="warpindir

If DEBUG=1 then Do
  SAY "Wic.exe is found in: "warpindir
  SAY "Press any key to continue"
  a=SysGetKey()
end

/*call directory origdir*/

/* set package name */
pkgname='TimidityMCD'

/*x = lastpos("\",origdir)
pkgname=right(origdir,length(origdir)-x)*/

If DEBUG=1 then Do
  SAY "Package name: "pkgname
  SAY "Press any key to continue"
  a=SysGetKey()
end

/* get environment variables for major/minor/revision/fixlevel */
major=VALUE('TimidityMAJOR',,'OS2ENVIRONMENT')
minor=VALUE('TimidityMINOR',,'OS2ENVIRONMENT')
revision=VALUE('TimidityREVISION',,'OS2ENVIRONMENT')
fixlevel=VALUE('TimidityFIXLEVEL',,'OS2ENVIRONMENT')

if major=''|minor=''|revision='' then Do
  /* If the environment variables are not set, ask for version number */
  version = ''
  SAY 
  SAY "Please enter the 3, or 4, digit version number, in the form 1\2\3 or 1\2\3\4 : "

  PARSE PULL major'\'minor'\'revision'\'fixlevel
end

/* This is where the version information is added to the file name */
if fixlevel='' then do
   version=major'\'minor'\'revision
   fileversion='-'major'-'minor'-'revision
end
else Do
   version=major'\'minor'\'revision'\'fixlevel
   fileversion='-'major'-'minor'-'revision'-'fixlevel
end

If DEBUG=1 then Do
  SAY "The WarpIn installer file name will be: "pkgname||fileversion".WPI"
  SAY "Press any key to continue"
  a=SysGetKey()
end

/* Now, update the package.wis file to add the version information */
InName=pkgname||".wis" 
OutName=pkgname||fileversion||".wis"
DO UNTIL (lines(InName)=0)
  linex=linein(InName)
  If POS('PACKAGEID="Github\Timidity++',linex)>0 then Do
    linex=linex||version||'"'
  end
  rc=lineout(Outname,linex)
END
call stream OutName, "C", "CLOSE"	/* close them */
call stream InName, "C", "CLOSE"

If DEBUG=1 then Do
  /* display the resulting WIS file */
  "@E.EXE "OutName
end

/* Create the packages as a WPI file */
/* Package 1 TimidityMCD */
warpindir"\wic.exe "pkgname||fileversion" -a 1 -r -cTimidity * -s "||pkgname||fileversion||".wis"

warpindir"\wic.exe "pkgname||fileversion" -a 3 -r -cMinstall * -s "||pkgname||fileversion||".wis"

If DEBUG=1 then Do
  SAY "Save the results? [Y/N]"
  PARSE UPPER PULL answer
end

IF answer = "Y"|DEBUG=0 then DO
   /* save the last attempt */
   '@copy 'pkgname||fileversion'.* History'
END
"@del "pkgname||fileversion".*"

exit
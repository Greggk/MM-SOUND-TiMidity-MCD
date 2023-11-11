/* CFG_PATH.CMD
 * Add a new path (for configuration or patch files) to TIMIDITY.CFG.
 */
SIGNAL ON NOVALUE

PARSE ARG newpath
PARSE SOURCE . . me
mydir  = FILESPEC('DRIVE', me ) || FILESPEC('PATH', me )
myname = TRANSLATE( FILESPEC('NAME', me ))
tm_cfg = mydir'TIMIDITY.CFG'

IF STREAM( tm_cfg, 'C', 'QUERY EXISTS') == '' THEN DO
    CALL LINEOUT 'STDERR:', 'Configuration file' tm_cfg 'was not found.'
    RETURN 1
END

IF newpath == '' THEN DO
    SAY myname '- Add a new directory to the TiMidity configuration file.'
    SAY 'Usage:' myname '<new_directory>'
    RETURN 0
END

/* Read in the configuration file.
 */
CALL LINEIN tm_cfg, 1, 0
_dcount = 0
_lineno = 0
lastdir = 0
DO WHILE LINES( tm_cfg )
    _line = LINEIN( tm_cfg )
    _lineno = _lineno + 1
    IF TRANSLATE( WORD( STRIP( _line ), 1 )) == 'DIR' THEN DO
        old_directory = STRIP( SUBWORD( _line, 2 ))
        IF old_directory \= '' THEN DO
            _dcount = _dcount + 1
            lastdir = _lineno
            dirlines._dcount = old_directory
        END
    END
    contents._lineno = _line
END
CALL STREAM tm_cfg, 'C', 'CLOSE'
contents.0 = _lineno
dirlines.0 = _dcount
IF lastdir == 0 THEN lastdir = _lineno

SAY 'Configuration file' tm_cfg':'
SAY ' ' contents.0 'lines'
SAY ' ' dirlines.0 'directories defined'
SAY

DO i = 1 TO dirlines.0
    IF TRANSLATE( dirlines.i ) == TRANSLATE( newpath ) THEN DO
        SAY 'Directory "'newpath'" is already defined.  No update is needed.'
        RETURN 0
    END
END

SAY 'Adding new directory:' newpath
SAY
CALL SysFileDelete tm_cfg
CALL LINEIN tm_cfg, 1, 0
DO i = 1 TO contents.0
    IF TRANSLATE( contents.i ) == 'DIR @@TIMIDITY_DIR@@' THEN DO
        lastdir = 0
        CALL LINEOUT tm_cfg, 'dir' newpath
    END
    ELSE
        CALL LINEOUT tm_cfg, contents.i
    IF i == lastdir THEN
        CALL LINEOUT tm_cfg, 'dir' newpath
END
CALL STREAM tm_cfg, 'C', 'CLOSE'
SAY i-1 'lines written.'

RETURN 0


/* -------------------------------------------------------------------------- *
 * CONDITION HANDLERS                                                         *
 * -------------------------------------------------------------------------- */
NOVALUE:
    SAY
    CALL LINEOUT 'STDERR:', RIGHT( sigl, 6 ) '+++' STRIP( SOURCELINE( sigl ))
    CALL LINEOUT 'STDERR:', 'Reference to non-initialized variable.'
    SAY
EXIT sigl


#ifdef MCD
typedef struct stCuePoints {
   ULONG CuePoint;
   HWND Callback;
   USHORT UserParm;
} tCuePoint;

void SendCue(USHORT MsgType, ULONG CueTime);

extern URL url_mmio_open(int fp);
#endif


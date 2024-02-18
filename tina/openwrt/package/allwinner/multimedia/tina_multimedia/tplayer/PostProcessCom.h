
#ifndef		___PostProcess_com_
#define		___PostProcess_com_
#define		MaxSpecGroup 10
typedef union{
   short* pcm16;
   int  * pcm32;
}databuf;

typedef struct __PostProcessSt{
    short     fadeinoutflag; //0:normal 1:fade in 2:fade out
    short     VPS;//-40 - 100
    short     spectrumflag;//0 : disable 1:enable
    short     spectrumOutNum;
    short     SpectrumOutval[MaxSpecGroup][32];
    short     UserEQ[11];//0-10
    int       channal;//1:mono 2:stereo
    int       samplerate;

    databuf*  InputPcmBuf;
    int       InputPcmLen;//samples number

    databuf*  OutputPcmBuf;
    int       OutputPcmBufTotLen;
    int       OutputPcmLen;

    int       bps;
}PostProcessSt;

int do_auPostProc(PostProcessSt *PostProInfo); //0:fail 1:succed

#endif

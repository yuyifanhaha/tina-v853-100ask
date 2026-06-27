#ifndef __DEMUX2ADEC2AO_DEMO_H__
#define __DEMUX2ADEC2AO_DEMO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "DemuxCompStream.h"
#include "mm_comm_demux.h"
#include "mm_comm_adec.h"

#include "mm_comm_vdec.h"
#include "mpi_vdec.h"

#include "mm_comm_vo.h"
#include "mpi_vo.h"

#include "tmessage.h"
#include "tsemaphore.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)

typedef enum {
   MEDIA_VIDEO = 0,
   MEDIA_AUDIO = 1,
   MEDIA_SUBTITLE = 2,
}MEDIA_TYPE_E;

typedef enum
{
    STATE_PREPARED = 0,
    STATE_PAUSE,
    STATE_PLAY,
    STATE_STOP,
}STATE_E;

typedef enum thdstate
{
   THD_StateInvalid = 0,
   THD_StateLoaded,
   THD_StateIdle,
   THD_StateExecuting,
   THD_StatePause,
   THD_Stop
}THDSTATE_T;

typedef enum thdcmd
{
   CMD_START = 0,
   CMD_STOP,
   CMD_PAUSE
}THDCMD_T;


typedef struct Demuxer2Vdec2Vo_Config
{
    char srcFile[MAX_FILE_PATH_LEN];

    int seekTime;

    int mMaxVdecOutputWidth;
    int mMaxVdecOutputHeight;
    int mInitRotation;
    int mUserSetPixelFormat;

    int mTestDuration;

    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;

    int mVeFreq;  // unit: MHz

    BOOL mbForceFramePackage;
}DEMUXER2VDEC2VO_CONFIG_S;


typedef struct sample_demux2av
{
    char srcFile[MAX_FILE_PATH_LEN];
    char confFilePath[MAX_FILE_PATH_LEN];

    DEMUXER2VDEC2VO_CONFIG_S mConfigPara;

    int srcFd;

    STATE_E mState;
    BOOL overFlag;

    CONFPARSER_S mConf;

    int seekTime;
    int mTrackDisableFlag;

    MPP_SYS_CONF_S mSysConf;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mDemuxMediaInfo;

    int mCodecType;
    
    VDEC_CHN mVdecChn;
    VDEC_CHN_ATTR_S mVdecChnAttr;

   int mUILayer;

    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_CHN mVoChn;
    VO_VIDEO_LAYER_ATTR_S mVoLayerAttr;
    VO_CHN_ATTR_S mVoChnAttr;

    ADEC_CHN mAdecChn;
    ADEC_CHN_ATTR_S mAdecChnAttr;
    AUDIO_FRAME_S mAudioFrame;

    AUDIO_DEV mAODevId;
    AO_CHN mAOChn;
    AIO_ATTR_S mAioAttr;

    cdx_sem_t mSemExit;
}SAMPLE_DEMUX2AV;

#endif //#define __DEMUX2ADEC2AO_DEMO_H__


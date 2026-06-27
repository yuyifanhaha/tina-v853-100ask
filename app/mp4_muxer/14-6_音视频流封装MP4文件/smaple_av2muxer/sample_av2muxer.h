#ifndef __SAMPLE_VI2VENC2MUXER_H__
#define __SAMPLE_VI2VENC2MUXER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cdx_list.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "mm_comm_vi.h"
#include "mpi_vi.h"
#include <mpi_isp.h>

#include "vencoder.h"
#include "mpi_venc.h"
#include "mm_comm_video.h"

#include "mm_comm_mux.h"
#include "mpi_mux.h"

#include "tmessage.h"
#include "tsemaphore.h"

#include <memoryAdapter.h>
#include "sc_interface.h"

#include <confparser.h>

#define MAX_FILE_PATH_LEN  (128)

typedef enum H264_PROFILE_E
{
   H264_PROFILE_BASE = 0,
   H264_PROFILE_MAIN,
   H264_PROFILE_HIGH,
}H264_PROFILE_E;

typedef enum H265_PROFILE_E
{
   H265_PROFILE_MAIN = 0,
   H265_PROFILE_MAIN10,
   H265_PROFILE_STI11,
}H265_PROFILE_E;

typedef struct output_sink_info_s
{
    int mMuxerId; //复用器ID
    MEDIA_FILE_FORMAT_E mOutputFormat; //输出文件的格式
    int mOutputFd; //输出文件的文件描述符，用于文件操作（如读写）。这是通过系统调用 open 获取的文件描述符。
    int mFallocateLen; //预分配的文件长度，用于文件系统优化。如果设置为非零值，表示在创建文件时预先分配的空间长度。
    BOOL mCallbackOutFlag; //回调输出标志，类型为布尔值（BOOL）。用于指示是否启用回调输出功能。
}OUTSINKINFO_S, *PTR_OUTSINKINFO_S;

typedef struct mux_chn_info_s
{
    OUTSINKINFO_S mSinkInfo;
    MUX_CHN_ATTR_S mMuxChnAttr;
    MUX_CHN mMuxChn;
    struct list_head mList;
}MUX_CHN_INFO_S, *PTR_MUX_CHN_INFO_S;

typedef struct venc_in_frame_s
{
    VIDEO_FRAME_INFO_S  mFrame;
    struct list_head mList;
}VENC_IN_FRAME_S, *PTR_VENC_IN_FRAME_S;

typedef enum RecordState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
}RECSTATE_E;

typedef enum Vi2Venc2MuxerMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    Vi_Timeout,
    MsgQueue_Stop,
}Vi2Venc2MuxerMsgType;

typedef struct CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}CMDLINEPARAM_S;

typedef struct Vi2Venc2Muxer_Config
{
    char dstVideoFile[MAX_FILE_PATH_LEN];
    int mbAddRepairInfo;
    int mMaxFrmsTagInterval;    //frames interval for repair. unit:us
    int mDstFileMaxCnt;

    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    int mSrcFrameRate;
    enum v4l2_colorspace mColorSpace;
    int dstWidth;
    int dstHeight;
    int mViDropFrameNum;   // for offline
    int mVencDropFrameNum; // for online
    int mViBufferNum;
    int mSaturationChange;

    int mVippDev;
    int mVeChn;

    int mVideoEncoderFmt;
    int mVideoFrameRate;
    int mVideoBitRate;
    int mMaxFileDuration;
    int mTestDuration;

    int mVeFreq; // MHz

    int mProductMode;
    int mSensorType;
    int mKeyFrameInterval;
    int mRcMode;

    int mInitQp;
    int mMinIQp;
    int mMaxIQp;
    int mMinPQp;
    int mMaxPQp;
    int mEnMbQpLimit;

    int mMovingTh;
    int mQuality;
    int mPBitsCoef;
    int mIBitsCoef;

    int mGopMode;
    int mGopSize;
    int mAdvancedRef_Base;
    int mAdvancedRef_Enhance;
    int mAdvancedRef_RefBaseEn;
    int mEnableFastEnc;
    BOOL mbEnableSmart;
    int mSVCLayer;  //0, 2, 3, 4
    int mEncodeRotate;  //clockwise.
    int mEncUseProfile;

    BOOL mColor2Grey;
    s2DfilterParam m2DnrPara;
    s3DfilterParam m3DnrPara;
    int mRoiNum;
    int mRoiQp;
    BOOL mRoiBgFrameRateEnable;
    int mRoiBgFrameRateAttenuation;
    int mIntraRefreshBlockNum;
    int mOrlNum;
    BOOL mHorizonFlipFlag; // mirror

    int mVbvBufferSize;  //unit:Byte
    int mVbvThreshSize;  //unit:Byte

    /* crop params */
    BOOL mCropEnable;
    int mCropRectX;
    int mCropRectY;
    int mCropRectWidth;
    int mCropRectHeight;

    int mVuiTimingInfoPresentFlag;

    int mOnlineEnable;
    int mOnlineShareBufNum;

    int wdr_en;
    int mEnableGdc;
    eGdcWarpType mGdcWarpMode;
    char mGdcLdcProLutBin[MAX_FILE_PATH_LEN];
    char *mpGdcLdcProLutBinData;
    int mGdcLdcProLutBinDataLen;

    int mEncppEnable;
    int mIspAndVeLinkageEnable;

    int mSuperFrmMode;
    int mSuperMaxRencodeTimes;
    float mSuperMaxP2IFrameBitsRatio;
    int mSuperIFrmBitsThr;
    int mSuperPFrmBitsThr;

    VencTargetBitsClipParam mBitsClipParam;
    VencAeDiffParam mAeDiffParam;

    int EnIFrmMbRcMoveStatusEnable;
    int EnIFrmMbRcMoveStatus;

    int mBitsRatioEnable;
    VencIPTargetBitsRatio mBitsRatio;

    int mWeakTextureThEnable;
    float mWeakTextureTh;

    int mChromaQPOffsetEnable;
    int mChromaQPOffset;

    int mH264ConstraintFlagEnable;
    VencH264ConstraintFlag mH264ConstraintFlag;

    VencVe2IspD2DLimit mVe2IspD2DLimit;

    int mVeRefFrameLbcMode;
    int mVeRecRefBufReduceDisable;
}VI2VENC2MUXER_CONFIG_S;

typedef struct Ai2VAenc2Muxer_Config
{
    PAYLOAD_TYPE_E mConfCodecType;
    int mConfChnCnt;
    int mConfBitWidth;
    int mConfSampleRate;
    int mConfBitRate;
}AI2AENC2MUXER_CONFIG_S;

typedef struct 
{
    char strFilePath[MAX_FILE_PATH_LEN];
    struct list_head mList;
}FilePathNode;


typedef struct sample_av2muxer_s
{
    VI2VENC2MUXER_CONFIG_S mConfigPara;
    AI2AENC2MUXER_CONFIG_S aConfigPara;
    CMDLINEPARAM_S mCmdLinePara;

    char mDstDir[MAX_FILE_PATH_LEN];    //tail don't contain '/', e.g.,/mnt/extsd/sample_virvi2venc2muxer_Files
    char mFirstFileName[MAX_FILE_PATH_LEN];

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    RECSTATE_E mCurrentState;

    VI_ATTR_S mViAttr;
    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;
    
    MPP_CHN_S mAiChn;
    MPP_CHN_S mAEncChn;

    AUDIO_DEV mAIDevId;
    AI_CHN mAIChnId;
    AIO_ATTR_S mAioAttr;
    AENC_CHN mAEncChnId;
    AENC_CHN_ATTR_S mAEncAttr;

    MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_GRP mMuxGrp;

    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    int mMuxId[2];
    struct list_head mMuxerFileListArray[2];    //FilePathNode
    int mMuxerIdCounter;


    pthread_mutex_t mMuxChnListLock;
    struct list_head mMuxChnList;   //MUX_CHN_INFO_S

    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;

    int resetCameraCnt;
}SAMPLE_AV2MUXER_S;

typedef struct Vi2Venc2Muxer_MessageData
{
    SAMPLE_AV2MUXER_S *mpVi2Venc2MuxerData;
}Vi2Venc2Muxer_MessageData;

#endif //#define __SAMPLE_VI2VENC2MUXER_H__

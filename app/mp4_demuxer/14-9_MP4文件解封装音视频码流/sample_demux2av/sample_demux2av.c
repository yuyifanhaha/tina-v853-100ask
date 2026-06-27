//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux2AV"

#include <unistd.h>
#include <fcntl.h>
#include "plat_log.h"
//#include <utils/plat_log.h>
#include <time.h>

#include <mpi_demux.h>
#include <mpi_adec.h>
#include <mpi_ao.h>
#include <mpi_clock.h>

#include <ClockCompPortIndex.h>

#include "hwdisplay.h"

#include "sample_demux2av.h"
#include "sample_demux2av_common.h"

#define DMX2ADEC2AO_DEFAULT_SRC_FILE        "/mnt/extsd/SAMPLE_DEMUX2AV/test.mp4"
#define PCM_FILE_OUTPUT


static SAMPLE_DEMUX2AV *pDemux2AVData;

static ERRORTYPE InitDemux2Adec2AOData(void)
{
    pDemux2AVData = (SAMPLE_DEMUX2AV* )malloc(sizeof(SAMPLE_DEMUX2AV));
    if (pDemux2AVData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }

    memset(pDemux2AVData, 0, sizeof(SAMPLE_DEMUX2AV));

    pDemux2AVData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    //pDemux2AVData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pDemux2AVData->mConfigPara.mMaxVdecOutputWidth = 1920;
    pDemux2AVData->mConfigPara.mMaxVdecOutputHeight= 1080;

    pDemux2AVData->mDmxChn = MM_INVALID_CHN;
    pDemux2AVData->mVdecChn = MM_INVALID_CHN;
    pDemux2AVData->mAdecChn = MM_INVALID_CHN;
    pDemux2AVData->mAOChn = MM_INVALID_CHN;
    pDemux2AVData->mClockChn = MM_INVALID_CHN;

    pDemux2AVData->mVoDev = MM_INVALID_DEV;
    pDemux2AVData->mVoLayer = MM_INVALID_CHN;
    pDemux2AVData->mVoChn = MM_INVALID_CHN;
    pDemux2AVData->mUILayer = MM_INVALID_CHN;


    strcpy(pDemux2AVData->srcFile, DMX2ADEC2AO_DEFAULT_SRC_FILE);

    return SUCCESS;
}

static int parseCmdLine(SAMPLE_DEMUX2AV *pSampleData, int argc, char** argv)
{
    int ret = 0;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pSampleData->confFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pSampleData->confFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/SAMPLE_DEMUX2AV.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}


static ERRORTYPE loadConfigPara(SAMPLE_DEMUX2AV *pSampleData, const char *conf_path)
{
    int ret;
    pSampleData->seekTime = 0;
    strcpy(pSampleData->srcFile, "/mnt/extsd/test.mp4");

    pSampleData->mConfigPara.seekTime = 0;
    pSampleData->mConfigPara.mMaxVdecOutputWidth = 1920;
    pSampleData->mConfigPara.mMaxVdecOutputHeight= 1080;
    pSampleData->mConfigPara.mInitRotation = 0;
    pSampleData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;   //MM_PIXEL_FORMAT_YVU_PLANAR_420
    pSampleData->mConfigPara.mTestDuration = 0;
    pSampleData->mConfigPara.mDisplayX = 0;
    pSampleData->mConfigPara.mDisplayY = 0;
    pSampleData->mConfigPara.mDisplayWidth = 480;
    pSampleData->mConfigPara.mDisplayHeight = 640;

    pSampleData->mConfigPara.mbForceFramePackage = FALSE;

    if(conf_path)
    {
        char *ptr;

        ret = createConfParser(conf_path, &pSampleData->mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }



        ptr = (char*)GetConfParaString(&pSampleData->mConf, DMX2ADEC2AO_CFG_SRC_FILE_STR, NULL);
        strcpy(pSampleData->srcFile, ptr);

        pSampleData->seekTime = GetConfParaInt(&pSampleData->mConf, DMX2ADEC2AO_CFG_SEEK_POSITION, 0);
        pSampleData->mConfigPara.mTestDuration = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_TEST_DURATION, 0);
        pSampleData->mConfigPara.mDisplayX = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_DISPLAY_X, 0);
        pSampleData->mConfigPara.mDisplayY = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_DISPLAY_Y, 0);
        pSampleData->mConfigPara.mDisplayWidth = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_DISPLAY_WIDTH, 0);
        pSampleData->mConfigPara.mDisplayHeight = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_DISPLAY_HEIGHT, 0);
        pSampleData->mConfigPara.mVeFreq = GetConfParaInt(&pSampleData->mConf, DMX2VDEC2VO_CFG_VE_FREQ, 0);

        destroyConfParser(&pSampleData->mConf);
    }
    return SUCCESS;
}

ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_DEMUX2AV *pSampleData = (SAMPLE_DEMUX2AV *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("demux get EOF flag");
            AW_MPI_ADEC_SetStreamEof(pSampleData->mAdecChn, 1);
            AW_MPI_VDEC_SetStreamEof(pSampleData->mVdecChn, 1);
            break;
        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_ADEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("adec get EOF flag");
            AW_MPI_AO_SetStreamEof(pSampleData->mAODevId, pSampleData->mAOChn, TRUE, TRUE);
            break;
        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_AO)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("ao get EOF flag");
            cdx_sem_up(&pSampleData->mSemExit);
            break;
        default:
            break;
        }
    }
        else if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vdec to the end of file");
            if (pSampleData->mVoChn >= 0)
            {
                AW_MPI_VO_SetStreamEof(pSampleData->mVoLayer, pSampleData->mVoChn, 1);
            }
            //cdx_sem_up(&pSampleData->mSemExit);
            break;

        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VOU)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vo to the end of file");
            cdx_sem_up(&pSampleData->mSemExit);
            break;

        case MPP_EVENT_RENDERING_START:
            alogd("vo start to rendering");
            break;

        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createClockChn(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    pSampleData->mClockChn = 0;
    //pSampleData->mClockChnAttr.nWaitMask = 0;
    //pSampleData->mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    while (pSampleData->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(pSampleData->mClockChn, &pSampleData->mClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pSampleData->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pSampleData->mClockChn);
            pSampleData->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pSampleData->mClockChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pSampleData->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        return FAILURE;
    }
    else
    {
        return SUCCESS;
    }
}

static ERRORTYPE ConfigDmxChnAttr(SAMPLE_DEMUX2AV *pSampleData)
{
    pSampleData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pSampleData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pSampleData->mDmxChnAttr.mSourceUrl = NULL;
    pSampleData->mDmxChnAttr.mFd = pSampleData->srcFd;
    alogd("fd=%d", pSampleData->mDmxChnAttr.mFd);
    pSampleData->mDmxChnAttr.mDemuxDisableTrack = pSampleData->mTrackDisableFlag = DEMUX_DISABLE_SUBTITLE_TRACK ;

    return SUCCESS;
}

static ERRORTYPE createDemuxChn(SAMPLE_DEMUX2AV *pSampleData)
{
    int ret;
    BOOL nSuccessFlag = FALSE;

    ConfigDmxChnAttr(pSampleData);

    pSampleData->mDmxChn = 0;
    while (pSampleData->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pSampleData->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", pSampleData->mDmxChn);
            pSampleData->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", pSampleData->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pSampleData->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pSampleData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pSampleData->mDmxChn, &cbInfo);
    }
    return SUCCESS;
}

static ERRORTYPE ConfigAdecChnAttr(SAMPLE_DEMUX2AV *pSampleData, DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pSampleData->mAdecChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
    pSampleData->mAdecChnAttr.mType = pStreamInfo->mCodecType;
    pSampleData->mAdecChnAttr.sampleRate = pStreamInfo->mSampleRate;
    pSampleData->mAdecChnAttr.channels = pStreamInfo->mChannelNum;
    pSampleData->mAdecChnAttr.bitsPerSample = pStreamInfo->mBitsPerSample;

    return SUCCESS;
}

static ERRORTYPE createAdecChn(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if (pSampleData->mDemuxMediaInfo.mAudioNum>0 && !(pSampleData->mDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_AUDIO_TRACK))
    {
        alogd("try to get adec chn");
        DEMUX_AUDIO_STREAM_INFO_S *pAudioStreamInfo =
            &(pSampleData->mDemuxMediaInfo.mAudioStreamInfo[pSampleData->mDemuxMediaInfo.mAudioIndex]);
        ConfigAdecChnAttr(pSampleData, pAudioStreamInfo);
        pSampleData->mAdecChn = 0;
        while (pSampleData->mAdecChn < ADEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_ADEC_CreateChn(pSampleData->mAdecChn, &pSampleData->mAdecChnAttr);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create adec channel[%d] success!", pSampleData->mAdecChn);
                break;
            }
            else if (ERR_ADEC_EXIST == ret)
            {
                alogd("adec channel[%d] is exist, find next!", pSampleData->mAdecChn);
                pSampleData->mAdecChn++;
            }
            else
            {
                alogd("create adec channel[%d] ret[0x%x]!", pSampleData->mAdecChn, ret);
                break;
            }
        }

        if (FALSE == nSuccessFlag)
        {
            pSampleData->mAdecChn = MM_INVALID_CHN;
            aloge("fatal error! create adec channel fail!");
            return FAILURE;
        }
        else
        {
            alogd("add call back");
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pSampleData;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_ADEC_RegisterCallback(pSampleData->mAdecChn, &cbInfo);
            return SUCCESS;
        }
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE ConfigVdecChnAttr(SAMPLE_DEMUX2AV *pDemux2Vdec2VoData, DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pDemux2Vdec2VoData->mVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pDemux2Vdec2VoData->mVdecChnAttr.mPicWidth = pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputWidth;
    pDemux2Vdec2VoData->mVdecChnAttr.mPicHeight = pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputHeight;
    pDemux2Vdec2VoData->mVdecChnAttr.mInitRotation = pDemux2Vdec2VoData->mConfigPara.mInitRotation;
    pDemux2Vdec2VoData->mVdecChnAttr.mOutputPixelFormat = pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat;
    pDemux2Vdec2VoData->mVdecChnAttr.mType = pStreamInfo->mCodecType;
    pDemux2Vdec2VoData->mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 0; //1
    pDemux2Vdec2VoData->mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;

    return SUCCESS;
}

static ERRORTYPE createVdecChn(SAMPLE_DEMUX2AV *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if ((pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoNum >0 ) && !(pDemux2Vdec2VoData->mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK))
    {
        ConfigVdecChnAttr(pDemux2Vdec2VoData, &pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoStreamInfo[pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoIndex]);
        pDemux2Vdec2VoData->mVdecChn = 0;
        while (pDemux2Vdec2VoData->mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(pDemux2Vdec2VoData->mVdecChn, &pDemux2Vdec2VoData->mVdecChnAttr);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create vdec channel[%d] success!", pDemux2Vdec2VoData->mVdecChn);
                break;
            }
            else if (ERR_VDEC_EXIST == ret)
            {
                alogd("vdec channel[%d] is exist, find next!", pDemux2Vdec2VoData->mVdecChn);
                pDemux2Vdec2VoData->mVdecChn++;
            }
            else
            {
                alogd("create vdec channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mVdecChn, ret);
                break;
            }
        }

        if (FALSE == nSuccessFlag)
        {
            pDemux2Vdec2VoData->mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel fail!");
            return FAILURE;
        }
        else
        {
            if (pDemux2Vdec2VoData->mConfigPara.mVeFreq)
            {
                alogd("vdec set ve freq %d MHz", pDemux2Vdec2VoData->mConfigPara.mVeFreq);
                AW_MPI_VDEC_SetVEFreq(pDemux2Vdec2VoData->mVdecChn, pDemux2Vdec2VoData->mConfigPara.mVeFreq);
            }
            alogd("add call back");
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pDemux2Vdec2VoData;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_VDEC_RegisterCallback(pDemux2Vdec2VoData->mVdecChn, &cbInfo);
            AW_MPI_VDEC_ForceFramePackage(pDemux2Vdec2VoData->mVdecChn, pDemux2Vdec2VoData->mConfigPara.mbForceFramePackage);
            return SUCCESS;
        }
    }
    else
    {
        return FAILURE;
    }
}

void config_AIO_ATTR_S_by_DEMUX_AUDIO_STREAM_INFO_S(AIO_ATTR_S *pAioAttr, DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(pAioAttr, 0, sizeof(AIO_ATTR_S));
    pAioAttr->mChnCnt = pStreamInfo->mChannelNum;
    pAioAttr->enBitwidth = (AUDIO_BIT_WIDTH_E)(pStreamInfo->mBitsPerSample/8 - 1);
    pAioAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)pStreamInfo->mSampleRate;
}

static ERRORTYPE createAOChn(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;

    pSampleData->mAODevId = 0;
    DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo =
        &(pSampleData->mDemuxMediaInfo.mAudioStreamInfo[pSampleData->mDemuxMediaInfo.mAudioIndex]);
    config_AIO_ATTR_S_by_DEMUX_AUDIO_STREAM_INFO_S(&pSampleData->mAioAttr, pStreamInfo);
    //AW_MPI_AO_SetPubAttr(pSampleData->mAODevId, &pSampleData->mAioAttr);

    //enable audio_hw_ao
    //ret = AW_MPI_AO_Enable(pSampleData->mAODevId);

    //create ao channel
    BOOL bSuccessFlag = FALSE;
    pSampleData->mAOChn = 0;
    while(pSampleData->mAOChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(pSampleData->mAODevId, pSampleData->mAOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", pSampleData->mAOChn);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", pSampleData->mAOChn);
            pSampleData->mAOChn++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", pSampleData->mAOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pSampleData->mAOChn = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
        ret = FAILURE;
    }
    else
    {
        alogd("add call back");
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pSampleData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AO_RegisterCallback(pSampleData->mAODevId, pSampleData->mAOChn, &cbInfo);
    }

    return ret;
}

static ERRORTYPE createVoChn(SAMPLE_DEMUX2AV *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    pDemux2Vdec2VoData->mVoDev = 0;
    pDemux2Vdec2VoData->mUILayer = HLAY(2, 0);

    AW_MPI_VO_Enable(pDemux2Vdec2VoData->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pDemux2Vdec2VoData->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pDemux2Vdec2VoData->mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pDemux2Vdec2VoData->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pDemux2Vdec2VoData->mVoDev, &spPubAttr);

   //enable vo layer
    int hlay0 = 0;
    while (hlay0 < VO_MAX_LAYER_NUM)
    {
        if (SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0++;
    }

    if (hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
        pDemux2Vdec2VoData->mVoLayer = MM_INVALID_DEV;
        AW_MPI_VO_RemoveOutsideVideoLayer(pDemux2Vdec2VoData->mUILayer);
        AW_MPI_VO_Disable(pDemux2Vdec2VoData->mVoDev);
        return FAILURE;
    }

    pDemux2Vdec2VoData->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pDemux2Vdec2VoData->mVoLayer, &pDemux2Vdec2VoData->mVoLayerAttr);

    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.X = pDemux2Vdec2VoData->mConfigPara.mDisplayX;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Y = pDemux2Vdec2VoData->mConfigPara.mDisplayY;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Width = pDemux2Vdec2VoData->mConfigPara.mDisplayWidth;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Height = pDemux2Vdec2VoData->mConfigPara.mDisplayHeight;
    pDemux2Vdec2VoData->mVoLayerAttr.enPixFormat = pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat;
    AW_MPI_VO_SetVideoLayerAttr(pDemux2Vdec2VoData->mVoLayer, &pDemux2Vdec2VoData->mVoLayerAttr);


    pDemux2Vdec2VoData->mVoChn = 0;
    while (pDemux2Vdec2VoData->mVoChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pDemux2Vdec2VoData->mVoChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", pDemux2Vdec2VoData->mVoChn);
            pDemux2Vdec2VoData->mVoChn++;
        }
        else
        {
            alogd("create vo channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mVoChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pDemux2Vdec2VoData->mVoChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pDemux2Vdec2VoData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VO_RegisterCallback(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, 2);
        return SUCCESS;
    }
}

static ERRORTYPE prepare(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;

    if (createDemuxChn(pSampleData) != SUCCESS)
    {
        aloge("create demuxchn fail");
        ret = -1;
    }

    if (AW_MPI_DEMUX_GetMediaInfo(pSampleData->mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return FAILURE;
    }

    if ((DemuxMediaInfo.mVideoNum >0 && DemuxMediaInfo.mVideoIndex>=DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum >0 && DemuxMediaInfo.mAudioIndex>=DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum >0 && DemuxMediaInfo.mSubtitleIndex>=DemuxMediaInfo.mSubtitleNum))
    {
        aloge("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
               DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex,
               DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex,
               DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return FAILURE;
    }

    memcpy(&pSampleData->mDemuxMediaInfo, &DemuxMediaInfo, sizeof(DEMUX_MEDIA_INFO_S));

    if (DemuxMediaInfo.mSubtitleNum > 0)
    {
        AW_MPI_DEMUX_GetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
        pSampleData->mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;
        AW_MPI_DEMUX_SetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
    }

    if (DemuxMediaInfo.mVideoNum > 0)
    {
        ret = createVdecChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & vdec");
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pSampleData->mVdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        }
        else
        {
            alogd("create vdec chn fail");
            return FAILURE;
        }

        ret = createVoChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind vdec & vo");
            MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pSampleData->mVdecChn};
            MPP_CHN_S VoChn = {MOD_ID_VOU, pSampleData->mVoLayer, pSampleData->mVoChn};

            AW_MPI_SYS_Bind(&VdecChn, &VoChn);
        }
        else
        {
            alogd("create vo chn fail");
            return FAILURE;
        }

    }

    if (DemuxMediaInfo.mAudioNum > 0)
    {
        ret = createAdecChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & adec");
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            MPP_CHN_S AdecChn = {MOD_ID_ADEC, 0, pSampleData->mAdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &AdecChn);
        }
        if (ret == SUCCESS)
        {
            alogd("bind clock & demux");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pSampleData->mClockChn};
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        }

        ret = createAOChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind adec & ao");
            MPP_CHN_S AdecChn = {MOD_ID_ADEC, 0, pSampleData->mAdecChn};
            MPP_CHN_S AoChn = {MOD_ID_AO, 0, pSampleData->mAOChn};
            AW_MPI_SYS_Bind(&AdecChn, &AoChn);
        }
        return ret;
    }

    if(ret == SUCCESS)
    {
        pSampleData->mClockChnAttr.nWaitMask = 0;
        pSampleData->mClockChnAttr.nWaitMask |=  (1 << CLOCK_PORT_INDEX_VIDEO | 1 << CLOCK_PORT_INDEX_AUDIO); //becareful this is too important!!!

        ret = createClockChn(pSampleData);
        
        alogd("bind clock & demux");
        MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pSampleData->mClockChn};
        MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
        MPP_CHN_S VoChn = {MOD_ID_VOU, pSampleData->mVoLayer, pSampleData->mVoChn};
        MPP_CHN_S AoChn = {MOD_ID_AO, 0, pSampleData->mAOChn};
        
        AW_MPI_SYS_Bind(&ClockChn, &AoChn);
        AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        AW_MPI_SYS_Bind(&ClockChn, &VoChn);
    }

    return ret;
}

static ERRORTYPE start(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;

    alogd("start stream");
    AW_MPI_CLOCK_Start(pSampleData->mClockChn);

    if (pSampleData->mAOChn >= 0)
    {
        AW_MPI_AO_StartChn(pSampleData->mAODevId, pSampleData->mAOChn);
    }

    if ((pSampleData->mVoLayer >= 0) && (pSampleData->mVoChn >= 0))
    {
        AW_MPI_VO_StartChn(pSampleData->mVoLayer, pSampleData->mVoChn); //开启显示通道
    }

    if (pSampleData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StartRecvStream(pSampleData->mAdecChn);
    }

    if (pSampleData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StartRecvStream(pSampleData->mVdecChn);//开启解码通道
    }

    ret = AW_MPI_DEMUX_Start(pSampleData->mDmxChn);

    return ret;
}

static ERRORTYPE stop(SAMPLE_DEMUX2AV *pSampleData)
{
    ERRORTYPE ret;
    ret = AW_MPI_DEMUX_Stop(pSampleData->mDmxChn);
    if (pSampleData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StopRecvStream(pSampleData->mAdecChn);
    }
    if (pSampleData->mAOChn >= 0)
    {
        AW_MPI_AO_StopChn(pSampleData->mAODevId, pSampleData->mAOChn);
    }
    if ((pSampleData->mVoLayer >=0) && (pSampleData->mVoChn >= 0))
    {
        alogd("stop vo chn");
        AW_MPI_VO_StopChn(pSampleData->mVoLayer, pSampleData->mVoChn);
    }
    if (pSampleData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(pSampleData->mVdecChn);
    }
    AW_MPI_CLOCK_Stop(pSampleData->mClockChn);

    return ret;
}

static ERRORTYPE seekto(SAMPLE_DEMUX2AV *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;

    alogd("seek to");
    ret = AW_MPI_DEMUX_Seek(pDemux2Vdec2VoData->mDmxChn, pDemux2Vdec2VoData->mConfigPara.seekTime);

    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_SetStreamEof(pDemux2Vdec2VoData->mVdecChn, 0); //重置视频解码器EOF标志
    }
    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_SetStreamEof(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, 0);//重置视频输出EOF标志
    }


    if (pDemux2Vdec2VoData->mVdecChn >= 0)//视频解码器执行跳转
    {
        AW_MPI_VDEC_Seek(pDemux2Vdec2VoData->mVdecChn);
    }
    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_Seek(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn); ////视频输出执行跳转
    }

    return ret;
}

static ERRORTYPE sendMsg(message_queue_t *msgQue, int cmd, int param0)
{
    ERRORTYPE eError = SUCCESS;
    message_t msg = {0};

    if (msgQue == NULL)
    {
        eError = FAILURE;
        return eError;
    }

    msg.command = cmd;
    msg.para0 = param0;
    put_message(msgQue, &msg);
    return eError;
}

static ERRORTYPE constructComp(void)
{
    pDemux2AVData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemux2AVData->mSysConf);
    AW_MPI_SYS_Init();
    return SUCCESS;
}


int main(int argc, char** argv)
{
    int ret = 0;

    if (InitDemux2Adec2AOData() != SUCCESS)
    {
        ret = -1;
        goto exit;
    }

    if (parseCmdLine(pDemux2AVData, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
        ret = -1;
    }
    char *pConfFilePath = NULL;
    if(strlen(pDemux2AVData->confFilePath) > 0)
    {
        pConfFilePath =  pDemux2AVData->confFilePath;
    }
    if (loadConfigPara(pDemux2AVData, pConfFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        ret = -1;
    }

    pDemux2AVData->srcFd = open(pDemux2AVData->srcFile, O_RDONLY);
    if (pDemux2AVData->srcFd < 0)
    {
        aloge("ERROR: cannot open mp4 src file");
        ret = -1;
    }
    else
    {
        alogd("mp4 src file fd=%d", pDemux2AVData->srcFd);
    }

    cdx_sem_init(&pDemux2AVData->mSemExit, 0);

    if (constructComp() != SUCCESS)
    {
        aloge("ERROR: demux construct fail");
        ret = -1;
    }

    if (prepare(pDemux2AVData) != SUCCESS)
    {
        aloge("prepare failed");
        ret = -1;
    }

    if (pDemux2AVData->mConfigPara.seekTime > 0)
    {
        seekto(pDemux2AVData);
    }

    if (start(pDemux2AVData) != SUCCESS)
    {
        aloge("start play fail");
        ret = -1;
    }

    AW_MPI_AO_SetDevVolume(pDemux2AVData->mAODevId, 90);

    alogd("wait until app receive EOF from DMX...");
    if (pDemux2AVData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pDemux2AVData->mSemExit, pDemux2AVData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pDemux2AVData->mSemExit);
    }
    alogd("play finished!");

    if (stop(pDemux2AVData) != SUCCESS)
    {
        alogw("stop fail");
        ret = -1;
    }

    if (pDemux2AVData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(pDemux2AVData->mClockChn);
    }
    if (pDemux2AVData->mVoLayer >= 0)
    {
        if (pDemux2AVData->mVoChn >= 0)
        {
            AW_MPI_VO_DestroyChn(pDemux2AVData->mVoLayer, pDemux2AVData->mVoChn);
            pDemux2AVData->mVoChn = MM_INVALID_CHN;
        }

        AW_MPI_VO_DisableVideoLayer(pDemux2AVData->mVoLayer);
        //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
        usleep(50*1000);
        pDemux2AVData->mVoLayer = MM_INVALID_CHN;
        AW_MPI_VO_RemoveOutsideVideoLayer(pDemux2AVData->mUILayer);
        
        AW_MPI_VO_Disable(pDemux2AVData->mVoDev);
        pDemux2AVData->mVoDev = MM_INVALID_DEV;
    }
    if (pDemux2AVData->mVdecChn >= 0)
    {
        int ret = AW_MPI_VDEC_DestroyChn(pDemux2AVData->mVdecChn);
        alogd("ret = %d", ret);
    }
    if (pDemux2AVData->mAOChn >= 0)
    {
        AW_MPI_AO_DestroyChn(pDemux2AVData->mAODevId, pDemux2AVData->mAOChn);
    }
    if (pDemux2AVData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_DestroyChn(pDemux2AVData->mAdecChn);
    }
    AW_MPI_DEMUX_DestroyChn(pDemux2AVData->mDmxChn);


   //exit mpp system
    AW_MPI_SYS_Exit();
    cdx_sem_deinit(&pDemux2AVData->mSemExit);
    close(pDemux2AVData->srcFd);
    free(pDemux2AVData);
    pDemux2AVData = NULL;

exit:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}

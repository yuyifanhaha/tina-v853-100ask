/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_ai2aenc2muxer.c
  Version       : V1.0
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/09/08
  Last Modified :
  Description   : test code for ai & aenc & muxer
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleAI2AEnc2Muxer"
#include "plat_log.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#include "mm_comm_sys.h"
#include "mm_comm_aio.h"
#include "mm_comm_aenc.h"
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <aenc_sw_lib.h>

#include "sample_ai2aenc2muxer.h"

// Default Params definition
#define DEFAULT_CONF_FILE_PATH      "/mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf"
#define DEFAULT_DST_FILE_PATH       "/mnt/extsd/test.aac"
#define DEFAULT_FILE_FORMAT         MEDIA_FILE_FORMAT_AAC
#define DEFAULT_AUDIO_ENCODE_TYPE   PT_AAC
#define DEFAULT_CAPTURE_DURATION    (10)   //Unit:Second
#define DEFAULT_CHANNEL_COUNT       (1)
#define DEFAULT_BIT_WIDTH           (16)
#define DEFAULT_SAMPLE_RATE         (8000)
#define DEFAULT_BITRATE             (16000)

static int parseCmdLine(SAMPLE_AI2AENC2MUXER_S *pSampleData, int argc, char** argv)
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

              strncpy(pSampleData->confFilePath, *argv, MAX_FILE_PATH_LEN);
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            alogd("CmdLine param: -path /mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_AI2AENC2MUXER_S *pSampleData)
{
    int ret;
    char *ptr;

    char *pConfFilePath;
	// Ensure config file valid
    if (!strlen(pSampleData->confFilePath))
    {
        //alogw("use dafault confFile [%s]", DEFAULT_CONF_FILE_PATH);
        //strncpy(pSampleData->confFilePath, DEFAULT_CONF_FILE_PATH, MAX_FILE_PATH_LEN);
        pConfFilePath = NULL;
    }
    else
    {
        pConfFilePath = pSampleData->confFilePath;
    }

    strncpy(pSampleData->mConfDstFile, DEFAULT_DST_FILE_PATH, MAX_FILE_PATH_LEN);
    pSampleData->mConfFileFormat = DEFAULT_FILE_FORMAT;
    pSampleData->mConfCodecType = DEFAULT_AUDIO_ENCODE_TYPE;
    pSampleData->mConfCapDuration = DEFAULT_CAPTURE_DURATION;
    pSampleData->mConfChnCnt      = DEFAULT_CHANNEL_COUNT;
    pSampleData->mConfBitWidth    = DEFAULT_BIT_WIDTH;
    pSampleData->mConfSampleRate  = DEFAULT_SAMPLE_RATE;
    pSampleData->mConfBitRate   = DEFAULT_BITRATE;
    pSampleData->mConfAISaveFileFlag = false;
        
    if(pConfFilePath)
    {
		// Load config file
        CONFPARSER_S cfg;
        ret = createConfParser(pSampleData->confFilePath, &cfg);
        if (ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }
		
		// Read params
        ptr = (char*)GetConfParaString(&cfg, DST_FILE_PATH, NULL);   //read dest file
        strncpy(pSampleData->mConfDstFile, ptr, MAX_FILE_PATH_LEN);
        ptr = strrchr(pSampleData->mConfDstFile, '.') + 1;
        if (!strcmp(ptr, "aac"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_AAC;
        }
        else if (!strcmp(ptr, "mp3"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_MP3;
        }
        else if (!strcmp(ptr, "wav"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_WAV;
        }
        else if (!strcmp(ptr, "mp4"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else
        {
            alogw("Unknown audio file format[%s]! Set to default [aac]", ptr);
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_AAC;
        }

        ptr = (char*)GetConfParaString(&cfg, CODEC_TYPE, "aac");   //read dest file
        if (!strcmp(ptr, "aac"))
        {
            pSampleData->mConfCodecType = PT_AAC;
        }
        else if (!strcmp(ptr, "mp3"))
        {
            pSampleData->mConfCodecType = PT_MP3;
        }
        else if (!strcmp(ptr, "pcm"))
        {
            pSampleData->mConfCodecType = PT_PCM_AUDIO;
        }
        else if (!strcmp(ptr, "g711a"))
        {
            pSampleData->mConfCodecType = PT_G711A;
        }
        else if (!strcmp(ptr, "g711u"))
        {
            pSampleData->mConfCodecType = PT_G711U;
        }
        else
        {
            alogw("Unknown audio codec type[%s]! Set to default [aac]", ptr);
            pSampleData->mConfCodecType = PT_AAC;
        }
       
        pSampleData->mConfCapDuration = GetConfParaInt(&cfg, CAPTURE_DURATION, 0); // capture duration
		
        pSampleData->mConfChnCnt     = GetConfParaInt(&cfg, CHANNEL_COUNT, 0);	// audio params
        pSampleData->mConfBitWidth   = GetConfParaInt(&cfg, BIT_WIDTH, 0);
        pSampleData->mConfSampleRate = GetConfParaInt(&cfg, SAMPLE_RATE, 0);
        pSampleData->mConfBitRate = GetConfParaInt(&cfg, KEY_BITRATE, 0);
		
		// Release config file
        destroyConfParser(&cfg);
    }
	// show config
    alogd("config para: dst_file [%s], codec_type [%d]\n"
          "             cap_duration [%d], chn_cnt [%d], bit_width [%d], sample_rate [%d], bitRate [%d]",
        pSampleData->mConfDstFile, pSampleData->mConfCodecType,
        pSampleData->mConfCapDuration, pSampleData->mConfChnCnt, pSampleData->mConfBitWidth, pSampleData->mConfSampleRate, pSampleData->mConfBitRate);
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_AI2AENC2MUXER_S *pContext = (SAMPLE_AI2AENC2MUXER_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return SUCCESS;
}

void configAioAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AIO_ATTR_S *pAttr = &ctx->mAioAttr;

    pAttr->mChnCnt    = ctx->mConfChnCnt;
    pAttr->enBitwidth   = map_BitWidth_to_AUDIO_BIT_WIDTH_E(ctx->mConfBitWidth);
    pAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(ctx->mConfSampleRate);
}

void configAEncAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AENC_CHN_ATTR_S *pAttr = &ctx->mAEncAttr;
    pAttr->AeAttr.Type = ctx->mConfCodecType; //编码器类型
    pAttr->AeAttr.channels = ctx->mConfChnCnt;//通道数
    pAttr->AeAttr.bitsPerSample = ctx->mConfBitWidth;//位宽
    pAttr->AeAttr.sampleRate = ctx->mConfSampleRate;//采样率
    pAttr->AeAttr.bitRate = ctx->mConfBitRate;//比特率
    pAttr->AeAttr.attachAACHeader = 0; //aacMuxer will add adts header, so aac encoder need not attach aac header.设置 attachAACHeader 为0，表示AAC编码器不需要附加AAC头部，因为AAC复用器会添加ADTS头部。
    pAttr->AeAttr.mInBufSize = 0;//设置输入缓冲区大小
    pAttr->AeAttr.mOutBufCnt = 0;//设置输出缓冲区计数
}

static ERRORTYPE configMuxGrpAttr(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    memset(&pContext->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));

    pContext->mMuxGrpAttr.mChannels = pContext->mConfChnCnt;
    pContext->mMuxGrpAttr.mBitsPerSample = pContext->mConfBitWidth;
    pContext->mMuxGrpAttr.mSamplesPerFrame = MAXDECODESAMPLE;
    pContext->mMuxGrpAttr.mSampleRate = pContext->mConfSampleRate;
    pContext->mMuxGrpAttr.mAudioEncodeType = pContext->mConfCodecType;
    return SUCCESS;
}

static ERRORTYPE configMuxChnAttr(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    memset(&pContext->mMuxChnAttr, 0, sizeof(pContext->mMuxChnAttr));
    pContext->mMuxChnAttr.mMuxerId = pContext->mMuxerIdCounter++;
    pContext->mMuxChnAttr.mMediaFileFormat = pContext->mConfFileFormat;//封装文件格式
    pContext->mMuxChnAttr.mMaxFileDuration = 0;
    pContext->mMuxChnAttr.mMaxFileSizeBytes = 0;
    pContext->mMuxChnAttr.mFallocateLen = 0;
    pContext->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pContext->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pContext->mMuxChnAttr.mSimpleCacheSize = 64*1024; //缓存大小
    pContext->mMuxChnAttr.mAddRepairInfo = 0;
    pContext->mMuxChnAttr.mMaxFrmsTagInterval = 0;
    return SUCCESS;
}
//创建音频输入（AI）通道
static ERRORTYPE createAIChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    //enable audio_hw_ai 启用音频硬件
    AW_MPI_AI_SetPubAttr(ctx->mAIDevId, &ctx->mAioAttr);
    AW_MPI_AI_Enable(ctx->mAIDevId);

    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAIChnId < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(ctx->mAIDevId, ctx->mAIChnId);//创建AI输入通道
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", ctx->mAIChnId);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", ctx->mAIChnId);
            ctx->mAIChnId++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", ctx->mAIChnId, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        ctx->mAIChnId = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAiChn.mModId = MOD_ID_AI;
        ctx->mAiChn.mDevId = ctx->mAIDevId;
        ctx->mAiChn.mChnId = ctx->mAIChnId;
    }

    return ret;
}

static ERRORTYPE createAEncChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAEncChnId < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(ctx->mAEncChnId, &ctx->mAEncAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", ctx->mAEncChnId);
            break;
        }
        else if (ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", ctx->mAEncChnId);
            ctx->mAEncChnId++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", ctx->mAEncChnId, ret);
            ctx->mAEncChnId++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        ctx->mAEncChnId = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAEncChn.mModId = MOD_ID_AENC;
        ctx->mAEncChn.mDevId = 0;
        ctx->mAEncChn.mChnId = ctx->mAEncChnId;
    }

    return ret;
}

static ERRORTYPE createMuxGrp(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pContext);//配置复用器组属性
    pContext->mMuxGrpId = 0;
    while (pContext->mMuxGrpId < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pContext->mMuxGrpId, &pContext->mMuxGrpAttr);//创建复用器组
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pContext->mMuxGrpId);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux group[%d] is exist, find next!", pContext->mMuxGrpId);
            pContext->mMuxGrpId++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pContext->mMuxGrpId, ret);
            pContext->mMuxGrpId++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pContext->mMuxGrpId = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxGrpId, &cbInfo);//注册回调函数

        pContext->mMuxGrp.mModId = MOD_ID_MUX;
        pContext->mMuxGrp.mDevId = 0;
        pContext->mMuxGrp.mChnId = pContext->mMuxGrpId;
        return SUCCESS;
    }
}

static ERRORTYPE createMuxChn(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    ERRORTYPE ret = SUCCESS;
    BOOL bSuccessFlag = FALSE;
    configMuxChnAttr(pContext);//配置复用器通道的属性
    while (pContext->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrpId, pContext->mMuxChn, &pContext->mMuxChnAttr, pContext->mFdDst);//创建复用器通道
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrpId, pContext->mMuxChn, pContext->mMuxChnAttr.mMuxerId);
            break;
        }
        else if(ERR_MUX_EXIST == ret)
        {
            pContext->mMuxChn++;
            //break;
        }
        else
        {
            aloge("fatal error! create mux chn[%d] fail:0x%x", pContext->mMuxChn, ret);
            pContext->mMuxChn++;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pContext->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrpId);
    }

    return ret;
}

int main(int argc, char** argv)
{
    int ret = 0;
    SAMPLE_AI2AENC2MUXER_S nSampleContext;
    memset(&nSampleContext, 0, sizeof(SAMPLE_AI2AENC2MUXER_S));

    if (parseCmdLine(&nSampleContext, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
    }

    if (loadConfigPara(&nSampleContext) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto _END;
    }

    nSampleContext.mFdDst = open(nSampleContext.mConfDstFile, O_RDWR | O_CREAT, 0666);
    if (nSampleContext.mFdDst < 0)
    {
        aloge("cann't open dest file %s", nSampleContext.mConfDstFile);
        goto _END;
    }
    // init mpp system
    nSampleContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&nSampleContext.mSysConf);
    AW_MPI_SYS_Init();

    // config ai & aenc attr
    configAioAttr(&nSampleContext);
    configAEncAttr(&nSampleContext);

    // config ai & aenc chn id
    nSampleContext.mAIDevId = 0;
    nSampleContext.mAIChnId = 0;
    nSampleContext.mAEncChnId = 0;

    // create ai & aenc chn
    if (createAIChn(&nSampleContext) != SUCCESS)
    {
        aloge("create ai chn fail!");
        goto _END;
    }
    if (createAEncChn(&nSampleContext) != SUCCESS)
    {
        aloge("create aenc chn fail!");
        goto _END;
    }
    if (createMuxGrp(&nSampleContext) != SUCCESS)
    {
        aloge("create mux group fail");
        goto _END;
    }
    if (createMuxChn(&nSampleContext) != SUCCESS)
    {
        aloge("create mux channel fail");
        goto _END;
    }

    // test ai save file api 音频输入保存
    if(nSampleContext.mConfAISaveFileFlag)
    {
        strcpy(nSampleContext.mSaveFileInfo.mFilePath, "/mnt/extsd/");
        strcpy(nSampleContext.mSaveFileInfo.mFileName, "SampleAi2Aenc2Muxer_AiSaveFile.pcm");
        AW_MPI_AI_SaveFile(nSampleContext.mAIDevId, nSampleContext.mAIChnId, &nSampleContext.mSaveFileInfo);
    }

    // bind ai & aenc & muxer
    AW_MPI_SYS_Bind(&nSampleContext.mAiChn, &nSampleContext.mAEncChn);
    AW_MPI_SYS_Bind(&nSampleContext.mAEncChn, &nSampleContext.mMuxGrp);

	// set start time
    alogd("will capture for %d seconds, wait ...", nSampleContext.mConfCapDuration);
//    struct timeval tv;
//    long long val_begin, val_end;
//    gettimeofday(&tv, NULL);
//    val_begin = 1000000 * tv.tv_sec + tv.tv_usec;

    //start ai & aenc & muxer
    AW_MPI_AI_EnableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StartRecvPcm(nSampleContext.mAEncChnId);
    AW_MPI_MUX_StartGrp(nSampleContext.mMuxGrpId);

    //capturing
    sleep(nSampleContext.mConfCapDuration);

    // stop ai & aenc
    AW_MPI_AI_DisableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StopRecvPcm(nSampleContext.mAEncChnId);
    AW_MPI_MUX_StopGrp(nSampleContext.mMuxGrpId);

    // destruct ai & aenc & muxer
    AW_MPI_MUX_DestroyGrp(nSampleContext.mMuxGrpId);
    //AW_MPI_AENC_ResetChn(nSampleContext.mAEncChnId);
    AW_MPI_AENC_DestroyChn(nSampleContext.mAEncChnId);
    //AW_MPI_AI_ResetChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AI_DestroyChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    
    // exit mpp system
    AW_MPI_SYS_Exit();

_END:
    if(nSampleContext.mFdDst >= 0)
    {
        close(nSampleContext.mFdDst);
        nSampleContext.mFdDst = -1;
    }
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}

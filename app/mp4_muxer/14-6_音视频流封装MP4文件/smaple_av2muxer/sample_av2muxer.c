/******************************************************************************
  Copyright (C), 2001-2024, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2024/12/20
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleVirVi2Venc2Muxer"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include <mpi_vi_private.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <aenc_sw_lib.h>

#include "sample_av2muxer.h"
#include "sample_av2muxer_conf.h"

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
//#define DOUBLE_ENCODER_FILE_OUT
#define ISP_RUN (1)


static SAMPLE_AV2MUXER_S *gpAV2MuxerData;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpAV2MuxerData)
    {
        cdx_sem_up(&gpAV2MuxerData->mSemExit);
    }
}

static int setOutputFileSync(SAMPLE_AV2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxerId);
static int addOutputFormatAndOutputSink_l(SAMPLE_AV2MUXER_S *pContext, OUTSINKINFO_S *pSinkInfo);

static ERRORTYPE InitAV2MuxerData(SAMPLE_AV2MUXER_S *pContext)
{
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pContext, 0, sizeof(SAMPLE_AV2MUXER_S));
    pContext->mMuxGrp = MM_INVALID_CHN;
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;
    //pContext->mAiChn = MM_INVALID_CHN;
    //pContext->mAEncChn = MM_INVALID_CHN;

    int i=0;
    for (i = 0; i < 2; i++)
    {
        INIT_LIST_HEAD(&pContext->mMuxerFileListArray[i]); //初始化复用输出文件列表，用于保存复用通道输出文件
    }
    alogd("&pContext->mMuxerFileListArray[0][%p], &pContext->mMuxerFileListArray[1][%p]",
        &pContext->mMuxerFileListArray[0], &pContext->mMuxerFileListArray[1]);

    pContext->mCurrentState = REC_NOT_PREPARED;

    if (message_create(&pContext->mMsgQueue) < 0) //创建消息队列
    {
        aloge("message create fail!");
        return FAILURE;
    }

    return SUCCESS;
}


static eGdcWarpType parserGdcWarpMode(char *pStr)
{
    if (!strcmp(pStr, "LDC"))
    {
        return Gdc_Warp_LDC;
    }
    else if (!strcmp(pStr, "LDC_Pro"))
    {
        return Gdc_Warp_LDC_Pro;
    }
    else
    {
        aloge("unsupport gdc warp mode[%s]", pStr);
    }
    return -1;
}

static ERRORTYPE parseCmdLine(SAMPLE_AV2MUXER_S *pContext, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    if(argc <= 1)
    {
        alogd("use default config.");
        return SUCCESS;
    }
    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_av2muxer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}
static ERRORTYPE loadConfigPara(SAMPLE_AV2MUXER_S *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if (conf_path != NULL)
    {
        CONFPARSER_S mConf;
        memset(&mConf, 0, sizeof(CONFPARSER_S));
        ret = createConfParser(conf_path, &mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pContext->mConfigPara.mVippDev = GetConfParaInt(&mConf, CFG_VIPP_DEV_ID, 0);
        pContext->mConfigPara.mVeChn = GetConfParaInt(&mConf, CFG_VENC_CH_ID, 0);
        alogd("vippDev: %d, veChn: %d", pContext->mConfigPara.mVippDev, pContext->mConfigPara.mVeChn);

        pContext->mConfigPara.srcWidth = GetConfParaInt(&mConf, CFG_SRC_WIDTH, 0);
        pContext->mConfigPara.srcHeight = GetConfParaInt(&mConf, CFG_SRC_HEIGHT, 0);
        alogd("srcWidth: %d, srcHeight: %d", pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight);

        pContext->mConfigPara.mSrcFrameRate = GetConfParaInt(&mConf, CFG_SRC_FRAMERATE, 0);

        pContext->mConfigPara.dstWidth = GetConfParaInt(&mConf, CFG_DST_VIDEO_WIDTH, 0);
        pContext->mConfigPara.dstHeight = GetConfParaInt(&mConf, CFG_DST_VIDEO_HEIGHT, 0);
        alogd("dstWidth: %d, dstHeight: %d", pContext->mConfigPara.dstWidth, pContext->mConfigPara.dstHeight);

        ptr = (char *)GetConfParaString(&mConf, CFG_SRC_PIXFMT, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "nv21"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "nv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            }
            else
            {
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.srcPixFmt);
            }
        }

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR_SPACE, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "jpeg"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
            else if (!strcmp(ptr, "rec709"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
            }
            else if (!strcmp(ptr, "rec709_part_range"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
            }
            else
            {
                aloge("fatal error! wrong color space:%s", ptr);
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
        }

        alogd("srcPixFmt=%d, ColorSpace=%d", pContext->mConfigPara.srcPixFmt, pContext->mConfigPara.mColorSpace);

        pContext->mConfigPara.mSaturationChange = GetConfParaInt(&mConf, CFG_SATURATION_CHANGE, 0);
        alogd("SaturationChange=%d", pContext->mConfigPara.mSaturationChange);
        
        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstVideoFile, ptr);
        }

        pContext->mConfigPara.mbAddRepairInfo = GetConfParaInt(&mConf, CFG_ADD_REPAIR_INFO, 0);
        pContext->mConfigPara.mMaxFrmsTagInterval = GetConfParaInt(&mConf, CFG_FRMSTAG_BACKUP_INTERVAL, 0);
        pContext->mConfigPara.mDstFileMaxCnt = GetConfParaInt(&mConf, CFG_DST_FILE_MAX_CNT, 0);
        pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);
        pContext->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION, 0);

        pContext->mConfigPara.mProductMode = GetConfParaInt(&mConf, CFG_PRODUCT_MODE, 0);
        pContext->mConfigPara.mSensorType = GetConfParaInt(&mConf, CFG_SENSOR_TYPE, 0);
        pContext->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, CFG_KEY_FRAME_INTERVAL, 0);
        pContext->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE, 0);
        pContext->mConfigPara.mInitQp = GetConfParaInt(&mConf, CFG_INIT_QP, 0);
        pContext->mConfigPara.mMinIQp = GetConfParaInt(&mConf, CFG_MIN_I_QP, 0);
        pContext->mConfigPara.mMaxIQp = GetConfParaInt(&mConf, CFG_MAX_I_QP, 0);
        pContext->mConfigPara.mMinPQp = GetConfParaInt(&mConf, CFG_MIN_P_QP, 0);
        pContext->mConfigPara.mMaxPQp = GetConfParaInt(&mConf, CFG_MAX_P_QP, 0);
        pContext->mConfigPara.mEnMbQpLimit = GetConfParaInt(&mConf, CFG_MB_QP_LIMIT, 0);
        pContext->mConfigPara.mMovingTh = GetConfParaInt(&mConf, CFG_MOVING_TH, 0);
        pContext->mConfigPara.mQuality = GetConfParaInt(&mConf, CFG_QUALITY, 0);
        pContext->mConfigPara.mPBitsCoef = GetConfParaInt(&mConf, CFG_P_BITS_COEF, 0);
        pContext->mConfigPara.mIBitsCoef = GetConfParaInt(&mConf, CFG_I_BITS_COEF, 0);
        pContext->mConfigPara.mGopMode = GetConfParaInt(&mConf, CFG_GOP_MODE, 0);
        pContext->mConfigPara.mGopSize = GetConfParaInt(&mConf, CFG_GOP_SIZE, 0);
        pContext->mConfigPara.mAdvancedRef_Base = GetConfParaInt(&mConf, CFG_AdvancedRef_Base, 0);
        pContext->mConfigPara.mAdvancedRef_Enhance = GetConfParaInt(&mConf, CFG_AdvancedRef_Enhance, 0);
        pContext->mConfigPara.mAdvancedRef_RefBaseEn = GetConfParaInt(&mConf, CFG_AdvancedRef_RefBaseEn, 0);
        pContext->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC, 0);
        pContext->mConfigPara.mbEnableSmart = GetConfParaBoolean(&mConf, CFG_ENABLE_SMART, 0);
        pContext->mConfigPara.mSVCLayer = GetConfParaInt(&mConf, CFG_SVC_LAYER, 0);
        pContext->mConfigPara.mEncodeRotate = GetConfParaInt(&mConf, CFG_ENCODE_ROTATE, 0);

        pContext->mConfigPara.m2DnrPara.enable_2d_filter = GetConfParaInt(&mConf, CFG_2DNR_EN, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_y = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_uv = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_C, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_y = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_uv = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_C, 0);

        pContext->mConfigPara.m3DnrPara.enable_3d_filter = GetConfParaInt(&mConf, CFG_3DNR_EN, 0);
        pContext->mConfigPara.m3DnrPara.adjust_pix_level_enable = GetConfParaInt(&mConf, CFG_3DNR_PIX_LEVEL_EN, 0);
        pContext->mConfigPara.m3DnrPara.smooth_filter_enable = GetConfParaInt(&mConf, CFG_3DNR_SMOOTH_EN, 0);
        pContext->mConfigPara.m3DnrPara.max_pix_diff_th = GetConfParaInt(&mConf, CFG_3DNR_PIX_DIFF_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mv_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MV_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mad_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MAD_TH, 0);
        pContext->mConfigPara.m3DnrPara.min_coef = GetConfParaInt(&mConf, CFG_3DNR_MIN_COEF, 0);
        pContext->mConfigPara.m3DnrPara.max_coef = GetConfParaInt(&mConf, CFG_3DNR_MAX_COEF, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "H.264"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H264;
                alogd("H.264");
            }
            else if (!strcmp(ptr, "H.265"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H265;
                alogd("H.265");
            }
            else if (!strcmp(ptr, "MJPEG"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
                alogd("MJPEG");
            }
            else
            {
                aloge("error conf encoder type");
            }
        }

        pContext->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);

        pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, CFG_DST_ENCODE_PROFILE, 0);

        alogd("vipp:%d, SrcFrameRate:%d, VideoFrameRate:%d, bitrate:%d, video_duration=%d, test_time=%d, profile=%d", pContext->mConfigPara.mVippDev,\
            pContext->mConfigPara.mSrcFrameRate, pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
            pContext->mConfigPara.mMaxFileDuration, pContext->mConfigPara.mTestDuration,\
            pContext->mConfigPara.mEncUseProfile);

        pContext->mConfigPara.mHorizonFlipFlag = GetConfParaInt(&mConf, CFG_MIRROR, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR2GREY, NULL);
        if (ptr != NULL)
        {
            if(!strcmp(ptr, "yes"))
            {
                pContext->mConfigPara.mColor2Grey = TRUE;
            }
            else
            {
                pContext->mConfigPara.mColor2Grey = FALSE;
            }
        }

        pContext->mConfigPara.mRoiNum = GetConfParaInt(&mConf, CFG_ROI_NUM, 0);
        pContext->mConfigPara.mRoiQp = GetConfParaInt(&mConf, CFG_ROI_QP, 0);
        pContext->mConfigPara.mRoiBgFrameRateEnable = GetConfParaBoolean(&mConf, CFG_ROI_BgFrameRateEnable, 0);
        pContext->mConfigPara.mRoiBgFrameRateAttenuation = GetConfParaInt(&mConf, CFG_ROI_BgFrameRateAttenuation, 0);
        pContext->mConfigPara.mIntraRefreshBlockNum = GetConfParaInt(&mConf, CFG_IntraRefresh_BlockNum, 0);
        pContext->mConfigPara.mOrlNum = GetConfParaInt(&mConf, CFG_ORL_NUM, 0);
        pContext->mConfigPara.mVbvBufferSize = GetConfParaInt(&mConf, CFG_vbvBufferSize, 0);
        pContext->mConfigPara.mVbvThreshSize = GetConfParaInt(&mConf, CFG_vbvThreshSize, 0);

        alogd("mirror:%d, Color2Grey:%d, RoiNum:%d, RoiQp:%d, RoiBgFrameRate Enable:%d Attenuation:%d, IntraRefreshBlockNum:%d, OrlNum:%d"
            "VbvBufferSize:%d, VbvThreshSize:%d",
            pContext->mConfigPara.mHorizonFlipFlag, pContext->mConfigPara.mColor2Grey,
            pContext->mConfigPara.mRoiNum, pContext->mConfigPara.mRoiQp,
            pContext->mConfigPara.mRoiBgFrameRateEnable, pContext->mConfigPara.mRoiBgFrameRateAttenuation,
            pContext->mConfigPara.mIntraRefreshBlockNum,
            pContext->mConfigPara.mOrlNum, pContext->mConfigPara.mVbvBufferSize,
            pContext->mConfigPara.mVbvThreshSize);

        pContext->mConfigPara.mCropEnable = GetConfParaInt(&mConf, CFG_CROP_ENABLE, 0);
        pContext->mConfigPara.mCropRectX = GetConfParaInt(&mConf, CFG_CROP_RECT_X, 0);
        pContext->mConfigPara.mCropRectY = GetConfParaInt(&mConf, CFG_CROP_RECT_Y, 0);
        pContext->mConfigPara.mCropRectWidth = GetConfParaInt(&mConf, CFG_CROP_RECT_WIDTH, 0);
        pContext->mConfigPara.mCropRectHeight = GetConfParaInt(&mConf, CFG_CROP_RECT_HEIGHT, 0);

        alogd("venc crop enable:%d, X:%d, Y:%d, Width:%d, Height:%d",
            pContext->mConfigPara.mCropEnable, pContext->mConfigPara.mCropRectX,
            pContext->mConfigPara.mCropRectY, pContext->mConfigPara.mCropRectWidth,
            pContext->mConfigPara.mCropRectHeight);

        pContext->mConfigPara.mVuiTimingInfoPresentFlag = GetConfParaInt(&mConf, CFG_vui_timing_info_present_flag, 0);
        alogd("VuiTimingInfoPresentFlag:%d", pContext->mConfigPara.mVuiTimingInfoPresentFlag);

        //pContext->mConfigPara.mVeFreq = GetConfParaInt(&mConf, CFG_Ve_Freq, 0);
        //alogd("mVeFreq:%d MHz", pContext->mConfigPara.mVeFreq);

        pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
        pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pContext->mConfigPara.mOnlineEnable,
            pContext->mConfigPara.mOnlineShareBufNum);

        if (0 == pContext->mConfigPara.mOnlineEnable)
        {
            // venc drop frame only support offline.
            pContext->mConfigPara.mViDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("ViDropFrameNum: %d", pContext->mConfigPara.mViDropFrameNum);
        }
        else
        {
            // venc drop frame support online and offline.
            pContext->mConfigPara.mVencDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("VencDropFrameNum: %d", pContext->mConfigPara.mVencDropFrameNum);
        }

        pContext->mConfigPara.wdr_en = GetConfParaInt(&mConf, CFG_WDR_EN, 0);
        alogd("wdr_en: %d", pContext->mConfigPara.wdr_en);

        pContext->mConfigPara.mEnableGdc = GetConfParaInt(&mConf, CFG_EnableGdc, 0);
        pContext->mConfigPara.mGdcWarpMode = parserGdcWarpMode((char *)GetConfParaString(&mConf, CFG_GDC_WARP_MODE, NULL));
        if (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode)
        {
            ptr = (char *)GetConfParaString(&mConf, CFG_GDC_LDC_Pro_Lut_Bin, NULL);
            strcpy(pContext->mConfigPara.mGdcLdcProLutBin, ptr);
        }
        alogd("EnableGdc: %d warp mode: %d gdc ldc pro lut bin[%s]", \
            pContext->mConfigPara.mEnableGdc, pContext->mConfigPara.mGdcWarpMode, pContext->mConfigPara.mGdcLdcProLutBin);

        pContext->mConfigPara.mEncppEnable = GetConfParaInt(&mConf, CFG_EncppEnable, 0);
        pContext->mConfigPara.mIspAndVeLinkageEnable = GetConfParaInt(&mConf, CFG_IspAndVeLinkageEnable, 0);
        alogd("EncppEnable: %d, IspAndVeLinkageEnable: %d", pContext->mConfigPara.mEncppEnable, pContext->mConfigPara.mIspAndVeLinkageEnable);

        pContext->mConfigPara.mSuperFrmMode = GetConfParaInt(&mConf, CFG_SuperFrmMode, 0);
        pContext->mConfigPara.mSuperMaxRencodeTimes = GetConfParaInt(&mConf, CFG_SuperMaxRencodeTimes, 0);
        pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio = (float)GetConfParaDouble(&mConf, CFG_SuperMaxP2IFrameBitsRatio, 0);
        pContext->mConfigPara.mSuperIFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperIFrmBitsThr, 0);
        pContext->mConfigPara.mSuperPFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperPFrmBitsThr, 0);
        alogd("SuperFrm Mode: %d, MaxRencodeTimes: %d, MaxP2IFrameBitsRatio: %.2f, IBitsThr: %d, PBitsThr: %d", pContext->mConfigPara.mSuperFrmMode,
            pContext->mConfigPara.mSuperMaxRencodeTimes, pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio,
            pContext->mConfigPara.mSuperIFrmBitsThr, pContext->mConfigPara.mSuperPFrmBitsThr);

        pContext->mConfigPara.mBitsClipParam.dis_default_para = GetConfParaBoolean(&mConf, CFG_BitsClipDisDefault, 0);
        pContext->mConfigPara.mBitsClipParam.mode = GetConfParaInt(&mConf, CFG_BitsClipMode, 0);
        pContext->mConfigPara.mBitsClipParam.en_gop_clip = GetConfParaInt(&mConf, CFG_BitsClipEnableGopClip, 0);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh0, 0);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh1, 1);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[2] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh2, 2);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef00, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef01, 0.2);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef10, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef11, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef20, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef21, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef30, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef31, 0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef40, 0.4);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef41, 0.7);

        alogd("BitsClipParam: %d %d %d {%.2f,%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}",
            pContext->mConfigPara.mBitsClipParam.dis_default_para,
            pContext->mConfigPara.mBitsClipParam.mode,
            pContext->mConfigPara.mBitsClipParam.en_gop_clip,
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[0],
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[1],
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[2],
            pContext->mConfigPara.mBitsClipParam.coef_th[0][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[0][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][1]);

        pContext->mConfigPara.mAeDiffParam.dis_default_para = GetConfParaInt(&mConf, CFG_AeDiffDisDefault, 0);
        pContext->mConfigPara.mAeDiffParam.diff_frames_th = GetConfParaInt(&mConf, CFG_AeDiffFramesTh, 40);
        pContext->mConfigPara.mAeDiffParam.stable_frames_th[0] = GetConfParaInt(&mConf, CFG_AeStableFramesTh0, 5);
        pContext->mConfigPara.mAeDiffParam.stable_frames_th[1] = GetConfParaInt(&mConf, CFG_AeStableFramesTh1, 100);
        pContext->mConfigPara.mAeDiffParam.diff_th[0] = (float)GetConfParaDouble(&mConf, CFG_AeDiffTh0, 0.1);
        pContext->mConfigPara.mAeDiffParam.diff_th[1] = (float)GetConfParaDouble(&mConf, CFG_AeDiffTh1, 0.6);
        pContext->mConfigPara.mAeDiffParam.small_diff_step = GetConfParaInt(&mConf, CFG_AeSmallDiffStep, 1);
        pContext->mConfigPara.mAeDiffParam.small_diff_qp[0] =  GetConfParaInt(&mConf, CFG_AeSmallDiffQp0, 20);
        pContext->mConfigPara.mAeDiffParam.small_diff_qp[1] =  GetConfParaInt(&mConf, CFG_AeSmallDiffQp1, 25);
        pContext->mConfigPara.mAeDiffParam.large_diff_qp[0] =  GetConfParaInt(&mConf, CFG_AeLargeDiffQp0, 35);
        pContext->mConfigPara.mAeDiffParam.large_diff_qp[1] =  GetConfParaInt(&mConf, CFG_AeLargeDiffQp1, 50);

        alogd("AeDiffParam: %d %d [%d,%d] [%.2f,%.2f], %d [%d,%d], [%d,%d]",
            pContext->mConfigPara.mAeDiffParam.dis_default_para,
            pContext->mConfigPara.mAeDiffParam.diff_frames_th,
            pContext->mConfigPara.mAeDiffParam.stable_frames_th[0],
            pContext->mConfigPara.mAeDiffParam.stable_frames_th[1],
            pContext->mConfigPara.mAeDiffParam.diff_th[0],
            pContext->mConfigPara.mAeDiffParam.diff_th[1],
            pContext->mConfigPara.mAeDiffParam.small_diff_step,
            pContext->mConfigPara.mAeDiffParam.small_diff_qp[0],
            pContext->mConfigPara.mAeDiffParam.small_diff_qp[1],
            pContext->mConfigPara.mAeDiffParam.large_diff_qp[0],
            pContext->mConfigPara.mAeDiffParam.large_diff_qp[1]);

        pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable = GetConfParaInt(&mConf, CFG_EnIFrmMbRcMoveStatusEnable, 0);
        pContext->mConfigPara.EnIFrmMbRcMoveStatus = GetConfParaInt(&mConf, CFG_EnIFrmMbRcMoveStatus, 3);
        alogd("EnIFrmMbRcMoveStatus: en %d, %d", pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable, pContext->mConfigPara.EnIFrmMbRcMoveStatus);

        pContext->mConfigPara.mBitsRatioEnable = GetConfParaInt(&mConf, CFG_IPTargetBitsRatioEnable, 0);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[0] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef0, 20);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[1] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef1, 17);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[2] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef2, 15);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[0] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef0, 1);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[1] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef1, 0.75);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[2] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef2, 0.5);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[3] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef3, 0.25);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[4] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef4, 0.25);

        alogd("BitsRatio: en %d, SceneCoef[%.2f,%.2f,%.2f] MoveCoef[%.2f,%.2f,%.2f,%.2f,%.2f]",
            pContext->mConfigPara.mBitsRatioEnable,
            pContext->mConfigPara.mBitsRatio.nSceneCoef[0],
            pContext->mConfigPara.mBitsRatio.nSceneCoef[1],
            pContext->mConfigPara.mBitsRatio.nSceneCoef[2],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[0],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[1],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[2],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[3],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[4]);

        pContext->mConfigPara.mWeakTextureThEnable = GetConfParaInt(&mConf, CFG_WeakTextureThEnable, 0);
        pContext->mConfigPara.mWeakTextureTh =  (float)GetConfParaDouble(&mConf, CFG_WeakTextureTh, 0);
        alogd("WeakTextureTh: %.2f", pContext->mConfigPara.mWeakTextureTh);

        pContext->mConfigPara.mChromaQPOffsetEnable = GetConfParaInt(&mConf, CFG_ChromaQPOffsetEnable, 0);
        pContext->mConfigPara.mChromaQPOffset = GetConfParaInt(&mConf, CFG_ChromaQPOffset, 0);

        pContext->mConfigPara.mH264ConstraintFlagEnable = GetConfParaInt(&mConf, CFG_H264ConstraintFlagEnable, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_0 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit0, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_1 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit1, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_2 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit2, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_3 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit3, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_4 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit4, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_5 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit5, 0);

        pContext->mConfigPara.mVe2IspD2DLimit.en_d2d_limit = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitEnable, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[0] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel0, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[1] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel1, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[2] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel2, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[3] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel3, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[4] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel4, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[5] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel5, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_VeRefFrameLbcMode, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_no_lossy"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_NO_LOSSY;
            }
            else
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT; /* default: 1.5x. */
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.mVeRefFrameLbcMode);
            }
        }
        pContext->mConfigPara.mVeRecRefBufReduceDisable = GetConfParaInt(&mConf, CFG_VeRecRefBufReduceDisable, 0);

        ptr = (char*)GetConfParaString(&mConf, AUDIO_CODEC_TYPE, "aac");   //read dest file
        if (!strcmp(ptr, "aac"))
        {
            pContext->aConfigPara.mConfCodecType = PT_AAC;
        }
        else if (!strcmp(ptr, "mp3"))
        {
            pContext->aConfigPara.mConfCodecType = PT_MP3;
        }
        else if (!strcmp(ptr, "pcm"))
        {
            pContext->aConfigPara.mConfCodecType = PT_PCM_AUDIO;
        }
        else if (!strcmp(ptr, "g711a"))
        {
            pContext->aConfigPara.mConfCodecType = PT_G711A;
        }
        else if (!strcmp(ptr, "g711u"))
        {
            pContext->aConfigPara.mConfCodecType = PT_G711U;
        }
        else
        {
            alogw("Unknown audio codec type[%s]! Set to default [aac]", ptr);
            pContext->aConfigPara.mConfCodecType = PT_AAC;
        }
        pContext->aConfigPara.mConfChnCnt     = GetConfParaInt(&mConf, AUDIO_CHANNEL_COUNT, 0);	// audio params
        pContext->aConfigPara.mConfBitWidth   = GetConfParaInt(&mConf, AUDIO_BIT_WIDTH, 0);
        pContext->aConfigPara.mConfSampleRate = GetConfParaInt(&mConf, AUDIO_SAMPLE_RATE, 0);
        pContext->aConfigPara.mConfBitRate = GetConfParaInt(&mConf, AUDIO_KEY_BITRATE, 0);

        destroyConfParser(&mConf);
    }

    //parse dst directory form dst file path.
    char *pLastSlash = strrchr(pContext->mConfigPara.dstVideoFile, '/');
    if(pLastSlash != NULL)
    {
        int dirLen = pLastSlash-pContext->mConfigPara.dstVideoFile;
        strncpy(pContext->mDstDir, pContext->mConfigPara.dstVideoFile, dirLen);
        pContext->mDstDir[dirLen] = '\0';
        
        char *pFileName = pLastSlash+1;
        strcpy(pContext->mFirstFileName, pFileName);
    }
    else
    {
        strcpy(pContext->mDstDir, "");
        strcpy(pContext->mFirstFileName, pContext->mConfigPara.dstVideoFile);
    }
    return SUCCESS;
}

static unsigned long long GetNowTimeUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

//根据当前时间生成文件名/获取指定的文件名
static int getFileNameByCurTime(SAMPLE_AV2MUXER_S *pContext, char *pNameBuf)
{
#if 0
    sprintf(pNameBuf, "%s", "/mnt/extsd/sample_mux/");
    sprintf(pNameBuf, "%s%llud.mp4", pNameBuf, GetNowTimeUs());
#else
    static int file_cnt = 0; //用于记录文件计数
    char strStemPath[MAX_FILE_PATH_LEN] = {0};
    int len = strlen(pContext->mConfigPara.dstVideoFile); //计算文件路径的长度 len
    char *ptr = pContext->mConfigPara.dstVideoFile;
    while (*(ptr+len-1) != '.') //从路径末尾向前查找第一个 . 字符的位置
    {
        len--;
    }

    ++file_cnt; //增加文件计数 file_cnt
    strncpy(strStemPath, pContext->mConfigPara.dstVideoFile, len-1);//将文件路径的主干部分复制到 strStemPath
    sprintf(pNameBuf, "%s_%d.mp4", strStemPath, file_cnt); //生成的文件名格式为 主干部分_计数.mp4
#endif
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_AV2MUXER_S *pContext = (SAMPLE_AV2MUXER_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_AV2MUXER_S*)cookie;
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
	    default:
		aloge("fatal error! unknow event type[0x%x]", event);
		break;
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (pIsp2VeParam)
                {
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mConfigPara.mVippDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mConfigPara.mVippDev, ret);
                        return -1;
                    }
                    struct enc_VencIsp2VeParam mIsp2VeParam;
                    memset(&mIsp2VeParam, 0, sizeof(struct enc_VencIsp2VeParam));
                    mIsp2VeParam.AeStatsInfo = (struct isp_ae_stats_s *)&pIsp2VeParam->mIspAeStatus;
                    ret = AW_MPI_ISP_GetIsp2VeParam(mIspDev, &mIsp2VeParam);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] GetIsp2VeParam failed! ret=%d", mIspDev, ret);
                        return -1;
                    }

                    if (mIsp2VeParam.encpp_en)
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (FALSE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = TRUE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                        memcpy(&pSharpParam->mDynamicParam, &mIsp2VeParam.mDynamicSharpCfg,sizeof(sEncppSharpParamDynamic));
                        memcpy(&pSharpParam->mStaticParam, &mIsp2VeParam.mStaticSharpCfg, sizeof(sEncppSharpParamStatic));
                    }
                    else
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (TRUE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = FALSE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                    }

                    pIsp2VeParam->mEnvLv = AW_MPI_ISP_GetEnvLV(mIspDev);
                    pIsp2VeParam->mAeWeightLum = AW_MPI_ISP_GetAeWeightLum(mIspDev);
                    pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
                if (pVe2IspParam && pContext->mConfigPara.mIspAndVeLinkageEnable)
                {
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mConfigPara.mVippDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mConfigPara.mVippDev, ret);
                        return -1;
                    }
                    alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, pContext->mConfigPara.mVippDev, pContext->mConfigPara.mVeChn);

                    alogv("isp[%d] d2d_level=%d, d3d_level=%d, is_overflow=%d", mIspDev, pVe2IspParam->d2d_level, pVe2IspParam->d3d_level, pVe2IspParam->mMovingLevelInfo.is_overflow);
                    struct enc_VencVe2IspParam mIspParam;
                    memset(&mIspParam, 0, sizeof(struct enc_VencVe2IspParam));
                    mIspParam.d2d_level = pVe2IspParam->d2d_level;
                    mIspParam.d3d_level = pVe2IspParam->d3d_level;
                    memcpy(&mIspParam.mMovingLevelInfo, &pVe2IspParam->mMovingLevelInfo, sizeof(MovingLevelInfo));
                    ret = AW_MPI_ISP_SetVe2IspParam(mIspDev, &mIspParam);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] SetVe2IspParam failed! ret=%d", mIspDev, ret);
                        return -1;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE: //指定的muxer ID 的录制已完成
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_AV2MUXER_S*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpAV2MuxerData->mMsgQueue, &stCmdMsg);  //带有事件数据的 Rec_FileDone 命令发送到消息队列
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD: //记录 muxer 需要下一个文件描述符
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_AV2MUXER_S*)cookie;
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpAV2MuxerData->mMsgQueue, &stCmdMsg);  //将带有事件数据的 Rec_NeedSetNextFd 命令发送到消息队列。
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

static ERRORTYPE configMuxGrpAttr(SAMPLE_AV2MUXER_S *pContext)
{
    memset(&pContext->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));

    pContext->mMuxGrpAttr.mVideoAttrValidNum = 1;
    pContext->mMuxGrpAttr.mVideoAttr[0].mVideoEncodeType = pContext->mConfigPara.mVideoEncoderFmt; //编码类型
    pContext->mMuxGrpAttr.mVideoAttr[0].mWidth = pContext->mConfigPara.dstWidth;//输出宽度
    pContext->mMuxGrpAttr.mVideoAttr[0].mHeight = pContext->mConfigPara.dstHeight;//输出高度
    pContext->mMuxGrpAttr.mVideoAttr[0].mVideoFrmRate = pContext->mConfigPara.mVideoFrameRate*1000; //视频帧率
    pContext->mMuxGrpAttr.mVideoAttr[0].mVeChn = pContext->mVeChn;//编码通道
    
    pContext->mMuxGrpAttr.mChannels = pContext->aConfigPara.mConfChnCnt;
    pContext->mMuxGrpAttr.mBitsPerSample = pContext->aConfigPara.mConfBitWidth;
    pContext->mMuxGrpAttr.mSamplesPerFrame = MAXDECODESAMPLE;
    pContext->mMuxGrpAttr.mSampleRate = pContext->aConfigPara.mConfSampleRate;
    pContext->mMuxGrpAttr.mAudioEncodeType = pContext->aConfigPara.mConfCodecType;

    return SUCCESS;
}

static ERRORTYPE createMuxGrp(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pContext); //配置复用器属性
    pContext->mMuxGrp = 0;
    while (pContext->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pContext->mMuxGrp, &pContext->mMuxGrpAttr); //创建复用器组
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pContext->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux group[%d] is exist, find next!", pContext->mMuxGrp);
            pContext->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pContext->mMuxGrp, ret);
            pContext->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pContext->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxGrp, &cbInfo); //注册回调函数
        return SUCCESS;
    }
}

static int addOutputFormatAndOutputSink_l(SAMPLE_AV2MUXER_S *pContext, OUTSINKINFO_S *pSinkInfo)
{
    int retMuxerId = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("fmt:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d", pSinkInfo->mOutputFormat, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen, pSinkInfo->mCallbackOutFlag);
    if(pSinkInfo->mOutputFd >= 0 && TRUE == pSinkInfo->mCallbackOutFlag) //检查文件描述符和回调标志位
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }

    //find if the same output_format sinkInfo exist or callback out stream is exist.
    pthread_mutex_lock(&pContext->mMuxChnListLock);//锁定互斥锁
    if (!list_empty(&pContext->mMuxChnList))//遍历列表，检查是否存在相同的输出格式。如果找到相同的格式，记录警告信息。
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFormat == pSinkInfo->mOutputFormat)
            {
                alogd("Be careful! same outputForamt[0x%x] exist in array", pSinkInfo->mOutputFormat);
            }
//            if (pEntry->mSinkInfo.mCallbackOutFlag == pSinkInfo->mCallbackOutFlag)
//            {
//                aloge("fatal error! only support one callback out stream");
//            }
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);//释放互斥锁

    MUX_CHN_INFO_S *p_node = (MUX_CHN_INFO_S *)malloc(sizeof(MUX_CHN_INFO_S));//为新节点分配内存并初始化其字段。
    if (p_node == NULL)
    {
        aloge("alloc mux chn info node fail");
        return -1;
    }

    memset(p_node, 0, sizeof(MUX_CHN_INFO_S));
    p_node->mSinkInfo.mMuxerId = pContext->mMuxerIdCounter;//设置复用器ID
    p_node->mSinkInfo.mOutputFormat = pSinkInfo->mOutputFormat;//设置输出格式
    if (pSinkInfo->mOutputFd > 0)
    {
        p_node->mSinkInfo.mOutputFd = dup(pSinkInfo->mOutputFd);//设置文件描述符
    }
    else
    {
        p_node->mSinkInfo.mOutputFd = -1;
    }
    p_node->mSinkInfo.mFallocateLen = pSinkInfo->mFallocateLen; //设置预分配长度
    p_node->mSinkInfo.mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;//设置回调标志位

    p_node->mMuxChnAttr.mMuxerId = p_node->mSinkInfo.mMuxerId; //复用器ID
    p_node->mMuxChnAttr.mMediaFileFormat = p_node->mSinkInfo.mOutputFormat;//输出文件格式
    p_node->mMuxChnAttr.mMaxFileDuration = pContext->mConfigPara.mMaxFileDuration *1000; //最大文件长度
    p_node->mMuxChnAttr.mFallocateLen = p_node->mSinkInfo.mFallocateLen;//预分配长度
    p_node->mMuxChnAttr.mCallbackOutFlag = p_node->mSinkInfo.mCallbackOutFlag;//回调标志位
    p_node->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE; //文件写入模型
    p_node->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS; //缓存大小
    p_node->mMuxChnAttr.mAddRepairInfo = pContext->mConfigPara.mbAddRepairInfo; //是否为MP4文件添加维修信息。例如文件头、索引信息、时间戳、元数据等
    p_node->mMuxChnAttr.mMaxFrmsTagInterval = pContext->mConfigPara.mMaxFrmsTagInterval; //帧标签备份间隔，用于指定在视频文件中定期插入帧标签的时间间隔。

    p_node->mMuxChn = MM_INVALID_CHN;
    //检查录制状态，如果为就绪状态或者录制中状态时
    if ((pContext->mCurrentState == REC_PREPARED) || (pContext->mCurrentState == REC_RECORDING))
    {
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while (nMuxChn < MUX_MAX_CHN_NUM)
        {   //创建复用通道
            ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrp, nMuxChn, &p_node->mMuxChnAttr, p_node->mSinkInfo.mOutputFd);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrp, nMuxChn, p_node->mMuxChnAttr.mMuxerId);
                break;
            }
            else if (ERR_MUX_EXIST == ret)
            {
                alogd("mux group[%d] channel[%d] is exist, find next!", pContext->mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", pContext->mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }

        if (nSuccessFlag)
        {
            retMuxerId = p_node->mSinkInfo.mMuxerId; //更新返回的复用器ID
            p_node->mMuxChn = nMuxChn; //更新复用器通道
            pContext->mMuxerIdCounter++; //增加复用器ID计数器。
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrp);
            if (p_node->mSinkInfo.mOutputFd >= 0)
            {
                close(p_node->mSinkInfo.mOutputFd);
                p_node->mSinkInfo.mOutputFd = -1;
            }

            retMuxerId = -1;
        }

        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList); //将新节点添加到列表中
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
    }
    else
    {
        retMuxerId = p_node->mSinkInfo.mMuxerId;
        pContext->mMuxerIdCounter++;
        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList);
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
    }

    return retMuxerId;
}

static int addOutputFormatAndOutputSink(SAMPLE_AV2MUXER_S *pContext, char* path, MEDIA_FILE_FORMAT_E format)
{
    //初始化变量
    int muxerId = -1;
    OUTSINKINFO_S sinkInfo = {0};

    if (path != NULL) //检查路径是否为空
    {
        sinkInfo.mFallocateLen = 0; //设置预分配的文件长度为0 ，即不指定长度
        sinkInfo.mCallbackOutFlag = FALSE; //回调输出功能为FLASE
        sinkInfo.mOutputFormat = format; //设置输出文件格式，如MP4
        sinkInfo.mOutputFd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);//打开输出文件描述符
        if (sinkInfo.mOutputFd < 0)
        {
            aloge("Failed to open %s", path);
            return -1;
        }

        muxerId = addOutputFormatAndOutputSink_l(pContext, &sinkInfo);//创建复用器通道节点ID
        close(sinkInfo.mOutputFd);
    }

    return muxerId;
}



static int setOutputFileSync_l(SAMPLE_AV2MUXER_S *pContext, int fd, int64_t fallocateLength, int muxerId)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("must be in recording state");
        return -1;
    }

    alogv("setOutputFileSync fd=%d", fd);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return -1;
    }

    MUX_CHN muxChn = MM_INVALID_CHN;
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mMuxChnAttr.mMuxerId == muxerId)
            {
                muxChn = pEntry->mMuxChn;
                break;
            }
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    if (muxChn != MM_INVALID_CHN)
    {
        alogd("switch fd");
        AW_MPI_MUX_SwitchFd(pContext->mMuxGrp, muxChn, fd, fallocateLength);//切换文件描述符
        return 0;
    }
    else
    {
        aloge("fatal error! can't find muxChn which muxerId[%d]", muxerId);
        return -1;
    }
}

//同步设置输出文件
static int setOutputFileSync(SAMPLE_AV2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxerId)
{
    int ret;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("not in recording state");
        return -1;
    }

    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666); //使用 open 函数以读写模式打开或创建文件
        if (fd < 0)
        {
            aloge("fail to open %s", path);
            return -1;
        }
        ret = setOutputFileSync_l(pContext, fd, fallocateLength, muxerId); //调用内部函数 setOutputFileSync_l，传递上下文、文件描述符、文件分配长度和多路复用器 ID
        close(fd);

        return ret;
    }
    else
    {
        return -1;
    }
}


static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;
        break;

    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;
        break;

    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;
        break;

    default:
        break;
    }

    return profile;
}
static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = H265_PROFILE_MAIN;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H265_PROFILE_MAIN;
        break;

    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;
        break;

    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;
        break;

    default:
        break;
    }
    return profile;
}



static void configLdcParam(SAMPLE_AV2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht = 2560;
    pGdcParam->calib_height = 1440;

    pGdcParam->eWarpMode = Gdc_Warp_LDC;

    pGdcParam->fx = 1706.57f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1595.14f;
    pGdcParam->fy_scale = 1601.57f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    pGdcParam->distCoef_wide_ra[0] = -0.428043f;
    pGdcParam->distCoef_wide_ra[1] = 0.227647f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.009734f;
    pGdcParam->distCoef_wide_ta[1] = -0.001312f;

    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;
}

static void configLdcProParam(SAMPLE_AV2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht = pContext->mConfigPara.dstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.dstHeight;
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpMode;
    pGdcParam->lut_data_buf = (unsigned int *)pContext->mConfigPara.mpGdcLdcProLutBinData;
    pGdcParam->lut_data_size = pContext->mConfigPara.mGdcLdcProLutBinDataLen;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;
}


static void configGdcParam(SAMPLE_AV2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    switch (pGdcParam->eWarpMode)
    {
        case Gdc_Warp_LDC:
            configLdcParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_LDC_Pro:
            configLdcProParam(pContext, pGdcParam);
            break;
        default:
            aloge("unsupport warp mode[%d]", pGdcParam->eWarpMode);
            break;
    }
}







static ERRORTYPE configVencChnAttr(SAMPLE_AV2MUXER_S *pContext) //
{
    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVencChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->mVencChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mKeyFrameInterval;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.srcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.srcHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = pContext->mConfigPara.mColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->mVencChnAttr.VeAttr.PixelFormat, pContext->mVencChnAttr.VeAttr.mColorSpace);
    pContext->mVencChnAttr.VeAttr.mDropFrameNum = pContext->mConfigPara.mVencDropFrameNum;
    alogd("DropFrameNum:%d", pContext->mVencChnAttr.VeAttr.mDropFrameNum);
    pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode = pContext->mConfigPara.mVeRefFrameLbcMode;
    alogd("VeRefFrameLbcMode:%d", pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode);
    pContext->mVencChnAttr.VeAttr.mVeRecRefBufReduceDisable = pContext->mConfigPara.mVeRecRefBufReduceDisable;
    alogd("VeRecRefBufReduceDisable:%d", pContext->mVencChnAttr.VeAttr.mVeRecRefBufReduceDisable);
    pContext->mVencChnAttr.EncppAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;
    switch(pContext->mConfigPara.mEncodeRotate)
    {
        case 90:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_90;
            break;
        case 180:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_180;
            break;
        case 270:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_270;
            break;
        default:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
            break;
    }

    pContext->mVencRcParam.product_mode = pContext->mConfigPara.mProductMode;
    pContext->mVencRcParam.sensor_type = pContext->mConfigPara.mSensorType;

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatusEnable = pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatus = pContext->mConfigPara.EnIFrmMbRcMoveStatus;
        pContext->mVencRcParam.mBitsRatioEnable = pContext->mConfigPara.mBitsRatioEnable;
        memcpy(&pContext->mVencRcParam.mBitsRatio, &pContext->mConfigPara.mBitsRatio, sizeof(VencIPTargetBitsRatio));
        pContext->mVencRcParam.mWeakTextureThEnable = pContext->mConfigPara.mWeakTextureThEnable;
        pContext->mVencRcParam.mWeakTextureTh = pContext->mConfigPara.mWeakTextureTh;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->mVencRcParam.ParamH264Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH264Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH264Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = TRUE;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatusEnable = pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatus = pContext->mConfigPara.EnIFrmMbRcMoveStatus;
        pContext->mVencRcParam.mBitsRatioEnable = pContext->mConfigPara.mBitsRatioEnable;
        memcpy(&pContext->mVencRcParam.mBitsRatio, &pContext->mConfigPara.mBitsRatio, sizeof(VencIPTargetBitsRatio));
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencRcParam.ParamH265Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH265Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH265Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = TRUE;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.dstHeight;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 0:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            break;
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 2:
        case 3:
        default:
            aloge("not support! use default cbr mode");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            break;
        }
    }

    alogd("venc set Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);

    if(0 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(1 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_DUALP;
    }
    else if(2 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_SMARTP;
        pContext->mVencChnAttr.GopAttr.stSmartP.mVirtualIFrameInterval = 15;
    }
    pContext->mVencChnAttr.GopAttr.mGopSize = pContext->mConfigPara.mGopSize;

    if (pContext->mConfigPara.mEnableGdc)
    {
        alogd("enable GDC and init GDC params");
        configGdcParam(pContext, &pContext->mVencChnAttr.GdcAttr);
    }

    memcpy(&pContext->mVencRcParam.mBitsClipParam, &pContext->mConfigPara.mBitsClipParam, sizeof(VencTargetBitsClipParam));
    memcpy(&pContext->mVencRcParam.mAeDiffParam, &pContext->mConfigPara.mAeDiffParam, sizeof(VencAeDiffParam));

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pContext); //配置编码属性
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVeChn = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mVeChn = pContext->mConfigPara.mVeChn;
    }
    //创建编码通道
    while (pContext->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pContext->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pContext->mVeChn);
            pContext->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pContext->mVeChn, ret);
            pContext->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pContext->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        //if (0 < pContext->mConfigPara.mVeFreq)
        //{
        //    AW_MPI_VENC_SetVEFreq(pContext->mVeChn, pContext->mConfigPara.mVeFreq);
        //    alogd("set VE freq %d MHz", pContext->mConfigPara.mVeFreq);
        //}
        //设置编码属性
        AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
        stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate: src %dfps, dst %dfps", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);

        if (pContext->mConfigPara.mAdvancedRef_Base)
        {
            VENC_PARAM_REF_S stRefParam;
            memset(&stRefParam, 0, sizeof(VENC_PARAM_REF_S));
            stRefParam.Base = pContext->mConfigPara.mAdvancedRef_Base;
            stRefParam.Enhance = pContext->mConfigPara.mAdvancedRef_Enhance;
            stRefParam.bEnablePred = pContext->mConfigPara.mAdvancedRef_RefBaseEn;
            AW_MPI_VENC_SetRefParam(pContext->mVeChn, &stRefParam);
            alogd("set RefParam %d %d %d", stRefParam.Base, stRefParam.Enhance, stRefParam.bEnablePred);
        }

        if (pContext->mConfigPara.m2DnrPara.enable_2d_filter)
        {
            AW_MPI_VENC_Set2DFilter(pContext->mVeChn, &pContext->mConfigPara.m2DnrPara);
            alogd("set 2DFilter param");
        }

        if (pContext->mConfigPara.m3DnrPara.enable_3d_filter)
        {
            AW_MPI_VENC_Set3DFilter(pContext->mVeChn, &pContext->mConfigPara.m3DnrPara);
            alogd("set 3DFilter param");
        }

        if (pContext->mConfigPara.mColor2Grey)
        {
            VENC_COLOR2GREY_S bColor2Grey;
            memset(&bColor2Grey, 0, sizeof(VENC_COLOR2GREY_S));
            bColor2Grey.bColor2Grey = pContext->mConfigPara.mColor2Grey;
            AW_MPI_VENC_SetColor2Grey(pContext->mVeChn, &bColor2Grey);
            alogd("set Color2Grey %d", pContext->mConfigPara.mColor2Grey);
        }

        if (pContext->mConfigPara.mHorizonFlipFlag)
        {
            AW_MPI_VENC_SetHorizonFlip(pContext->mVeChn, pContext->mConfigPara.mHorizonFlipFlag);
            alogd("set HorizonFlip %d", pContext->mConfigPara.mHorizonFlipFlag);
        }

        if (pContext->mConfigPara.mCropEnable)
        {
            VENC_CROP_CFG_S stCropCfg;
            memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
            stCropCfg.bEnable = pContext->mConfigPara.mCropEnable;
            stCropCfg.Rect.X = pContext->mConfigPara.mCropRectX;
            stCropCfg.Rect.Y = pContext->mConfigPara.mCropRectY;
            stCropCfg.Rect.Width = pContext->mConfigPara.mCropRectWidth;
            stCropCfg.Rect.Height = pContext->mConfigPara.mCropRectHeight;
            AW_MPI_VENC_SetCrop(pContext->mVeChn, &stCropCfg);
            alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
        }

        //test PIntraRefresh
        if(pContext->mConfigPara.mIntraRefreshBlockNum > 0)
        {
            VENC_PARAM_INTRA_REFRESH_S stIntraRefresh;
            memset(&stIntraRefresh, 0, sizeof(VENC_PARAM_INTRA_REFRESH_S));
            stIntraRefresh.bRefreshEnable = TRUE;
            stIntraRefresh.RefreshLineNum = pContext->mConfigPara.mIntraRefreshBlockNum;
            ret = AW_MPI_VENC_SetIntraRefresh(pContext->mVeChn, &stIntraRefresh);
            if(ret != SUCCESS)
            {
                aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
            }
            else
            {
                alogd("set intra refresh:%d", stIntraRefresh.RefreshLineNum);
            }
        }

        if(pContext->mConfigPara.mbEnableSmart)
        {
            VencSmartFun smartParam;
            memset(&smartParam, 0, sizeof(VencSmartFun));
            smartParam.smart_fun_en = 1;
            smartParam.img_bin_en = 1;
            smartParam.img_bin_th = 0;
            smartParam.shift_bits = 2;
            AW_MPI_VENC_SetSmartP(pContext->mVeChn, &smartParam);
        }

        if(pContext->mConfigPara.mSVCLayer > 0)
        {
            VencH264SVCSkip stSVCSkip;
            memset(&stSVCSkip, 0, sizeof(VencH264SVCSkip));
            stSVCSkip.nTemporalSVC = pContext->mConfigPara.mSVCLayer;
            AW_MPI_VENC_SetH264SVCSkip(pContext->mVeChn, &stSVCSkip);
        }

        if (pContext->mConfigPara.mVuiTimingInfoPresentFlag)
        {
            /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
            if(PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H264_VUI_S H264Vui;
                memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
                AW_MPI_VENC_GetH264Vui(pContext->mVeChn, &H264Vui);
                H264Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H264Vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
                H264Vui.VuiTimeInfo.num_units_in_tick = 1000;
                H264Vui.VuiTimeInfo.time_scale = H264Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate * 2;
                AW_MPI_VENC_SetH264Vui(pContext->mVeChn, &H264Vui);
                alogd("VencChn[%d] fill framerate %d to H264VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
            else if(PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H265_VUI_S H265Vui;
                memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
                AW_MPI_VENC_GetH265Vui(pContext->mVeChn, &H265Vui);
                H265Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H265Vui.VuiTimeInfo.num_units_in_tick = 1000;
                /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
                H265Vui.VuiTimeInfo.time_scale = H265Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate;
                H265Vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = H265Vui.VuiTimeInfo.num_units_in_tick;
                AW_MPI_VENC_SetH265Vui(pContext->mVeChn, &H265Vui);
                alogd("VencChn[%d] fill framerate %d to H265VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
        }

        if (0 <= pContext->mConfigPara.mSuperFrmMode)
        {
            VENC_SUPERFRAME_CFG_S mSuperFrmParam;
            memset(&mSuperFrmParam, 0, sizeof(VENC_SUPERFRAME_CFG_S));
            mSuperFrmParam.enSuperFrmMode = pContext->mConfigPara.mSuperFrmMode;
            mSuperFrmParam.MaxRencodeTimes = pContext->mConfigPara.mSuperMaxRencodeTimes;
            mSuperFrmParam.MaxP2IFrameBitsRatio = pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio;
            if (0 == pContext->mConfigPara.mSuperIFrmBitsThr || 0 == pContext->mConfigPara.mSuperPFrmBitsThr)
            {
                float cmp_bits = 1.5*1024*1024 / 20;
                float dst_bits = (float)pContext->mConfigPara.mVideoBitRate / pContext->mConfigPara.mVideoFrameRate;
                float bits_ratio = dst_bits / cmp_bits;
                mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*200*1024) * bits_ratio);
                mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
            }
            else
            {
                mSuperFrmParam.SuperIFrmBitsThr = pContext->mConfigPara.mSuperIFrmBitsThr;
                mSuperFrmParam.SuperPFrmBitsThr = pContext->mConfigPara.mSuperPFrmBitsThr;
            }
            alogd("SuperFrm Mode:%d, MaxRencodeTimes:%d, MaxP2IFrameBitsRatio:%.2f, IBitsThr:%d, PBitsThr:%d",
                mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.MaxRencodeTimes, mSuperFrmParam.MaxP2IFrameBitsRatio,
                mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
            AW_MPI_VENC_SetSuperFrameCfg(pContext->mVeChn, &mSuperFrmParam);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mChromaQPOffsetEnable)
            {
                AW_MPI_VENC_SetChromaQPOffset(pContext->mVeChn, pContext->mConfigPara.mChromaQPOffset);
            }
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mH264ConstraintFlagEnable)
            {
                AW_MPI_VENC_SetH264ConstraintFlag(pContext->mVeChn, &pContext->mConfigPara.mH264ConstraintFlag);
            }
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            AW_MPI_VENC_SetVe2IspD2DLimit(pContext->mVeChn, &pContext->mConfigPara.mVe2IspD2DLimit);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            int dstWidthAlign = AWALIGN(pContext->mConfigPara.dstWidth, 16);
            int dstHeightAlign = AWALIGN(pContext->mConfigPara.dstHeight, 16);
            if (dstWidthAlign != pContext->mConfigPara.dstWidth || dstHeightAlign != pContext->mConfigPara.dstHeight)
            {
                VencForceConfWin stConfWin;
                memset(&stConfWin, 0, sizeof(VencForceConfWin));
                stConfWin.en_force_conf = 1;
                stConfWin.left_offset = 0;
                stConfWin.right_offset = dstWidthAlign - pContext->mConfigPara.dstWidth;
                stConfWin.top_offset = 0;
                stConfWin.bottom_offset = dstHeightAlign - pContext->mConfigPara.dstHeight;
                alogd("set ForceConfWin en %d, left %d right %d top %d bottom %d", stConfWin.en_force_conf, stConfWin.left_offset,
                    stConfWin.right_offset, stConfWin.top_offset, stConfWin.bottom_offset);
                AW_MPI_VENC_SetForceConfWin(pContext->mVeChn, &stConfWin);
            }
        }

        if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = pContext->mConfigPara.mQuality;
            AW_MPI_VENC_SetJpegParam(pContext->mVeChn, &stJpegParam);
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret;

    //create vi channel
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViDev = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mViDev = pContext->mConfigPara.mVippDev;
    }
    pContext->mIspDev = 0;
    pContext->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pContext->mViDev); //创建VI设备
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }
    //初始化VI属性
    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViAttr.mOnlineEnable = 1;
        pContext->mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.srcPixFmt);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = pContext->mConfigPara.mColorSpace;
    pContext->mViAttr.format.width = pContext->mConfigPara.srcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.srcHeight;
    pContext->mViAttr.nbufs =  pContext->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->mViAttr.nbufs, pContext->mViAttr.format.colorspace);
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.wdr_mode = pContext->mConfigPara.wdr_en;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
    pContext->mViAttr.drop_frame_num = pContext->mConfigPara.mViDropFrameNum;
    pContext->mViAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;
    ret = AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pContext->mIspDev);//启动ISP
#endif

    // Saturation change 饱和度调整
    if (pContext->mConfigPara.mSaturationChange)
    {
        int nSaturationValue = 0;
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("current SaturationValue: %d", nSaturationValue);
        nSaturationValue = nSaturationValue + pContext->mConfigPara.mSaturationChange;
        AW_MPI_ISP_SetSaturation(pContext->mIspDev, nSaturationValue);
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("after change, SaturationValue: %d", nSaturationValue);
    }
    //创建虚拟通道
    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, &stVirChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->mViChn);
    }
    //注册回调函数
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VI_RegisterCallback(pContext->mViDev, &cbInfo);
    //启用VI设备
    ret = AW_MPI_VI_EnableVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    return ret;
}


void configAioAttr(SAMPLE_AV2MUXER_S *ctx)
{
    AIO_ATTR_S *pAttr = &ctx->mAioAttr;

    pAttr->mChnCnt    = ctx->aConfigPara.mConfChnCnt;
    pAttr->enBitwidth   = map_BitWidth_to_AUDIO_BIT_WIDTH_E(ctx->aConfigPara.mConfBitWidth);
    pAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(ctx->aConfigPara.mConfSampleRate);
}

void configAEncAttr(SAMPLE_AV2MUXER_S *ctx)
{
    AENC_CHN_ATTR_S *pAttr = &ctx->mAEncAttr;
    pAttr->AeAttr.Type = ctx->aConfigPara.mConfCodecType; //编码器类型
    pAttr->AeAttr.channels = ctx->aConfigPara.mConfChnCnt;//通道数
    pAttr->AeAttr.bitsPerSample = ctx->aConfigPara.mConfBitWidth;//位宽
    pAttr->AeAttr.sampleRate = ctx->aConfigPara.mConfSampleRate;//采样率
    pAttr->AeAttr.bitRate = ctx->aConfigPara.mConfBitRate;//比特率
    pAttr->AeAttr.attachAACHeader = 0; //aacMuxer will add adts header, so aac encoder need not attach aac header.设置 attachAACHeader 为0，表示AAC编码器不需要附加AAC头部，因为AAC复用器会添加ADTS头部。
    pAttr->AeAttr.mInBufSize = 0;//设置输入缓冲区大小
    pAttr->AeAttr.mOutBufCnt = 0;//设置输出缓冲区计数
}

static ERRORTYPE createAiChn(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret;
    
    configAioAttr(pContext);

    pContext->mAIDevId = 0;
    pContext->mAIChnId = 0;

    AW_MPI_AI_SetPubAttr(pContext->mAIDevId, &pContext->mAioAttr);
    AW_MPI_AI_Enable(pContext->mAIDevId);
    AW_MPI_AI_SetDevVolume(pContext->mAIDevId, 100);

    BOOL nSuccessFlag = FALSE;
    while (pContext->mAIChnId < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(pContext->mAIDevId, pContext->mAIChnId);//创建AI输入通道
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", pContext->mAIChnId);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", pContext->mAIChnId);
            pContext->mAIChnId++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", pContext->mAIChnId, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        pContext->mAIChnId = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        ret = -1;
    }
    else
    {
        pContext->mAiChn.mModId = MOD_ID_AI;
        pContext->mAiChn.mDevId = pContext->mAIDevId;
        pContext->mAiChn.mChnId = pContext->mAIChnId;
    }

    return ret;
}

static ERRORTYPE createAencChn(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret;

    configAEncAttr(pContext);
    alogd("create AEnc Chn starting !!!");
    pContext->mAEncChnId = 0;

    BOOL nSuccessFlag = FALSE;
    while (pContext->mAEncChnId < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(pContext->mAEncChnId, &pContext->mAEncAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", pContext->mAEncChnId);
            break;
        }
        else if (ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", pContext->mAEncChnId);
            pContext->mAEncChnId++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", pContext->mAEncChnId, ret);
            pContext->mAEncChnId++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        pContext->mAEncChnId = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    else
    {
        pContext->mAEncChn.mModId = MOD_ID_AENC;
        pContext->mAEncChn.mDevId = 0;
        pContext->mAEncChn.mChnId = pContext->mAEncChnId;
    }

    return ret;
}




static ERRORTYPE prepare(SAMPLE_AV2MUXER_S *pContext)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
    ERRORTYPE result = FAILURE;

    if (createViChn(pContext) != SUCCESS)
    {
        aloge("create vi chn fail");
        return result;
    }

    if (createVencChn(pContext) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    if (createAiChn(pContext) != SUCCESS)
    {
        aloge("create ai chn fail");
        return result;
    }

    if (createAencChn(pContext) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    if (createMuxGrp(pContext) != SUCCESS)
    {
        aloge("create mux group fail");
        return result;
    }

    //set spspps
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &H264SpsPpsInfo); //设置编码的H264spspps信息
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH264SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH264SpsPpsInfo(pContext->mMuxGrp, pContext->mVeChn, &H264SpsPpsInfo); //设置muxgroup 的H264spspps 信息。
    }
    else if(pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH265SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH265SpsPpsInfo(pContext->mMuxGrp, pContext->mVeChn, &H265SpsPpsInfo);
    }

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            nMuxChn = 0;
            nSuccessFlag = FALSE;
            while (pEntry->mMuxChn < MUX_MAX_CHN_NUM)
            {   //创建多路复用器（muxer）通道
                ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrp, nMuxChn, &pEntry->mMuxChnAttr, pEntry->mSinkInfo.mOutputFd);
                if (SUCCESS == ret)
                {
                    nSuccessFlag = TRUE;
                    alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrp, \
                        nMuxChn, pEntry->mMuxChnAttr.mMuxerId);
                    break;
                }
                else if(ERR_MUX_EXIST == ret)
                {
                    nMuxChn++;
                    //break;
                }
                else
                {
                    nMuxChn++;
                }
            }

            if (FALSE == nSuccessFlag)
            {
                pEntry->mMuxChn = MM_INVALID_CHN;
                aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrp);
            }
            else
            {
                result = SUCCESS;
                pEntry->mMuxChn = nMuxChn;
            }
        }
    }
    else
    {
        aloge("maybe something wrong,mux chn list is empty");
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    if ((pContext->mAIDevId >= 0 && pContext->mAIChnId >= 0) && pContext->mAEncChnId >= 0)
    {
        AW_MPI_SYS_Bind(&pContext->mAiChn, &pContext->mAEncChn);
    }

    if (pContext->mVeChn >= 0 && pContext->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pContext->mMuxGrp};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        //pContext->mCurrentState = REC_PREPARED;
    }

    if (pContext->mAEncChn.mChnId >= 0 && pContext->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pContext->mMuxGrp};

        AW_MPI_SYS_Bind(&pContext->mAEncChn, &MuxGrp);
        pContext->mCurrentState = REC_PREPARED;
    }

    return result;
}

static ERRORTYPE start(SAMPLE_AV2MUXER_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    ret = AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn); //开启VI虚通道
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }

    ret = AW_MPI_AI_EnableChn(pContext->mAIDevId, pContext->mAIChnId); //开启AI虚通道
    if (ret != SUCCESS)
    {
        alogd("AR enable error!");
        return FAILURE;
    }
    

    if (pContext->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);//开启视频编码
    }

    if (pContext->mAEncChnId >= 0)
    {
        AW_MPI_AENC_StartRecvPcm(pContext->mAEncChnId);//开启音频编码
    }

    if (pContext->mMuxGrp >= 0)
    {
        AW_MPI_MUX_StartGrp(pContext->mMuxGrp);//开启封装通道接受数据
    }

    pContext->mCurrentState = REC_RECORDING;//更改状态为录制中

    return ret;
}

static ERRORTYPE stop(SAMPLE_AV2MUXER_S *pContext)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mAIChnId >= 0)
    {
        AW_MPI_AI_DisableChn(pContext->mAIDevId, pContext->mAIChnId);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if (pContext->mAEncChnId >= 0)
    {
        alogd("stop aenc");
        AW_MPI_AENC_StopRecvPcm(pContext->mAEncChnId);
    }

    if (pContext->mMuxGrp >= 0)
    {
        alogd("stop mux grp");
        AW_MPI_MUX_StopGrp(pContext->mMuxGrp);
    }
    if (pContext->mMuxGrp >= 0)
    {
        alogd("destory mux grp");
        AW_MPI_MUX_DestroyGrp(pContext->mMuxGrp);
        pContext->mMuxGrp = MM_INVALID_CHN;
    }
    if (pContext->mAEncChnId >= 0)
    {
        alogd("destory aenc");
        AW_MPI_AENC_DestroyChn(pContext->mAEncChnId);
        pContext->mVeChn = MM_INVALID_CHN;
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("destory venc");
        //AW_MPI_VENC_ResetChn(pContext->mVeChn);
        AW_MPI_VENC_DestroyChn(pContext->mVeChn);
        pContext->mVeChn = MM_INVALID_CHN;
    }

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
        AW_MPI_VI_DisableVipp(pContext->mViDev);
    #if ISP_RUN
        AW_MPI_ISP_Stop(pContext->mIspDev);
    #endif
        AW_MPI_VI_DestroyVipp(pContext->mViDev);
    }

    if (pContext->mAIChnId >= 0)
    {
        AW_MPI_AI_DestroyChn(pContext->mAIDevId, pContext->mAIChnId);
    }    

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        alogd("free chn list node");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                alogd("close file");
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }

            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    return SUCCESS;
}
ERRORTYPE SampleAV2Muxer_CreateFolder(const char* pStrFolderPath)
{
    if(NULL == pStrFolderPath || 0 == strlen(pStrFolderPath)) //检查输入参数
    {
        aloge("folder path is wrong!");
        return FAILURE;
    }
    //check folder existence
    struct stat sb;
    if (stat(pStrFolderPath, &sb) == 0) //使用 stat 函数检查 pStrFolderPath 路径是否存在。
    {
        if(S_ISDIR(sb.st_mode))//如果路径存在且是一个目录，返回 SUCCESS。
        {
            return SUCCESS;
        }
        else //如果路径存在但不是目录，打印错误信息并返回 FAILURE。
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pStrFolderPath, (int)sb.st_mode);
            return FAILURE;
        }
    }
    //create folder if necessary 如果路径不存在，使用 mkdir 函数创建文件夹，权限设置为 S_IRWXU | S_IRWXG | S_IRWXO（即用户、组和其他人都具有读、写和执行权限）。
    int ret = mkdir(pStrFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pStrFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pStrFolderPath);
        return FAILURE;
    }
}

static ERRORTYPE resetCamera(SAMPLE_AV2MUXER_S *pContext)
{
    alogd("stop");

    pContext->resetCameraCnt++;

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn); //关闭vi虚通道
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn); //停止编码
    }

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("UnBind vi ve");
        AW_MPI_SYS_UnBind(&ViChn, &VeChn);//解绑VI和VENC
    }

    if (pContext->mViChn >= 0)
    {
        alogd("DestroyVirChn");
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);//销毁VI虚通道
        alogd("DisableVipp");
        AW_MPI_VI_DisableVipp(pContext->mViDev);//关闭VI设备
#if ISP_RUN
        alogd("ISP_Stop");
        AW_MPI_ISP_Stop(pContext->mIspDev);//停止ISP
#endif
        alogd("DestroyVipp");
        AW_MPI_VI_DestroyVipp(pContext->mViDev);//销毁VI设备
    }

    alogd("createViChn");
    createViChn(pContext); //创建VI虚通道
    
    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("Bind vi ve");
        AW_MPI_SYS_Bind(&ViChn, &VeChn);//重新绑定VI和VENC
    }

    alogd("start");

    if (pContext->mViDev >= 0 && pContext->mViChn >= 0)
    {
        alogd("VI_EnableVirChn");
        AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);//开启VI虚通道
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("VENC_StartRecvPic");
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);//开启编码
    }

    return SUCCESS;
}


static int initGdcLdcProParam(SAMPLE_AV2MUXER_S *pContext)
{
    int ret = 0;
    FILE *fp = fopen(pContext->mConfigPara.mGdcLdcProLutBin, "rb");
    if (NULL == fp)
    {
        ret = -1;
        aloge("fatal error! gdc ldc pro lut bin[%s] opne fail!", \
            pContext->mConfigPara.mGdcLdcProLutBin);
        goto _exit;
    }
    fseek(fp, 0, SEEK_END);
    pContext->mConfigPara.mGdcLdcProLutBinDataLen = ftell(fp);
    pContext->mConfigPara.mpGdcLdcProLutBinData = (char *)malloc(pContext->mConfigPara.mGdcLdcProLutBinDataLen);
    if (NULL == pContext->mConfigPara.mpGdcLdcProLutBinData)
    {
        ret = -1;
        aloge("fatal error! malloc gdc ldc pro lut data buffer fail!");
        goto _close_file;
    }
    fseek(fp, 0, SEEK_SET);
    fread(pContext->mConfigPara.mpGdcLdcProLutBinData, pContext->mConfigPara.mGdcLdcProLutBinDataLen, 1 ,fp);
_close_file:
    fclose(fp);
    fp = NULL;
_exit:
    return ret;
}

static void deInitGdcLdcProParam(SAMPLE_AV2MUXER_S *pContext)
{
    if (pContext->mConfigPara.mpGdcLdcProLutBinData)
    {
        free(pContext->mConfigPara.mpGdcLdcProLutBinData);
        pContext->mConfigPara.mpGdcLdcProLutBinData = NULL;
    }
    return;
}

//消息队列线程函数
static void *MsgQueueThread(void *pThreadData)
{
    SAMPLE_AV2MUXER_S *pContext = (SAMPLE_AV2MUXER_S*)pThreadData;
    message_t stCmdMsg; //定义消息
    Vi2Venc2MuxerMsgType cmd; //定义命令
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))//从消息队列获取消息
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd: //处理下一个文件
                {
                    int muxerId = nCmdPara;
                    char fileName[MAX_FILE_PATH_LEN] = {0};
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[0])
                    {
                        getFileNameByCurTime(pMsgData->mpVi2Venc2MuxerData, fileName);  //获取文件名
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode, 0, sizeof(FilePathNode));
                        strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[0]); //将文件路径节点加入队列
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                        static int cnt = 0;
                        cnt++;
                        sprintf(fileName, "/mnt/extsd/test_%d.ts", cnt);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[1]);
                    #endif
                    alogd("muxId[%d] set next fd, filepath=%s", muxerId, fileName);
                    setOutputFileSync(pContext, fileName, 0, muxerId); //切换文件描述符
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone: //录制结束
                {
                    int ret;
                    int muxerId = nCmdPara;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;
                    int idx = -1;

                    if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[0]) //确认多路复用器索引
                    {
                        idx = 0;
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                    else if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[1])
                    {
                        idx = 1;
                    }
                    #endif
                    if (idx >= 0)
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[idx]){cnt++;} //计算文件数量
                        FilePathNode *pNode = NULL;
                        while (cnt > pMsgData->mpVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt) //检查文件数量是否超过配置的最大文件数 mDstFileMaxCnt
                        {
                            pNode = list_first_entry(&pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[idx], FilePathNode, mList);
                            if ((ret = remove(pNode->strFilePath)) != 0) //删除最早的文件路径
                            {
                                aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                            }
                            else
                            {
                                alogd("delete file[%s] success", pNode->strFilePath);
                            }
                            cnt--;
                            list_del(&pNode->mList);
                            free(pNode);
                        }
                    }
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Vi_Timeout: //处理VI超时
                {
                    int ret;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    resetCamera(pContext); //重新开启摄像头
                    
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);//等待消息队列 msg_queue 中有消息可用
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    
	printf("sample_av2muxer running!\n");
    SAMPLE_AV2MUXER_S *pContext = (SAMPLE_AV2MUXER_S* )malloc(sizeof(SAMPLE_AV2MUXER_S));
    
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitAV2MuxerData(pContext) != SUCCESS)
    {
        return -1;
    }

    gpAV2MuxerData = pContext;
    cdx_sem_init(&pContext->mSemExit, 0);

    
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        result = FAILURE;
        goto err_out_0;
    }

    char *pConfPath = NULL;
    if(argc > 1)
    {
        pConfPath = pContext->mCmdLinePara.mConfigFilePath;
    }

    if (loadConfigPara(pContext, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }
    alogd("ViDropFrameNum=%d", pContext->mConfigPara.mViDropFrameNum);
    SampleAV2Muxer_CreateFolder(pContext->mDstDir);
    if ((pContext->mConfigPara.mEnableGdc) && (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode))
    {
        int ret = initGdcLdcProParam(pContext);
        if (ret)
        {
            aloge("initGdcLdcProParam fail! disable gdc!");
            pContext->mConfigPara.mEnableGdc = 0;
        }
    }
    INIT_LIST_HEAD(&pContext->mMuxChnList); //初始化一个双向链表的头节点,用于保存复用器通道
    pthread_mutex_init(&pContext->mMuxChnListLock, NULL);

    //初始化MPP系统
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    pContext->mMuxId[0] = addOutputFormatAndOutputSink(pContext, pContext->mConfigPara.dstVideoFile, MEDIA_FILE_FORMAT_MP4);

    if (pContext->mMuxId[0] < 0)
    {
        aloge("add first out file fail");
        goto err_out_1;
    }
    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));//创建一个新的文件路径节点
    memset(pFilePathNode, 0, sizeof(FilePathNode));
    strncpy(pFilePathNode->strFilePath, pContext->mConfigPara.dstVideoFile, MAX_FILE_PATH_LEN-1);//字符串复制
    list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[0]);//添加路径到链表中

#ifdef DOUBLE_ENCODER_FILE_OUT
    char mov_path[MAX_FILE_PATH_LEN];
    strcpy(mov_path, "/mnt/extsd/test.ts");
    pContext->mMuxId[1] = addOutputFormatAndOutputSink(pContext, mov_path, MEDIA_FILE_FORMAT_TS);
    if (pContext->mMuxId[1] < 0)
    {
        alogd("add mMuxId[1] ts file sink fail");
    }
    else
    {
        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
        memset(pFilePathNode, 0, sizeof(FilePathNode));
        strncpy(pFilePathNode->strFilePath, mov_path, MAX_FILE_PATH_LEN-1);
        list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[1]);
    }
#endif

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_2;
    }

    //create msg queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pContext->mMsgQueueThreadId);
    }

    start(pContext);   

    //test roi.
    int i = 0;
    ERRORTYPE ret;
    VENC_ROI_CFG_S stMppRoiBlockInfo;
    memset(&stMppRoiBlockInfo, 0, sizeof(VENC_ROI_CFG_S));
    for(i=0; i<pContext->mConfigPara.mRoiNum; i++)
    {
        stMppRoiBlockInfo.Index = i;
        stMppRoiBlockInfo.bEnable = TRUE;
        stMppRoiBlockInfo.bAbsQp = TRUE;
        stMppRoiBlockInfo.Qp = pContext->mConfigPara.mRoiQp;
        stMppRoiBlockInfo.Rect.X = 128*i;
        stMppRoiBlockInfo.Rect.Y = 128*i;
        stMppRoiBlockInfo.Rect.Width = 128;
        stMppRoiBlockInfo.Rect.Height = 128;
        ret = AW_MPI_VENC_SetRoiCfg(pContext->mVeChn, &stMppRoiBlockInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roi[%d] fail[0x%x]!", i, ret);
        }
        else
        {
            alogd("set roiIndex:%d, Qp:%d-%d, Rect[%d,%d,%dx%d]", i, stMppRoiBlockInfo.bAbsQp, stMppRoiBlockInfo.Qp, 
                stMppRoiBlockInfo.Rect.X, stMppRoiBlockInfo.Rect.Y, stMppRoiBlockInfo.Rect.Width, stMppRoiBlockInfo.Rect.Height);
        }
    }
    
    if(pContext->mConfigPara.mRoiNum>0 && pContext->mConfigPara.mRoiBgFrameRateEnable)
    {
        VENC_ROIBG_FRAME_RATE_S stRoiBgFrmRate;
        ret = AW_MPI_VENC_GetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("get roi bg frame rate:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
        if (pContext->mConfigPara.mRoiBgFrameRateAttenuation)
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate/pContext->mConfigPara.mRoiBgFrameRateAttenuation;
        }
        else
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate;
        }
        if(stRoiBgFrmRate.mDstFrmRate <= 0)
        {
            stRoiBgFrmRate.mDstFrmRate = 1;
        }
        ret = AW_MPI_VENC_SetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("set roi bg frame rate param:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
    }

    //test orl
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    MPP_CHN_S viChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        stRgnAttr.enType = ORL_RGN;
        ret = AW_MPI_RGN_Create(i, &stRgnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why create ORL region fail?[0x%x]", ret);
            break;
        }
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = ORL_RGN;
        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = i*120;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = i*60;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = 100;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = 50;
        stRgnChnAttr.unChnAttr.stOrlChn.mColor = 0xFF0000 >> ((i % 3)*8);
        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = i;
        ret = AW_MPI_RGN_AttachToChn(i, &viChn, &stRgnChnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why attach to vi channel[%d,%d] fail?", pContext->mViDev, pContext->mViChn);
        }
        
    }

    if(pContext->mConfigPara.mOrlNum > 0)
    {
        alogd("sleep 10s ...");
        sleep(10);
    }
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        ret = AW_MPI_RGN_Destroy(i);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why destory region:%d fail?", i);
        }
    }

    alogd("wait for test time %ds ...", pContext->mConfigPara.mTestDuration);

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    alogd("test time %ds is up, stop test", pContext->mConfigPara.mTestDuration); 
    
        result = 0;

    //check result
    if (0 < pContext->resetCameraCnt)
    {
        result = FAILURE;
        aloge("fatal error! no frame input, resetCameraCnt=%d", pContext->resetCameraCnt);
    }

    //stop msg queue thread
    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop; 
    put_message(&pContext->mMsgQueue, &stMsgCmd);//发送停止命令
    pthread_join(pContext->mMsgQueueThreadId, NULL);//结束消息线程
    alogd("start to free res");
err_out_3:
    stop(pContext); //清理操作
err_out_2:
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList)) //如果复用器通道列表不为空则进行清理
    {
        alogd("chn list not empty");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList) 
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }
        }

        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);
err_out_1:
    AW_MPI_SYS_Exit(); //退出MPP系统

    pthread_mutex_destroy(&pContext->mMuxChnListLock);//销毁复用器互斥锁
err_out_0:
    cdx_sem_deinit(&pContext->mSemExit); 
    message_destroy(&pContext->mMsgQueue);////销毁消息队列中的所有消息，并释放相关资源
    if ((pContext->mConfigPara.mEnableGdc) && (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode))
    {
        deInitGdcLdcProParam(pContext);
    }
    free(pContext);
    gpAV2MuxerData = pContext = NULL;
_err0:
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;    
}
/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleVenc"

#include "sample_venc.h"
#include "plat_log.h"
#include <time.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include <plat_math.h>
//#include "rgb_ctrl.h"
#include "sunxi_camera_v2.h"

static SAMPLE_VENC_PARA_S *pVencPara = NULL;

/* Not currently supported. */
//#define TEST_OSD

#ifdef TEST_OSD
#define ALIGN_XXB(y, x) (((x) + ((y)-1)) & ~((y)-1))

typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned short WORD;

#pragma pack(2)
typedef struct {
        WORD    bfType;
        DWORD   bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        DWORD   bfOffBits;
}BMPFILEHEADER_T;

#pragma pack(2)
typedef struct{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
}BMPINFOHEADER_T;

//#define USE_LOCAL_RGBPIC_FILE
#define MAX_OSD_RGB_PIC 12

#if 0
static RGB_PIC_S *g_pRgbPic[MAX_OSD_RGB_PIC] = {0};
#endif
static char *pOsdStr[] = {
    "123456901234567899876543210",
    "1234569012345678998765432101",
    "1234569012345678998765432102",
    "test test test test test0",
    "test test test test test1",
    "test test test test test2",
    "all winner tech, test osd osd0",
    "all winner tech, test osd osd1",
    "all winner tech, test osd osd2",
    "this is just a demo0",
    "this is just a demo1",
    "this is just a demo2",
};
static RECT_S regionRect[] = {
    {250, 330, 80, 20},
    {300, 360, 100, 20},
    {350, 390, 100, 20},
    {400, 420, 100, 20},
    {450, 450, 100, 20},
    {500, 490, 100, 20},
    {550, 520, 100, 20},
    {600, 560, 100, 20},
    {650, 590, 100, 20},
    {700, 620, 100, 20},
    {750, 650, 100, 20},
    {800, 980, 100, 20},
};
static unsigned int priority[] = {
    1, 20, 4, 15, 45, 34, 5, 22, 6, 2, 30, 21,
};

static void savebmp(char * pdata, char * bmp_file, int width, int height)
{
    //int size = width*height*3*sizeof(char);       // rgb888  - rgb24
    int size = width * height * 4 * sizeof(char);   // rgb8888 - rgb32

    /* RGB head */
    BMPFILEHEADER_T bfh;
    bfh.bfType = (WORD) 0x4d42;  //bm
    /* data size + first section size + second section size */
    bfh.bfSize = size + sizeof(BMPFILEHEADER_T) + sizeof(BMPINFOHEADER_T);
    bfh.bfReserved1 = 0; // reserved
    bfh.bfReserved2 = 0; // reserved
    bfh.bfOffBits = sizeof(BMPFILEHEADER_T) + sizeof(BMPINFOHEADER_T);

    // RGB info
    BMPINFOHEADER_T bih;
    bih.biSize = sizeof(BMPINFOHEADER_T);
    bih.biWidth = width;
    bih.biHeight = -height;
    bih.biPlanes = 1;
    //bih.biBitCount = 24;  // RGB888
    bih.biBitCount = 32;    // RGB8888
    bih.biCompression = 0;
    bih.biSizeImage = size;
    bih.biXPelsPerMeter = 2835;
    bih.biYPelsPerMeter = 2835;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;
    FILE * fp = NULL;
    fp = fopen(bmp_file, "wb");
    if (!fp) {
        return;
    }

    fwrite(&bfh, 8, 1, fp);
    fwrite(&bfh.bfReserved2, sizeof(bfh.bfReserved2), 1, fp);
    fwrite(&bfh.bfOffBits, sizeof(bfh.bfOffBits), 1, fp);
    fwrite(&bih, sizeof(BMPINFOHEADER_T), 1, fp);
    fwrite(pdata, size, 1, fp);
    fclose(fp);
}

static RGB_PIC_S *createRgbPic_1(char *pOsdStr)
{
    int ret;
    RGB_PIC_S *pRgb_pic;
    FONT_RGBPIC_S font_pic;
    static int rgb_pic_cnt = 0;

    pRgb_pic = (RGB_PIC_S *)malloc(sizeof(RGB_PIC_S));
    if (NULL == pRgb_pic)
    {
        return NULL;
    }

    memset(pRgb_pic, 0, sizeof(RGB_PIC_S));

    pRgb_pic->pic_addr = NULL;
    pRgb_pic->rgb_type = OSD_RGB_32;
    pRgb_pic->background[0]   = 0xC2;
    pRgb_pic->background[1]   = 0xC2;
    pRgb_pic->background[2]   = 0xC2;
    pRgb_pic->enable_mosaic   = 0;
    pRgb_pic->mosaic_size     = 0;
    pRgb_pic->mosaic_color[0] = 0xff;
    pRgb_pic->mosaic_color[1] = 0xff;
    pRgb_pic->mosaic_color[2] = 0xff;
    pRgb_pic->mosaic_color[3] = 0xff;

    //font_pic.font_type = FONT_SIZE_16;
    font_pic.font_type = FONT_SIZE_32;
    font_pic.rgb_type  = pRgb_pic->rgb_type;
    font_pic.background[0] = 0x60;
    font_pic.background[1] = 0x50;
    font_pic.background[2] = 0xBA;
    font_pic.background[3] = 0xAB;
    font_pic.foreground[0] = 0x30;
    font_pic.foreground[1] = 0x30;
    font_pic.foreground[2] = 0x50;
    font_pic.foreground[3] = 0x50;
    font_pic.enable_bg = 0;

    ret = create_font_rectangle(pOsdStr, &font_pic, pRgb_pic);
    if (ret != 0)
    {
        aloge("creat osd rect fail!!");
    }
    else
    {
        //char path[128] = {0};
        //sprintf(path, "/mnt/extsd/sample_venc/rgb_%d.bmp", rgb_pic_cnt);
        //savebmp(pRgb_pic->pic_addr, path, pRgb_pic->wide, pRgb_pic->high);
        //rgb_pic_cnt++;
    }

    return pRgb_pic;
}

static int releaseRgbPic_1(RGB_PIC_S *pRgbPic)
{
    return release_rgb_picture(pRgbPic);
}

static int createOSDRgbPic(void)
{
    int i;

    alogd("create osd rgb pic");

    //load_font_file(FONT_SIZE_16);
    load_font_file(FONT_SIZE_32);

    for(i=0; i < MAX_OSD_RGB_PIC; i++)
    {
        g_pRgbPic[i] = createRgbPic_1(pOsdStr[i]);
        if (NULL == g_pRgbPic[i])
        {
            aloge("create osd rgb pic fail");
            break;
        }
        alogd("rgb_pic_w=%d, h=%d, size=%d", g_pRgbPic[i]->wide, g_pRgbPic[i]->high, g_pRgbPic[i]->pic_size);
    }

    alogd("create [%d]osd rgb pic", i);
    return 0;
}

static int destroyOSDRgbPic(void)
{
    int i;

    for(i=0; i < MAX_OSD_RGB_PIC; i++)
    {
        if (g_pRgbPic[i] != NULL)
        {
            releaseRgbPic_1(g_pRgbPic[i]);
            g_pRgbPic[i] = NULL;
        }
    }

    unload_gb2312_font();
    return 0;
}

static int openVencOsd(SAMPLE_VENC_PARA_S *pVencPara)
{
    int ret;
    int i, index = 0;
    VENC_OVERLAY_INFO *pOverLayInfo;

    alogd("open venc osd");
    pOverLayInfo = (VENC_OVERLAY_INFO *)malloc(sizeof(VENC_OVERLAY_INFO));
    if (pOverLayInfo == NULL)
    {
        aloge("no mem! open venc ose fail!");
        return -1;
    }
    memset(pOverLayInfo, 0, sizeof(VENC_OVERLAY_INFO));

    createOSDRgbPic();

    pOverLayInfo->nBitMapColorType = BITMAP_COLOR_ARGB8888;

    for (i=0; i < MAX_OSD_RGB_PIC; i++)
    {
        if (g_pRgbPic[i] != NULL)
        {
            if (index == 0)
            {//for test cover
                pOverLayInfo->region[index].bOverlayType = OVERLAY_STYLE_COVER;
                pOverLayInfo->region[index].nPriority = 10;
                pOverLayInfo->region[index].rect.X = 100;
                pOverLayInfo->region[index].rect.Y = 100;
                pOverLayInfo->region[index].rect.Width = 1100;
                pOverLayInfo->region[index].rect.Height = 900;
                pOverLayInfo->region[index].coverYUV.bCoverY = 0x00;
                pOverLayInfo->region[index].coverYUV.bCoverU = 0x00;
                pOverLayInfo->region[index].coverYUV.bCoverV = 0x00;
            }
            else
            {
                pOverLayInfo->region[index].bOverlayType = OVERLAY_STYLE_NORMAL;
                pOverLayInfo->region[index].bRegionID = index;
                pOverLayInfo->region[index].nPriority = priority[i];
                pOverLayInfo->region[index].extraAlphaFlag = 0;
                pOverLayInfo->region[index].rect.X = regionRect[i].X;
                pOverLayInfo->region[index].rect.Y = regionRect[i].Y;
                pOverLayInfo->region[index].rect.Width = g_pRgbPic[i]->wide;
                pOverLayInfo->region[index].rect.Height = g_pRgbPic[i]->high;
                pOverLayInfo->region[index].pBitMapAddr = g_pRgbPic[i]->pic_addr;
                pOverLayInfo->region[index].nBitMapSize = g_pRgbPic[i]->pic_size;
                if (!(index % 2 ))
                {
                    pOverLayInfo->region[index].bOverlayType = OVERLAY_STYLE_LUMA_REVERSE;
                }
            }

            index++;
        }
    }

    pOverLayInfo->regionNum = index;
    ret = AW_MPI_VENC_setOsdMaskRegions(pVencPara->mVeChn, pOverLayInfo);
    if (ret != SUCCESS)
    {
        aloge("creat venc osd regions fail!!");
    }

    free(pOverLayInfo);
    return 0;
}

static int closeVencOsd(SAMPLE_VENC_PARA_S *pVencPara)
{
    AW_MPI_VENC_removeOsdMaskRegions(pVencPara->mVeChn);
    destroyOSDRgbPic();
    return 0;
}

static int openVencOsd_2(SAMPLE_VENC_PARA_S *pVencPara)
{
#define ARGB_DATA_FILE "/mnt/extsd/sample_venc/argb_pic.dat"
    FILE* rgbPic_hdle = NULL;
    int i, ret;
    int width  = 96;
    int height = 48;
    int num_rgbpic = 13;
    int width_aligh16 = ALIGN_XXB(16, width);
    int height_aligh16 = ALIGN_XXB(16, height);

    rgbPic_hdle = fopen(ARGB_DATA_FILE, "r");
    if (rgbPic_hdle == NULL)
    {
        aloge("get bitmap_hdle error\n");
        return -1;
    }

    alogd("open venc osd2");
    VENC_OVERLAY_INFO *pOverLayInfo;
    pOverLayInfo = (VENC_OVERLAY_INFO *)malloc(sizeof(VENC_OVERLAY_INFO));
    if (pOverLayInfo == NULL)
    {
        alogd("no mem! open venc osd fail!");
        fclose(rgbPic_hdle);
        return -1;
    }
    memset(pOverLayInfo, 0, sizeof(VENC_OVERLAY_INFO));

    pOverLayInfo->nBitMapColorType = BITMAP_COLOR_ARGB8888;

    static RECT_S osdRect[] =
    {
       {100, 200, 80, 20},
       {150, 240, 80, 20},
       {200, 300, 80, 20},
       {250, 330, 80, 20},
       {300, 360, 100, 20},
       {350, 390, 100, 20},
       {400, 420, 100, 20},
       {450, 450, 100, 20},
       {500, 490, 100, 20},
       {550, 520, 100, 20},
       {600, 560, 100, 20},
       {650, 590, 100, 20},
       {700, 620, 100, 20},
       {750, 650, 100, 20},
       {800, 980, 100, 20},
    };

    int cnt = 0;
    for (i = 0; i < num_rgbpic; i++)
    {
        int size = width_aligh16 * height_aligh16 * 4;
        pOverLayInfo->region[i].pBitMapAddr = malloc(size);
        if (pOverLayInfo->region[i].pBitMapAddr == NULL)
        {
            aloge("malloc bit_map_info[%d].argb_addr fail\n", i);
            break;
        }
        ret = fread(pOverLayInfo->region[i].pBitMapAddr, 1, size, rgbPic_hdle);
        if(ret != size)
        {
            aloge("read rgbpic[%d] error, ret value:%d\n", i, ret);
        }

        pOverLayInfo->region[i].nBitMapSize = size;
        pOverLayInfo->region[i].bRegionID = pOverLayInfo->region[i].nPriority = i;
        pOverLayInfo->region[i].bOverlayType = OVERLAY_STYLE_NORMAL;
        pOverLayInfo->region[i].rect.X = osdRect[i].X;
        pOverLayInfo->region[i].rect.Y = osdRect[i].Y;
        pOverLayInfo->region[i].rect.Width = width_aligh16;
        pOverLayInfo->region[i].rect.Height = height_aligh16;

        if (!(i % 2 ))
        {
            pOverLayInfo->region[i].bOverlayType = OVERLAY_STYLE_LUMA_REVERSE;
        }

        {
            //char path[128] = {0};
            //sprintf(path, "/mnt/extsd/sample_venc/rgb_%d.bmp", cnt);
            //savebmp(pOverLayInfo->region[i].pBitMapAddr, path, pOverLayInfo->region[i].rect.Width , pOverLayInfo->region[i].rect.Height);
        }

        alogd("###[%d]:x=%d,y=%d,w=%d,h=%d", i, pOverLayInfo->region[i].rect.X, pOverLayInfo->region[i].rect.Y, \
                pOverLayInfo->region[i].rect.Width, pOverLayInfo->region[i].rect.Height);

        cnt++;
    }
    pOverLayInfo->regionNum = cnt;

    ret = AW_MPI_VENC_setOsdMaskRegions(pVencPara->mVeChn, pOverLayInfo);
    if (ret != SUCCESS)
    {
        aloge("creat venc osd regions fail!!");
    }

    alogd("free bitmap resource");
    for (i = 0; i < num_rgbpic; i++)
    {
        if (pOverLayInfo->region[i].pBitMapAddr != NULL)
        {
            free(pOverLayInfo->region[i].pBitMapAddr);
            pOverLayInfo->region[i].pBitMapAddr = NULL;
        }
    }
    free(pOverLayInfo);

    if (rgbPic_hdle)
    {
        fclose(rgbPic_hdle);
    }

    return 0;
}

static int closeVencOsd_2(SAMPLE_VENC_PARA_S *pVencPara)
{
    AW_MPI_VENC_removeOsdMaskRegions(pVencPara->mVeChn);
    return 0;
}

static int enableVencOsd(SAMPLE_VENC_PARA_S *pVencPara, int enableFlag)
{
#ifdef USE_LOCAL_RGBPIC_FILE
    if (enableFlag)
    {
        openVencOsd_2(pVencPara);
    }
    else
    {
        closeVencOsd_2(pVencPara);
    }
#else
    if (enableFlag)
    {
        openVencOsd(pVencPara);
    }
    else
    {
        closeVencOsd(pVencPara);
    }
#endif
    return 0;
}
#endif /* TEST_OSD */

static int parseCmdLine(SAMPLE_VENC_PARA_S *pVencPara, int argc, char** argv)
{
    int ret = -1;

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

              strncpy(pVencPara->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pVencPara->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_venc.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21s"))
    {
        PicFormat = MM_PIXEL_FORMAT_AW_NV21S;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static PAYLOAD_TYPE_E getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        alogd("H.264");
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        alogd("H.265");
        EncType = PT_H265;
    }
    else if (!strcmp(ptr, "MJPEG"))
    {
        alogd("MJPEG");
        EncType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static ERRORTYPE loadConfigPara(SAMPLE_VENC_PARA_S *pVencPara, char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S mConf;

    ret = createConfParser(conf_path, &mConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    pVencPara->mConfigPara.mOsdEnable = 0;

    ptr = (char *)GetConfParaString(&mConf, VENC_CFG_SRC_FILE_STR, NULL);
    strcpy(pVencPara->mConfigPara.intputFile, ptr);
    pVencPara->mConfigPara.srcWidth  = GetConfParaInt(&mConf, VENC_CFG_SRC_WIDTH, 0);
    pVencPara->mConfigPara.srcHeight = GetConfParaInt(&mConf, VENC_CFG_SRC_HEIGHT, 0);

    ptr = (char *)GetConfParaString(&mConf, VENC_CFG_DST_FILE_STR, NULL);
    strcpy(pVencPara->mConfigPara.outputFile, ptr);
    pVencPara->mConfigPara.dstWidth  = GetConfParaInt(&mConf, VENC_CFG_DST_WIDTH, 0);
    pVencPara->mConfigPara.dstHeight = GetConfParaInt(&mConf, VENC_CFG_DST_HEIGHT, 0);

    alogd("intputFile=%s, srcWidth=%d, srcHeight=%d, outputFile=%s, dstWidth=%d, dstHeight=%d",
        pVencPara->mConfigPara.intputFile, pVencPara->mConfigPara.srcWidth, pVencPara->mConfigPara.srcHeight,
        pVencPara->mConfigPara.outputFile, pVencPara->mConfigPara.dstWidth, pVencPara->mConfigPara.dstHeight);

    pVencPara->mConfigPara.srcPixFmt = getPicFormatFromConfig(&mConf, VENC_CFG_SRC_PIXFMT);

    ptr = (char *)GetConfParaString(&mConf, VENC_CFG_COLOR_SPACE, NULL);
    if (!strcmp(ptr, "jpeg"))
    {
        pVencPara->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
    }
    else if (!strcmp(ptr, "rec709"))
    {
        pVencPara->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
    }
    else if (!strcmp(ptr, "rec709_part_range"))
    {
        pVencPara->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    }
    else
    {
        aloge("fatal error! wrong color space:%s", ptr);
        pVencPara->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
    }

    pVencPara->mConfigPara.mVideoEncoderFmt = getEncoderTypeFromConfig(&mConf, VENC_CFG_ENCODER);
    pVencPara->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, VENC_CFG_PROFILE, 0);
    pVencPara->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, VENC_CFG_FRAMERATE, 0);
    pVencPara->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, VENC_CFG_BITRATE, 0);

    int rotate = GetConfParaInt(&mConf, VENC_CFG_ROTATE, 0);
    if (0 == rotate)
    {
        pVencPara->mConfigPara.rotate = ROTATE_NONE;
    }
    else if (90 == rotate)
    {
        pVencPara->mConfigPara.rotate = ROTATE_90;
    }
    else if (180 == rotate)
    {
        pVencPara->mConfigPara.rotate = ROTATE_180;
    }
    else if (270 == rotate)
    {
        pVencPara->mConfigPara.rotate = ROTATE_270;
    }
    else
    {
        pVencPara->mConfigPara.rotate = ROTATE_NONE;
    }

    alogd("srcPixFmt=%d, mVideoEncoderFmt=%d, mEncUseProfile=%d, mVideoFrameRate=%d, mVideoBitRate=%d, rotate=%d",
        pVencPara->mConfigPara.srcPixFmt, pVencPara->mConfigPara.mVideoEncoderFmt,
        pVencPara->mConfigPara.mEncUseProfile, pVencPara->mConfigPara.mVideoFrameRate,
        pVencPara->mConfigPara.mVideoBitRate, pVencPara->mConfigPara.rotate);

    pVencPara->mConfigPara.mRcMode = GetConfParaInt(&mConf, VENC_CFG_RC_MODE, 0);
    pVencPara->mConfigPara.mGopMode = GetConfParaInt(&mConf, VENC_CFG_GOP_MODE, 0);
    pVencPara->mConfigPara.mGopSize = GetConfParaInt(&mConf, VENC_CFG_GOP_SIZE, 0);
    pVencPara->mConfigPara.mProductMode = GetConfParaInt(&mConf, VENC_CFG_PRODUCT_MODE, 0);
    pVencPara->mConfigPara.mSensorType = GetConfParaInt(&mConf, VENC_CFG_SENSOR_TYPE, 0);
    pVencPara->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, VENC_CFG_KEY_FRAME_INTERVAL, 0);
    
    /* VBR/CBR */
    pVencPara->mConfigPara.mInitQp = GetConfParaInt(&mConf, VENC_CFG_INIT_QP, 0);
    pVencPara->mConfigPara.mMinIQp = GetConfParaInt(&mConf, VENC_CFG_MIN_I_QP, 0);
    pVencPara->mConfigPara.mMaxIQp = GetConfParaInt(&mConf, VENC_CFG_MAX_I_QP, 0);
    pVencPara->mConfigPara.mMinPQp = GetConfParaInt(&mConf, VENC_CFG_MIN_P_QP, 0);
    pVencPara->mConfigPara.mMaxPQp = GetConfParaInt(&mConf, VENC_CFG_MAX_P_QP, 0);
    /* FixQp */
    pVencPara->mConfigPara.mIQp = GetConfParaInt(&mConf, VENC_CFG_I_QP, 0);
    pVencPara->mConfigPara.mPQp = GetConfParaInt(&mConf, VENC_CFG_P_QP, 0);
    /* VBR */
    pVencPara->mConfigPara.mMovingTh = GetConfParaInt(&mConf, VENC_CFG_MOVING_TH, 0);
    pVencPara->mConfigPara.mQuality = GetConfParaInt(&mConf, VENC_CFG_QUALITY, 0);
    pVencPara->mConfigPara.mPBitsCoef = GetConfParaInt(&mConf, VENC_CFG_P_BITS_COEF, 0);
    pVencPara->mConfigPara.mIBitsCoef = GetConfParaInt(&mConf, VENC_CFG_I_BITS_COEF, 0);

    pVencPara->mConfigPara.mTestDuration = GetConfParaInt(&mConf, VENC_CFG_TEST_DURATION, 0);

    pVencPara->mConfigPara.mbEnableGdc = GetConfParaInt(&mConf, VENC_CFG_ENABLE_GDC, 0);
    pVencPara->mConfigPara.mbGdcMirror = GetConfParaInt(&mConf, VENC_CFG_GDC_MIRROR, 0);
    pVencPara->mConfigPara.mGdcWarpMode = (eGdcWarpType)GetConfParaInt(&mConf, VENC_CFG_GDC_WARP_MODE, 0);
    pVencPara->mConfigPara.mGdcMountType = (eGdcMountType)GetConfParaInt(&mConf, VENC_CFG_GDC_MOUNT_TYPE, 0);
    if ((pVencPara->mConfigPara.mbEnableGdc) && (Gdc_Warp_LDC_Pro == pVencPara->mConfigPara.mGdcWarpMode))
    {
        ptr = (char *)GetConfParaString(&mConf, VENC_CFG_GDC_LDC_Pro_Lut_Bin, NULL);
        int str_len = strlen(ptr);
        strncpy(pVencPara->mConfigPara.mGdcLDCProLutBin, ptr, str_len+1);
        alogd("use LDC Pro lut bin[%s]", pVencPara->mConfigPara.mGdcLDCProLutBin);
    }
    if (pVencPara->mVencChnAttr.VeAttr.PixelFormat >= MM_PIXEL_FORMAT_YUV_AW_AFBC \
        && pVencPara->mVencChnAttr.VeAttr.PixelFormat <= MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X)
    {
        if (pVencPara->mConfigPara.mbGdcMirror)
        {
            alogw("don't support mirror when input format is LBC");
            pVencPara->mConfigPara.mbGdcMirror = FALSE;
        }
    }

    alogd("mRcMode=%d, mGopMode=%d, mGopSize=%d, mProductMode=%d, mSensorType=%d, mKeyFrameInterval=%d",
        pVencPara->mConfigPara.mRcMode, pVencPara->mConfigPara.mGopMode,
        pVencPara->mConfigPara.mGopSize, pVencPara->mConfigPara.mProductMode,
        pVencPara->mConfigPara.mSensorType, pVencPara->mConfigPara.mKeyFrameInterval);

    alogd("mInitQp=%d, mMinIQp=%d, mMaxIQp=%d, mMinPQp=%d, mMaxPQp=%d",
        pVencPara->mConfigPara.mInitQp, pVencPara->mConfigPara.mMinIQp,
        pVencPara->mConfigPara.mMaxIQp, pVencPara->mConfigPara.mMinPQp,
        pVencPara->mConfigPara.mMaxPQp);

    alogd("mIQp=%d, mPQp=%d", pVencPara->mConfigPara.mIQp, pVencPara->mConfigPara.mPQp);

    alogd("mMovingTh=%d, mQuality=%d, mPBitsCoef=%d, mIBitsCoef=%d, mTestDuration=%d",
        pVencPara->mConfigPara.mMovingTh, pVencPara->mConfigPara.mQuality,
        pVencPara->mConfigPara.mPBitsCoef, pVencPara->mConfigPara.mIBitsCoef,
        pVencPara->mConfigPara.mTestDuration);

    alogd("enable gdc=%d, mirror=%d, warp mode=%d, mount type=%d", \
        pVencPara->mConfigPara.mbEnableGdc, pVencPara->mConfigPara.mbGdcMirror, \
        pVencPara->mConfigPara.mGdcWarpMode, pVencPara->mConfigPara.mGdcMountType);

    destroyConfParser(&mConf);

    return SUCCESS;
}

{
static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
    SAMPLE_VENC_PARA_S *pVencPara = (SAMPLE_VENC_PARA_S *)cookie;
    VIDEO_FRAME_INFO_S *pFrame = (VIDEO_FRAME_INFO_S *)pEventData;

    switch (event)
    {
    case MPP_EVENT_RELEASE_VIDEO_BUFFER:
        if (pFrame != NULL)
        {
            pthread_mutex_lock(&pVencPara->mInBuf_Q.mUseListLock);
            if (!list_empty(&pVencPara->mInBuf_Q.mUseList))
            {
                IN_FRAME_NODE_S *pEntry, *pTmp;
                list_for_each_entry_safe(pEntry, pTmp, &pVencPara->mInBuf_Q.mUseList, mList)//遍历使用中列表
                {
                    if (pEntry->mFrame.mId == pFrame->mId)
                    {
                        pthread_mutex_lock(&pVencPara->mInBuf_Q.mIdleListLock);
                        list_move_tail(&pEntry->mList, &pVencPara->mInBuf_Q.mIdleList);//将节点移动到空闲列表
                        pthread_mutex_unlock(&pVencPara->mInBuf_Q.mIdleListLock);
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&pVencPara->mInBuf_Q.mUseListLock);
        }
        break;

    default:
        break;
    }

    return SUCCESS;
}
//根据输入参数选择H264编码质量
static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;//基本级别
        break;

    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;//主要级别
        break;

    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;//高级别
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
        profile = (unsigned int)H265_PROFILE_MAIN;//主要级别
        break;

    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;//支持10位色深的主要级别
        break;

    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;//STI (Screen Content Coding) 11级别，适合屏幕内容编码
        break;

    default:
        break;
    }
    return profile;
}

static void configGdcLDCParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    /* LDC test parameters use GC4663 lens parameters */
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->fx = 1718.15f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1582.29f;
    pGdcParam->fy_scale = 1593.43f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.427011f;
    pGdcParam->distCoef_wide_ra[1] = 0.196667f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.000143f;
    pGdcParam->distCoef_wide_ta[1] = 0.002161f;
    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    /* LDC correction parameters */
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eLensDistModel = pVencPara->mConfigPara.mGdcLensDistModel;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcLDCProParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    pGdcParam->lut_data_buf = pVencPara->mpGdcLdcProLutData;
    pGdcParam->lut_data_size = pVencPara->mGdcLdcProLutDataLen;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcPano180Param(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    /* only support mount wall */
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Pano180 correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->fanDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
}

static void configGdcPano360Param(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    if (Gdc_Mount_Wall == pVencPara->mConfigPara.mGdcMountType)
    {
        alogw("pano 180 don't support mount wall");
    }
    alogd("configGdcPano360Param");

    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->cx = 960;
    pGdcParam->cx = 960;
    pGdcParam->distCoef_fish_k[0] = 611;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Pano360 correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->scale = 100;
    pGdcParam->innerRadius = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcNormalParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->innerRadius = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Normal correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 50;
    pGdcParam->scale = 100;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcFish2WideParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->innerRadius = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Normal correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcPerspectiveParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    /* LDC test parameters use GC4663 lens parameters */
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pVencPara->mConfigPara.dstWidth;
    pGdcParam->calib_height = pVencPara->mConfigPara.dstHeight;

    /* lens paramters */
    pGdcParam->fx = 1718.15f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1582.29f;
    pGdcParam->fy_scale = 1593.43f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.427011f;
    pGdcParam->distCoef_wide_ra[1] = 0.196667f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.000143f;
    pGdcParam->distCoef_wide_ta[1] = 0.002161f;
    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    /* LDC correction parameters */
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eLensDistModel = pVencPara->mConfigPara.mGdcLensDistModel;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;

    /* Perspective parameters */
    pGdcParam->perspectiveProjMat[0] = 0.8f;
    pGdcParam->perspectiveProjMat[1] = 0.1f;
    pGdcParam->perspectiveProjMat[2] = 7.0f;
    pGdcParam->perspectiveProjMat[3] = 0.01f;
    pGdcParam->perspectiveProjMat[4] = 0.87;
    pGdcParam->perspectiveProjMat[5] = 5.0f;
    pGdcParam->perspectiveProjMat[6] = 0.0f;
    pGdcParam->perspectiveProjMat[7] = 0.0;
    pGdcParam->perspectiveProjMat[8] = 1.0f;
    pGdcParam->perspFunc = Gdc_Persp_LDC;
}

static void configGdcBirdsEyeParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pVencPara->mConfigPara.mGdcWarpMode;
    pGdcParam->bMirror = pVencPara->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = 1920;
    pGdcParam->calib_height = 1080;
    pGdcParam->birdsImg_width = pVencPara->mConfigPara.dstWidth;
    pGdcParam->birdsImg_height = pVencPara->mConfigPara.dstHeight;

    /* need set LDC parameters, this just show how to set birds eyeee paramters */
    /* lens paramters */
    pGdcParam->fx = 1891.85f / 2.0f;
    pGdcParam->fy = 1891.85f / 2.0f;
    pGdcParam->cx = 1928.20f / 2.0f;
    pGdcParam->cy = 1115.24f / 2.0f;
    pGdcParam->fx_scale = 1899.46f / 2.0f;
    pGdcParam->fy_scale = 1899.46f / 2.0f;
    pGdcParam->cx_scale = 1919.50f / 2.0f;
    pGdcParam->cy_scale = 1919.50f / 2.0f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.3560;
    pGdcParam->distCoef_wide_ra[1] = 0.1886f;
    pGdcParam->distCoef_wide_ra[2] = -0.0656f;
    pGdcParam->distCoef_wide_ta[0] = 0.00025f;
    pGdcParam->distCoef_wide_ta[1] = 0.00006f;
    pGdcParam->distCoef_fish_k[0] = -0.246f;
    pGdcParam->distCoef_fish_k[1] = -0.164f;
    pGdcParam->distCoef_fish_k[2] = 0.0207f;
    pGdcParam->distCoef_fish_k[3] = -0.0074f;

    /* BirdsEye correction parameters */
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = -100;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->mountHeight = 0.85;
    pGdcParam->roll = -21;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->roiDist_ahead = 4.5f;
    pGdcParam->roiDist_left = -1.5f;
    pGdcParam->roiDist_right = 1.5f;
    pGdcParam->roiDist_bottom = 0.65f;
    pGdcParam->eMountMode = pVencPara->mConfigPara.mGdcMountType;
}

static void configGdcParam(SAMPLE_VENC_PARA_S *pVencPara, sGdcParam *pGdcParam)
{
    switch (pVencPara->mConfigPara.mGdcWarpMode)
    {
        case Gdc_Warp_LDC:
            configGdcLDCParam(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_LDC_Pro:
            configGdcLDCProParam(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_Pano180:
            configGdcPano180Param(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_Pano360:
            configGdcPano360Param(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_Normal:
            configGdcNormalParam(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_Fish2Wide:
            configGdcFish2WideParam(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_Perspective:
            configGdcPerspectiveParam(pVencPara, pGdcParam);
            break;
        case Gdc_Warp_BirdsEye:
            configGdcBirdsEyeParam(pVencPara, pGdcParam);
            break;
        default:
            aloge("fatal error! unsupport gdc warp mode[%d] disbale gdc!", pVencPara->mConfigPara.mGdcWarpMode);
            break;
    }
}

static ERRORTYPE configVencChnAttr(SAMPLE_VENC_PARA_S *pVencPara)
{
    memset(&pVencPara->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pVencPara->mVencChnAttr.VeAttr.Type = pVencPara->mConfigPara.mVideoEncoderFmt;//编码格式，例如H264/H265

    pVencPara->mVencChnAttr.VeAttr.SrcPicWidth  = pVencPara->mConfigPara.srcWidth;//图像宽度
    pVencPara->mVencChnAttr.VeAttr.SrcPicHeight = pVencPara->mConfigPara.srcHeight;//图像高度
    pVencPara->mVencChnAttr.VeAttr.Rotate = pVencPara->mConfigPara.rotate;//旋转角度

    if (pVencPara->mConfigPara.srcPixFmt == MM_PIXEL_FORMAT_YUV_AW_AFBC)//如果图像格式是全志自定的数据格式
    {
        alogd("aw_afbc");
        pVencPara->mVencChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
        pVencPara->mConfigPara.dstWidth = pVencPara->mConfigPara.srcWidth;  //cannot compress_encoder
        pVencPara->mConfigPara.dstHeight = pVencPara->mConfigPara.srcHeight; //cannot compress_encoder
    }
    else
    {
        pVencPara->mVencChnAttr.VeAttr.PixelFormat = pVencPara->mConfigPara.srcPixFmt;//图像格式 NV12/NV21
    }
    pVencPara->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;

    pVencPara->mVencRcParam.product_mode = pVencPara->mConfigPara.mProductMode;//产品模式 0：正常   1：IPC
    pVencPara->mVencChnAttr.VeAttr.MaxKeyInterval = pVencPara->mConfigPara.mKeyFrameInterval;//关键帧间隔
    pVencPara->mVencRcParam.sensor_type = pVencPara->mConfigPara.mSensorType;//设置传感器类型，根据不同传感器的输出进行编码参数的调整
    pVencPara->mVencChnAttr.GopAttr.enGopMode = pVencPara->mConfigPara.mGopMode;//一组图像帧模式
    pVencPara->mVencChnAttr.GopAttr.mGopSize = pVencPara->mConfigPara.mGopSize;//一组图像帧大小
    pVencPara->mVencChnAttr.VeAttr.mColorSpace = pVencPara->mConfigPara.mColorSpace;//颜色空间

    if (PT_H264 == pVencPara->mVencChnAttr.VeAttr.Type)//如果编码格式为H264
    {
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pVencPara->mConfigPara.mEncUseProfile);//编码的质量
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_Default;//H.264编码器的级别
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pVencPara->mConfigPara.dstWidth;//图像的宽度
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pVencPara->mConfigPara.dstHeight;//图像的高度
        pVencPara->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pVencPara->mConfigPara.mRcMode)
        {
        case 1:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR; //设置为可变码率模式
            pVencPara->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pVencPara->mConfigPara.mVideoBitRate;//设置最大比特率为
            pVencPara->mVencRcParam.ParamH264Vbr.mMinQp = pVencPara->mConfigPara.mMinIQp;//最小的I帧QP值
            pVencPara->mVencRcParam.ParamH264Vbr.mMaxQp = pVencPara->mConfigPara.mMaxIQp;//最大的I帧QP值
            pVencPara->mVencRcParam.ParamH264Vbr.mMinPqp = pVencPara->mConfigPara.mMinPQp;//最小的P帧QP值
            pVencPara->mVencRcParam.ParamH264Vbr.mMaxPqp = pVencPara->mConfigPara.mMaxPQp;//最大的P帧QP值
            pVencPara->mVencRcParam.ParamH264Vbr.mQpInit = pVencPara->mConfigPara.mInitQp;//设置初始QP值
            pVencPara->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;//禁用MB级别的QP限制
            pVencPara->mVencRcParam.ParamH264Vbr.mMovingTh = pVencPara->mConfigPara.mMovingTh;//设置移动检测阈值
            pVencPara->mVencRcParam.ParamH264Vbr.mQuality = pVencPara->mConfigPara.mQuality;//设置视频质量参数
            pVencPara->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = pVencPara->mConfigPara.mIBitsCoef;//设置I帧的比特率系数
            pVencPara->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = pVencPara->mConfigPara.mPBitsCoef;//设置P帧的比特率系数
            break;
        case 2:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;//固定QP模式
            pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pVencPara->mConfigPara.mIQp;//设置I帧的QP值
            pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pVencPara->mConfigPara.mPQp;//设置P帧的QP值
            break;
        case 3:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;//不支持测试QP映射模式
            alogd("The sample is not support test QPMAP, please use sample_QPMAP/sample_venc_QPMAP.\n");
            break;
        case 0:
        default:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;//恒定码率模式
            pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pVencPara->mConfigPara.mVideoFrameRate;//源帧率
            pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.fr32DstFrmRate = pVencPara->mConfigPara.mVideoFrameRate;//目标帧率
            pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pVencPara->mConfigPara.mVideoBitRate;//比特率
            pVencPara->mVencRcParam.ParamH264Cbr.mMinQp = pVencPara->mConfigPara.mMinIQp;//设置最小的I帧QP值
            pVencPara->mVencRcParam.ParamH264Cbr.mMaxQp = pVencPara->mConfigPara.mMaxIQp;//设置最大的I帧QP值
            pVencPara->mVencRcParam.ParamH264Cbr.mMinPqp = pVencPara->mConfigPara.mMinPQp;//设置最小的P帧QP值
            pVencPara->mVencRcParam.ParamH264Cbr.mMaxPqp = pVencPara->mConfigPara.mMaxPQp;//设置最小的I帧QP值
            pVencPara->mVencRcParam.ParamH264Cbr.mQpInit = pVencPara->mConfigPara.mInitQp;//设置初始QP值
            pVencPara->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;//禁用MB级别的QP限制
            break;
        }
    }
    else if (PT_H265 == pVencPara->mVencChnAttr.VeAttr.Type)//如果编码格式为H265
    {
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pVencPara->mConfigPara.mEncUseProfile);
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_Default;
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pVencPara->mConfigPara.dstWidth;
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pVencPara->mConfigPara.dstHeight;
        pVencPara->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pVencPara->mConfigPara.mRcMode)
        {
        case 1:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pVencPara->mConfigPara.mVideoBitRate;
            pVencPara->mVencRcParam.ParamH265Vbr.mMinQp = pVencPara->mConfigPara.mMinIQp;
            pVencPara->mVencRcParam.ParamH265Vbr.mMaxQp = pVencPara->mConfigPara.mMaxIQp;
            pVencPara->mVencRcParam.ParamH265Vbr.mMinPqp = pVencPara->mConfigPara.mMinPQp;
            pVencPara->mVencRcParam.ParamH265Vbr.mMaxPqp = pVencPara->mConfigPara.mMaxPQp;
            pVencPara->mVencRcParam.ParamH265Vbr.mQpInit = pVencPara->mConfigPara.mInitQp;
            pVencPara->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
            pVencPara->mVencRcParam.ParamH265Vbr.mMovingTh = pVencPara->mConfigPara.mMovingTh;
            pVencPara->mVencRcParam.ParamH265Vbr.mQuality = pVencPara->mConfigPara.mQuality;
            pVencPara->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = pVencPara->mConfigPara.mIBitsCoef;
            pVencPara->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = pVencPara->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pVencPara->mConfigPara.mIQp;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pVencPara->mConfigPara.mPQp;
            break;
        case 3:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265QPMAP;
            alogd("The sample is not support test QPMAP, please use sample_QPMAP/sample_venc_QPMAP.\n");
            break;
        case 0:
        default:
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pVencPara->mConfigPara.mVideoFrameRate;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.fr32DstFrmRate = pVencPara->mConfigPara.mVideoFrameRate;
            pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVencPara->mConfigPara.mVideoBitRate;
            pVencPara->mVencRcParam.ParamH265Cbr.mMinQp = pVencPara->mConfigPara.mMinIQp;
            pVencPara->mVencRcParam.ParamH265Cbr.mMaxQp = pVencPara->mConfigPara.mMaxIQp;
            pVencPara->mVencRcParam.ParamH265Cbr.mMinPqp = pVencPara->mConfigPara.mMinPQp;
            pVencPara->mVencRcParam.ParamH265Cbr.mMaxPqp = pVencPara->mConfigPara.mMaxPQp;
            pVencPara->mVencRcParam.ParamH265Cbr.mQpInit = pVencPara->mConfigPara.mInitQp;
            pVencPara->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 0;
            break;
        }
    }
    else if (PT_MJPEG == pVencPara->mVencChnAttr.VeAttr.Type)
    {
        pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pVencPara->mConfigPara.dstWidth;//输出图像宽度
        pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pVencPara->mConfigPara.dstHeight;//输出图像高度
        pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;//恒定码流模式
        pVencPara->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVencPara->mConfigPara.mVideoBitRate;//比特率
    }

    if (pVencPara->mConfigPara.mbEnableGdc)
    {
        if (pVencPara->mVencChnAttr.VeAttr.PixelFormat >= MM_PIXEL_FORMAT_YUV_AW_AFBC \
            && pVencPara->mVencChnAttr.VeAttr.PixelFormat <= MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X)
        {
            if (pVencPara->mConfigPara.mGdcWarpMode != Gdc_Warp_LDC \
                || pVencPara->mConfigPara.mGdcWarpMode != Gdc_Warp_LDC_Pro)
            {
                aloge("fatal error! only support LDC and LDC Pro whne input format is LBC");
                pVencPara->mConfigPara.mbEnableGdc = FALSE;
                return SUCCESS;
            }
        }
        pVencPara->mVencChnAttr.GdcAttr.bGDC_en = pVencPara->mConfigPara.mbEnableGdc?1:0;
        configGdcParam(pVencPara, &pVencPara->mVencChnAttr.GdcAttr);
    }

    return SUCCESS;
}

{
static ERRORTYPE createVencChn(SAMPLE_VENC_PARA_S *pVencPara)
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pVencPara);//配置编码参数
    pVencPara->mVeChn = 0;
    while(pVencPara->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pVencPara->mVeChn, &pVencPara->mVencChnAttr);//创建编码通道
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pVencPara->mVeChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pVencPara->mVeChn);
            pVencPara->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pVencPara->mVeChn, ret);
            pVencPara->mVeChn++;
        }
    }

    if (!nSuccessFlag)
    {
        pVencPara->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        ret = AW_MPI_VENC_SetRcParam(pVencPara->mVeChn, &pVencPara->mVencRcParam);//设置编码速率控制参考参数
        if (ret != SUCCESS)
        {
            aloge("fatal error! venc chn[%d] set rc param fail[0x%x]!", pVencPara->mVeChn, ret);
        }

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pVencPara->mConfigPara.mVideoFrameRate;
        AW_MPI_VENC_SetFrameRate(pVencPara->mVeChn, &stFrameRate);//设置编码通道帧率控制属性

        if (PT_MJPEG == pVencPara->mVencChnAttr.VeAttr.Type)//如果MJPEG编码格式
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = pVencPara->mConfigPara.mQuality;//设置编码质量
            AW_MPI_VENC_SetJpegParam(pVencPara->mVeChn, &stJpegParam);//设置JPEG 协议编码通道的高级参数
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVencPara;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pVencPara->mVeChn, &cbInfo);//设置编码回调函数
        return SUCCESS;
    }
}
//创建视频帧管理器
static int vencBufMgrCreate(int frmNum, int width, int height, INPUT_BUF_Q *pBufList, BOOL isAwAfbc)
{
    unsigned int size = 0;
    unsigned int afbc_header;

    /**
     * The buffer size must be 16 aligned.
     */
    size = AWALIGN(width, 16) * AWALIGN(height, 16);

    INIT_LIST_HEAD(&pBufList->mIdleList);//初始化空闲列表
    INIT_LIST_HEAD(&pBufList->mReadyList);//初始化就绪列表
    INIT_LIST_HEAD(&pBufList->mUseList);//初始化使用中列表

    pthread_mutex_init(&pBufList->mIdleListLock, NULL);
    pthread_mutex_init(&pBufList->mReadyListLock, NULL);
    pthread_mutex_init(&pBufList->mUseListLock, NULL);

    if (isAwAfbc)
    {
        afbc_header = ((width +127)>>7)*((height+31)>>5)*96;
    }

    int i;
    for (i = 0; i < frmNum; ++i) //创建视频帧节点
    {
        IN_FRAME_NODE_S *pFrameNode = (IN_FRAME_NODE_S*)malloc(sizeof(IN_FRAME_NODE_S));
        if (pFrameNode == NULL)
        {
            aloge("alloc IN_FRAME_NODE_S error!");
            break;
        }
        memset(pFrameNode, 0, sizeof(IN_FRAME_NODE_S));

        if (!isAwAfbc)//如果不是全志自定格式
        {
            pFrameNode->mFrame.VFrame.mpVirAddr[0] = (void *)venc_allocMem(size);//申请YUV中的Y内存
            if (pFrameNode->mFrame.VFrame.mpVirAddr[0] == NULL)
            {
                aloge("alloc y_vir_addr size %d error!", size);
                free(pFrameNode);
                break;
            }
            pFrameNode->mFrame.VFrame.mpVirAddr[1] = (void *)venc_allocMem(size/2);//申请YUV中的UV内存
            if (pFrameNode->mFrame.VFrame.mpVirAddr[1] == NULL)
            {
                aloge("alloc uv_vir_addr size %d error!", size/2);
                venc_freeMem(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
                pFrameNode->mFrame.VFrame.mpVirAddr[0] = NULL;
                free(pFrameNode);
                break;
            }
            //获取内存对应的物理地址
            pFrameNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
            pFrameNode->mFrame.VFrame.mPhyAddr[1] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[1]);
        }
        else
        {
            pFrameNode->mFrame.VFrame.mpVirAddr[0] = (void *)venc_allocMem(size+size/2+afbc_header);
            if (pFrameNode->mFrame.VFrame.mpVirAddr[0] == NULL)
            {
                aloge("fail to alloc aw_afbc y_vir_addr!");
                free(pFrameNode);
                break;
            }
            pFrameNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
        }

        pFrameNode->mFrame.mId = i;
        list_add_tail(&pFrameNode->mList, &pBufList->mIdleList);//创建的视频帧节点加入空闲列表
        pBufList->mFrameNum++;
    }

    if (pBufList->mFrameNum == 0)
    {
        aloge("alloc no node!!");
        return -1;
    }
    else
    {
        return 0;
    }
}

static int vencBufMgrDestroy(INPUT_BUF_Q *pBufList)
{
    IN_FRAME_NODE_S *pEntry, *pTmp;
    int frmnum = 0;

    if (pBufList == NULL)
    {
        aloge("pBufList null");
        return -1;
    }

    pthread_mutex_lock(&pBufList->mUseListLock);
    if (!list_empty(&pBufList->mUseList))//判断使用中列表是否为空
    {
        aloge("error! SendingFrmList should be 0! maybe some frames not release!");
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mUseList, mList)//遍历使用中列表
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);//释放Y内存
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);//释放VU内存
            }
            free(pEntry);//释放帧节点
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mUseListLock);

    pthread_mutex_lock(&pBufList->mReadyListLock);
    if (!list_empty(&pBufList->mReadyList))//判断就绪列表是否为空
    {
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mReadyList, mList)//遍历就绪列表
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);//释放Y内存
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);//释放VU内存
            }
            free(pEntry);//释放帧节点
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mReadyListLock);


    pthread_mutex_lock(&pBufList->mIdleListLock);
    if (!list_empty(&pBufList->mIdleList))//判断空闲列表是否为空
    {
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mIdleList, mList)//遍历空闲列表
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);//释放Y内存
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);//释放VU内存
            }
            free(pEntry);//释放帧节点
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mIdleListLock);

    if (frmnum != pBufList->mFrameNum)
    {
        aloge("Fatal error! frame node number is not match[%d][%d]", frmnum, pBufList->mFrameNum);
    }

    pthread_mutex_destroy(&pBufList->mIdleListLock);//销毁互斥锁
    pthread_mutex_destroy(&pBufList->mReadyListLock);
    pthread_mutex_destroy(&pBufList->mUseListLock);

    return 0;
}

static int Yu12ToNv21(int width, int height, char *addr_uv, char *addr_tmp_uv)
{
    int i, chroma_bytes;
    char *u_addr = NULL;
    char *v_addr = NULL;
    char *tmp_addr = NULL;

    chroma_bytes = width*height/4;

    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for (i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(v_addr++);
        *(tmp_addr++) = *(u_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}

static void *readInFrameThread(void *pThreadData)
{
    int ret;
    int read_len;
    int size1, size2;
    SAMPLE_VENC_PARA_S *pVencPara = (SAMPLE_VENC_PARA_S*)pThreadData;
    uint64_t curPts = 0;
    IN_FRAME_NODE_S *pFrame;

    fseek(pVencPara->mConfigPara.fd_in, 0, SEEK_SET);

    while (!pVencPara->mOverFlag)
    {
        pthread_mutex_lock(&pVencPara->mInBuf_Q.mReadyListLock);
        while (1)
        {
            if (list_empty(&pVencPara->mInBuf_Q.mReadyList))//如果就绪列表为空就跳出循环
            {
                //alogd("ready list empty");
                break;
            }

            pFrame = list_first_entry(&pVencPara->mInBuf_Q.mReadyList, IN_FRAME_NODE_S, mList);//取出就绪列表中的第一个视频帧节点
            pthread_mutex_lock(&pVencPara->mInBuf_Q.mUseListLock);
            list_move_tail(&pFrame->mList, &pVencPara->mInBuf_Q.mUseList);//将节点移动到使用中列表里
            pthread_mutex_unlock(&pVencPara->mInBuf_Q.mUseListLock);
            ret = AW_MPI_VENC_SendFrame(pVencPara->mVeChn, &pFrame->mFrame, 0);//向编码器传送待编码图像数据帧
            if (ret == SUCCESS)
            {
                //alogd("send frame[%d] to enc", pFrame->mFrame.mId);
            }
            else
            {
                alogd("send frame[%d] fail", pFrame->mFrame.mId);
                pthread_mutex_lock(&pVencPara->mInBuf_Q.mUseListLock);
                list_move(&pFrame->mList, &pVencPara->mInBuf_Q.mReadyList);//将视频帧节点从使用中列表删除并添加到就绪列表的头部
                pthread_mutex_unlock(&pVencPara->mInBuf_Q.mUseListLock);
                break;
            }
        }
        pthread_mutex_unlock(&pVencPara->mInBuf_Q.mReadyListLock);

        if (feof(pVencPara->mConfigPara.fd_in))//判断是否读取到文件末尾
        {
            alogd("read to end");
            cdx_sem_up(&pVencPara->mSemExit);
            break;
        }

        pthread_mutex_lock(&pVencPara->mInBuf_Q.mIdleListLock);
        if (!list_empty(&pVencPara->mInBuf_Q.mIdleList))//检查空闲列表是否为空
        {   //获取空闲列表中的第一个帧节点
            IN_FRAME_NODE_S *pFrame = list_first_entry(&pVencPara->mInBuf_Q.mIdleList, IN_FRAME_NODE_S, mList);
            if (pFrame != NULL)
            {   //设置帧的基本属性
                pFrame->mFrame.VFrame.mWidth  = pVencPara->mConfigPara.srcWidth;//输入图像宽度
                pFrame->mFrame.VFrame.mHeight = pVencPara->mConfigPara.srcHeight;//输入图像高度
                pFrame->mFrame.VFrame.mOffsetLeft = 0;
                pFrame->mFrame.VFrame.mOffsetTop  = 0;
                pFrame->mFrame.VFrame.mOffsetRight = pFrame->mFrame.VFrame.mOffsetLeft + pFrame->mFrame.VFrame.mWidth;
                pFrame->mFrame.VFrame.mOffsetBottom = pFrame->mFrame.VFrame.mOffsetTop + pFrame->mFrame.VFrame.mHeight;
                pFrame->mFrame.VFrame.mField  = VIDEO_FIELD_FRAME;//视频帧的场类型
                pFrame->mFrame.VFrame.mVideoFormat	= VIDEO_FORMAT_LINEAR;
                pFrame->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;
                pFrame->mFrame.VFrame.mpts = curPts;
                //alogd("frame pts=%llu", pFrame->mFrame.VFrame.mpts);
                curPts += (1*1000*1000) / pVencPara->mConfigPara.mVideoFrameRate;//计算每帧之间的时间的间隔

                if (MM_PIXEL_FORMAT_YUV_AW_AFBC != pVencPara->mConfigPara.srcPixFmt)//如果源像素格式不是全志自定的数据格式
                {
                    //pFrame->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
                    read_len = pFrame->mFrame.VFrame.mWidth * pFrame->mFrame.VFrame.mHeight;//计算读取长度
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pVencPara->mConfigPara.fd_in);//读取Y数据
                    size2 = fread(pFrame->mFrame.VFrame.mpVirAddr[1], 1, read_len /2, pVencPara->mConfigPara.fd_in);//读取UV数据
                    if ((size1 != read_len) || (size2 != read_len/2))
                    {
                        alogw("warning: read to eof or somthing wrong, should stop. readlen=%d, size1=%d, size2=%d, w=%d,h=%d", read_len,\
                                        size1, size2, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pVencPara->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pVencPara->mSemExit);
                        break;
                    }
                    //Yu12ToNv21(pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight, pFrame->mFrame.VFrame.mpVirAddr[1], pVencPara->tmpBuf);
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[0], read_len);//刷新Cache
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[1], read_len/2);
                }
                else //如果源像素格式是全志自定的数据格式
                {   //读取 AFBC 格式的数据
                    pFrame->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
                    unsigned int afbc_size;
                    afbc_size = ((pFrame->mFrame.VFrame.mWidth + 127)>>7) * ((pFrame->mFrame.VFrame.mHeight + 31)>>5)*96;
                    read_len = pFrame->mFrame.VFrame.mWidth * pFrame->mFrame.VFrame.mHeight;
                    read_len = read_len + read_len/2 + afbc_size;
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pVencPara->mConfigPara.fd_in);
                    if (size1 != read_len)
                    {
                        alogw("warning: awafbc eof or somthing wrong, should stop. readlen=%d, size=%d, w=%d,h=%d", read_len,\
                                        size1, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pVencPara->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pVencPara->mSemExit);
                        break;
                    }
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[0], read_len);
                }
                //将帧节点移动到准备好的列表
                pthread_mutex_lock(&pVencPara->mInBuf_Q.mReadyListLock);
                list_move_tail(&pFrame->mList, &pVencPara->mInBuf_Q.mReadyList);
                pthread_mutex_unlock(&pVencPara->mInBuf_Q.mReadyListLock);
                //alogd("get frame[%d] to ready list", pFrame->mFrame.mId);
            }
        }
        else
        {
            //alogd("idle list empty!!");
        }
        pthread_mutex_unlock(&pVencPara->mInBuf_Q.mIdleListLock);
    }

    alogd("read thread exit!");
    return NULL;
}

static void writeSpsPpsHead(SAMPLE_VENC_PARA_S *pVencPara)
{
    int result = 0;
    alogd("write spspps head");
    if (pVencPara->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData pH264SpsPpsInfo={0};
        result = AW_MPI_VENC_GetH264SpsPpsInfo(pVencPara->mVeChn, &pH264SpsPpsInfo);//获取编码器H264 编码的sps、pps 头信息。
        if (SUCCESS == result)
        {
            if (pH264SpsPpsInfo.nLength)//写入sps、pps 头信息
            {
                fwrite(pH264SpsPpsInfo.pBuffer, 1, pH264SpsPpsInfo.nLength, pVencPara->mConfigPara.fd_out);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
        }
    }
    else if (pVencPara->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        result = AW_MPI_VENC_GetH265SpsPpsInfo(pVencPara->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS == result)
        {
            if (H265SpsPpsInfo.nLength)
            {
                fwrite(H265SpsPpsInfo.pBuffer, 1, H265SpsPpsInfo.nLength, pVencPara->mConfigPara.fd_out);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
        }
    }
}

static void *saveFromEncThread(void *pThreadData)
{
    int ret;
    VENC_PACK_S mpPack;
    VENC_STREAM_S vencFrame;
    SAMPLE_VENC_PARA_S *pVencPara = (SAMPLE_VENC_PARA_S*)pThreadData;
    int cnt = 0;
    unsigned int totalBS = 0;
    unsigned int encFramecnt = 0;

    fseek(pVencPara->mConfigPara.fd_out, 0, SEEK_SET);
    writeSpsPpsHead(pVencPara);//写入文件头

    while (!pVencPara->mOverFlag)
    {
        memset(&vencFrame, 0, sizeof(VENC_STREAM_S));
        vencFrame.mpPack = &mpPack;
        vencFrame.mPackCount = 1;

        ret = AW_MPI_VENC_GetStream(pVencPara->mVeChn, &vencFrame, 5000);//获取编码器编码后的码流
        if (SUCCESS == ret) //将码流写入文件
        {
            if (vencFrame.mpPack->mLen0)
            {
                fwrite(vencFrame.mpPack->mpAddr0, 1, vencFrame.mpPack->mLen0, pVencPara->mConfigPara.fd_out);//写入Y数据
            }

            if (vencFrame.mpPack->mLen1)
            {
                fwrite(vencFrame.mpPack->mpAddr1, 1, vencFrame.mpPack->mLen1, pVencPara->mConfigPara.fd_out);//写入UV数据
            }
            //alogd("get pts=%llu, seq=%d", vencFrame.mpPack->mPTS, vencFrame.mSeq);
            AW_MPI_VENC_ReleaseStream(pVencPara->mVeChn, &vencFrame);//释放已获取的编码器编码码流（内存）

            encFramecnt++;
            if (encFramecnt >= pVencPara->mConfigPara.mVideoFrameRate)
            //if ((vencFrame.mpPack->mDataType.enH264EType == H264E_NALU_ISLICE) || (vencFrame.mpPack->mDataType.enH265EType == H265E_NALU_ISLICE))
            {
                alogd("total encoder size=[%d]", totalBS);
                encFramecnt = 0;
                totalBS = vencFrame.mpPack->mLen0 + vencFrame.mpPack->mLen1;
            }
            else
            {
                totalBS = totalBS + vencFrame.mpPack->mLen0 + vencFrame.mpPack->mLen1;
            }
        }

        #ifdef TEST_OSD
        if (pVencPara->mConfigPara.mOsdEnable)
        {
            static int cnt = 0;
            static int enflag = 1;
            cnt++;
            if (cnt > 30)
            {
                cnt = 0;
                if (enflag)
                {
                    alogd("remove venc osd");
                    enableVencOsd(pVencPara, 0);
                }
                else
                {
                    alogd("add venc osd");
                    enableVencOsd(pVencPara, 1);
                }
                enflag = !enflag;
            }
        }
        #endif
    }

    alogd("get thread exit!");
    return NULL;
}


int main(int argc, char** argv)
{
    int result = -1;
    int size;
    int ret;

    pVencPara = (SAMPLE_VENC_PARA_S *)malloc(sizeof(SAMPLE_VENC_PARA_S));
    if (pVencPara == NULL)
    {
        aloge("alloc vencinfo pointer faile");
        return -1;
    }
    memset(pVencPara, 0, sizeof(SAMPLE_VENC_PARA_S));
    pVencPara->mVeChn = MM_INVALID_CHN;

    cdx_sem_init(&pVencPara->mSemExit, 0);

    if (parseCmdLine(pVencPara, argc, argv) != 0)
    {
        goto err_out_0;
    }

    if (loadConfigPara(pVencPara, pVencPara->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("somthing wrong in loading conf file");
        goto err_out_0;
    }

    if ((pVencPara->mConfigPara.mbEnableGdc) && (Gdc_Warp_LDC_Pro == pVencPara->mConfigPara.mGdcWarpMode))//高级几何失真校正，暂不讨论
    {
        FILE *fp = fopen(pVencPara->mConfigPara.mGdcLDCProLutBin, "rb");
        if (NULL == fp)
        {
            aloge("fatal error! LDC Pro lut bin[%s] open fail!", pVencPara->mConfigPara.mGdcLDCProLutBin);
            pVencPara->mConfigPara.mbEnableGdc = FALSE;
        }
        else
        {
            fseek(fp, 0, SEEK_END);
            pVencPara->mGdcLdcProLutDataLen = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            pVencPara->mpGdcLdcProLutData = malloc(pVencPara->mGdcLdcProLutDataLen);
            if (NULL == pVencPara->mpGdcLdcProLutData)
            {
                aloge("fatal error! malloc lut buffer fail! lut len[%d]", pVencPara->mGdcLdcProLutDataLen);
                pVencPara->mConfigPara.mbEnableGdc = FALSE;
            }
            else
            {
                memset(pVencPara->mpGdcLdcProLutData, 0, pVencPara->mGdcLdcProLutDataLen);
                fread(pVencPara->mpGdcLdcProLutData, pVencPara->mGdcLdcProLutDataLen, 1, fp);
            }
            fclose(fp);
            fp = NULL;
        }
    }

    if (venc_MemOpen() != 0 )//打开ION设备节点
    {
        alogd("open mem fail!");
        goto err_out_0;
    }

    BOOL isAwAfbc = (MM_PIXEL_FORMAT_YUV_AW_AFBC == pVencPara->mConfigPara.srcPixFmt);//是否为全志特定数据格式
    ret = vencBufMgrCreate(IN_FRAME_BUF_NUM, pVencPara->mConfigPara.srcWidth, pVencPara->mConfigPara.srcHeight, &pVencPara->mInBuf_Q, isAwAfbc);
    if (ret != 0)
    {
        aloge("ERROR: create FrameBuf manager fail");
        goto err_out_1;
    }

    pVencPara->mConfigPara.fd_in = fopen(pVencPara->mConfigPara.intputFile, "r");//打开输入文件
    if (pVencPara->mConfigPara.fd_in == NULL)
    {
        aloge("ERROR: cannot open yuv src in file");
        goto err_out_2;
    }

    pVencPara->mConfigPara.fd_out = fopen(pVencPara->mConfigPara.outputFile, "wb");//打开输出文件
    if (pVencPara->mConfigPara.fd_out == NULL)
    {
        aloge("ERROR: cannot create out file");
        goto err_out_3;
    }

    size = pVencPara->mConfigPara.srcWidth * pVencPara->mConfigPara.srcHeight / 2; //for yuv420p -> yuv420sp tmp_buf
    pVencPara->tmpBuf = (char *)malloc(size);//申请一块临时内存
    if (pVencPara->tmpBuf == NULL)
    {
        goto err_out_4;
    }

    pVencPara->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pVencPara->mSysConf);
    AW_MPI_SYS_Init();

    if (createVencChn(pVencPara) != SUCCESS)//创建编码通道
    {
        aloge("create vec chn fail");
        goto err_out_5;
    }

#ifdef TEST_OSD
    if (pVencPara->mConfigPara.mOsdEnable)
    {
        enableVencOsd(pVencPara, 1);
    }
#endif

    ret = pthread_create(&pVencPara->mReadFrmThdId, NULL, readInFrameThread, pVencPara);//读取视频帧编码线程
    if (ret || !pVencPara->mReadFrmThdId)
    {
        goto err_out_6;
    }

    ret = pthread_create(&pVencPara->mSaveEncThdId, NULL, saveFromEncThread, pVencPara);//从编码通道获取视频帧并写入文件
    if (ret || !pVencPara->mSaveEncThdId)
    {
        goto err_out_7;
    }

    AW_MPI_VENC_StartRecvPic(pVencPara->mVeChn);//启动编码通道

    if (pVencPara->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pVencPara->mSemExit, pVencPara->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pVencPara->mSemExit);
    }
    pVencPara->mOverFlag = TRUE;

#ifdef TEST_OSD
    if (pVencPara->mConfigPara.mOsdEnable)
    {
        enableVencOsd(pVencPara, 0);
    }
#endif

    AW_MPI_VENC_StopRecvPic(pVencPara->mVeChn);//停止编码
    result = 0;

    alogd("over, join thread");
    pthread_join(pVencPara->mSaveEncThdId, (void*) &ret);//结束保存图像帧线程
err_out_7:
    pthread_join(pVencPara->mReadFrmThdId, (void*) &ret);//结束读取图像帧线程
err_out_6:
    if (pVencPara->mVeChn >= 0)
    {
        AW_MPI_VENC_ResetChn(pVencPara->mVeChn);//重置编码通道到初始化状态
        AW_MPI_VENC_DestroyChn(pVencPara->mVeChn);//销毁一个编码通道
        pVencPara->mVeChn = MM_INVALID_CHN;
    }
err_out_5:
    AW_MPI_SYS_Exit();//退出MPP平台
    if (pVencPara->tmpBuf != NULL)
    {
        free(pVencPara->tmpBuf);
        pVencPara->tmpBuf = NULL;
    }
err_out_4:
    fclose(pVencPara->mConfigPara.fd_out);//关闭输出文件节点
    pVencPara->mConfigPara.fd_out = NULL;
err_out_3:
    fclose(pVencPara->mConfigPara.fd_in);//关闭输入文件节点
    pVencPara->mConfigPara.fd_in = NULL;
err_out_2:
    vencBufMgrDestroy(&pVencPara->mInBuf_Q);
err_out_1:
    venc_MemClose();
err_out_0:
    if (pVencPara->mpGdcLdcProLutData)
    {
        free(pVencPara->mpGdcLdcProLutData);
        pVencPara->mpGdcLdcProLutData = NULL;
    }
    cdx_sem_deinit(&pVencPara->mSemExit);
    free(pVencPara);
    pVencPara = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

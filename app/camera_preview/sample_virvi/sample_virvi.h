#ifndef _SAMPLE_VIRVI_H_
#define _SAMPLE_VIRVI_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE  (256)
#define MAX_CAPTURE_NUM     (4)

typedef struct SampleVirViCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
} SampleVirViCmdLineParam;

typedef struct SampleVirViConfig
{
    int AutoTestCount; //自动测试计数
    int GetFrameCount; //获取帧计数
    int DevNum; //设备数量
    int mIspDevNum; //ISP 设备数量
    int PicWidth; //图片宽度
    int PicHeight; //图片高度
    int FrameRate; //帧率
    PIXEL_FORMAT_E PicFormat; //图片格式
    int mEnableWDRMode; //是否启用 WDR 模式的标志
    enum v4l2_colorspace mColorSpace; //颜色空间
    int mViDropFrmCnt; //VI 丢帧计数
} SampleVirViConfig;

typedef struct SampleVirviCap
{
    BOOL mbCapValid; //采集是否有效的布尔值
    BOOL mbExitFlag; //线程退出的布尔值
    BOOL mbTrdRunning; //线程是否正在运行
    pthread_t thid; //线程 ID
    VI_DEV Dev; //视频输入设备
    ISP_DEV mIspDev; //ISP 设备
    VI_CHN Chn; //视频通道
    int s32MilliSec; //采集时间间隔（毫秒）
    VIDEO_FRAME_INFO_S pstFrameInfo; //视频帧信息
    int mRawStoreNum;   //current save raw picture num
    void *mpContext;    //SampleVirViContext*

    SampleVirViConfig mConfig; //采集配置信息
} SampleVirviCap;

//管理和存储视频帧的信息
typedef struct SampleVirviSaveBufNode
{
    int mId; //缓冲区的编号
    int mFrmCnt; //视频帧的计数
    int mDataLen; //数据长度
    unsigned int mDataPhyAddr; //数据的物理地址
    void *mpDataVirAddr; //数据的虚拟地址
    int mFrmLen; //帧长度
    SIZE_S mFrmSize; //帧尺寸
    PIXEL_FORMAT_E mFrmFmt; //帧格式
    void *mpCap;    //SampleVirViContext*

    struct list_head mList; //构建链表的成员变量
}SampleVirviSaveBufNode;

typedef struct SampleVirviSaveBufMgrConfig
{
    VI_DEV mSavePicDev; //设备号
    int mYuvFrameCount; //YUV 帧的计数
    char mYuvFile[MAX_FILE_PATH_SIZE]; //存储 YUV 文件的路径

    int mRawStoreCount; //the picture number of storing. 0 means don't store pictures. 要存储的原始图片数量。如果为0，表示不存储图片
    int mRawStoreInterval; //n: store one picture of n pictures. 存储图片的间隔。例如，如果设置为 n，表示每隔 n 张图片存储一张。
    char mStoreDirectory[MAX_FILE_PATH_SIZE];   //e.g.: /mnt/extsd  存储目录的字符串数组

    int mSavePicBufferNum; //保存图片的缓冲区数量
    int mSavePicBufferLen; //保存图片的缓冲区长度
}SampleVirviSaveBufMgrConfig;

typedef struct SampleVirviSaveBufMgr
{
    BOOL mbTrdRunningFlag;          //管理器的线程是否正在运行
    struct list_head mIdleList;     //SampleVirviSaveBufNode
    struct list_head mReadyList;    //SampleVirviSaveBufNode
    pthread_mutex_t mIdleListLock;  //保护 mIdleList 链表的互斥锁
    pthread_mutex_t mReadyListLock; //保护 mReadyList 链表的互斥锁

    SampleVirviSaveBufMgrConfig mConfig; //配置信息结构体
}SampleVirviSaveBufMgr;

typedef struct SampleVirViContext
{
    SampleVirViCmdLineParam mCmdLinePara;
    SampleVirviCap mCaps[MAX_CAPTURE_NUM];

    int mTestDuration;
    cdx_sem_t mSemExit;
    BOOL mbSaveCsiTrdExitFlag;
    SampleVirviSaveBufMgrConfig mSaveBufMgrConfig;
    SampleVirviSaveBufMgr *mpSaveBufMgr;
} SampleVirViContext;

#endif  /* _SAMPLE_VIRVI_H_ */

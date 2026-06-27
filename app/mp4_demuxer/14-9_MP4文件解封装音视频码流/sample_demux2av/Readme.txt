sample_demux2av测试流程：
	根据配置参数读取视频文件，解封装，获取到音视频数据后，送解码器，然后播放出声音，从扬声器输出声音；播放出视频，从视频显示画面。

读取测试参数的流程：
	sample_demux2av提供了sample_demux2av.conf，测试参数包括：MP4视频文件路径(src_file)。
	启动sample_demux2av时，在命令行参数中给出sample_demux2av.conf的具体路径，sample_demux2av会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_demux2av的指令：
	./sample_demux2av -path /mnt/extsd/sample_demux2av/sample_demux2av.conf
	"-path /mnt/extsd/sample_demux2av/sample_demux2av.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定mp4视频文件的路径。
(2)src_size：指定原始视频文件的视频大小，如1080p
(3)seek_position：指定原始视频文件的开始解析位置(ms)
(4)y_dst_file: 视频数据帧解码出来y数据分量
(5)u_dst_file: 视频数据帧解码出来u数据分量
(6)v_dst_file: 视频数据帧解码出来v数据分量
(7)yuv_dst_file: 视频数据帧解码出来对应的yuv数据文件
(8)test_duration: sample一次测试时间（单位：s）
# 使用说明
本项目实现了一个python库，基于ffmpeg api函数实现推送rtmp流，实现了音视频同步。

下载代码
```
$ git clone https://github.com/lipku/python_rtmpstream.git
$ cd python_rtmpstream
$ git submodule update --init
```

安装依赖库
```
$ pip install wheel
$ conda install ffmpeg 
或者 apt install libavcodec-dev libavformat-dev libswscale-dev
```

修改python/CMakeLists.txt文件, **根据python和ffmpeg安装路径修改如下部分(这一步一定要做，很多错误都是这里没改引起的)**
```
set(PYTHON_EXECUTABLE /opt/anaconda3/envs/python37/bin/python)  #python bin dir

include_directories("/opt/anaconda3/envs/python37/include") #ffmpeg include dir
find_library(AVCODEC_LIBRARY avcodec /opt/anaconda3/envs/python37/lib) #ffmpeg lib dir
find_library(AVFORMAT_LIBRARY avformat /opt/anaconda3/envs/python37/lib)
find_library(AVUTIL_LIBRARY avutil /opt/anaconda3/envs/python37/lib)
find_library(SWSCALE_LIBRARY swscale /opt/anaconda3/envs/python37/lib)
```
如果是python3.10以下，不能用相对路径，需要将如下部分改成绝对路径
```
include_directories("../streamer")

pybind11_add_module( ../streamer/streamer.cpp) 
```

安装python库
```
$ cd python
$ pip install .
```

运行测试程序
```
$ python test_stream.py
```

更多详细使用请参考https://github.com/lipku/nerfstream

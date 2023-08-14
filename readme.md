
# RTSP Client

## Introduction


## Dependency

    - SDL2 2.26.5+  
    - FFMPEG 6.0+  

## Setup

### For Windows

#### SDL2

1. 

### For Ubuntu

#### SDL2

    sudo apt install libsdl2-dev  
    sudo apt install libsdl2-image-dev libsdl2-mixer-dev libsdl2-net-dev libsdl2-ttf-dev  

#### FFMPEG

NOTE1: https://ubuntuhandbook.org/index.php/2023/03/ffmpeg-6-0-released-how-to-install-in-ubuntu-22-04-20-04/  

FFMPEG 6.0 or later must be installed.  
Follow the steps below to install from PPA.  

1. Add a ffmpeg 6.0 PPA.  

    sudo add-apt-repository ppa:ubuntuhandbook1/ffmpeg6

2. update.  

    sudo apt update  

3. Install ffmpeg 6.0.  

    sudo apt install ffmpeg

4. Check for installable 'libav...'.  

    apt policy libavcodec-dev libavfilter-dev libavformat-dev libavutil-dev libavdevice-dev libswscale-dev libswresample-dev  

The following output would be obtained.  
[Installed:] is already installed.  

-------------------------------------------------------------------------------------------------------------
libavcodec-dev:
  Installed: 7:6.0-1build8~22.04
  Candidate: 7:6.0-1build8~22.04
  Version table:
 *** 7:6.0-1build8~22.04 500
        500 https://ppa.launchpadcontent.net/ubuntuhandbook1/ffmpeg6/ubuntu jammy/main amd64 Packages
        100 /var/lib/dpkg/status
     7:6.0-0ubuntu1~22.04.sav2 500
        500 https://ppa.launchpadcontent.net/savoury1/ffmpeg6/ubuntu jammy/main amd64 Packages
     7:4.4.4-0ubuntu1~22.04.sav1 500
        500 https://ppa.launchpadcontent.net/savoury1/ffmpeg4/ubuntu jammy/main amd64 Packages
     7:4.4.2-0ubuntu0.22.04.1 500
        500 http://archive.ubuntu.com/ubuntu jammy-updates/universe amd64 Packages
        500 http://security.ubuntu.com/ubuntu jammy-security/universe amd64 Packages
     7:4.4.1-3ubuntu5 500
        500 http://archive.ubuntu.com/ubuntu jammy/universe amd64 Packages
-------------------------------------------------------------------------------------------------------------

5. Install the required version.  
Only three are specified, but the other libav... are dependencies and will be installed on their own.  

    sudo apt install libavformat-dev=7:6.0-1build8~22.04 libavfilter-dev=7:6.0-1build8~22.04 libavdevice-dev=7:6.0-1build8~22.04  

6. Confirm that everything has been installed.  

    apt policy libavcodec-dev libavfilter-dev libavformat-dev libavutil-dev libavdevice-dev libswscale-dev libswresample-dev  

## Build

### For Windows

#### Debug

- Ninja + LLVM 16.0  
powershell.exe cmake -S . -B build -G "\"Ninja Multi-Config"\" -D FFMPEG_PATH="/path/to/ffmpeg" -D SDL2_PATH="/path/to/sdl2"  

    powershell.exe cmake -S . -B build -G "\"Ninja Multi-Config"\" -D FFMPEG_PATH="C:\software\ffmpeg-n6.0-latest-win64-lgpl-shared-6.0" -D SDL2_PATH="C:\software\sdl2\SDL2-devel-2.26.5-vc"  
    powershell.exe cmake --build build  

### For Ubuntu(22.04 LTS)

#### Debug

- GCC 11.4.0(Ubuntu 11.4.0-1ubuntu1~22.04)

    cmake -S . -B build  
    cmake --build build  


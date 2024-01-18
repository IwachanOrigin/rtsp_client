
#ifndef MY_AV_PACKET_LIST_H_
#define MY_AV_PACKET_LIST_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace player
{

struct MyAVPacketList
{
  AVPacket* pkt = nullptr;
  int frameNumber = 0;
};

} // player

#endif // MY_AV_PACKET_LIST_H_



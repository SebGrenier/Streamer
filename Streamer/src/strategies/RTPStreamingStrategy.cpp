#include "RTPStreamingStrategy.h"

extern "C"
{
#include <libavformat/avformat.h>
}

RTPStreamingStrategy::RTPStreamingStrategy(const std::string ip, const short port)
	: _is_open(false)
	, _ip(ip)
	, _port(port)
{
	avformat_network_init();
}

RTPStreamingStrategy::~RTPStreamingStrategy()
{

}

bool RTPStreamingStrategy::open_stream()
{

	return true;
}

void RTPStreamingStrategy::close_stream()
{

}

bool RTPStreamingStrategy::push_frame(const uint8_t* frame, const int size)
{
	return true;
}


#pragma once
#include <cstdint>

class IStreamingStrategy
{
public:
	virtual ~IStreamingStrategy() = default;

	virtual bool open_stream() = 0;
	virtual void close_stream() = 0;

	virtual bool push_frame(const uint8_t *frame, const int size) = 0;
};

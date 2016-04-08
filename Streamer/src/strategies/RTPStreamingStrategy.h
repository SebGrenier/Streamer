#pragma once
#include "IStreamingStrategy.h"
#include <string>

class RTPStreamingStrategy : public IStreamingStrategy
{
public:
	RTPStreamingStrategy(const std::string ip, const short port);
	virtual ~RTPStreamingStrategy();

	virtual bool open_stream() override;
	virtual void close_stream() override;

	virtual bool push_frame(const uint8_t *frame, const int size) override;

	inline bool is_open() const { return _is_open; }

private:
	bool _is_open;
	std::string _ip;
	short _port;
};

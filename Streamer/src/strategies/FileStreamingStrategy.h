#pragma once
#include "IStreamingStrategy.h"
#include <string>

class FileStreamingStrategy : public IStreamingStrategy
{
public:
	FileStreamingStrategy(const std::string &file_path);
	virtual ~FileStreamingStrategy();

	virtual bool open_stream() override;
	virtual void close_stream() override;

	virtual bool push_frame(const uint8_t *frame, const int size) override;

	inline bool is_open() const { return _is_open; }

private:
	std::string _file_path;
	bool _is_open;
	FILE *_file_handle;
};

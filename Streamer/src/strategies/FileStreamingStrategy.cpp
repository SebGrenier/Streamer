#include "FileStreamingStrategy.h"

FileStreamingStrategy::FileStreamingStrategy(const std::string& file_path)
	: _file_path(file_path)
	, _is_open(false)
	, _file_handle(nullptr)
{}

FileStreamingStrategy::~FileStreamingStrategy()
{
	if (_is_open) {
		fclose(_file_handle);
	}
}

bool FileStreamingStrategy::open_stream()
{
	fopen_s(&_file_handle, _file_path.c_str(), "wb");
	if (_file_handle == nullptr) {
		return false;
	}

	_is_open = true;
	return true;
}

void FileStreamingStrategy::close_stream()
{
	if (_is_open) {
		// Put end code sequence to get a real video file
		uint8_t endcode[] = { 0, 0, 1, 0xb7 };
		fwrite(endcode, 1, sizeof(endcode), _file_handle);
		fclose(_file_handle);
	}
	_is_open = false;
}

bool FileStreamingStrategy::push_frame(const uint8_t* frame, const int size)
{
	fwrite(frame, 1, size, _file_handle);
	return ferror(_file_handle) == 0;
}


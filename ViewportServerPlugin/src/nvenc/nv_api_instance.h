#pragma once

#include "nvEncodeAPI.h"

class NVApiInstance
{
public:
	NVApiInstance();
	~NVApiInstance();

	int init();

	NV_ENCODE_API_FUNCTION_LIST* api() { return &_function_list; }

private:
	HINSTANCE _lib_module;
	NV_ENCODE_API_FUNCTION_LIST _function_list;
};
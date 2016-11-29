#include "nv_api_instance.h"

typedef NVENCSTATUS(NVENCAPI *get_nvenv_api)(NV_ENCODE_API_FUNCTION_LIST*);

NVApiInstance::NVApiInstance()
	: _lib_module(nullptr)
{
}

NVApiInstance::~NVApiInstance()
{
}

int NVApiInstance::init()
{
	_lib_module = LoadLibrary(TEXT("nvEncodeAPI64.dll"));

	if (_lib_module == nullptr)
		return NV_ENC_ERR_OUT_OF_MEMORY;

	auto nvEncodeAPICreateInstance = reinterpret_cast<get_nvenv_api>(GetProcAddress(_lib_module, "NvEncodeAPICreateInstance"));
	if (nvEncodeAPICreateInstance == nullptr)
		return NV_ENC_ERR_OUT_OF_MEMORY;

	_function_list.version = NV_ENCODE_API_FUNCTION_LIST_VER;
	auto nvStatus = nvEncodeAPICreateInstance(&_function_list);
	if (nvStatus != NV_ENC_SUCCESS)
		return nvStatus;

	return NV_ENC_SUCCESS;
}

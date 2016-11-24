#include "nv_encode_session.h"

NVEncodeSession::NVEncodeSession(_NV_ENCODE_API_FUNCTION_LIST *api)
	: _api(api)
	, _encoder(nullptr)
{

}

NVEncodeSession::~NVEncodeSession()
{

}

int NVEncodeSession::open(void* device, _NV_ENC_DEVICE_TYPE device_type)
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openSessionExParams;

	memset(&openSessionExParams, 0, sizeof(openSessionExParams));
	openSessionExParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;

	openSessionExParams.device = device;
	openSessionExParams.deviceType = device_type;
	openSessionExParams.apiVersion = NVENCAPI_VERSION;

	auto success = _api->nvEncOpenEncodeSessionEx(&openSessionExParams, &_encoder);
	return success;
}


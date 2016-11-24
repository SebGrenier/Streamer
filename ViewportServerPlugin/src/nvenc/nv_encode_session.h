#pragma once
#include "../common.h"
#include <nvEncodeAPI.h>

class NVEncodeSession
{
public:
	NVEncodeSession(_NV_ENCODE_API_FUNCTION_LIST *api, CommunicationHandlers *comm);
	~NVEncodeSession();

	int open(void* device, _NV_ENC_DEVICE_TYPE device_type);
private:
	CommunicationHandlers *_comm;
	_NV_ENCODE_API_FUNCTION_LIST *_api;
	void *_encoder;

	GUID _codec_guid;
	GUID _preset_guid;
	GUID _profile_guid;
};
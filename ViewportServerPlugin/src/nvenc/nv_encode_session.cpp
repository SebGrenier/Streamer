#include "nv_encode_session.h"

NVEncodeSession::NVEncodeSession(_NV_ENCODE_API_FUNCTION_LIST *api, CommunicationHandlers *comm)
	: _comm(comm)
	, _api(api)
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
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to open encode session");
		return success;
	}

	// Select codec guid
	_codec_guid = NV_ENC_CODEC_H264_GUID;

	// Select preset guid
	_preset_guid = NV_ENC_PRESET_LOW_LATENCY_HP_GUID;

	// Get preset configuration
	NV_ENC_PRESET_CONFIG config;
	memset(&config, 0, sizeof(config));
	config.version = NV_ENC_PRESET_CONFIG_VER;
	config.presetCfg.version = NV_ENC_CONFIG_VER;
	success = _api->nvEncGetEncodePresetConfig(_encoder, _codec_guid, _preset_guid, &config);
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to get encode preset config");
		return success;
	}

	// Get profile guid
	_profile_guid = NV_ENC_H264_PROFILE_BASELINE_GUID;

	// Populate encoder parameters
	NV_ENC_INITIALIZE_PARAMS params;
	memset(&params, 0, sizeof(params));
	params.version = NV_ENC_INITIALIZE_PARAMS_VER;

	params.encodeGUID = _codec_guid;
	params.encodeWidth = 1920;
	params.encodeHeight = 1080;

	success = _api->nvEncInitializeEncoder(_encoder, &params);
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to initialize encoder");
		return success;
	}

	return success;
}


#include "nv_encode_session.h"
#include <sstream>

template <typename T>
T parse_value(const std::string &key, const T default_value, const EncodingOptions &options)
{
	auto it = options.find(key);
	if (it != options.end()) {
		std::stringstream ss;
		ss << it->second;
		T val = default_value;
		ss >> val;
		return val;
	}
	return default_value;
};

GUID get_codec(const EncodingOptions &options)
{
	return parse_value<std::string>("codec", "h264", options) == "h264" ? NV_ENC_CODEC_H264_GUID : NV_ENC_CODEC_HEVC_GUID;
}

GUID get_preset(const EncodingOptions &options)
{
	auto preset = parse_value<std::string>("preset", "llhp", options);
	if (preset == "llhp") return NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
	if (preset == "ll") return NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;

	return NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
}

GUID get_profile(const EncodingOptions &options)
{
	auto profile = parse_value<std::string>("profile", "baseline", options);
	if (profile == "baseline") return NV_ENC_H264_PROFILE_BASELINE_GUID;

	return NV_ENC_H264_PROFILE_BASELINE_GUID;
}

NVEncodeSession::NVEncodeSession(_NV_ENCODE_API_FUNCTION_LIST *api, CommunicationHandlers *comm)
	: _comm(comm)
	, _api(api)
	, _encoder(nullptr)
	, _registered_input(nullptr)
{

}

NVEncodeSession::~NVEncodeSession()
{

}

int NVEncodeSession::open(void* device, _NV_ENC_DEVICE_TYPE device_type, void *d3d11resource, const EncodingOptions &options)
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
	_codec_guid = get_codec(options);

	// Select preset guid
	_preset_guid = get_preset(options);

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
	_profile_guid = get_profile(options);

	// Populate encoder parameters
	NV_ENC_INITIALIZE_PARAMS params;
	memset(&params, 0, sizeof(params));
	params.version = NV_ENC_INITIALIZE_PARAMS_VER;

	auto width = parse_value("width", 1920, options);
	auto height = parse_value("height", 1080, options);

	params.encodeGUID = _codec_guid;
	params.encodeWidth = width;
	params.encodeHeight = height;
	params.enableEncodeAsync = 1;

	success = _api->nvEncInitializeEncoder(_encoder, &params);
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to initialize encoder");
		return success;
	}

	// Register resources
	NV_ENC_REGISTER_RESOURCE input_resource;
	memset(&input_resource, 0, sizeof(input_resource));
	input_resource.version = NV_ENC_REGISTER_RESOURCE_VER;
	input_resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	input_resource.resourceToRegister = d3d11resource;
	input_resource.width = width;
	input_resource.height = height;
	input_resource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
	success = _api->nvEncRegisterResource(_encoder, &input_resource);
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to register input resource");
		return success;
	}

	_registered_input = input_resource.registeredResource;

	return success;
}

int NVEncodeSession::close()
{
	// Flush encoding stream
	NV_ENC_PIC_PARAMS params;
	memset(&params, 0, sizeof(params));
	params.version = NV_ENC_PIC_PARAMS_VER;
	params.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
	auto success = _api->nvEncEncodePicture(_encoder, &params);
	if (success != NV_ENC_SUCCESS) {
		_comm->error("Failed to flush encode stream");
		return success;
	}

	// Unregister resources
	if (_registered_input != nullptr) {
		success = _api->nvEncUnregisterResource(_encoder, _registered_input);
		if (success != NV_ENC_SUCCESS) {
			_comm->error("Failed to unregister input");
			return success;
		}
		_registered_input = nullptr;
	}

	// Close encoder
	if (_encoder != nullptr) {
		success = _api->nvEncDestroyEncoder(_encoder);
		if (success != NV_ENC_SUCCESS) {
			_comm->error("Failed to destroy encoder");
			return success;
		}
		_encoder = nullptr;
	}

	return success;
}


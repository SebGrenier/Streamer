#pragma once
#include <functional>
#include <map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

//#define ENABLE_LOGGING

struct LoggingApi;
struct StreamCaptureApi;
struct RenderBufferApi;
struct RenderInterfaceApi;
struct ThreadApi;
struct AllocatorApi;
struct AllocatorObject;
struct ScriptApi;
struct LuaApi;
struct ApplicationApi;
struct ProfilerApi;
class ViewportClient;

constexpr const char *PLUGIN_NAME = "Viewport Server Plugin";
constexpr const char *H264_NAME = "libx264";
constexpr const char *NVENC_H264_NAME = "h264_nvenc";

using EncodingOptions = std::map<std::string, std::string>;

using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;

struct EnginePluginApis
{
	LoggingApi *logging_api;
	StreamCaptureApi *stream_capture_api;
	RenderBufferApi *render_buffer_api;
	RenderInterfaceApi *render_interface_api;
	ThreadApi *thread_api;
	ScriptApi *script_api;
	AllocatorApi *allocator_api;
	LuaApi *lua_api;
	ApplicationApi *application_api;
	ProfilerApi *profiler_api;
};

struct CommunicationHandlers
{
	std::function<void(const std::string&)> info;
	std::function<void(const std::string&)> warning;
	std::function<void(const std::string&)> error;
	std::function<void(websocketpp::connection_hdl, void *, int)> send_binary;
	std::function<void(websocketpp::connection_hdl, const std::string&)> send_text;
};

#include "viewport_server.h"

#include <engine_plugin_api/plugin_api.h>

ViewportServer viewport_server;

void plugins_loaded(GetApiFunction get_engine_api)
{
	// Check if application is loaded
	auto *application_api = static_cast<ApplicationApi*>(get_engine_api(APPLICATION_API_ID));
	if (application_api == nullptr)
		return;

	auto *logging_api = static_cast<LoggingApi*>(get_engine_api(LOGGING_API_ID));
	auto *stream_api = static_cast<StreamCaptureApi*>(get_engine_api(STREAM_CAPTURE_API));
	auto *rb_api = static_cast<RenderBufferApi*>(get_engine_api(RENDER_BUFFER_API_ID));
	auto *thread_api = static_cast<ThreadApi*>(get_engine_api(THREAD_API_ID));
	auto *allocator_api = static_cast<AllocatorApi*>(get_engine_api(ALLOCATOR_API_ID));

	viewport_server.init({ logging_api, stream_api, rb_api, thread_api, allocator_api }, "127.0.0.1", 54321);

	logging_api->info("ViewportServerPlugin", "plugin loaded");
}

const char *get_name()
{
	return "Viewport Server Plugin";
}


extern "C" {
	__declspec(dllexport) void *get_plugin_api(unsigned api_id)
	{
		if (api_id == PLUGIN_API_ID) {
			static struct PluginApi api = { nullptr };
			api.plugins_loaded = plugins_loaded;
			api.get_name = get_name;

			return &api;
		}

		return nullptr;
	}
}
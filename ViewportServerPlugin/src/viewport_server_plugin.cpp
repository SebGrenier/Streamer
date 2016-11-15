#include "viewport_server.h"

#include <engine_plugin_api/plugin_api.h>
#include <engine_plugin_api/plugin_c_api.h>

ViewportServer viewport_server;

void setup_game(GetApiFunction get_engine_api)
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
	auto *ri_api = static_cast<RenderInterfaceApi*>(get_engine_api(RENDER_INTERFACE_API_ID));
	auto *c_api = static_cast<ScriptApi*>(get_engine_api(C_API_ID));

	viewport_server.init({ logging_api, stream_api, rb_api, ri_api, thread_api, c_api, allocator_api }, "127.0.0.1", 54321);

	logging_api->info("ViewportServerPlugin", "plugin loaded");
}

void update_game(float dt)
{

}

void shutdown_game()
{
	viewport_server.uninit();
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
			api.setup_game = setup_game;
			api.update_game = update_game;
			api.shutdown_game = shutdown_game;
			api.get_name = get_name;

			return &api;
		}

		return nullptr;
	}
}
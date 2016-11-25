#include "viewport_server.h"
#include "nvenc/nv_api_instance.h"

#include <engine_plugin_api/plugin_api.h>
#include <engine_plugin_api/plugin_c_api.h>
#include <plugin_foundation/const_config.h>

using namespace stingray_plugin_foundation;

ViewportServer viewport_server;
NVApiInstance nvApiInstance;
EnginePluginApis apis;
AP_ReceiverUserDataWrapper udw;


void console_receiver(void *user_data, int client_id, ConstConfigRootPtr dv, const char *data, uint32_t data_length)
{
	ConstConfigItem root_item(*(ConstConfigRoot*)dv);

	auto send_answer = [](const char* request_id)
	{
		std::stringstream ss;
		ss << "{\"id\":\"" << request_id << "\"}";
		auto message = ss.str();

		apis.application_api->console_send_with_binary_data(message.c_str(), message.size(), "", 0, false, apis.application_api->current_client_id());
	};

	if (!root_item.is_object()) {
		apis.logging_api->error(PLUGIN_NAME, "Data is not an objects");
		return;
	}

	auto id_item = root_item["id"];
	if (!id_item.is_string()) {
		apis.logging_api->error(PLUGIN_NAME, "invalid id");
		return;
	}
	auto id = id_item.to_string();

	if (viewport_server.started()) {
		send_answer(id);
		return;
	}

	auto arg_item = root_item["arg"];
	if (!arg_item.is_object()) {
		apis.logging_api->error(PLUGIN_NAME, "invalid arg");
		return;
	}

	auto start_command_item = arg_item["start"];
	if (start_command_item.is_integer()) {
		auto port = start_command_item.to_integer();
		viewport_server.open_connection("127.0.0.1", port);
	}
	else {
		apis.logging_api->error(PLUGIN_NAME, "invalid start command");
		return;
	}

	send_answer(id);
}

void setup_game(GetApiFunction get_engine_api)
{
	// Check if application is loaded
	auto *application_api = static_cast<ApplicationApi*>(get_engine_api(APPLICATION_API_ID));
	if (application_api == nullptr)
		return;

	auto *logging_api = static_cast<LoggingApi*>(get_engine_api(LOGGING_API_ID));
	auto *stream_api = static_cast<StreamCaptureApi*>(get_engine_api(STREAM_CAPTURE_API_ID));
	auto *rb_api = static_cast<RenderBufferApi*>(get_engine_api(RENDER_BUFFER_API_ID));
	auto *thread_api = static_cast<ThreadApi*>(get_engine_api(THREAD_API_ID));
	auto *allocator_api = static_cast<AllocatorApi*>(get_engine_api(ALLOCATOR_API_ID));
	auto *ri_api = static_cast<RenderInterfaceApi*>(get_engine_api(RENDER_INTERFACE_API_ID));
	auto *c_api = static_cast<ScriptApi*>(get_engine_api(C_API_ID));
	auto *lua_api = static_cast<LuaApi*>(get_engine_api(LUA_API_ID));
	auto *profiler_api = static_cast<ProfilerApi*>(get_engine_api(PROFILER_API_ID));

	if (nvApiInstance.init() != NV_ENC_SUCCESS) {
		logging_api->error(PLUGIN_NAME, "Failed to initialize nvenc api");
	}

	auto nvApi = nvApiInstance.api();

	apis = { logging_api, stream_api, rb_api, ri_api, thread_api, c_api, allocator_api, lua_api, application_api, profiler_api, nvApi };
	viewport_server.init(apis);

	udw.user_data = nullptr;
	udw.function = console_receiver;

	application_api->hook_console_receiver("viewport_server", &udw);

	logging_api->info("ViewportServerPlugin", "plugin loaded");
}

void update_game(float dt)
{
	if (viewport_server.initialized())
		viewport_server.update();
}

void shutdown_game()
{
	if (viewport_server.initialized())
		viewport_server.uninit();
}

const char *get_name()
{
	return "Viewport Server Plugin";
}

int present_frame(unsigned swap_chain_handle)
{

	// Keep default functionality
	return 0;
}

void create_swap_chain(unsigned swap_chain_handle, unsigned width, unsigned height)
{
	viewport_server.add_swap_chain(swap_chain_handle, width, height);
}

void swap_chain_resized(unsigned swap_chain_handle, unsigned width, unsigned height)
{
	viewport_server.resize_swap_chain(swap_chain_handle, width, height);
}

void destroy_swap_chain(unsigned swap_chain_handle)
{
	viewport_server.remove_swap_chain(swap_chain_handle);
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
		} else if (api_id == RENDER_OVERRIDES_PLUGIN_API_ID) {
			static struct RenderOverridesPluginApi rop_api = { nullptr };
			rop_api.present = present_frame;
			return &rop_api;
		} else if (api_id == RENDER_CALLBACKS_PLUGIN_API_ID) {
			static struct RenderCallbacksPluginApi rc_api = { nullptr };
			rc_api.create_swap_chain = create_swap_chain;
			rc_api.destroy_swap_chain = destroy_swap_chain;
			rc_api.prepare_resize_swap_chain = swap_chain_resized;
			return &rc_api;
		}

		return nullptr;
	}
}
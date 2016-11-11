#include "viewport_server.h"

#include <engine_plugin_api/plugin_api.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

constexpr const char *PLUGIN_NAME = "Viewport Server Plugin";

using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;
server serv;

using critical_section_holder = std::lock_guard<std::mutex>;

void send_buffer(websocketpp::connection_hdl h, void *buffer, int size)
{
	serv.send(h, buffer, size, websocketpp::frame::opcode::BINARY);
}

class ViewportClient
{
public:
	ViewportClient()
		: _closed(false)
	{}

	~ViewportClient()
	{}

	bool closed() const { return _closed; }

private:
	bool _closed;
};

ViewportServer::ViewportServer()
	: _initialized(false)
	, _allocator(nullptr)
	, _ws_thread(nullptr)
	, _quit(false)
{

}

ViewportServer::~ViewportServer()
{
	if (_initialized)
		uninit();
}

void ViewportServer::init(EnginePluginApis apis, const char *ip, int port)
{
	_apis = apis;
	_allocator = _apis.allocator_api->make_plugin_allocator(PLUGIN_NAME);

	start_ws_server(ip, port);

	_initialized = true;
}

void ViewportServer::uninit()
{
	_quit = true;
	close_all_clients();
	stop_ws_server();

	_apis.allocator_api->destroy_plugin_allocator(_allocator);
	_allocator = nullptr;

	_initialized = false;
}

void ViewportServer::start_ws_server(const char* ip, int port)
{
	struct ViewportServerThreadData
	{
		const char *ip;
		int port;
		ViewportServer *self;
	};

	auto *td = static_cast<ViewportServerThreadData*>(_apis.allocator_api->allocate(_allocator, sizeof(ViewportServerThreadData), alignof(ViewportServerThreadData)));
	td->ip = ip;
	td->port = port;
	td->self = this;

	_ws_thread = new std::thread([](void *user_data)
	{
		auto data = static_cast<ViewportServerThreadData*>(user_data);
		auto self = data->self;
		auto port = data->port;
		self->_apis.allocator_api->deallocate(self->_allocator, data);

		try {
			// Set logging settings
			serv.set_access_channels(websocketpp::log::alevel::none);
			serv.clear_access_channels(websocketpp::log::alevel::frame_payload);

			// Initialize ASIO
			serv.init_asio();
			serv.set_reuse_addr(true);

			// Register our message handler
			serv.set_message_handler([self](websocketpp::connection_hdl hdl, msg_ptr msg)
			{
				auto opcode = msg->get_opcode();

				if (opcode == websocketpp::frame::opcode::TEXT) {
					std::stringstream ss;
					ss << "on_message called with hdl: " << hdl.lock().get()
						<< " and message: " << msg->get_payload();
					self->info(ss.str().c_str());


				}
			});

			serv.set_http_handler([](websocketpp::connection_hdl hdl)
			{
				auto con = serv.get_con_from_hdl(hdl);

				auto res = con->get_request_body();

				std::stringstream ss;
				ss << "got HTTP request with " << res.size() << " bytes of body data.";

				con->set_body(ss.str());
				con->set_status(websocketpp::http::status_code::ok);
			});
			serv.set_fail_handler([self](websocketpp::connection_hdl hdl)
			{
				auto con = serv.get_con_from_hdl(hdl);
				std::stringstream ss;
				ss << "Fail handler: " << con->get_ec() << " " << con->get_ec().message() << std::endl;
				self->error(ss.str().c_str());
			});
			serv.set_close_handler([self](websocketpp::connection_hdl hdl)
			{
				self->info("Close handler");
			});
			serv.set_validate_handler([](websocketpp::connection_hdl)
			{
				// TODO: check if the connection is for the viewport server.
				return true;
			});
			serv.set_open_handler([self](websocketpp::connection_hdl hdl)
			{
				auto con = serv.get_con_from_hdl(hdl);
				self->info("Open handler");
			});

			// Listen on port
			serv.listen(port);

			// Start the server accept loop
			serv.start_accept();

			// Start the ASIO io_service run loop
			serv.run();
		}
		catch (const std::exception & e) {
			self->error(e.what());
		}
		catch (websocketpp::lib::error_code e) {
			self->error(e.message().c_str());
		}
		catch (...) {
			self->error("Other exception");
		}
	}, td);

}

void ViewportServer::stop_ws_server()
{
	serv.stop();
	_ws_thread->join();
	delete _ws_thread;
}

void ViewportServer::run_client(ViewportClient *client)
{
	// We transfer ownership of the client to this thread
	auto thread_id = new std::thread([this](ViewportClient *c)
	{
		while(!_quit && !c->closed()) {

		}

		delete c;
	}, client);

	_clients.push_back(thread_id);
}

void ViewportServer::close_all_clients()
{
	critical_section_holder csh(_client_mutex);
	for (auto &t : _clients) {
		t->join();
		delete t;
	}
	_clients.clear();
}


void ViewportServer::info(const char* message)
{
	if (_apis.logging_api)
		_apis.logging_api->info(PLUGIN_NAME, message);
}

void ViewportServer::warning(const char* message)
{
	if (_apis.logging_api)
		_apis.logging_api->warning(PLUGIN_NAME, message);
}

void ViewportServer::error(const char* message)
{
	if (_apis.logging_api)
		_apis.logging_api->error(PLUGIN_NAME, message);
}

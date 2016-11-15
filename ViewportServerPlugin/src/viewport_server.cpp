#include "viewport_server.h"

#include <engine_plugin_api/plugin_api.h>
#include <plugin_foundation/id_string.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using namespace stingray_plugin_foundation;

using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;
server serv;

using critical_section_holder = std::lock_guard<std::mutex>;

void send_buffer(websocketpp::connection_hdl h, void *buffer, int size)
{
	serv.send(h, buffer, size, websocketpp::frame::opcode::BINARY);
}

void send_text(websocketpp::connection_hdl h, const std::string &message)
{
	serv.send(h, message, websocketpp::frame::opcode::TEXT);
}

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
			serv.set_validate_handler([self](websocketpp::connection_hdl hdl)
			{
				auto con = serv.get_con_from_hdl(hdl);
				auto origin = con->get_origin();
				self->info("(Validate) Origin: " + origin);
				// TODO: check if the connection is for the viewport server.
				return true;
			});
			serv.set_open_handler([self](websocketpp::connection_hdl hdl)
			{
				auto con = serv.get_con_from_hdl(hdl);
				self->info("Open handler");

				CommunicationHandlers h;
				h.info = [self](auto msg) {self->info(msg); };
				h.warning = [self](auto msg) {self->warning(msg); };
				h.error = [self](auto msg) {self->error(msg); };
				h.send_binary = [](auto hdl, auto buffer, auto size) {send_buffer(hdl, buffer, size); };
				h.send_text = [](auto hdl, auto msg) {send_text(hdl, msg); };

				auto *client = new ViewportClient(self->_apis, h, hdl, self->_allocator);

				// Register our message handler
				con->set_message_handler([client](websocketpp::connection_hdl hdl, msg_ptr msg)
				{
					client->handle_message(hdl, msg);
				});
				con->set_close_handler([self, client](websocketpp::connection_hdl hdl)
				{
					client->handle_close(hdl);
				});
				con->set_fail_handler([self, client](websocketpp::connection_hdl hdl)
				{
					client->handle_fail(hdl);
				});

				self->run_client(client);
			});

			// Listen on port
			serv.listen(port);

			// Start the server accept loop
			serv.start_accept();

			// Start the ASIO io_service run loop
			serv.run();
		}
		catch (websocketpp::lib::error_code e) {
			self->error("ws error: " + e.message());
		}
		catch (const std::exception & e) {
			auto s = std::string(e.what());
			self->error("std error: " + s);
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
	critical_section_holder csh(_client_mutex);
	if (_quit) {
		delete client;
		return;
	}

	client->run();
	_clients.push_back(client);
}

void ViewportServer::close_all_clients()
{
	critical_section_holder csh(_client_mutex);
	for (auto &t : _clients) {
		t->stop();
		delete t;
	}
	_clients.clear();
}

void ViewportServer::info(const std::string &message)
{
	if (_apis.logging_api)
		_apis.logging_api->info(PLUGIN_NAME, message.c_str());
}

void ViewportServer::warning(const std::string &message)
{
	if (_apis.logging_api)
		_apis.logging_api->warning(PLUGIN_NAME, message.c_str());
}

void ViewportServer::error(const std::string &message)
{
	if (_apis.logging_api)
		_apis.logging_api->error(PLUGIN_NAME, message.c_str());
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

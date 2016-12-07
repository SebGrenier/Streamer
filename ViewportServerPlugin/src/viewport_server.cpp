#include "viewport_server.h"

#include <engine_plugin_api/plugin_api.h>
#include <plugin_foundation/id_string.h>
#include <iostream>

using namespace stingray_plugin_foundation;

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
	, _server_started(false)
	, _allocator(nullptr)
	, _ws_thread(nullptr)
	, _quit(false)
	, _ws_ostream(nullptr)
{
	_ws_ostream = new ofunctionstream([this](std::string &m) { info(m); });
}

ViewportServer::~ViewportServer()
{
	if (_initialized)
		uninit();

	if (_ws_ostream != nullptr)
		delete _ws_ostream;
}

void ViewportServer::init(EnginePluginApis apis)
{
	_apis = apis;
	_allocator = _apis.allocator_api->make_plugin_allocator(PLUGIN_NAME);

	_initialized = true;
}

void ViewportServer::uninit()
{
	close_connection();

	_apis.allocator_api->destroy_plugin_allocator(_allocator);
	_allocator = nullptr;

	_initialized = false;
}

void ViewportServer::update()
{
	_apis.profiler_api->profile_start("ViewportServer:update");
	if (_server_started)
		serv.poll();
	sweep_clients();
	run_all_clients();
	_apis.profiler_api->profile_stop();
}

void ViewportServer::render(unsigned sch)
{

}

void ViewportServer::open_connection(const char* ip, int port)
{
	start_ws_server(ip, port);
}

void ViewportServer::close_connection()
{
	_quit = true;
	close_all_clients();
	stop_ws_server();
}

void ViewportServer::start_ws_server(const char* ip, int port)
{

	try {
		// Set logging settings
		serv.set_access_channels(websocketpp::log::alevel::none);
		serv.clear_access_channels(websocketpp::log::alevel::frame_payload);

		serv.get_alog().set_ostream(_ws_ostream);

		// Initialize ASIO
		serv.init_asio();
		serv.set_reuse_addr(true);

		serv.set_fail_handler([this](websocketpp::connection_hdl hdl)
		{
			auto con = serv.get_con_from_hdl(hdl);
			std::stringstream ss;
			ss << "Fail handler: " << con->get_ec() << " " << con->get_ec().message() << std::endl;
			error(ss.str().c_str());
		});
		serv.set_close_handler([this](websocketpp::connection_hdl hdl)
		{
			info("Close handler");
		});
		serv.set_validate_handler([this](websocketpp::connection_hdl hdl)
		{
			auto con = serv.get_con_from_hdl(hdl);
			auto origin = con->get_origin();
			info("(Validate) Origin: " + origin);
			// TODO: check if the connection is for the viewport server.
			return true;
		});
		serv.set_open_handler([this](websocketpp::connection_hdl hdl)
		{
			auto con = serv.get_con_from_hdl(hdl);
			info("Open handler");

			CommunicationHandlers h;
			h.info = [this](auto msg) {info(msg); };
			h.warning = [this](auto msg) {warning(msg); };
			h.error = [this](auto msg) {error(msg); };
			h.send_binary = [](auto hdl, auto buffer, auto size) {send_buffer(hdl, buffer, size); };
			h.send_text = [](auto hdl, auto msg) {send_text(hdl, msg); };

			auto *client = new ViewportClient(this, h, hdl, _allocator);

			// Register our message handler
			con->set_message_handler([client](websocketpp::connection_hdl hdl, msg_ptr msg)
			{
				client->handle_message(hdl, msg);
			});
			con->set_close_handler([client](websocketpp::connection_hdl hdl)
			{
				client->handle_close(hdl);
			});
			con->set_fail_handler([client](websocketpp::connection_hdl hdl)
			{
				client->handle_fail(hdl);
			});

			_clients.push_back(client);
		});

		// Listen on port
		serv.listen(port);

		// Start the server accept loop
		serv.start_accept();

		_server_started = true;
	}
	catch (websocketpp::lib::error_code e) {
		error("ws error: " + e.message());
	}
	catch (const std::exception & e) {
		auto s = std::string(e.what());
		error("std error: " + s);
	}
	catch (...) {
		error("Other exception");
	}

}

void ViewportServer::stop_ws_server()
{
	if (_server_started)
		serv.stop();
	_server_started = false;
}

void ViewportServer::run_client(ViewportClient *client)
{
	client->run();
}

void ViewportServer::run_all_clients()
{
	_apis.profiler_api->profile_start("ViewportServer:run_all_clients");
	for (auto *c: _clients) {
		c->run();
	}
	_apis.profiler_api->profile_stop();
}

void ViewportServer::close_all_clients()
{
	for (auto &t : _clients) {
		t->stop();
		delete t;
	}
	_clients.clear();
}

void ViewportServer::sweep_clients()
{
	auto it = _clients.begin();
	while (it != _clients.end()) {
		if ((*it)->closed()) {
			delete (*it);
			it = _clients.erase(it);
		} else {
			++it;
		}
	}
}

void ViewportServer::info(const std::string &message)
{
	info(message.c_str());
}

void ViewportServer::warning(const std::string &message)
{
	warning(message.c_str());
}

void ViewportServer::error(const std::string &message)
{
	error(message.c_str());
}

void ViewportServer::info(const char* message)
{
#ifdef ENABLE_LOGGING
	if (_apis.logging_api)
		_apis.logging_api->info(PLUGIN_NAME, message);
#endif
}

void ViewportServer::warning(const char* message)
{
#ifdef ENABLE_LOGGING
	if (_apis.logging_api)
		_apis.logging_api->warning(PLUGIN_NAME, message);
#endif
}

void ViewportServer::error(const char* message)
{
#ifdef ENABLE_LOGGING
	if (_apis.logging_api)
		_apis.logging_api->error(PLUGIN_NAME, message);
#endif
}

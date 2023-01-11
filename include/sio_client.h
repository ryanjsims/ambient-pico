#pragma once

#include "eio_client.h"
#include "http_client.h"

#include "nlohmann/json.hpp"

#include <map>
#include <memory>
#include <functional>
class sio_client;
class sio_socket {
    friend class sio_client;
public:
    void on(std::string event, std::function<void(nlohmann::json)> handler) {
        event_handlers[event] = handler;
    }

    void once(std::string event, std::function<void(nlohmann::json)> handler) {
        event_handlers[event] = [&](nlohmann::json body) {
            handler(body);
            this->event_handlers.erase(event);
        };
    }

    void emit(std::string event, nlohmann::json array = nlohmann::json::array()) {
        std::string packet = "2" + (ns_ != "/" ? ns_ + "," : "");
        if(!array.is_array()) {
            array = {event, array};
        } else {
            array.insert(array.begin(), event);
        }
        packet += array.dump();
        debug("emit:\n\tNamespace '%s'\n\tpacket: '%s'\n", ns_.c_str(), packet.c_str());
        if(engine) {
            engine->send_message({(uint8_t*)packet.data(), packet.size()});
        }
    }

    bool connected() const {
        return sid_ != "";
    }

    void update_engine(eio_client *engine_ref) {
        engine = engine_ref;
    }

private:
    sio_socket() = default;
    sio_socket(eio_client *engine_ref, std::string ns): ns_(ns), engine(engine_ref) {

    }
    eio_client *engine;
    std::string ns_, sid_;
    std::map<std::string, std::function<void(nlohmann::json)>> event_handlers;
\
    void connect_callback(nlohmann::json body) {
        debug("sio_socket::connect_callback\n%s\n", body.dump(4).c_str());
        if(body.contains("sid")){
            debug1("setting sid...\n");
            sid_ = body["sid"];
        }

        if(event_handlers.find("connect") != event_handlers.end()){
            event_handlers["connect"](body);
        }
    }

    void event_callback(nlohmann::json array) {
        if(array.size() == 0) {
            error1("Array too small!\n");
            return;
        }
        std::string event = array[0];
        array.erase(0);
        debug("sio_socket::event_callback for event '%s'\n", event.c_str());
        if(event_handlers.find(event) != event_handlers.end()) {
            event_handlers[event](array);
        }
    }
};

class sio_client {
public:
    enum class packet_type: uint8_t {
        connect = '0',
        disconnect,
        event,
        ack,
        connect_error,
        binary_event,
        binary_ack
    };

    sio_client(std::string url, std::map<std::string, std::string> query): http(url), engine(nullptr) {
        std::string query_string = "?EIO=4&transport=websocket";
        for(std::map<std::string, std::string>::const_iterator iter = query.cbegin(); iter != query.cend(); iter++) {
            query_string += "&" + iter->first + "=" + iter->second;
        }
        http.on_response([&](){
            info("Got http response: %d %s\n", http.response().status(), http.response().get_status_text().c_str());
            
            if(http.response().status() == 101) {
                trace1("sio_client: creating engine\n");
                //http.on_response([](){});
                engine = new eio_client(http.release_tcp_client());
                trace1("sio_client: engine created\n");
                if(!engine) {
                    error1("Engine is nullptr\n");
                    return;
                }
                engine->on_open([this](){
                    open = true;
                });
                trace1("sio_client: set engine open\n");
                engine->on_receive(std::bind(&sio_client::engine_recv_callback, this));
                trace1("sio_client: set engine recv\n");
                for(auto iter = namespace_connections.begin(); iter != namespace_connections.end(); iter++) {
                    iter->second->update_engine(engine);
                }
                engine->read_initial_packet();
            }
        });
        http.get("/socket.io/" + query_string);
    }

    void connect(std::string ns = "/") {
        if(!engine) {
            error1("connect: Engine not initialized!\n");
            return;
        }
        std::string socket_io_payload = "0" + (ns != "/" ? ns + "," : "");
        engine->send_message({(uint8_t*)socket_io_payload.data(), socket_io_payload.size()});
    }

    void disconnect(std::string ns = "/") {
        if(!engine) {
            error1("disconnect: Engine not initialized!\n");
            return;
        }
        if(namespace_connections.find(ns) != namespace_connections.end()) {
            namespace_connections[ns].reset();
            namespace_connections.erase(ns);
            std::string packet = "1" + (ns != "/" ? ns + "," : "");
            engine->send_message({(uint8_t*)packet.data(), packet.size()});
        }
    }

    std::unique_ptr<sio_socket> &socket(std::string ns = "/") {
        if(namespace_connections.find(ns) == namespace_connections.end()) {
            debug("socket: Creating new socket for namespace '%s'\n", ns.c_str());
            namespace_connections[ns] = std::unique_ptr<sio_socket>(new sio_socket(engine, ns));
        }
        return namespace_connections[ns];
    }

    bool ready() {
        return open;
    }

private:
    eio_client *engine;
    http_client http;
    std::map<std::string, std::unique_ptr<sio_socket>> namespace_connections;
    bool open = false;

    void engine_recv_callback() {
        debug1("sio_client::engine_recv_callback\n");
        std::string data;
        data.resize(engine->packet_size());
        engine->read({(uint8_t*)data.data(), data.size()});
        debug("read data: '%s'\n", data.c_str());
        std::string ns = "/";
        nlohmann::json body;
        trace1("created json\n");
        size_t tok_start, tok_end;
        if(data.size() > 1) {
            if((tok_end = data.find(",")) != std::string::npos && (tok_start = data.find("/")) != std::string::npos && tok_end < data.find("[")) {
                tok_start++;
                ns.append(data.begin() + tok_start, data.begin() + tok_end);
            }
        }
        trace1("read namespace\n");

        debug("Packet type: %c\n", data[0]);

        switch((packet_type)data[0]) {
        case packet_type::connect:{
            if((tok_start = data.find("{")) != std::string::npos) {
                tok_end = data.find("}");
                body = nlohmann::json::parse(data.substr(tok_start, (tok_end + 1) - tok_start));
            }
            if(namespace_connections.find(ns) == namespace_connections.end()) {
                debug("recv: Creating new socket for namespace '%s'\n", ns.c_str());
                namespace_connections[ns] = std::unique_ptr<sio_socket>(new sio_socket(engine, ns));
            } else {
                debug("Found existing socket for namespace '%s'\n", ns.c_str());
            }
            namespace_connections[ns]->connect_callback(body);
            break;
        }
        case packet_type::event:
            if((tok_start = data.find_first_of("[")) != std::string::npos) {
                tok_end = data.find_last_of("]");
                body = nlohmann::json::parse(data.substr(tok_start, (tok_end + 1) - tok_start));
            }
            if(namespace_connections.find(ns) != namespace_connections.end()) {
                namespace_connections[ns]->event_callback(body);
            }
            break;
        }
    }
};
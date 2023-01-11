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
        if(auto lock = engine.lock()) {
            lock->send_message({(uint8_t*)packet.data(), packet.size()});
        }
    }

private:
    sio_socket() = default;
    sio_socket(std::weak_ptr<eio_client> engine_ref, std::string ns): ns_(ns), engine(engine_ref) {

    }
    std::weak_ptr<eio_client> engine;
    std::string ns_, sid_;
    std::map<std::string, std::function<void(nlohmann::json)>> event_handlers;

    void connect_callback(nlohmann::json body) {
        if(body.contains("sid")){
            sid_ = body["sid"];
        }

        if(event_handlers.find("connect") != event_handlers.end()){
            event_handlers["connect"](body);
        }
    }

    void event_callback(nlohmann::json array) {
        std::string event = array[0];
        array.erase(0);
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

    sio_client(std::string url, std::map<std::string, std::string> query) {
        http_client http(url);
        std::string query_string = "?EIO=4&transport=websocket";
        for(std::map<std::string, std::string>::const_iterator iter = query.cbegin(); iter != query.cend(); iter++) {
            query_string += "&" + iter->first + "=" + iter->second;
        }
        http.on_response([&](){
            info("Got http response: %d %s\n", http.response().status(), http.response().get_status_text().c_str());
            
            if(http.response().status() == 101) {
                http.on_response([](){});
                engine = std::make_shared<eio_client>(http.release_tcp_client());
                engine->on_receive(std::bind(&sio_client::engine_recv_callback, this));
                engine->read_initial_packet();
            }
        });
        http.get("/socket.io/" + query_string);
    }

    void connect(std::string ns = "/") {
        std::string socket_io_payload = "0" + (ns != "/" ? ns + "," : "");
        engine->send_message({(uint8_t*)socket_io_payload.data(), socket_io_payload.size()});
    }

    void disconnect(std::string ns = "/") {
        if(namespace_connections.find(ns) != namespace_connections.end()) {
            namespace_connections[ns].reset();
            namespace_connections.erase(ns);
            std::string packet = "1" + (ns != "/" ? ns + "," : "");
            engine->send_message({(uint8_t*)packet.data(), packet.size()});
        }
    }

    std::unique_ptr<sio_socket> &socket(std::string ns = "/") {
        if(namespace_connections.find(ns) == namespace_connections.end()) {
            namespace_connections[ns] = std::make_unique<sio_socket>(engine, ns);
        }
        return namespace_connections[ns];
    }

private:
    std::shared_ptr<eio_client> engine;
    std::map<std::string, std::unique_ptr<sio_socket>> namespace_connections;

    void engine_recv_callback() {
        std::string data;
        data.resize(engine->packet_size());
        engine->read({(uint8_t*)data.data(), data.size()});
        std::string ns = "/";
        nlohmann::json body;
        size_t tok_start, tok_end;
        if(data.size() > 1) {
            if((tok_end = data.find(",")) != std::string::npos) {
                tok_start = data.find("/") + 1;
                ns.append(data.begin() + tok_start, data.begin() + tok_end);
            }
        }

        switch((packet_type)data[0]) {
        case packet_type::connect:{
            if((tok_start = data.find("{")) != std::string::npos) {
                tok_end = data.find("}");
                body = nlohmann::json::parse(data.substr(tok_start, (tok_end + 1) - tok_start));
            }
            if(namespace_connections.find(ns) == namespace_connections.end()) {
                namespace_connections[ns] = std::make_unique<sio_socket>(engine, ns);
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
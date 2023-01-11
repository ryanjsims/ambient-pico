#pragma once
#include "tcp_client.h"
#include "tcp_tls_client.h"
#include <string>
#include <string_view>
#include <charconv>
#include <map>
#include <vector>

#include "LUrlParser.h"

#include "logger.h"

#include <algorithm>

bool iequals(const std::string& a, const std::string& b);

class http_request {
    friend class http_client;
    friend class https_client;
public:
    http_request(): method_(""), target_(""), body_("") {}
    http_request(std::string method, std::string target): method_(method), target_(target), body_("") {
        ready_ = true;
    };
    http_request(std::string method, std::string target, std::string body): method_(method), target_(target), body_(body) {
        ready_ = true;
    }

    void add_header(std::string key, std::string value) {
        headers[key] = value;
    }

    std::string serialize() {
        std::string to_return = method_ + " " + target_ + " HTTP/1.1\r\n";
        for(auto iter = headers.cbegin(); iter != headers.cend(); iter++) {
            to_return += iter->first + ": " + iter->second + "\r\n";
        }
        to_return += "\r\n" + body_;
        return to_return;
    }
private:
    std::string method_, target_, body_;
    std::map<std::string, std::string> headers;
    bool ready_ = false;
};

class http_response {
    friend class http_client;
    friend class https_client;
    enum class parse_state {
        status_line,
        headers,
        body,
        done
    };
public:
    http_response(): status_code(0), state(parse_state::status_line) {}

    void parse(const std::string &data) {
        debug1("Parsing http response:\n");

        size_t line_start = 0, line_end = data.find("\r\n");
        while(line_end != std::string::npos) {
            parse_line({data.begin() + line_start, data.begin() + line_end});
            line_start = line_end + 2;
            line_end = data.find("\r\n", line_start);
        }
        if(line_start < data.size()) {
            parse_line({data.begin() + line_start, data.end()});
        }
    }

    void parse_line(std::string_view line) {
        size_t token_start, token_end;
        switch(state) {
        case parse_state::status_line:
            debug1("Parsing status line\n");
            // First find the protocol, which is from the beginning to the first space
            token_end = line.find(" ");
            protocol = std::string(line.begin(), token_end);
            debug("    Protocol: %s\n", protocol.c_str());

            // Then parse the status code, 1 character after the first space to the second space
            //   std::from_chars converts it to an integer
            token_start = token_end + 1;
            token_end = line.find(" ", token_start);
            std::from_chars(line.begin() + token_start, line.begin() + token_end, status_code);
            debug("    status code: %d\n", status_code);

            // Then the status text is the rest of the line
            token_start = token_end + 1;
            status_text = std::string(line.begin() + token_start, line.end());
            debug("    status text: %s\n", status_text.c_str());
            state = parse_state::headers;
            break;
        case parse_state::headers:{
            debug1("Parsing header\n");
            if(line.size() == 0) {
                debug("Empty header, transition to %s\n", content_length > 0 ? "body" : "done");
                if(content_length == 0) {
                    state = parse_state::done;
                } else if(content_length > 0) {
                    state = parse_state::body;
                } else {
                    error1("No content length!\n");
                    state = parse_state::done;
                }
                break;
            }
            token_end = line.find(": ");
            std::string key(line.begin(), token_end);
            std::string value(line.begin() + token_end + 2, line.end());
            debug("        %s: %s\n", key.c_str(), value.c_str());
            headers[key] = value;
            if(iequals(key, "Content-Length")) {
                std::from_chars(line.begin() + token_end + 2, line.end(), content_length);
            }
            break;
        }
        case parse_state::body:
            debug("Parsing body:\n%s\n", line.data());
            body += line;
            if(body.size() == content_length) {
                debug1("Transition to done\n");
                state = parse_state::done;
            }
            break;
        default:
            error1("Shouldn't happen? parse_state == done\n");
            break;
        }
    }

    const std::map<std::string, std::string> &get_headers() const {
        return headers;
    }

    uint16_t status() const {
        return status_code;
    }

    const std::string &get_status_text() const {
        return status_text;
    }

    const std::string &get_protocol() const {
        return protocol;
    }

    const std::string &get_body() const {
        return body;
    }

    void add_data(std::string data) {
        this->data += data;
    }

private:
    uint16_t status_code;
    int content_length = -1;
    std::string protocol, status_text, body, data;
    std::map<std::string, std::string> headers;
    parse_state state;
};

// class http_client {
// public:
//     http_client(std::string host): host_(host) {
//         tcp = new tcp_client();
//     }
//
//     http_client(std::string host, uint16_t port): host_(host), port_(port) {
//         tcp = new tcp_client();
//     }
//
//     http_client(http_client&&) = default;
//
//     ~http_client() {
//         delete tcp;
//     }
//
//     void get(std::string target) {
//         current_request = {"GET", target};
//         current_request.add_header("Host", host_);
//         current_request.add_header("User-Agent", "pico");
//         send_request();
//     }
//
//     bool has_response() {
//         return response_ready;
//     }
//
//     const http_response &response() {
//         response_ready = false;
//         return current_response;
//     }
//
// private:
//     tcp_client *tcp;
//     bool response_ready = false;
//     http_request current_request;
//     http_response current_response;
//     std::string host_;
//     uint16_t port_ = 80;
//
//     void send_request() {
//         response_ready = false;
//         tcp->on_receive(std::bind(&http_client::tcp_recv_callback, this));
//         tcp->on_closed(std::bind(&http_client::tcp_closed_callback, this));
//         if(!tcp->initialized()) {
//             tcp->init();
//         }
//
//         if(!tcp->ready()) {
//             tcp->on_connected(std::bind(&http_client::tcp_connected_callback, this));
//             tcp->connect(host_, port_);
//         } else {
//             tcp_connected_callback();
//         }
//     }
//
//     void tcp_connected_callback() {
//         debug1("http_client connected callback\n");
//         std::string serialized = current_request.serialize();
//         tcp->write({(uint8_t*)serialized.c_str(), serialized.size()});
//     }
//
//     void tcp_recv_callback() {
//         debug1("http_client recv callback\n");
//         std::string data;
//         data.resize(tcp->available());
//         std::span<uint8_t> span = {(uint8_t*)data.data(), data.size()};
//         tcp->read(span);
//         current_response.parse(data);
//         response_ready = current_response.state == http_response::parse_state::done;
//     }
//
//     void tcp_closed_callback() {
//         debug1("http_client closed callback\n");
//     }
// };

class http_client {
public:
    http_client(std::string url): host_(""), url_(url), port_(-1) {
        init();
    }

    http_client(http_client&&) = default;

    ~http_client() {
        if(tcp)
            delete tcp;
    }

    void get(std::string target) {
        send_request("GET", target);
    }

    void post(std::string target, std::string body = "") {
        send_request("POST", target, body);
    }

    void send_request(std::string method, std::string target, std::string body = "") {
        current_request = {method, target, body};
        send_request();
    }

    void resend_request() {
        send_request();
    }

    bool has_response() {
        return response_ready;
    }

    const http_response &response() {
        response_ready = false;
        return current_response;
    }

    void on_response(std::function<void()> callback) {
        user_response_callback = callback;
    }

    tcp_base *release_tcp_client() {
        tcp_base *to_return = tcp;
        tcp = nullptr;
        return std::move(to_return);
    }

    LUrlParser::ParseURL get_parsed_url() {
        return URL;
    }

private:
    tcp_base *tcp;
    bool response_ready = false;
    http_request current_request;
    http_response current_response;
    std::string host_, url_;
    int port_;
    LUrlParser::ParseURL URL;
    std::function<void()> user_response_callback;

    bool init() {
        URL = LUrlParser::ParseURL::parseURL(url_);
        if(!URL.isValid()) {
            error("Invalid URL: %s\n", url_.c_str());
            return false;
        }

        host_ = URL.host_;
        if(URL.port_.size() > 0)
            URL.getPort(&port_);
        if(URL.scheme_ == "https" || URL.scheme_ == "wss") {
            tcp = new tcp_tls_client();
            if(port_ == -1) {
                port_ = 443;
            }
        } else {
            tcp = new tcp_client();
            if(port_ == -1) {
                port_ = 80;
            }
        }
        return true;
    }

    void send_request() {
        response_ready = false;
        current_response = {};
        current_request.add_header("Host", host_);
        current_request.add_header("User-Agent", "pico");
        if(current_request.body_.size() > 0) {
            current_request.add_header("Content-Length", std::to_string(current_request.body_.size()));
        }
        current_request.add_header("Upgrade", "websocket");
        current_request.add_header("Sec-WebSocket-Key", "8xtVmuvomB2taGWDXBxVMw==");
        current_request.add_header("Sec-WebSocket-Version", "13");
        tcp->on_receive(std::bind(&http_client::tcp_recv_callback, this));
        tcp->on_closed(std::bind(&http_client::tcp_closed_callback, this));
        if(!tcp->initialized()) {
            tcp->init();
        }

        if(!tcp->ready()) {
            tcp->on_connected(std::bind(&http_client::tcp_connected_callback, this));
            tcp->connect(host_, port_);
        } else {
            tcp_connected_callback();
        }
    }

    void tcp_connected_callback() {
        std::string serialized = current_request.serialize();
        debug("http_client sending:\n%s\n", serialized.c_str());
        tcp->write({(uint8_t*)serialized.c_str(), serialized.size()});
    }

    void tcp_recv_callback() {
        std::string data;
        data.resize(tcp->available());
        std::span<uint8_t> span = {(uint8_t*)data.data(), data.size()};
        tcp->read(span);
        debug("http_client recv'd:\n%s\n", data.c_str());
        current_response.parse(data);
        response_ready = current_response.state == http_response::parse_state::done;
        if(response_ready) {
            tcp->on_receive([](){});
        }
    }

    void tcp_closed_callback() {
        debug1("http_client closed callback\n");
    }
};
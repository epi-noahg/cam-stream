#include "WsServer.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <iostream>
#include <vector>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

namespace dart::api {

// ── One connection: its own read thread + a write mutex ─────────────────────
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, WsServer::ClientId id, WsServer& server)
        : ws_(std::move(socket)), id_(id), server_(server) {}

    void run() {
        auto self = shared_from_this();
        std::thread([self] { self->loop(); }).detach();
    }

    void send(const std::string& msg) {
        std::lock_guard<std::mutex> lk(write_mtx_);
        try {
            ws_.text(true);
            ws_.write(net::buffer(msg));
        } catch (const std::exception&) {
            // The read loop will observe the failure and clean up.
        }
    }

    void close() {
        beast::error_code ec;
        ws_.next_layer().close(ec);  // unblock a blocked read()
    }

private:
    void loop() {
        try {
            ws_.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(beast::http::field::server, "dartserver");
                }));
            ws_.accept();
            server_.dispatchConnect_(id_);

            for (;;) {
                beast::flat_buffer buffer;
                ws_.read(buffer);
                server_.dispatchMessage_(id_, beast::buffers_to_string(buffer.data()));
            }
        } catch (const std::exception&) {
            // normal on disconnect / close
        }
        server_.removeSession_(id_);
    }

    websocket::stream<tcp::socket> ws_;
    WsServer::ClientId             id_;
    WsServer&                      server_;
    std::mutex                     write_mtx_;
};

// ── Acceptor implementation hidden from the header ──────────────────────────
struct WsServer::Impl {
    net::io_context ioc{1};
    tcp::acceptor   acceptor;
    explicit Impl(std::uint16_t port)
        : acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {}
};

WsServer::WsServer(std::uint16_t port)
    : port_(port), impl_(std::make_unique<Impl>(port)) {}

WsServer::~WsServer() { stop(); }

void WsServer::start() {
    if (running_) return;
    running_ = true;
    accept_thread_ = std::thread([this] { acceptLoop_(); });
    std::cout << "[ws] listening on port " << port_ << "\n";
}

void WsServer::stop() {
    if (!running_) return;
    running_ = false;

    beast::error_code ec;
    impl_->acceptor.close(ec);

    {   // unblock all session read loops
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        for (auto& [id, s] : sessions_) s->close();
    }
    if (accept_thread_.joinable()) accept_thread_.join();
}

void WsServer::acceptLoop_() {
    while (running_) {
        tcp::socket socket(impl_->ioc);
        beast::error_code ec;
        impl_->acceptor.accept(socket, ec);
        if (ec) {
            if (!running_) break;
            continue;
        }
        const ClientId id = next_id_++;
        auto s = std::make_shared<WsSession>(std::move(socket), id, *this);
        registerSession_(id, s);
        s->run();
    }
}

void WsServer::registerSession_(ClientId id, std::shared_ptr<WsSession> s) {
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    sessions_[id] = std::move(s);
}

void WsServer::removeSession_(ClientId id) {
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    sessions_.erase(id);
}

void WsServer::dispatchMessage_(ClientId id, const std::string& msg) {
    if (on_message_) on_message_(id, msg);
}

void WsServer::dispatchConnect_(ClientId id) {
    if (on_connect_) on_connect_(id);
}

void WsServer::broadcast(const std::string& msg) {
    std::vector<std::shared_ptr<WsSession>> snap;
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        snap.reserve(sessions_.size());
        for (auto& [id, s] : sessions_) snap.push_back(s);
    }
    for (auto& s : snap) s->send(msg);
}

void WsServer::sendTo(ClientId id, const std::string& msg) {
    std::shared_ptr<WsSession> s;
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) s = it->second;
    }
    if (s) s->send(msg);
}

} // namespace dart::api

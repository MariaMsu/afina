#include <utility>

#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONN
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <queue>

#include <sys/epoll.h>
#include <spdlog/logger.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace STnonblock {



class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> pl) :
            _socket(s),
            pStorage(std::move(ps)),
            _logger(std::move(pl)) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;

        // check if iterator is random_access
        static_assert(std::is_same<std::iterator_traits<decltype(answer_buf.begin())>::iterator_category,
                      std::random_access_iterator_tag>::value, "requires random iterator");
        }

    inline bool isAlive() const { return is_alive; }

    void Start();

protected:
    void OnError();

    void OnClose();

    void DoRead();

    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    bool is_alive = true;
    std::shared_ptr<spdlog::logger> _logger;

    int already_read_bytes = 0;
    char client_buffer[4096];
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::shared_ptr<Afina::Storage> pStorage;

    std::vector<std::string> answer_buf;

    int cur_position = 0;

};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H































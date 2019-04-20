#include "Connection.h"

#include <iostream>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Connection on {} socket started", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; // без записи
    _event.data.fd = _socket;
    _event.data.ptr = this;
}

// See Connection.h
void Connection::OnError() {
    _logger->warn("Connection on {} socket has error", _socket);
    is_alive = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("Connection on {} socket closed", _socket);
    is_alive = false;
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("Do read on {} socket", _socket);
    try {
        int got_bytes = -1;
        while ((got_bytes = read(_socket, client_buffer + already_read_bytes,
                                 sizeof(client_buffer) - already_read_bytes)) > 0) {
            already_read_bytes += got_bytes;
            _logger->debug("Got {} bytes from socket", got_bytes);

            // Single block of data read from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (already_read_bytes > 0) {
                _logger->debug("Process {} bytes", already_read_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, already_read_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, already_read_bytes - parsed);
                        already_read_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", already_read_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(already_read_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, already_read_bytes - to_read);
                    arg_remains -= to_read;
                    already_read_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Save response
                    result += "\r\n";

                    bool add_EPOLLOUT = answer_buf.empty();

                    answer_buf.push_back(result);
                    if (add_EPOLLOUT)
                        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (read_bytes)
        }

        is_alive = false;
        if (got_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->debug("Do write on {} socket", _socket);

    struct iovec iovecs[answer_buf.size()];
    for (int i = 0; i < answer_buf.size(); i++) {
        iovecs[i].iov_len = answer_buf[i].size();
        iovecs[i].iov_base = &(answer_buf[i][0]);
    }
    iovecs[0].iov_base = static_cast<char *>(iovecs[0].iov_base) + cur_position;

    int written;
    if ((written = writev(_socket, iovecs, answer_buf.size())) <= 0) {
        is_alive = false;
        throw std::runtime_error("Failed to send response");
    }

    cur_position += written;

    int i = 0;
    while ((i < answer_buf.size()) && ((cur_position - iovecs[i].iov_len) >= 0)) {
        i++;
        cur_position -= iovecs[i].iov_len;
    }

    answer_buf.erase(answer_buf.begin(), answer_buf.begin() + i);
    if (answer_buf.empty()) {
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; // без записи
    }


//    _logger->debug("Do write on {} socket", _socket);
//
//    int count;
//    ioctl(_socket, FIONREAD, &count); // размер сетевой карты
//
//    std::string answer;
//    while (answer.size() + answer_buf.front().size() < count) {
//        answer = answer_buf.front();
//        answer_buf.pop();
//    }
//
//    unsigned long tail_position = count - answer.size();
//    answer += answer_buf.front().substr(0, tail_position);
//    answer_buf.front() = answer_buf.front().substr(tail_position);
//
//    try {
//        if (send(_socket, answer.c_str(), answer.size(), 0) <= 0) {
//            is_alive = false;
//            throw std::runtime_error("Failed to send response");
//        }
//    } catch (std::runtime_error &ex) {
//        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
//    }
//
//    if (answer_buf.empty())
//        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; // без записи
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina

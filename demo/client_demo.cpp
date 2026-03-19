#include <iostream>
#include <string>

#include <boost/asio.hpp>

int main() {
    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        auto endpoints = resolver.resolve("127.0.0.1", "3000");

        boost::asio::ip::tcp::socket socket(io);
        boost::asio::connect(socket, endpoints);
        std::cout << "Connected to 127.0.0.1:3000\n";

        while(true) {
            std::cout << "> ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;
            }
            line += "\n";

            boost::asio::write(socket, boost::asio::buffer(line));

            boost::asio::streambuf buf;
            boost::asio::read_until(socket, buf, '\n');
            std::istream is(&buf);
            std::string response;
            std::getline(is, response);
            std::cout << "< " << response << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}


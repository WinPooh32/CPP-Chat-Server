#include <list>
#include <set>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "MyClient.h"

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;

io_service service;
ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 1314));

void handle_accept(MyClient::ptr client,
        const boost::system::error_code & err) {
    client->start();
    MyClient::ptr new_client = MyClient::new_(service);
    acceptor.async_accept(new_client->sock(),
            boost::bind(handle_accept, new_client, _1));
}

int main() {
    std::cout << "Waiting for new connections." << std::endl;

    MyClient::ptr client = MyClient::new_(service);
    acceptor.async_accept(client->sock(),
            boost::bind(handle_accept, client, _1));
    service.run();
}

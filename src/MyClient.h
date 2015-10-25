#ifndef MYCLIENT_H_
#define MYCLIENT_H_

#include <iostream>
#include <list>
#include <set>
#include <cstring>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)

enum chat_error {
	ERORR_AUTH_FAIL = 0,
	ERROR_TAKEN_NICKNAME,
	ERROR_BAD_PASSWORD,
	ERROR_UNKNOWN_ROOM,
	ERROR_FORBIDDEN,
	ERROR_TIME_OUT,
	ERROR_BAD_REQUEST
};

enum chat_request {
	REQUEST_AUTH = 0,
	REQUEST_REGISTER,
	REQUEST_DISCONNECT_ROOM,
	REQUEST_DISCONNECT,
	REQUEST_PING,
	REQUEST_JOIN,
	REQUEST_MESSAGE
};

class MyClient: public boost::enable_shared_from_this<MyClient>,
		boost::noncopyable {
	MyClient(io_service& service) :
			sock_(service), started_(false) {
	};
	typedef MyClient self_type;
public:
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<MyClient> ptr;

	void start();
	static ptr new_(io_service& service);
	void stop();
	ip::tcp::socket & sock();

private:
	int parse_request(const string& request, chat_error* err, string& arg1,
			string& arg2);
	void on_read(const error_code & err, size_t bytes);
	void on_write(const error_code & err, size_t bytes);
	void do_read();
	void do_write(const std::string & msg);
	void do_write_error(const std::string & msg);
	void do_write_broadcast(const std::string & msg);
	size_t read_complete(const boost::system::error_code & err, size_t bytes);

	enum {
		max_msg = 1024
	};
	char read_buffer_[max_msg];
	char write_buffer_[max_msg];
	ip::tcp::socket sock_;
	bool started_;
	bool auth_done_ = false;
	string nick_;

	static std::set<ptr> clients_;
};
#endif /* MYCLIENT_H_ */

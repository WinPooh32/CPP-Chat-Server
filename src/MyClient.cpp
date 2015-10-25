#include "MyClient.h"

std::set<MyClient::ptr> MyClient::clients_;

void MyClient::start() {
    started_ = true;
    clients_.insert(shared_from_this());

    //cout << "Connected " << sock_.remote_endpoint().address().to_string()
    //     << ":" << sock_.remote_endpoint().port() << endl;

    do_read();
}

MyClient::ptr MyClient::new_(io_service& service) {
    ptr new_(new MyClient(service));
    return new_;
}

void MyClient::stop() {
    if (!started_)
        return;
    started_ = false;

    cout << "Disconnected" << endl;

    if (!sock_.is_open()) {
        sock_.close();
    }

    clients_.erase(shared_from_this());
}

ip::tcp::socket & MyClient::sock() {
    return sock_;
}

//Returns end position and value of word
int get_next_word(const string& source, int begin, string& arg) {
    begin = source.find('\"', begin);
    int end = -1;

    for (int i = begin + 1; i != -1;) {
        i = source.find("\"", i);
        if (i != -1)
            if (source[i - 1] != '\\') {
                end = i;
                break;
            }
    }

    if (begin != -1 && end != -1) {
        arg = source.substr(begin + 1, end - begin - 1);
        return end;
    } else {
        return -1;
    }
}

int MyClient::parse_request(const string& request, chat_error* err,
        string& arg1, string& arg2) {
#define AUTH_LEN 5
#define REG_LEN 10
#define DIS_R_LEN 17
#define DIS_LEN 10
#define PING_LEN 5
#define JOIN_LEN 5
#define MSG_LEN 8

    if (strncmp(request.c_str(), "MESSAGE:", MSG_LEN) == 0) {
        if (get_next_word(request, MSG_LEN, arg1) == -1) {
            if (err)
                *err = ERROR_BAD_REQUEST;
            return -1;
        };
        return REQUEST_MESSAGE;
    } else if (strncmp(request.c_str(), "PING", PING_LEN) == 0) {
        return REQUEST_PING;
    } else if (strncmp(request.c_str(), "DISCONNECT", DIS_LEN) == 0) {
        return REQUEST_DISCONNECT;
    } else if (strncmp(request.c_str(), "AUTH:", AUTH_LEN) == 0) {
        int end_first_word = get_next_word(request, AUTH_LEN, arg1);

        cout << arg1 << endl;

        int end_second_word = get_next_word(request, end_first_word + 1, arg2);

        if (end_first_word == -1 || end_second_word == -1) {
            if (err)
                *err = ERROR_BAD_REQUEST;
            return -1;
        }

        return REQUEST_AUTH;
    }

    if (err)
        *err = ERROR_BAD_REQUEST;
    return -1;
}

void MyClient::on_read(const error_code & err, size_t bytes) {
#define STOPIFNAUTH if(auth_done_ == false){ stop(); return; }

    if (!err) {
        read_buffer_[max_msg - 1] = '\0';
        std::string msg(read_buffer_, bytes);
        //cout << read_buffer_ << endl;

        chat_error err;
        string arg1, arg2;
        int result = -1;

        //auth_done_ = true; //TODO REMOVE

        cout << "RAW_MSG: " << msg << endl;
        if ((result = parse_request(msg, &err, arg1, arg2)) != -1) {
            /*
			 REQUEST_AUTH = 0,
			 REQUEST_REGISTER,
			 REQUEST_DISCONNECT_ROOM,
			 REQUEST_DISCONNECT,
			 REQUEST_PING,
			 REQUEST_JOIN,
			 REQUEST_MESSAGE
             */

            switch (result) {

            case REQUEST_MESSAGE:
                STOPIFNAUTH
                do_write_broadcast(
                        string("MESSAGE:\"") + "<" + nick_ + ">" + arg1 + "\"");

                cout << arg1 << endl;
                break;

            case REQUEST_AUTH:
                nick_ = arg1;
                cout << "Nick: " << nick_ << endl;
                //string pass = arg2;
                auth_done_ = true; // FIXME fix this
                do_write("AUTH_OK");
                //do_write_broadcast("[SERVER] " + nick_ + " connected.");
                break;

            case REQUEST_PING:
                STOPIFNAUTH
                do_write("PING_OK");
                break;

            default:
                cerr << "I LOVE MAGIC!" << endl;
            }

        } else {
            if (err == ERROR_BAD_REQUEST) {
                cout << "ERROR_BAD_REQUEST" << std::endl;
                do_write("ERROR:\"6\"");
                do_write("DISCONNECTED");
            } else {
                cerr << "SOME ERR" << err << endl;
            }

            stop();
        }
    } else {
        cerr << "SOME MAGIC HAS BEEN HAPPENNED!" << endl;
        stop();
    }
}

void MyClient::on_write(const error_code & err, size_t bytes) {
    do_read();
}

void MyClient::do_read() {

    sock_.async_read_some(buffer(read_buffer_),
            boost::bind(&MyClient::on_read, shared_from_this(), _1, _2));
    //async_read(sock_, buffer(read_buffer_), MEM_FN2(read_complete,_1,_2), MEM_FN2(on_read,_1,_2));
    //async_read(sock_, buffer(read_buffer_), transfer_all(), MEM_FN2(on_read,_1,_2));
}

void MyClient::do_write(const std::string & msg) {
    if (sock_.is_open()) {
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
                MEM_FN2(on_write, _1, _2));
    } else {
        stop();
    }
}

void MyClient::do_write_error(const std::string & msg) {
    if (sock_.is_open()) {
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
                MEM_FN2(on_write, _1, _2));
    }
    stop();
}

void MyClient::do_write_broadcast(const std::string & msg) {
    std::copy(msg.begin(), msg.end(), write_buffer_);
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
        if (it->get()->sock_.is_open()) {
            it->get()->sock_.async_write_some(buffer(write_buffer_, msg.size()),
                    MEM_FN2(on_write, _1, _2));
        } else {
            it->get()->stop();
        }
    }
}

size_t MyClient::read_complete(const boost::system::error_code & err,
        size_t bytes) {
    if (err)
        return 0;
    //bool found = std::find(read_buffer_, read_buffer_ + bytes, '\0') < read_buffer_ + bytes;
    bool found = (read_buffer_[bytes] != '\0') && bytes < max_msg;
    // we read one-by-one until we get to enter, no buffering
    cout << "TOREAD " << bytes << endl;

    return found ? 0 : 1;
}

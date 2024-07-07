#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <netdb.h>
#include <regex>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

std::string str_tolower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](char ch) { return std::tolower(ch); });
  return str;
}

std::string get_pathname(const std::string &req_buffer, std::string route) {

  int route_index_start =
      req_buffer.find_first_of(route + "/", *req_buffer.begin());
  int route_index_end = route_index_start + route.length() + 2;

  int first_whitespace_index = req_buffer.find_first_of(" ", route_index_end);

  return req_buffer.substr(route_index_end,
                           first_whitespace_index - route_index_end);
}

bool route_match(std::string req_path, std::string route_name) {
  if (std::count(route_name.begin(), route_name.end(), ':')) {
    const std::string base_path =
        route_name.substr(0, route_name.find_first_of(':') - 1);

    const std::string req_base_path =
        req_path.substr(0, route_name.find_first_of(':') - 1);

    return req_base_path == base_path;
  }
  return req_path == route_name;
}

std::string extract_method(const std::string &req_buffer) {
  int first_whitespace_index = req_buffer.find_first_of(' ');
  return req_buffer.substr(0, first_whitespace_index);
}

std::string extract_path(const std::string &req_buffer) {
  int start_index = req_buffer.find_first_of('/');
  int end_index = req_buffer.find_first_of(' ', start_index);
  return req_buffer.substr(start_index, end_index - start_index);
}

std::string extract_headers(const std::string &req_buffer) {
  int headers_start_index = req_buffer.find_first_of("\r\n") + 2;

  return req_buffer.substr(headers_start_index,
                           req_buffer.find_last_of("\r\n\r\n") -
                               headers_start_index);
}

std::string extract_body(const std::string &req_buffer) {
  int body_start_index = req_buffer.find_last_of("\r\n\r\n");

  return req_buffer
      .substr(body_start_index, req_buffer.length() - body_start_index)
      .c_str();
}

struct Request {
  std::string method;
  std::string path;
  std::string headers;
  std::string body;
  Request(std::string &req_buffer) {
    method = extract_method(req_buffer);
    // std::cout << "method: " << method << '\n';

    path = extract_path(req_buffer);
    // std::cout << "path: " << path << '\n';

    headers = extract_headers(req_buffer);
    // std::cout << "headers: " << headers << '\n';

    body = extract_body(req_buffer);
    // std::cout << "body: " << body << '\n';
  };
};

struct Response {
public:
  std::string status;
  std::vector<std::string> headers;
  std::string body;
  std::string not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
  std::string ok = "HTTP/1.1 200 OK\r\n\r\n";

  std::string m_construct_res() {
    std::ostringstream res;
    std::string newline = "\r\n";

    std::string content_length;
    std::string headers_string;

    add_content_length_header();

    for (const std::string &header_line : headers) {
      headers_string += header_line + newline;
    }

    res << "HTTP/1.1 " << status << newline << headers_string << newline
        << body;

    return res.str();
  }

private:
  void add_content_length_header() {
    if (body.length() > 0) {
      headers.push_back("Content-Length: " + std::to_string(body.length()));
    }
  }
};

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 10;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  std::vector<int> clients;

  while (true) {

    std::cout << "client_addr: " << client_addr.sin_addr.s_addr << '\n';

    std::string req_buffer;
    req_buffer.resize(1024);

    int client = accept(server_fd, (struct sockaddr *)&client_addr,
                        (socklen_t *)&client_addr_len);

    clients.push_back(client);

    size_t bytes_received = recv(client, &req_buffer[0], req_buffer.size(), 0);

    std::ostringstream res;

    if (bytes_received > 0) {

      Request request(req_buffer);
      Response response;
      if (route_match(request.path, "/")) {
        res << response.ok;
      } else if (route_match(request.path, "/echo")) {

        response.status = "200 OK";
        response.headers.push_back("Content-Type: text/plain");
        response.body = get_pathname(req_buffer, "echo");

        res << response.m_construct_res();

      } else if (route_match(request.path, "/user-agent")) {

        std::string headers = request.headers;

        int user_agent_index_start =
            str_tolower(headers).find("user-agent", 0) + 12;

        int user_agent_index_end =
            headers.find_first_of("\r\n", user_agent_index_start);

        std::string user_agent =
            headers.substr(user_agent_index_start,
                           user_agent_index_end - user_agent_index_start);

        response.status = "200 OK";
        response.headers.push_back("Content-Type: text/plain");
        response.body = user_agent;

        res << response.m_construct_res();

      } else if (route_match(request.path, "/files/:filename")) {
        std::string filepath = argv[2] + get_pathname(request.path, "/file");

        if (std::filesystem::exists(filepath)) {
          std::ifstream ifs(filepath);
          std::string file_content((std::istreambuf_iterator<char>(ifs)),
                                   (std::istreambuf_iterator<char>()));

          response.status = "200 OK";
          response.headers.push_back("Content-Type: application/octet-stream");
          response.body = file_content;

          std::cout << "response.m_construct_res(): "
                    << response.m_construct_res() << '\n';

          res << response.m_construct_res();

        } else {
          res << response.not_found;
        }

      } else if (route_match(request.path, "/chat")) {
        response.status = "200 OK";
        response.headers.push_back("Content-Type: text/plain");
        if (request.body.length() > 1) {
          response.body = request.body.c_str();
        }
        res << response.m_construct_res();
      } else {
        res << response.not_found;
      }
    }

    for (const int c : clients) {
      if (send(c, res.str().c_str(), res.str().size(), 0)) {
        std::cout << "response: " << res.str() << '\n';
      }
    }
  }
  close(server_fd);

  return 0;
}

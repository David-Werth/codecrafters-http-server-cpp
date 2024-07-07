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

std::string build_res(std::string code, std::string content_type,
                      std::string body, std::ostringstream &res) {
  res << "HTTP/1.1 " << code << "\r\n"
      << "Content-Type: " << content_type << "\r\n"
      << "Content-Length: " << body.length() << "\r\n\r\n"
      << body;

  return res.str();
}

std::string str_tolower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](char ch) { return std::tolower(ch); });
  return str;
}

std::string get_pathname(std::string req_buffer, std::string route) {
  int route_index_start =
      req_buffer.find_first_of("/" + route + "/", *req_buffer.begin());
  int route_index_end = route_index_start + route.length() + 2;

  int first_whitespace_index = req_buffer.find_first_of(" ", route_index_end);

  return req_buffer.substr(route_index_end,
                           first_whitespace_index - route_index_end);
}

bool route_match(const std::string &req_path, const std::string &route_name) {
  bool match;

  if (std::count(route_name.begin(), route_name.end(), ':')) {
    const std::string base_path =
        route_name.substr(0, route_name.find_first_of(':') - 1);
    const std::string req_base_path =
        base_path.substr(0, route_name.find_first_of(':') - 1);

    return req_base_path == base_path;
  }
  return req_path == route_name;
}

std::string extract_method(const std::string &req_buffer) {
  int first_whitespace_index = req_buffer.find_first_of(' ');
  return req_buffer.substr(*req_buffer.begin(), first_whitespace_index);
}

std::string extract_path(const std::string &req_buffer) {
  int start_index = req_buffer.find_first_of('/');
  int end_index = req_buffer.find_first_of(' ', start_index);
  return req_buffer.substr(start_index, end_index - start_index);
}

std::string extract_headers(const std::string &req_buffer) {
  int headers_start_index = req_buffer.find_first_of("\r\n") + 2;

  return req_buffer.substr(headers_start_index,
                           req_buffer.find_first_of("\r\n\r\n") -
                               headers_start_index);
}

struct Request {
  std::string method;
  std::string path;
  std::string headers;
  std::string body;
  Request(std::string &req_buffer) {
    method = extract_method(req_buffer);
    path = extract_path(req_buffer);
    headers = extract_headers(req_buffer);
    // body = extract_body(req_buffer);
  };
};

struct Response {
  std::string headers;
  std::string body;
  Response(std::string &req_buffer){};
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

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  std::vector<int> connections;

  while (true) {

    std::string req_buffer(1024, '\0');

    int client = accept(server_fd, (struct sockaddr *)&client_addr,
                        (socklen_t *)&client_addr_len);

    size_t bytes_received = recv(client, &req_buffer[0], req_buffer.size(), 0);

    std::ostringstream res;

    if (bytes_received > 0) {

      Request request(req_buffer);

      // std::cout << "req_buffer: " << req_buffer << '\n';

      if (route_match(request.path, "/")) {
        res << "HTTP/1.1 200 OK\r\n\r\n";
      } else if (route_match(request.path, "/echo")) {
        res << build_res("200 OK", "text/plain",
                         get_pathname(req_buffer, "echo"), res);
      } else if (route_match(request.path, "/user-agent")) {

        std::string headers = request.headers;

        int user_agent_index_start =
            str_tolower(headers).find("user-agent", 0) + 12;

        int user_agent_index_end =
            headers.find_first_of("\r\n", user_agent_index_start);

        std::string user_agent =
            headers.substr(user_agent_index_start,
                           user_agent_index_end - user_agent_index_start);

        res << build_res("200 OK", "text/plain", user_agent, res);

      } else if (route_match(request.path, "/files/:filename")) {
        std::string filepath = argv[2] + get_pathname(req_buffer, "files");

        if (std::filesystem::exists(filepath)) {
          std::ifstream ifs(filepath);
          std::string file_content((std::istreambuf_iterator<char>(ifs)),
                                   (std::istreambuf_iterator<char>()));

          res << build_res("200 OK", "application/octet-stream", file_content,
                           res);

        } else {
          res << "HTTP/1.1 404 Not Found\r\n\r\n";
        }

      } else {
        res << "HTTP/1.1 404 Not Found\r\n\r\n";
      }
    }

    // std::cout << "res: " << res.str() << '\n';

    send(client, res.str().c_str(), res.str().size(), 0);
  }

  close(server_fd);

  return 0;
}

#include <_ctype.h>
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstdio>
#include <netdb.h>
#include <regex>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

  int client = accept(server_fd, (struct sockaddr *)&client_addr,
                      (socklen_t *)&client_addr_len);

  std::string req_buffer(1024, '\0');

  size_t bytes_received = recv(client, &req_buffer[0], req_buffer.size(), 0);

  std::ostringstream res;

  if (bytes_received > 0) {

    if (req_buffer.starts_with("GET / HTTP/1.1")) {
      res << "HTTP/1.1 200 OK\r\n\r\n";

    } else if (req_buffer.starts_with("GET /echo/")) {
      int route_index_start = req_buffer.find_first_of("/echo/", 0);
      int route_index_end = route_index_start + 6;

      int first_whitespace_index =
          req_buffer.find_first_of(" ", route_index_end);

      std::string extracted_str = req_buffer.substr(
          route_index_end, first_whitespace_index - route_index_end);

      res << build_res("200 OK", "text/plain", extracted_str, res);

    } else if (req_buffer.starts_with("GET /user-agent")) {
      int headers_index_start = req_buffer.find_first_of("\r\n", 0) + 2;
      int headers_index_end =
          req_buffer.substr(headers_index_start, *req_buffer.end())
              .find_first_of("\r\n", 0);

      std::string headers = req_buffer.substr(
          headers_index_start, headers_index_end - headers_index_start);

      int user_agent_index_start =
          str_tolower(headers).find("user-agent", 0) + 12;

      int user_agent_index_end = headers.find("\r\n", user_agent_index_start);

      std::string user_agent =
          headers.substr(user_agent_index_start,
                         user_agent_index_end - user_agent_index_start);

      res << build_res("200 OK", "text/plain", user_agent, res);

    } else {
      res << "HTTP/1.1 404 Not Found\r\n\r\n";
    }
  }

  // std::cout << "res: " << res.str() << '\n';

  // std::cout << "Buffer: " << buffer << '\n';

  send(client, res.str().c_str(), res.str().size(), 0);

  close(server_fd);

  return 0;
}

#include "socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

void get_URL(const string &host, const string &path) {
    // Your code here.
    // 使用TCPSocket类创建 TCP 套接字
    TCPSocket sock;

    // 连通过connect方法连接到指定主机的 HTTP 服务（默认端口 80）
    sock.connect(Address(host, "http"));

    // 发送HTTP GET请求，这里我们构造标准的 HTTP 1.1 GET 请求
    //请求方法和路径（GET [path] HTTP/1.1）
    string request = "GET " + path + " HTTP/1.1\r\n";
    //主机头（Host: [host]）
    request += "Host: " + host + "\r\n";
    //连接关闭标志（Connection: close）
    request += "Connection: close\r\n";
    request += "\r\n";  // 请求结束标志
    //使用write方法发送请求
    sock.write(request);

    // 调用shutdown(SHUT_WR)关闭写入端，告知服务器已完成请求发送
    sock.shutdown(SHUT_WR);

    // 读取并打印服务器的所有响应，直到EOF
    string response;
    //循环调用read方法直到到达文件末尾（eof()），读取所有响应数据
    while (!sock.eof()) {
        string segment = sock.read();
        response += segment;
    }
    //将读取到的响应打印到标准输出

    cout << response;

    // 关闭套接字
    sock.close();
    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    cerr << "Warning: get_URL() has not been implemented yet.\n";
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

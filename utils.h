// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

void set_no_delay(int fd);
int start_listen(const char *adapter, int portnr, int listen_queue_size);
std::string get_endpoint_name(int fd);
ssize_t READ(int fd, char *whereto, size_t len);
ssize_t WRITE(int fd, const char *whereto, size_t len);
double get_ts();
std::string myformat(const char *const fmt, ...);
unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len);
int connect_to(const std::string & hostname, int port);
void set_thread_name(const std::string & name);

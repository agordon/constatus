// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "source.h"
#include "utils.h"

void set_no_delay(int fd, bool use_no_delay)
{
        int flag = use_no_delay;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
                error_exit(true, "could not set TCP_NODELAY on socket");
}

int start_listen(const char *adapter, int portnr, int listen_queue_size)
{
	struct sockaddr_in6 server_addr;
	int server_addr_len = 0;
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd == -1)
		error_exit(true, "failed creating socket");

	int reuse_addr = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
		error_exit(true, "setsockopt(SO_REUSEADDR) failed");

#ifdef TCP_FASTOPEN
	int qlen = 128;
	setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof qlen);
#endif

	server_addr_len = sizeof(server_addr);
	memset((char *)&server_addr, 0x00, server_addr_len);
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_port = htons(portnr);
	if (!adapter || strcmp(adapter, "0.0.0.0") == 0)
	{
		server_addr.sin6_addr = in6addr_any;
	}
	else
	{
		if (inet_pton(AF_INET6, adapter, &server_addr.sin6_addr) == 0)
		{
			fprintf(stderr, "\n");
			fprintf(stderr, " * inet_pton(%s) failed: %s\n", adapter, strerror(errno));
			fprintf(stderr, " * If you're trying to use an IPv4 address (e.g. 192.168.0.1 or so)\n");
			fprintf(stderr, " * then do not forget to place ::FFFF: in front of the address,\n");
			fprintf(stderr, " * e.g.: ::FFFF:192.168.0.1\n\n");
			error_exit(false, "listen socket initialisation failure");
		}
	}

	if (bind(fd, (struct sockaddr *)&server_addr, server_addr_len) == -1)
		error_exit(true, "bind() failed");

	if (listen(fd, listen_queue_size) == -1)
		error_exit(true, "listen() failed");

	return fd;
}

std::string get_endpoint_name(int fd)
{
	char buffer[4096] = { "?" };
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof addr;

	if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1)
		snprintf(buffer, sizeof buffer, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	else
	{
		char buffer2[4096];

		if (inet_ntop(AF_INET6, &addr.sin6_addr, buffer2, sizeof buffer2))
			snprintf(buffer, sizeof buffer, "[%s]:%d", buffer2, ntohs(addr.sin6_port));
		else
			snprintf(buffer, sizeof buffer, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	}

	return std::string(buffer);
}

ssize_t READ(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = read(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return -1;
		}
		else if (rc == 0)
			break;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t WRITE(int fd, const char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return -1;
		}
		else if (rc == 0)
			return -1;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

double get_ts()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

        return double(tv.tv_sec) + double(tv.tv_nsec) / 1000000000.0;
}

uint64_t get_us()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	return uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);
}

std::string myformat(const char *const fmt, ...)
{
	char *buffer = NULL;
        va_list ap;

        va_start(ap, fmt);
        (void)vasprintf(&buffer, fmt, ap);
        va_end(ap);

	std::string result = buffer;
	free(buffer);

	return result;
}

unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len)
{
	if (haystack_len < needle_len)
		return NULL;

	unsigned int pos = 0;
	for(;(haystack_len - pos) >= needle_len;)
	{
		unsigned char *p = (unsigned char *)memchr(&haystack[pos], needle[0], haystack_len - pos);
		if (!p)
			return NULL;

		pos = (unsigned int)(p - haystack);
		if ((haystack_len - pos) < needle_len)
			break;

		if (memcmp(&haystack[pos], needle, needle_len) == 0)
			return &haystack[pos];

		pos++;
	}

	return NULL;
}

void set_thread_name(const std::string & name)
{
	std::string full_name = "cs:" + name;

	if (full_name.length() > 15)
		full_name = full_name.substr(0, 15);

	pthread_setname_np(pthread_self(), full_name.c_str());
}

std::string get_thread_name()
{
	char buffer[16];

	pthread_getname_np(pthread_self(), buffer, sizeof buffer);

	return buffer;
}

void mysleep(double slp, std::atomic_bool *const stop_flag, source *const s)
{
	bool unreg = slp >= 1.0;

	if (unreg && s)
		s -> unregister_user();

	while(slp > 0 && !*stop_flag) {
		double cur = std::min(slp, 0.1);
		slp -= cur;

		int s = cur;

		uint64_t us = (cur - s) * 1000 * 1000;

		// printf("%d, %lu\n", s, us);

		if (s)
			::sleep(s); // FIXME handle signals

		if (us)
			usleep(us); // FIXME handle signals
	}

	if (unreg && s)
		s -> register_user();
}

void *find_symbol(void *library, const char *const symbol, const char *const what, const char *const library_name)
{
	void *ret = dlsym(library, symbol);

	if (!ret)
		error_exit(true, "Failed finding %s \"%s\" in %s: %s", what, symbol, library_name, dlerror());

	return ret;
}

char * un_url_escape(const char *const in)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char *temp = curl_easy_unescape(ch, in, 0, NULL);
	char *out = strdup(temp);

	curl_free(temp);
	curl_easy_cleanup(ch);

	return out;
}

std::string url_escape(const std::string & in)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char *temp = curl_easy_escape(ch, in.c_str(), in.size());
	std::string out = temp;

	curl_free(temp);
	curl_easy_cleanup(ch);

	return out;
}

std::vector<std::string> * split(std::string in, std::string splitter)
{
	std::vector<std::string> *out = new std::vector<std::string>;
	size_t splitter_size = splitter.size();

	for(;;)
	{
		size_t pos = in.find(splitter);
		if (pos == std::string::npos)
			break;

		std::string before = in.substr(0, pos);
		out -> push_back(before);

		size_t bytes_left = in.size() - (pos + splitter_size);
		if (bytes_left == 0)
		{
			out -> push_back("");
			return out;
		}

		in = in.substr(pos + splitter_size);
	}

	if (in.size() > 0)
		out -> push_back(in);

	return out;
}

std::string search_replace(const std::string & in, const std::string & search, const std::string & replace)
{
	std::string work = in;
	std::string::size_type pos = 0u;

	while((pos = work.find(search, pos)) != std::string::npos) {
		work.replace(pos, search.length(), replace);
		pos += replace.length();
	}

	return work;
}

std::vector<file_t> * load_filelist(const std::string & dir, const std::string & prefix)
{
	auto *out = new std::vector<file_t>;

	DIR *d = opendir(dir.c_str());
	if (!d)
		return out;

	for(;;) {
		struct dirent *de = readdir(d);
		if (!de)
			break;

		struct stat st;
		if (fstatat(dirfd(d), de -> d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
			log(LL_WARNING, "fstatat failed for %s: %s", de -> d_name, strerror(errno));
			continue;
		}

		if (!S_ISREG(st.st_mode))
			continue;

		if (strncmp(de -> d_name, prefix.c_str(), prefix.size()) == 0) {
			file_t f = { de -> d_name, st.st_mtim.tv_sec, st.st_size };

			out -> push_back(f);
		}
	}

	closedir(d);

	return out;
}

std::string myctime(const time_t t)
{
	struct tm ptm;
	localtime_r(&t, &ptm);

	char buffer[4096];
	strftime(buffer, sizeof buffer, "%c", &ptm);

	return buffer;
}

int connect_to(std::string hostname, int port, std::atomic_bool *abort)
{
	std::string portstr = myformat("%d", port);
	int fd = -1;

        struct addrinfo hints;
        struct addrinfo* result = NULL;
        memset(&hints, 0x00, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;
        int rc = getaddrinfo(hostname.c_str(), portstr.c_str(), &hints, &result);
        if (rc) {
                log(LL_ERR, "Cannot resolve host name %s: %s", hostname.c_str(), gai_strerror(rc));
		freeaddrinfo(result);
		return -1;
        }

	for (struct addrinfo *rp = result; rp != NULL; rp = rp -> ai_next) {
		fd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
		if (fd == -1)
			continue;

		int old_flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, O_NONBLOCK);

		/* wait for connection */
		/* connect to peer */
		if (connect(fd, rp -> ai_addr, rp -> ai_addrlen) == 0) {
			/* connection made, return */
			(void)fcntl(fd, F_SETFL, old_flags);
			freeaddrinfo(result);
			return fd;
		}

		for(;;) {
			fd_set wfds;
			FD_ZERO(&wfds);
			FD_SET(fd, &wfds);

			struct timeval tv;
			tv.tv_sec  = 0;
			tv.tv_usec = 100 * 1000;

			/* wait for connection */
			rc = select(fd + 1, NULL, &wfds, NULL, &tv);
			if (rc == 0)	// time out
			{
			}
			else if (rc == -1)	// error
			{
				if (errno == EINTR || errno == EAGAIN || errno == EINPROGRESS)
					continue;

				log(LL_ERR, "Select failed during connect to %s: %s", hostname.c_str(), strerror(errno));
				break;
			}
			else if (FD_ISSET(fd, &wfds))
			{
				int optval=0;
				socklen_t optvallen = sizeof(optval);

				/* see if the connect succeeded or failed */
				getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optvallen);

				/* no error? */
				if (optval == 0)
				{
					fcntl(fd, F_SETFL, old_flags);
					freeaddrinfo(result);
					return fd;
				}
			}

			if (abort -> exchange(false))
			{
				abort -> store(true);
				close(fd);
				freeaddrinfo(result);
				return -1;
			}
		}

		close(fd);
		fd = -1;
	}

	freeaddrinfo(result);

	return fd;
}

long int hash(const std::string & what)
{
	return std::hash<std::string>{}(what);
}

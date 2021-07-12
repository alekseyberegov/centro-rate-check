#include "io.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h> // link with -lz


static bool file_read(int fd, char*& buf, size_t& len) {
	struct stat ss;
	if (fstat(fd, &ss) == -1) {
		LOG_ERRNO("fstat()");
		close(fd);
		return false;
	}
	len = ss.st_size;

	buf = (char*)malloc(len + 1);
	if (read(fd, buf, len) != (ssize_t)len) {
		LOG("read()");
		close(fd);
		free(buf);
		return false;
	}
	buf[len] = '\0';

	return true;
}

bool file_read(const char* fn, char*& buf, size_t& len) {
	int fd = open(fn, O_RDONLY);
	if (fd == -1) {
		LOG_ERRNO("open(%s)", fn);
		return false;
	}
	bool rv = file_read(fd, buf, len);
	close(fd);
	return rv;
}

bool file_write(const char* fn, const char* buf, size_t len) {
	int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd == -1) {
		LOG_ERRNO("open(%s)", fn);
		return false;
	}
	errno = 0;
	if (write(fd, buf, len) != (ssize_t)len) {
		LOG_ERRNO("write(%s)", fn);
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

bool docompress(const char* src, size_t srclen, char*& dst, size_t* dstlen) {
	assert(srclen);
	*dstlen = PAD4(MAX(srclen, compressBound(srclen)));
	dst = (char*)calloc(1, *dstlen);

	int rv;
	if ((rv = compress2((unsigned char*)dst, (unsigned long*)dstlen, (unsigned char*)src, srclen, Z_BEST_COMPRESSION)) != Z_OK) {
		LOG("cannot compress: %i", rv);
		free(dst);
		dst = NULL;
		*dstlen = 0;
		return false;
	}

	if (*dstlen >= srclen) {
		memset(dst, 0, *dstlen);
		memcpy(dst, src, srclen);
		*dstlen = srclen;
	}

	return true;
}

bool decompress(const char* src, size_t srclen, char*& dst, size_t dstlen) {
	assert(!dst);
	dst = (char*)calloc(1, PAD4(dstlen));

	if (dstlen < srclen) {
		return false;
	} else if (dstlen == srclen) {
		memcpy(dst, src, dstlen);
	} else {
		// https://www.zlib.net/manual.html#Utility
		int rv;
		long unsigned int l = dstlen;
		#ifndef NDEBUG
			void* bup = memcpy(malloc(srclen), src, srclen);
		#endif
		if ((rv = uncompress((unsigned char*)dst, &l, (unsigned char*)src, srclen)) != Z_OK) {
			LOG("cannot uncompress: %i", rv);
			free(dst);
			dst = NULL;
			return false;
		};
		#ifndef NDEBUG
			assert(memcmp(bup, src, srclen) == 0);
			free(bup);
		#endif
		if (l != dstlen) {
			LOG("expected %zu bytes, got %lu", dstlen, l);
			free(dst);
			dst = NULL;
			return false;
		}
	}
	return true;
}

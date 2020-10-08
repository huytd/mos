#include "stdio.h"

#include <errno.h>
#include <libc-pointer-arith.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int vsprintf(char *buffer, const char *fmt, va_list args)
{
	if (!fmt)
		return 0;

	char *buffer_iter = buffer;
	char *fmt_iter = fmt;
	char number_buf[32] = {0};
	for (; *fmt_iter; fmt_iter++)
	{
		if (*fmt_iter != '%')
		{
			*buffer_iter++ = *fmt_iter;
			continue;
		}

		switch (*++fmt_iter)
		{
		case 'c':
		{
			*buffer_iter++ = (char)va_arg(args, int);
			break;
		}

		case 's':
		{
			char *s = (char *)va_arg(args, char *);

			while (s && *s)
				*buffer_iter++ = *s++;
			break;
		}

		case 'd':
		case 'i':
		{
			int n = (int)va_arg(args, int);
			itoa_s(n, 10, number_buf);

			for (char *c = number_buf; *c; c++)
				*buffer_iter++ = *c;
			break;
		}

		case 'u':
		{
			unsigned int n = va_arg(args, unsigned int);
			itoa_s(n, 10, number_buf);

			for (char *c = number_buf; *c; c++)
				*buffer_iter++ = *c;
			break;
		}

		case 'l':
		{
			long long n = (long long)va_arg(args, long long);
			itoa_s(n, 10, number_buf);

			for (char *c = number_buf; *c; c++)
				*buffer_iter++ = *c;
			break;
		}

		case 'X':
		case 'x':
		{
			unsigned int n = va_arg(args, unsigned int);
			itoa_s(n, 16, number_buf);

			for (char *c = number_buf; *c; c++)
				*buffer_iter++ = *c;
			break;
		}
		default:
			*buffer_iter++ = *fmt_iter;
			break;
		}
	}

	*buffer_iter = '\0';
	return buffer_iter - buffer;
}

int sprintf(char *buffer, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	int size = vsprintf(buffer, fmt, args);

	va_end(args);

	return size;
}

bool valid_stream(FILE *stream)
{
	return stream->fd != -1;
}

FILE *fopen(const char *filename, const char *mode)
{
	int flags = O_RDWR;
	// don't support character b in mode
	if (mode[0] == 'r' && mode[1] != '+')
		flags = O_RDONLY;
	else if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] != '+')
		flags = O_WRONLY;

	int fd = open(filename, flags, 0);
	if (fd < 0)
		return NULL;

	return fdopen(fd, mode);
}

FILE *fdopen(int fd, const char *mode)
{
	FILE *stream = calloc(1, sizeof(FILE));
	stream->fd = fd;
	stream->flags |= _IO_UNBUFFERED;

	if (mode[0] == 'r' && mode[1] != '+')
		stream->flags |= _IO_NO_WRITES;
	else if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] != '+')
		stream->flags |= _IO_NO_READS;

	if (mode[0] == 'a')
	{
		stream->flags |= _IO_IS_APPENDING;
		stream->pos = lseek(fd, 0, SEEK_END);
	}

	// fill in blksize and mode
	struct stat stat;
	memset(&stat, 0, sizeof(struct stat));
	fstat(fd, &stat);

	if (S_ISREG(stat.mode))
		stream->flags |= _IO_FULLY_BUF;
	else if (isatty(fd))
		stream->flags |= _IO_LINE_BUF;

	stream->blksize = stat.blksize;
	return stream;
}

int feof(FILE *stream)
{
	return stream->flags & _IO_EOF_SEEN;
}

int ferror(FILE *stream)
{
	return stream->flags & _IO_ERR_SEEN;
}

int fileno(FILE *stream)
{
	if (valid_stream(stream))
		return stream->fd;
	return -EBADF;
}

void clearerr(FILE *stream)
{
	stream->flags &= ~_IO_ERR_SEEN & ~_IO_EOF_SEEN;
}

int fgetc(FILE *stream)
{
	assert(stream->read_ptr <= stream->read_end);

	if (stream->read_ptr == stream->read_end)
	{
		int count = (stream->flags & _IO_FULLY_BUF) ? max(stream->blksize, 1) : 1;
		char *buf = calloc(count, sizeof(char));

		count = read(stream->fd, buf, count);
		// end of file
		if (!count)
		{
			stream->flags |= _IO_EOF_SEEN;
			free(buf);
			return EOF;
		}

		free(stream->read_base);
		stream->read_base = stream->read_ptr = buf;
		stream->read_end = buf + count;
	}

	stream->pos++;
	return (unsigned char)*stream->read_ptr++;
}

char *fgets(char *s, int n, FILE *stream)
{
	assert(stream->read_ptr <= stream->read_end);

	int i;
	for (i = 0; i < n; ++i)
	{
		int ch = fgetc(stream);

		if (ch == '\r')
			break;
		if (ch == EOF)
			return NULL;
	}

	s[i] = 0;
	return s;
}

size_t fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
	int i;
	for (i = 0; i < nitems; ++i)
	{
		char *buf = calloc(size, sizeof(char));
		int ret = fgets(buf, size, stream);

		if (!ret)
		{
			free(buf);
			break;
		}

		memcpy((char *)ptr + i * size, buf, size);
		free(buf);
	}
	return i;
}

long int ftell(FILE *stream)
{
	return stream->pos;
}

off_t ftello(FILE *stream)
{
	return stream->pos;
}

int getchar()
{
	return getc(stdin);
}

int ungetc(int c, FILE *stream)
{
	if (c == EOF)
		return EOF;

	if (stream->read_ptr == stream->read_base)
	{
		int size = stream->read_end - stream->read_base + 1;
		char *buf = calloc(size, sizeof(char));

		memcpy(buf + 1, stream->read_base, size - 1);
		free(stream->read_base);

		stream->read_base = buf;
		stream->read_ptr = buf + 1;
		stream->read_end = buf + size;
	}
	stream->pos--;
	*--stream->read_ptr = (unsigned char)c;
	return (unsigned char)c;
}

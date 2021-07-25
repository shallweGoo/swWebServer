#include "warp.hpp"

static ssize_t my_read(int fd, char *ptr);

void Sys_err(const char* str)
{
	perror(str);
	exit(-1);
}

int Socket(int domain, int type, int protocol)
{
	int n = socket(domain,type,protocol);
	if(n == -1)	Sys_err("socket error");
	return n;
}

 
int Bind(int sockfd, const struct sockaddr *addr,socklen_t addrlen)
{
	int n = bind(sockfd, addr,addrlen);
	if(n == -1) Sys_err("bind error");
	return n;
}


int Listen(int sockfd, int backlog)
{
	int n = listen(sockfd,backlog);
	if(n == -1) Sys_err("bind error");
	return n;

}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int n = accept(sockfd,addr, addrlen);
again:
	if(n < 0)
	{
		if((errno == ECONNABORTED) || (errno == EINTR))
			goto again;
		else
		 	Sys_err("accept error");
	}
	return n;
}


int Connect(int sockfd, const struct sockaddr *addr,socklen_t addrlen)
{
	int n = connect(sockfd,addr,addrlen);
	if(n < 0) Sys_err("connect error");
	return n;
}




ssize_t Read(int fd, void *buf, size_t count)
{
	int n = read(fd,buf,count);
    if(n < 0) Sys_err("read error");
	return n;
}

ssize_t Write(int fd, const void *buf, size_t count)
{
	int n = write(fd,buf,count);	
    if(n < 0) Sys_err("write error");
	return n;

}


int Close(int fd)
{

	int n = close(fd);	
    if(n < 0) Sys_err("close error");
	return n;
}



ssize_t Readn(int fd, char *vptr, size_t n)
{
	size_t  nleft;              //usigned int 剩余未读取的字节数
	ssize_t nread;              //int 实际读到的字节数
	char   *ptr;

	ptr = vptr;
	nleft = n;                  //n 未读取字节数

	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		} else if (nread == 0)
			break;

		nleft -= nread;   //nleft = nleft - nread 
		ptr += nread;
	}
	return n - nleft;
}


ssize_t Writen(int fd, const char *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

static ssize_t my_read(int fd, char *ptr)
{
	static int read_cnt;
	static char *read_ptr;
	static char read_buf[100];

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {   //"hello\n"
			if (errno == EINTR)
				goto again;
			return -1;
		} else if (read_cnt == 0)
			return 0;

		read_ptr = read_buf;
	}
	read_cnt--;
	*ptr = *read_ptr++;

	return 1;
}

/*readline --- fgets*/    
//传出参数 vptr
ssize_t Readline(int fd, char *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char    c, *ptr;
	ptr = vptr;

	for (n = 1; n < maxlen; n++) {
		if ((rc = my_read(fd, &c)) == 1) {   //ptr[] = hello\n
			*ptr++ = c;
			if (c == '\n')
				break;
		} else if (rc == 0) {
			*ptr = 0;
			return n-1;
		} else
			return -1;
	}
	*ptr = 0;

	return n;
}

//获取一行 \r\n结尾的数据
int get_line(int cfd,char* buf,int size)
{
	int i = 0;
	char c= '\0';
	int n;
	while((i<size-1) && (c!= '\n'))
	{
		n = recv(cfd,&c,1,0);
		if(n>0)
		{
			if(c == '\r')
			{
				n = recv(cfd,&c,1,MSG_PEEK);
				if((n>0) && (c == '\n'))
				{
					recv(cfd,&c,1,0);
				}
				else c = '\n';
			}
			buf[i] = c;
			++i;
		}
		else
		{
			c = '\n';
		}
	}
	buf[i] = '\0';
	if(-1 == n) i = n;
	return i;
}
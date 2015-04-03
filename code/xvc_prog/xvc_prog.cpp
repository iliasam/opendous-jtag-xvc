//Code in this file is particly taken from XVCD project
#include "stdafx.h"
#include "usb_functions.h"
#include "opendous.h"

static int jtag_state;
static int verbose;

enum
{
	test_logic_reset, run_test_idle,

	select_dr_scan, capture_dr, shift_dr,
	exit1_dr, pause_dr, exit2_dr, update_dr,

	select_ir_scan, capture_ir, shift_ir,
	exit1_ir, pause_ir, exit2_ir, update_ir,

	num_states
};

static int jtag_step(int state, int tms)
{
	static const int next_state[num_states][2] ={
		{run_test_idle, test_logic_reset},
		{run_test_idle, select_dr_scan},

		{capture_dr, select_ir_scan},
		{shift_dr, exit1_dr},
		{shift_dr, exit1_dr},
		{pause_dr, update_dr},
		{pause_dr, exit2_dr},
		{shift_dr, update_dr},
		{run_test_idle, select_dr_scan},

		{capture_ir, test_logic_reset},
		{shift_ir, exit1_ir},
		{shift_ir, exit1_ir},
		{pause_ir, update_ir},
		{pause_ir, exit2_ir},
		{shift_ir, update_ir},
		{run_test_idle, select_dr_scan}
	};

	return next_state[state][tms];
}


static int sread(int fd, void *target, int len)
{
	char *t = (char*)target;
	while (len)
	{
		//int r = _read(fd, t, len);
		int r = recv(fd, t, len,0);
		if (r <= 0)
			return r;
		t += r;
		len -= r;
	}
	return 1;
}

// handle_data(fd) handles JTAG shift instructions.
//   To allow multiple programs to access the JTAG chain
//   at the same time, we only allow switching between
//   different clients only when we're in run_test_idle
//   after going test_logic_reset. This ensures that one
//   client can't disrupt the other client's IR or state.
int handle_data(int fd)
{
	int i;
	int seen_tlr = 0;

	do
	{
		char cmd[16];
		unsigned char buffer[2*2048];
		unsigned char result[2*1024];
		
		if (sread(fd, cmd, 6) != 1)
			return 1;
		
		if (memcmp(cmd, "shift:", 6))
		{
			cmd[6] = 0;
			fprintf(stderr, "invalid cmd '%s'\n", cmd);
			return 2;
		}
		
		int len;
		if (sread(fd, &len, 4) != 1)
		{
			fprintf(stderr, "reading length failed\n");
			return 3;
		}
		
		int nr_bytes = (len + 7) / 8;
		if (nr_bytes * 2 > sizeof(buffer))
		{
			fprintf(stderr, "buffer size exceeded\n");
			return 4;
		}
		
		if (sread(fd, buffer, nr_bytes * 2) != 1)
		{
			fprintf(stderr, "reading data failed\n");
			return 5;
		}
		
		memset(result, 0, nr_bytes);

		if (verbose)
		{
			printf("#");
			printf("tap length: %d ",len);
			for (i = 0; i < nr_bytes * 2; ++i)
				printf("%02x ", buffer[i]);
			printf("\n");
		}

		//
		// Only allow exiting if the state is rti and the IR
		// has the default value (IDCODE) by going through test_logic_reset.
		// As soon as going through capture_dr or capture_ir no exit is
		// allowed as this will change DR/IR.
		//
		seen_tlr = (seen_tlr || jtag_state == test_logic_reset) && (jtag_state != capture_dr) && (jtag_state != capture_ir);
		// Due to a weird bug(??) xilinx impacts goes through another "capture_ir"/"capture_dr" cycle after
		// reading IR/DR which unfortunately sets IR to the read-out IR value.
		// Just ignore these transactions.
		
		if ((jtag_state == exit1_ir && len == 5 && buffer[0] == 0x17) || (jtag_state == exit1_dr && len == 4 && buffer[0] == 0x0b))
		{
			if (verbose)
				printf("ignoring bogus jtag state movement in jtag_state %d\n", jtag_state);
		} else
		{
			for (i = 0; i < len; ++i)
			{
				// Do the actual cycle.
				
				int tms = !!(buffer[i/8] & (1<<(i&7)));
				// Track the state.
				jtag_state = jtag_step(jtag_state, tms);
			}

			if (io_scan(buffer, buffer + nr_bytes, result, len) < 0)
			{
				fprintf(stderr, "io_scan failed\n");
				printf("Wait for any key");
				getchar();

				exit(1);
			}
		}

		if (send(fd, (char*)result, nr_bytes,0) != nr_bytes)
		{
			perror("write");
			return 1;
		}
		
		if (verbose)
		{
			printf("jtag state %d\n", jtag_state);
		}
	} while (!(seen_tlr && jtag_state == run_test_idle));
	return 0;
}//end of handle data



int _tmain(int argc, _TCHAR* argv[])
{
	int iOptVal;
	int s;
	int c;
	int port = 2542;
	struct sockaddr_in address;
	WSADATA wsda;

	verbose = 0;	
	
	if (opendous_init())
	{
		fprintf(stderr, "io_init failed\n");
		getchar();
		return 1;
	}

	WSAStartup(MAKEWORD(1,1), &wsda);
	
	// Listen on port 2542.
	s = socket(AF_INET, SOCK_STREAM, 0);
	
	if (s < 0)
	{
		perror("socket");
		getchar();
		return 1;
	}
	
	
	iOptVal = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&iOptVal, sizeof iOptVal);
	
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	address.sin_family = AF_INET;
	
	if (bind(s, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind");
		getchar();
		return 1;
	}
	
	if (listen(s, 0) < 0)
	{
		perror("listen");
		getchar();
		return 1;
	}
	
	
	fd_set conn;
	int maxfd = 0;
	
	FD_ZERO(&conn);
	FD_SET(s, &conn);
	
	maxfd = s;

	
	printf("waiting for connection on port %d...\n", port);
	
	while (1)
	{
		fd_set read = conn, except = conn;
		int fd;
		
		
		//
		// Look for work to do.
		//
		
		if (select(maxfd + 1, &read, 0, &except, 0) < 0)
		{
			perror("select");
			break;
		}
		
		for (fd = 0; fd <= maxfd; ++fd)
		{
			if (FD_ISSET(fd, &read))
			{
				//
				// Readable listen socket? Accept connection.
				//
				
				if (fd == s)
				{
					int newfd;
					int nsize = sizeof(struct sockaddr_in);
					
					newfd = accept(s, (struct sockaddr*)&address, &nsize);

					printf("connection accepted - fd %d\n", newfd);
					if (newfd < 0)
					{
						perror("accept");
					} else
					{
						if (newfd > maxfd)
						{
							maxfd = newfd;
						}
						FD_SET(newfd, &conn);
					}
				}
				//
				// Otherwise, do work.
				//
				else if (handle_data(fd))
				{
					//
					// Close connection when required.
					//
					printf("connection closed - fd %d\n", fd);
					closesocket(fd);
					FD_CLR(fd, &conn);
				}
			}
			//
			// Abort connection?
			//
			else if (FD_ISSET(fd, &except))
			{
				if (verbose)
					printf("connection aborted - fd %d\n", fd);
				closesocket(fd);
				FD_CLR(fd, &conn);
				if (fd == s)
					break;
			}
		}
		
	}
	
	
	opendous_quit();
	WSACleanup();
	getchar();
	return 0;
}




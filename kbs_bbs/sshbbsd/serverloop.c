/*

serverloop.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Sun Sep 10 00:30:37 1995 ylo

Server main loop for handling the interactive session.

*/

/*
 * $Id: serverloop.c,v 1.11 2006/04/29 19:52:04 atppp Exp $
 * $Log: serverloop.c,v $
 * Revision 1.11  2006/04/29 19:52:04  atppp
 * em?
 *
 * Revision 1.10  2006/04/29 19:50:12  atppp
 * this seems to be a warning
 *
 * Revision 1.9  2006/04/11 09:16:10  etnlegend
 * nowarning ... libBBS src sshbbsd
 *
 * Revision 1.8  2004/05/19 02:53:56  yuhuan
 * ssh_select
 *
 * Revision 1.7  2003/12/26 13:20:10  stiger
 * ����config.h�еĲ���redefined��û������warning��
 *
 * Revision 1.6  2003/04/18 13:59:15  bad
 * ssh����
 *
 * Revision 1.5  2002/08/04 11:39:43  kcn
 * format c
 *
 * Revision 1.4  2002/08/04 11:08:48  kcn
 * format C
 *
 * Revision 1.3  2002/05/09 12:50:46  kxn
 * fixed a buffer overflow in ssh_read, ft
 *
 * Revision 1.2  2002/05/09 09:53:25  kxn
 * max output packet is now limited to 512 bytes as many ssh implementions do
 *
 * Revision 1.1.1.1  2002/04/27 05:47:26  kxn
 * no message
 *
 * Revision 1.2  2002/04/25 05:37:26  kxn
 * bugs fixed: disconnect, chinese ime
 * features added : run as normal user
 *
 * Revision 1.1  2001/07/04 06:07:11  bbsdev
 * bbs sshd
 *
 * Revision 1.18  1999/02/21 19:52:36  ylo
 * 	Intermediate commit of ssh1.2.27 stuff.
 * 	Main change is sprintf -> snprintf; however, there are also
 * 	many other changes.
 *
 * Revision 1.17  1998/05/23 20:37:21  kivinen
 *      Changed () -> (void).
 *
 * Revision 1.16  1998/05/04  13:36:28  kivinen
 *      Fixed no_port_forwarding_flag so that it will also disable
 *      local port forwardings from the server side. Moved
 *      packet_get_all after reading the the remote_channel number
 *      from the packet.
 *
 * Revision 1.15  1998/03/27 17:01:02  kivinen
 *      Fixed idle_time code.
 *
 * Revision 1.14  1997/04/21 01:04:46  kivinen
 *      Changed server_loop to call pty_cleanup_proc instead of
 *      pty_release, so we can be sure it is never called twice.
 *
 * Revision 1.13  1997/04/17 04:19:28  kivinen
 *      Added ttyame argument to wait_until_can_do_something and
 *      server_loop functions.
 *      Release pty before closing it.
 *
 * Revision 1.12  1997/04/05 21:52:54  kivinen
 *      Fixed closing of pty, and changed it to use shutdown first and
 *      close the pty only after pty have been released.
 *
 * Revision 1.11  1997/03/26 07:16:11  kivinen
 *      Fixed idle_time code.
 *
 * Revision 1.10  1997/03/26 05:28:18  kivinen
 *      Added idle timeout support.
 *
 * Revision 1.9  1997/03/25 05:48:49  kivinen
 *      Moved closing of sockets/pipes out from server_loop.
 *
 * Revision 1.8  1997/03/19 19:25:17  kivinen
 *      Added input buffer clearing for error conditions, so packet.c
 *      can check that buffer must be empty before new packet is read
 *      in.
 *
 * Revision 1.7  1997/03/19 17:56:31  kivinen
 *      Fixed sigchld race condition.
 *
 * Revision 1.6  1996/11/24 08:25:14  kivinen
 *      Added SSHD_NO_PORT_FORWARDING support.
 *      Changed all code that checked EAGAIN to check EWOULDBLOCK too.
 *
 * Revision 1.5  1996/09/29 13:42:55  ylo
 *      Increased the time to wait for more data from 10 ms to 17 ms
 *      and bytes to 512 (I'm worried it might not always be
 *      working due to the delay being shorter than the systems
 *      fundamental clock tick).
 *
 * Revision 1.4  1996/09/14 08:42:26  ylo
 *      Added cvs logs.
 *
 * $EndLog$
 */

#include "includes.h"
#include "bbs.h"
#include "ssh_funcs.h"
#include "xmalloc.h"
#include "ssh.h"
#include "packet.h"
#include "buffer.h"

int ssh_write(int fd, const void *buf, size_t count)
{
    int len;
    const char *data = buf;
    int result = count;

    while (count > 0) {
        len = count > 512 ? 512 : count;
        packet_start(SSH_SMSG_STDOUT_DATA);
        packet_put_string(data, len);
        packet_send();
        packet_write_wait();
        count -= len;
        data += len;
    }
    return result;
}
static Buffer NetworkBuf;
void ProcessOnePacket(int wait);
ssize_t ssh_read(int fd, void *buf, size_t count)
{
    int retlen = 0;

    if (count < 0)
        return count;
    ProcessOnePacket(0);
    while (buffer_len(&NetworkBuf) <= 0) {
        ProcessOnePacket(1);
        ProcessOnePacket(0);
    }
    retlen = buffer_len(&NetworkBuf);
    retlen = retlen > count ? count : retlen;
    buffer_get(&NetworkBuf, buf, retlen);
    return retlen;
}
int ssh_select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	if (buffer_len(&NetworkBuf) > 0)
		return 1;
	return select(n, readfds, writefds, exceptfds, timeout);
}
int ssh_init(void)
{
    buffer_init(&NetworkBuf);
    return 0;
}
void ProcessOnePacket(int wait)
{
    int type;
    char *data;
    unsigned int data_len;
    int row, col, xpixel, ypixel;

    while (1) {
        if (wait)
            type = packet_read();
        else
            type = packet_read_poll();
        if (type == SSH_MSG_NONE)
            goto read_done;
        switch (type) {
        case SSH_CMSG_STDIN_DATA:
            /* Stdin data from the client.  Append it to the buffer. */
            data = packet_get_string(&data_len);
            buffer_append(&NetworkBuf, data, data_len);
            memset(data, 0, data_len);
            xfree(data);
            if (wait)
                goto read_done;
            break;

        case SSH_CMSG_EOF:
            /* Eof from the client.  The stdin descriptor to the program
               will be closed when all buffered data has drained. */
            debug("EOF received for stdin.");
            goto read_done;
            break;

        case SSH_CMSG_WINDOW_SIZE:
            debug("Window change received.");
            row = packet_get_int();
            col = packet_get_int();
            xpixel = packet_get_int();
            ypixel = packet_get_int();
            do_naws(row, col);
//            pty_change_window_size(fdin, row, col, xpixel, ypixel);
            break;

        case SSH_MSG_PORT_OPEN:
            break;

        case SSH_MSG_CHANNEL_OPEN_CONFIRMATION:
            debug("Received channel open confirmation.");
            break;

        case SSH_MSG_CHANNEL_OPEN_FAILURE:
            debug("Received channel open failure.");
            break;

        case SSH_MSG_CHANNEL_DATA:
            break;

#ifdef SUPPORT_OLD_CHANNELS
        case SSH_MSG_CHANNEL_CLOSE:
            debug("Received channel close.");
            break;

        case SSH_MSG_CHANNEL_CLOSE_CONFIRMATION:
            debug("Received channel close confirmation.");
            break;
#else
        case SSH_MSG_CHANNEL_INPUT_EOF:
            debug("Received channel input eof.");
            break;

        case SSH_MSG_CHANNEL_OUTPUT_CLOSED:
            debug("Received channel output closed.");
            break;

#endif

        default:
            /* In this phase, any unexpected messages cause a protocol
               error.  This is to ease debugging; also, since no 
               confirmations are sent messages, unprocessed unknown 
               messages could cause strange problems.  Any compatible 
               protocol extensions must be negotiated before entering the 
               interactive session. */
            packet_disconnect("Protocol error during session: type %d", type);
        }
    }
read_done:
	return;

}

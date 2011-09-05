#ifndef _IFTAP_H_
#define _IFTAP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>



int tap_alloc (char * dev);
int tap_up (char * dev);


#endif /* _IFTAP_H_ */

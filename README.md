vxlan
=====

ID : https://tools.ietf.org/html/draft-mahalingam-dutt-dcops-vxlan-00

vxlan includes "vxland" and "vxlanctl" commands.
vxland, is vxlan daemon, forwards packet to VXLAN 
Overlay Network. vxlanctl is command for controlling vxlan. 
You can create/destroy vxlan tunnel interface using 
vxlanctl. 

Install
-------
VXLAN requires uthash package late 1.9.
Please see http://uthash.sourceforge.net/ .

	% git clone git://github.com/upa/vxlan.git
	% cd vxlan
	% make
	% make install


How to Use
----------

### vxlan.conf ###

Multicast Address and Interface are configured in 
/usr/local/etc/vxlan.conf . vxland can work with 
IPv4 and IPv6.

	# vxlan.conf
	multicast_address 239.0.0.1
	multicast_interface eth0

### vxland ###

vxland is daemon that creates vxlan instances, and inforward packets.

+ start : _/etc/init.d/vxlan start_
+ stop : _/etc/init.d/vxlan stop_

Other configurations are installed by vxlanctl.


### vxlanctl ###

vxlanctl is command tool for configuring vxlan. you can create/destroy
vxlan tunnel interface.  if you want to use vlan, create vlan
interface using vconfig.

	### Usage ####
	 % vxlanctl --help
	    Usage :
	 	  vxlanctl [commands]
	   
	    commands:    (VNI is hex)
	  
	     create <VNI>                             add vxlan interface
	     destroy <VNI>                            delete vxlan interface
	  	 
	### Create vxlan interface  ####
	 % vxlanctl create 0
	 created
	 %
	 % ifconfig vxlan0
	 vxlan0    Link encap:Ethernet  HWaddr 06:b3:1a:45:86:9a  
	           inet6 addr: fe80::4b3:1aff:fe45:869a/64 Scope:Link
	           UP BROADCAST RUNNING  MTU:1500  Metric:1
	           RX packets:0 errors:0 dropped:0 overruns:0 frame:0
	           TX packets:4 errors:0 dropped:0 overruns:0 carrier:0
	           collisions:0 txqueuelen:500 
	           RX bytes:0 (0.0 B)  TX bytes:328 (328.0 B)
	 
	 
	### using with 802.1q vlan ####
	 % vconfig add vxlan0 100
	 Added VLAN with VID == 100 to IF -:vxlan0:-
	 % ifconfig vxlan0.100 up
	 % ifconfig vxlan0.100
	 vxlan0.100    Link encap:Ethernet  HWaddr 06:b3:1a:45:86:9a  
	               inet6 addr: fe80::4b3:1aff:fe45:869a/64 Scope:Link
	               UP BROADCAST RUNNING  MTU:1500  Metric:1
	               RX packets:0 errors:0 dropped:0 overruns:0 frame:0
	               TX packets:6 errors:0 dropped:0 overruns:0 carrier:0
	               collisions:0 txqueuelen:500 
	               RX bytes:0 (0.0 B)  TX bytes:468 (468.0 B)


ToDo
----
+ Per VLAN Forwarding Database (it is Per VNI FDB now)
+ show command of vxlanctl
+ Support *BSD 


License
-------
Under BSD LICENSE.


Contact
-------
upa@haeena.net



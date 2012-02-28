hogelan
=======

hogelan is simple vxlan implementation using Linux tap interface.

ID : https://tools.ietf.org/html/draft-mahalingam-dutt-dcops-vxlan-00

	Usage

	   vxlan -m [MCASTADDR] -i [INTERFACE] [-d] VNI1(HEX) VNI2 VNI3 ...
	
	         -m : Multicast Address(v4/v6)
	         -i : Multicast Interface
		 -c : Accesslist Configuration File
	         -e : Print Error Massage to STDOUT
	         -d : Daemon Mode


hogelan creates tap interface each VNI.

name of tap interface is vxlan[VNI].

#Overview#
netopeer-server-sl is a system resources collecting program which is based on libnetconf, netopeer and supports netconf function.  The ability of netopeer-server-sl is to collect the resource utilization rate of CPU、Memory、Disk、NIC、vSwitch.

#Function#
* Provide network connection for the Monitor-restconf by SSH
* Define YANG model of information collection to realize data collecting 

#Environment#
libnetconf

netopeer

#Installation#
	$ make
	$ sudo make install

#Run#
Add following sentences at the last two lines of /etc/sshd/sshd_config:
	Port 830
	Subsystem netconf /usr/local/bin/netopeer-server-sl

finally restart sshd

	systemctl restart sshd

#Corporation#

* Guangzhou Research Institute of China Telecom 

#Autor#

##Design##
* Hong Tang(chinatelecom.sdn.group@gmail.com)

##Coder##
* Peng li (chinatelecom.sdn.group@gmail.com)
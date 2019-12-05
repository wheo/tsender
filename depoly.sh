#/bin/bash
make && make install
cd /opt/tnmtech
scp setting.json tmuxer-cmd tsender root@192.168.2.11:/ICMS/SSD/FFX-II/tnmtech
#scp setting.json tmuxer-cmd tsender root@192.168.2.31:/ICMS/SSD/FFX-II/tnmtech
#scp /usr/lib64/libbz2.so* root@192.168.2.11:/usr/lib64
#scp /usr/lib64/libbz2.so* root@192.168.2.31:/usr/lib64
scp /usr/sbin/iftop root@192.168.2.11:/usr/sbin
#scp /usr/sbin/iftop root@192.168.2.31:/usr/sbin
cd /root/tsender

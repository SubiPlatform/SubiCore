#!/bin/bash

HEIGHT=15
WIDTH=40
CHOICE_HEIGHT=6
BACKTITLE="SUBI Masternode Setup Wizard"
TITLE="SUBI VPS Setup"
MENU="Choose one of the following options:"

OPTIONS=(1 "Install New VPS Server"
         2 "Update to new version VPS Server"
         3 "Start SUBI Masternode"
	 4 "Stop SUBI Masternode"
	 5 "SUBI Server Status"
	 6 "Rebuild SUBI Masternode Index")


CHOICE=$(whiptail --clear\
		--backtitle "$BACKTITLE" \
                --title "$TITLE" \
                --menu "$MENU" \
                $HEIGHT $WIDTH $CHOICE_HEIGHT \
                "${OPTIONS[@]}" \
                2>&1 >/dev/tty)

clear
case $CHOICE in
        1)
            echo Starting the install process.
echo Checking and installing VPS server prerequisites. Please wait.
echo -e "Checking if swap space is needed."
PHYMEM=$(free -g|awk '/^Mem:/{print $2}')
SWAP=$(swapon -s)
if [[ "$PHYMEM" -lt "2" && -z "$SWAP" ]];
  then
    echo -e "${GREEN}Server is running with less than 2G of RAM, creating 2G swap file.${NC}"
    dd if=/dev/zero of=/swapfile bs=1024 count=2M
    chmod 600 /swapfile
    mkswap /swapfile
    swapon -a /swapfile
else
  echo -e "${GREEN}The server running with at least 2G of RAM, or SWAP exists.${NC}"
fi
if [[ $(lsb_release -d) != *16.04* ]]; then
  echo -e "${RED}You are not running Ubuntu 16.04. Installation is cancelled.${NC}"
  exit 1
fi

if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}$0 must be run as root.${NC}"
   exit 1
fi
clear
sudo apt update
sudo apt-get -y upgrade
sudo apt-get install git -y
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils -y
sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev -y
sudo apt-get install libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev -y
sudo apt-get install libboost-all-dev -y
sudo apt-get install software-properties-common -y
sudo add-apt-repository ppa:bitcoin/bitcoin -y
sudo apt-get update
sudo apt-get install libdb4.8-dev libdb4.8++-dev -y
sudo apt-get install libminiupnpc-dev -y
sudo apt-get install libzmq3-dev -y
sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler -y
sudo apt-get install libqt4-dev libprotobuf-dev protobuf-compiler -y
clear
echo VPS Server prerequisites installed.


echo Configuring server firewall.
sudo apt-get install -y ufw
sudo ufw allow 5335 
sudo ufw allow ssh/tcp
sudo ufw limit ssh/tcp
sudo ufw logging on
echo "y" | sudo ufw enable
sudo ufw status
echo Server firewall configuration completed.

echo Downloading SUBI install files.
wget https://github.com/MotoAcidic/SubiCore/releases/download/1.0.0.2/SUBI-linux.tar.gz
echo Download complete.

echo Installing SUBI.
tar -xvf SUBI-linux.tar.gz
chmod 775 ./subid
chmod 775 ./subi-cli
echo SUBI install complete. 
sudo rm -rf SUBI-linux.tar.gz
clear

echo Now ready to setup SUBI configuration file.

RPCUSER=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
RPCPASSWORD=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
EXTIP=`curl -s4 icanhazip.com`
echo Please input your private key.
read GENKEY

mkdir -p /root/.subi && touch /root/.subi/subi.conf

cat << EOF > /root/.subi/subi.conf
rpcuser=$RPCUSER
rpcpassword=$RPCPASSWORD
rpcallowip=127.0.0.1
server=1
listen=1
daemon=1
staking=1
rpcallowip=127.0.0.1
rpcport=5336
port=5335
prune=500
addnode=104.207.150.126:5335
addnode=106.38.72.242:18881
addnode=173.212.230.240:35732
addnode=175.198.222.101:33010
addnode=178.32.3.8:50333
addnode=183.80.142.161:50738
addnode=185.231.69.199:57117
addnode=198.12.95.45:44566
addnode=2.225.249.244:43206
addnode=2.62.240.135:64200
addnode=206.189.225.34:48768
addnode=207.148.111.79:55320
addnode=207.180.235.82:5335
addnode=213.202.219.104:63737
addnode=217.61.57.85:50384
addnode=222.152.177.81:50351
addnode=24.77.76.107:65033
addnode=37.46.130.49:53805
addnode=45.77.227.219:5335
addnode=50.116.23.240:55380
addnode=51.75.248.227:53868
addnode=59.124.138.151:10264
addnode=60.227.80.119:55616
addnode=66.42.70.57:5335
addnode=66.42.82.183:36318
addnode=71.178.50.80:61071
addnode=71.222.11.235:49517
addnode=74.63.213.101:60742
addnode=78.36.193.51:50803
addnode=78.36.193.51:65011
addnode=78.36.201.200:45932
addnode=80.211.66.183:53812
addnode=83.69.1.106:53051
addnode=86.145.224.53:56802
addnode=89.163.129.202:59575
addnode=91.210.224.211:59376
addnode=95.216.101.102:42356
addnode=95.216.77.7:47122
addnode=98.183.71.6:64841
addnode=[2001:0:5ef5:79fb:107c:44a:cdca:4b7c]:5335
addnode=[2001:0:9d38:6ab8:1081:ec2:b1db:3ecc]:50915
addnode=[2001:0:9d38:6ab8:1480:63e:95d9:b70d]:64098
addnode=[2001:0:9d38:6abd:24bb:a115:5039:219a]:5335
addnode=[2001:0:9d38:90d7:2ce9:1d59:30ae:920]:5335
addnode=[2001:19f0:4400:6be5:5400:1ff:fed0:4968]:36814
addnode=[2001:19f0:4400:7e5e:5400:1ff:febb:a679]:54052
addnode=[2001:19f0:4401:2f1:5400:1ff:fed3:1ccf]:44726
addnode=[2001:19f0:7001:1a49:5400:1ff:feb8:644]:51460
addnode=[2001:8003:20ce:f401:ed62:9dcc:343a:6a9f]:5335
addnode=[2002:252e:8231::252e:8231]:50843
addnode=[2002:a010:bedc::a010:bedc]:55268
addnode=[2002:b92b:df4c::b92b:df4c]:54562
addnode=[2002:b9e7:45c7::b9e7:45c7]:51356
logtimestamps=1
maxconnections=256
masternode=1
externalip=$EXTIP
masternodeprivkey=$GENKEY
EOF
clear
./subid -daemon
./subi-cli stop
sleep=5
./subid -daemon
clear
echo SUBI configuration file created successfully. 
echo SUBI Server Started Successfully using the command ./subid -daemon
echo If you get a message asking to rebuild the database, please hit Ctr + C and run ./subid -daemon -reindex
echo If you still have further issues please reach out to support in our Discord channel. 
echo Please use the following Private Key when setting up your wallet: $GENKEY
            ;;
	    
    
        2)
sudo ./subi-cli -daemon stop
echo "! Stopping SUBI Daemon !"

echo Configuring server firewall.
sudo apt-get install -y ufw
sudo ufw allow 5335
sudo ufw allow ssh/tcp
sudo ufw limit ssh/tcp
sudo ufw logging on
echo "y" | sudo ufw enable
sudo ufw status
echo Server firewall configuration completed.

echo "! Removing SUBI !"
sudo rm -rf subi-1.0.0.2-b-x86_64-linux-gnu.tar.gz


wget https://github.com/MotoAcidic/SubiCore/releases/download/1.0.0.2/SUBI-linux.tar.gz
echo Download complete.
echo Installing SUBI.
tar -xvf SUBI-linux.tar.gz
chmod 775 ./subid
chmod 775 ./subi-cli
sudo rm -rf SUBI-linux.tar.gz
./subid -daemon
echo SUBI install complete. 


            ;;
        3)
            ./subid -daemon
		echo "If you get a message asking to rebuild the database, please hit Ctr + C and rebuild SUBI Index. (Option 6)"
            ;;
	4)
            ./subi-cli stop
            ;;
	5)
	    ./subi-cli getinfo
	    ;;
        6)
	     ./subid -daemon -reindex
            ;;
esac

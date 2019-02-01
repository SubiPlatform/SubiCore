![logo](https://github.com/SubiPlatform/SubiCore/blob/master/src/qt/res/icons/subi.png)

# SUBI v1.0.0.9 Masternode Setup Guide [ Ubuntu 16.04 ]

THIS GUIDE IS FOR ROOT USERS -

YOU MUST BE A MEMBER OF THE FOLLOWING GROUP
```
User=root
Group=root
```

Shell script to install a SUBI Masternode on a Linux server running Ubuntu 16.04. Use it on your own risk.
***

## Private Key


Steps generate your own private key. 
1.  Download and install SUBI v1.0.0.9 for Windows -   Download Link  - https://github.com/SubiPlatform/SubiCore/releases
2.  Go to **Settings"** 
3.  Type the following command: **subinode genkey**  
4. You now have your generated **Private Key**  (MasternodePrivKey)


## VPS installation
First you will need a VPS to continue on with this guide. If you do not have one get one from here [Vultr.](https://www.vultr.com/?ref=7705609)

Next step is to download the script on the vps with command below.
```
cd &&  bash -c "$(wget -O - https://raw.githubusercontent.com/SubiPlatform/SubiCore/master/scripts/Subi_MN_Install.sh)"
```
You will have 6 options once you run the command above.
1. This option Will install a fresh MNN VPS instance
2. This option will update your MN wallet on the vps if a network or wallet update is needed.
3. This option will Start SUBI Masternode
4. This option will Stop SUBI Masternode
5. This option will show SUBI Server Status
6. This option will Rebuild SUBI Masternode Index


If you need to go back and either start or stop Subi just use this command.
```
cd &&  bash -c "$(wget -O - https://raw.githubusercontent.com/SubiPlatform/SubiCore/master/scripts/Subi_MN_Install.sh)"
```
That command above will be your shortcut to control your masternode. 
More commands will come in time.

Once the VPS installation is finished.

Check the block height

```
watch ./subi-cli getinfo
```

We want the blocks to match whats on the SUBI block explorer

Once they match you can proceed with the rest of the guide.



Once the block height matches the block explorer issue the following command.
```
CTRL and C  at the same time  (CTRL KEY and C KEY)
```
***

## Desktop wallet setup  

After the MN is up and running, you need to configure the desktop wallet accordingly. Here are the steps:  
1. Open the SUBI Desktop Wallet.  
2. Go to HELP -> DEBUG then click on CONSOLE tab and type **getnewaddress MN1 legacy**  
3. Send **10000** SUBI to **MN1**. You need to send 10000 coins in one single transaction.
4. Wait for 10 confirmations.  
5. Go to **Settings"** 
6. Type the following command: **subinode outputs**  
7. Go to  **Subinode -> Subinode -> Create Subinode button**
8. Add the following entry:
```
IP Address Password Privkey Alias TxHash TxIndex
```
## SAMPLE OF HOW YOUR SUBINODE.CONF SHOULD LOOK LIKE.  (This should all be on one line)  

```
MN1 127.0.0.2:5335 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c0
```

* Password: **Wallet password"
* Alias: **MN1**
* Address: **VPS_IP:PORT**
* Privkey: **Masternode Private Key**
* TxHash: **First value from Step 6**
* TxIndex:  **Second value from Step 6**
9. Save and close the file.
10. Go to **Masternode Tab**. 
If you tab is not shown, please enable it from: **Settings - Options - Wallet - Show Masternodes Tab**
11. Click **Update status** to see your node. If it is not shown, close the wallet and start it again. 
12. On the Subinode page click **Start Subinode** to start it.
13. Login to your VPS and check your masternode status by running the following command:.

```
./subi-cli subinode status
```

You want to see **"Masternode started successfully and Status 4"**

***

## Usage:

```
./subi-cli getinfo
./subi-cli subinodesync status
./subi-cli subinode status
```
  
Thank you too Franco#6184 for catching some errors! :)

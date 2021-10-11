# ledger-app-aergo

Aergo application for Ledger Nano S (beta version)

![ledger-app-aergo](https://user-images.githubusercontent.com/7624275/75844801-f048fb80-5db5-11ea-9957-3016f3970ccc.jpg)

The most secure method to protect your private key(s)

This Ledger application can be used to sign Aergo transactions, like:

* Transfer
* Stake & Unstake
* Votes (BP and DAO)
* Name System (create and update)
* Smart Contract deploy and redeploy
* Smart Contract function call


## Manual Installation

### 1. Build the application

https://developers.ledger.com/docs/nano-app/build/

### 2. Load the app to a Nano S

https://developers.ledger.com/docs/nano-app/load/


## Old Instructions (firmware 1.6)

To build and install the app on your Ledger Nano S you must set up the Ledger Nano S build environments.

Only Linux is supported to build the Ledger app so if you do not have one you can use a Virtual Machine (eg: VirtualBox, VMWare).

Open a terminal on Linux and execute the following commands.

First install the required tools by executing this line (copy and paste):

```
sudo apt-get install git gcc-multilib g++-multilib python3-venv python3-dev libudev-dev libusb-1.0-0-dev
```

It will ask for your password on Linux.

Set up the udev rules for the Ledger devices by executing this line:

```
wget -q -O - https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh | sudo bash
```

Then clone this repository and install the requirements in a virtual environnment:

```
git clone https://github.com/aergoio/ledger-app-aergo
cd ledger-app-aergo
source prepare-devenv.sh
```

You can optionally follow the [Getting Started](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html) instructions from Ledger.


## Installation

Connect the device to your computer and type this command on the same terminal open before:

```
make load
```


## Uninstall

Connect the device to your computer and type this command on the same terminal open before:

```
make delete
```

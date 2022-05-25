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

Follow these instructions to compile and install the Aergo app on your Ledger Nano S, while the app is not available on the Ledger Live

This was only tested on Linux, but it may work on Mac too. You will need docker and git.

Open a terminal and then run these commands, one line at a time:

```
sudo docker build -t ledger-app-builder:latest .
git clone https://github.com/aergoio/ledger-app-aergo
cd ledger-app-aergo
sudo docker run --rm -ti -v "/dev/bus/usb:/dev/bus/usb" -v "$(realpath .):/app" --privileged ledger-app-builder:latest
```

A prompt like this may appear:

```
root@656be163fe84:/app#
```

Then run this command on the above prompt:

```
make
```

Now plug the Nano S on the computer and unlock it with your password. Then run:

```
make load
```

When done, exit the docker terminal by typing:

```
exit
```


### Details

More information is available on the Ledger portal:

Building the application:

https://developers.ledger.com/docs/nano-app/build/

Loading the app to a Nano S:

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


### Installation

Connect the device to your computer and type this command on the same terminal open before:

```
make load
```


### Uninstall

Connect the device to your computer and type this command on the same terminal open before:

```
make delete
```

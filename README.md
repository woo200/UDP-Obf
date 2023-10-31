# UDP Obfuscation Tunnel

This is a project with the goal of bypassing simple firewalls that block UDP based protocols such as OpenVPN or Wireguard. It implements extremely simple obfuscation (Note: NOT encryption, that is up to the protocol you use), literally just adding 1 to every byte, and subtracting it at the other end. It will bypass most firewalls, however you can impliment some other method in case your firewall somehow knows how to deobfuscate it.

## Installation
To compile, run
```
git clone https://github.com/woo200/UDP-Obf.git
cd UDP-Obf/
make
```
You can then use the binary created at `./udp_obf`

## Usage
Run the program from the command line, specifying your mode and network settings:
`./udp_obf [-csh] [bind_addr] [bind_port] [remote_addr] [remote_port]`
#### Options:
 - `-c` Run as client mode
 - `-s` Run as server mode
 - `-h` Show the help menu

#### Server Mode:
 - `bind_addr`: Address to bind to.
 - `bind_port`: Port to bind to.
 - `remote_addr`: Address to connect to when a client sends a datagram.
 - `remote_port`: Port to connect to when a client sends a datagram.

#### Client Mode
 - `bind_addr`: Address to bind to.
 - `bind_port`: Port to bind to.
 - `remote_addr`: Address of the UDP obfuscation utility running in server mode.
 - `remote_port`: Port of the UDP obfuscation utility running as server mode.

## License
This project is licensed under the MIT License. For more details, see the LICENSE file in the root directory of this distribution.
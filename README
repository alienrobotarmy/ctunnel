
Copyright (c) 2009-2018 Jess Mahan ctunnel@alienrobotarmy.com

----------------
ctunnel    0.7
----------------

  What it is:
    ctunnel is a software for proxying and forwarding TCP or UDP
    connections via a cryptographic or plain tunnel.
    
    ctunnel can also operate as a VPN creating a private network 
    between ctunnel hosts.

    ctunnel can be used to simply proxy TCP or UDP traffic, proxy
    and compress, or to secure any existing TCP or UDP based 
    protocol( such as HTTP, Telnet, FTP, RSH, MySQL, etc).
    You can even tunnel SSH! (if you are really paranoid!).

    You can also chain/bounce connections to any number of intermediary
    hosts (including VPN mode).

----------------
Where to get it
----------------

  Official releases and snapshots may be obtained from the following location:
  http://alienrobotarmy.com/ctunnel

  You may also fork us on github!

----------------
How it works
----------------

  In tunnel mode (default)
  ctunnel works by listening on the client machine, encrypting the
  TCP or UDP traffic, and then forwarding the encrypted traffic to the server,
  where another instance of ctunnel will decrypt that traffic in turn
  and forward the decrypted traffic to the destination port.

  In VPN mode
  ctunnel has a point-to-point VPN mode (multiple clients may work but
  your milage may vary). A tun device on both the client and server are
  required (using ppp instead of tuntap is experimental and routes must
  be added to /etc/ppp/ip-up). Once the ctunnel is connected and the 
  VPN is established, it is up to you to add any IPTABLES/Forwarding rules
  on the client or server.
  (Examples for post-up forwarding are including in libexec/up.sh)


----------------
Examples
----------------

  How about an example?

  ** Note, the examples below are for OpenSSL            **
  ** Substitute '-C aes-256-cfb' with '-C aes256 -M cfb' **
  ** when compiled with libgcrypt                        **

  For instance, your local machine has an IP of 10.0.0.2. Now let's say you've 
  got a VNC server running on 10.0.0.4, listening on 5901 (the default port for
  vnc) and you want to secure it.

  On the client machine (10.0.0.2) we'll run ctunnel.

    ./ctunnel -c -l 127.0.0.1:2221 -f 10.0.0.4:2222 -C aes-256-cfb

  On the server machine (10.0.0.4 running the vnc server) we'll also run ctunnel.

    ./ctunnel -s -l 10.0.0.4:2222 -f 127.0.0.1:5901 -C aes-256-cfb

  On the client machine (10.0.0.2) we run vncviewr throught the tunnel.

    ./vncviewer 127.0.0.1::2221
    

  Ta DA! You've got an encrypted tunnel right to your VNC Server.

  An even more secure example would be to make sure that VNC Server on
  10.0.0.4 was only listening on it's local loopback interface of 127.0.0.1,
  this way the only way to access it would be via ctunnel.

  MySQL:

  Client/10.0.0.2

    ./ctunnel -c -l 127.0.0.1:3306 -f 10.0.0.4:2222 -C aes-256-cfb

  Server/10.0.0.4

    ./ctunnel -s -l 10.0.0.4:2222 -f 127.0.0.1:3306 -C aes-256-cfb

  Client

    mysql -u root -p -h 127.0.0.1 


  You can also bounce connections off an intermediary proxy, like this:

  Client/10.0.0.2

    ./ctunnel -c -l 127.0.0.1:2221 -f 10.0.0.3:2222 -C aes-256-cfb

  Proxy/10.0.0.3

    ./ctunnel -s -l 10.0.0.3:2222 -f 127.0.0.1:2223 -C aes-256-cfb &
    ./ctunnel -c -l 127.0.0.1:2223 -f 10.0.0.4:2224 -C aes-256-cfb

  Server/10.0.0.4

    ./ctunnel -s -l 10.0.0.4:2224 -f localhost:3306 -C aes-256-cfb


  DNS:

  Server/10.0.0.3

    ./ctunnel -U -n -s -l 0.0.0.0:5001 -f localhost:53 -C aes-256-cfb

  Client/10.0.0.2
    ./ctunnel -U -n -c -l 0.0.0.0:53 -f 10.0.0.3:5001 -C aes-256-cfb
    dig @localhost alienrobotarmy.com 


  VPN:
  
  (TUN/TAP - default)
  Server/192.168.1.2
    ./ctunnel -V -U -n -s -l 192.168.1.2:1024 -C aes-128-cfb -r 192.168.1.0/24

  Client/10.0.0.50
    ./ctunnel -V -U -n -c -f 192.168.1.2:1024 -C aes-128-cfb -r 10.0.0.0/24

  (PPP mode)
  Server/192.168.1.2
    ./ctunnel -V -U -n -s -t 1 -l 192.168.1.2:1024 -C rc4 \
    -P '/usr/sbin/pppd nodetach noauth unit 1'

  Client/10.0.0.50
    ./ctunnel -V -U -n -c -f 192.168.1.2:1024 -C rc4 \ 
    -P '/usr/sbin/pppd nodetach noauth passive 10.0.5.2:10.0.5.1'


----------------
Ciphers
----------------

  ctunnel currently allows you to specifcy any OpenSSL/libgcrypt cipher via
  the -C switch (-C and -M for libgcrypt). ctunnel does not check wether 
  you are using a stream or block cipher, but you MUST use a stream cipher
  for it to work.

  ******** YOU MUST USE A STREAM CIPHER ********
  (or a block cipher in cfb,ofb,ctr mode - stream)

  In the example above we use aes-256-cfb, which is the Cipher Feeback mode
  for aes-256. 


----------------
Keys
----------------

  So, how do we securely make a tunnel with a stream cipher? I'll
  bet you're thinking Keys, and you're correct, partly! Thinking passwords?
  You're correct there also.

  Let's explain:
  CTunnel does not rely on traditional PEM format keys, or a CA authority.
  It uses pre shared keys (passwords). CTunnel will store your "Passkey" 
  in ~/.passkey. It stores a 16 character Key and IV in this
  file. SO PROTECT IT! 

  On your first run of CTunnel you will be prompted to enter your Key and
  IV, after which CTunnel won't prompt you again until you remove
  your passkey file located in ~/.passkey


----------------
Requirments
----------------

  OpenSSL    http://www.openssl.org
  or
  libgcrypt  http://www.gnupg.org

  Typically you can just apt-get install libssl-dev or grab the openssl
  or libgcrypt development libraries and headers for your distro.

  VPN Mode requires a TUNTAP Driver or pppd.
  TUNTAP is standard on Linux. For win32 and
  OSX, you will need a 3rd party tuntap driver such as the one budled
  with OpenVPN http://openvpn.net

  VPN Mode may be used with PPP in place of TUNTAP. In this case you need
  a working pppd binary compiled for your system


----------------
Building
----------------

  Yup, you guessed it...
  If you have met all the *requirements* then just do:
  make; make install


----------------
Known Issues
----------------

  aes-256 cfb in mixed openssl / gcrypt implementations does not work, use
  aes-128 instead.

  Using PPP mode, routes are not exchanged between endpoints. Routes should
  not be added to the post up exec scrip. Routes should be added to ppp's 
  internal hook-script /etc/ppp/ip-up (or script passed to ipparam option
  to pppd).

  using PPP mode is slower than tun/tap - this is to be expected.

  Win32: using an asterisk when trying to bind to an interface with -l may 
  result 
  in segfault or "bind(): Result to large". Specify IP instead


----------------
Roadmap
----------------

  Next release the -C encryption option will be replaced with per
  endpoint encryption options. For instnace:

  -l localhost:22:aes-128-cfb -f 10.0.0.1:22:rc4

  This will allow greater flexibility especially when ctunnel is the
  intermediary proxy and each remote endpoint have different encryption. 

  Perhaps adding the ability for individual keys per endpoint.

  Add options for per endpoint protocol:
  -l localhost:22:udp:aes-128-cfb -f 10.0.0.1:22:tcp:rc4


----------------
Getting Help
----------------

  If you need help, please make sure before asking a question
  that you do indeed have the ssl development libraries installed,
  and that you have read and understand the section "Examples" and
  the section "Ciphers".

  More often than not you are either getting your -c/-s switches 
  mixed up, or you are not using a stream cipher as specified in
  the "Ciphers" section.

  NOTE: If you do not specify the -U switch (to operate in UDP mode), 
  CTunnel will operate in TCP mode.

  If you are still having trouble, feel free to contact me. I can
  be reached via ctunnel-<unixtimestamp>@nardcore.org


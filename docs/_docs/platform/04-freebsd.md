---
title: FreeBSD
head_inline: "<style> .blue { color: blue; } </style>"
---

This guide is based on **FreeBSD Relase 11.1**.
{: .blue}

### Getting MongoDB
---

Install MongoDB with package manager.
```bash
$ sudo pkg install mongodb
```

Run MongoDB server.
```bash
$ mkdir -p ./data/db
$ mongod --dbpath ./data/db
```

### Setting up TUN device (No persistent after rebooting)
---

Configure the TUN device.
```bash
$ sudo ifconfig lo0 alias 127.0.0.2 netmask 255.255.255.255
$ sudo ifconfig lo0 alias 127.0.0.3 netmask 255.255.255.255
$ sudo ifconfig lo0 alias 127.0.0.4 netmask 255.255.255.255
$ sudo ifconfig lo0 alias 127.0.0.5 netmask 255.255.255.255
```

Enable IP forwarding
```bash
$ sudo sysctl -w net.inet.ip.forwarding=1
```

**Tip:** The script provided in [$GIT_REPO/support/network/restart.sh](https://github.com/{{ site.github_username }}/nextepc/blob/master/support/network/restart.sh) makes it easy to configure the TUN device as follows:  
`$ sudo ./support/network/restart.sh`
{: .notice--info}

### Building NextEPC
---

Install the depedencies for building the source code.
```bash
$ sudo pkg install git gcc bison gsed pkgconf autoconf automake libtool mongo-c-driver gnutls libgcrypt libidn libyaml
```

Git clone with `--recursive` option.

```bash
➜  open5gs git clone --recursive https://github.com/{{ site.github_username }}/nextepc
```

To compile with autotools:

```bash
➜  open5gs cd nextepc
➜  nextepc git:(master) ✗ autoreconf -iv
➜  nextepc git:(master) ✗ ./configure --prefix=`pwd`/install
➜  nextepc git:(master) ✗ make -j `nproc`
```

Check whether the compilation is correct.

**Note:** This should require *sudo* due to access `/dev/tun0`.
{: .notice--danger}
```bash
➜  nextepc git:(master) ✗ sudo make check
```

**Tip:** You can also check the result of `make check` with a tool that captures packets. If you are running `wireshark`, select the `loopback` interface and set FILTER to `s1ap || gtpv2 || diameter || gtp`.  You can see the virtually created packets. [[testcomplex.pcapng]]({{ site.url }}{{ site.baseurl }}/assets/pcapng/testcomplex.pcapng)
{: .notice--info}

You need to perform **the installation process**.
```bash
➜  nextepc git:(master) ✗ make install
```

### Building WebUI of NextEPC
---

[Node.js](https://nodejs.org/) is required to build WebUI of NextEPC

```bash
$ sudo pkg install node
```

Install the dependencies to run WebUI

```bash
➜  nextepc git:(master) ✗ cd webui
➜  webui git:(master) ✗ npm install
```

The WebUI runs as an [npm](https://www.npmjs.com/) script.

```bash
➜  webui git:(master) ✗ npm run dev
```


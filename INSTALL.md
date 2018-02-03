Requirements
------------

All platforms
=============

* CMake >= 3.8
* glib
* curl
* OpenSSL
* Goocanvas
* Gtk 2.x

N900
====

* scratchbox with all the usual stuff
* liblocation (propietary package from Nokia!)
* OpenSSL 1.0.2 (https://github.com/osm2go/openssl/releases)
* recent curl linked against the OpenSSL above

curl
====

The system curl on the N900 is old as the rest of the system. A version that is
linked against a recent OpenSSL is needed to be able to communicate with the
OSM API encrypted.

Sadly it is not easily possible to just add a new version like it is possible
with OpenSSL as the library major version for the most recent curl is still the
same. This could cause other applications suddenly see the new version, which
could cause massive chaos in case the application also needs the system OpenSSL
version for some reason.

So, one needs to set up a static curl that will be linked directly into OSM2go.

Download the latest version from https://curl.haxx.se and unpack it. Then do
this:

```shell
pushd your/curl/directory

CURL_VERSION=$(sed -rn '/^Version /s/^Version ([^ ]+) .*/\1/p' CHANGES |head -n 1)
CURL_DIR=/opt/curl-${CURL_VERSION}

./configure --disable-shared --disable-ftp --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-manual --disable-sspi --without-libssh2 --prefix=${CURL_DIR} --with-ca-path=/etc/certs/common-ca
make -j 10
make install

popd
```

Now you can configure OSM2go using the new curl:

```shell
cmake -D CMAKE_PREFIX_PATH=${CURL_DIR} -D CMAKE_BUILD_TYPE=Debug ~/osm2go
```

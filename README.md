CernVM FS cache plugin backed by Redis
======================================

Build
-----

Install the CernVM FS client and development packages.

Install requirements:

* Linux (Debian, Ubuntu):

        $ sudo apt-get install g++ cmake make redis-{server,tools} libhiredis-dev libev-dev

* macOS:

        $ brew install cmake redis hiredis libev

Build:

    $ git submodule init && git submodule update
    $ mkdir build && cmake ../ && make && sudo make install


License and copyright
---------------------

See LICENSE in the project root.


CernVM FS cache plugin backed by Redis
======================================

Build
-----

Install requirements:

* Linux (Debian, Ubuntu):

        $ sudo apt-get install cmake redis-{server,tools} libhiredis-dev libev-dev

* macOS:

        $ brew install redis hiredis libev

Build:

    $ mkdir build && cmake ../ && make && sudo make install


License and copyright
---------------------

See LICENSE in the project root.


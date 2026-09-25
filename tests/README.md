Feed download regression tests use Qt 5 Core, Network and Test, plus a C++11
compiler. From the repository root, build outside the source tree:

```sh
mkdir -p /tmp/tezi-feed-tests
cd /tmp/tezi-feed-tests
qmake /path/to/qt-tezi/tests/imagelistdownload.pro
make
./tst_imagelistdownload
```

The tests compile the production download and image-list sources without text
substitutions. Each test supplies a private QTemporaryDir as the storage root;
normal application callers still use /var/volatile. No device or network access
is required. Replies are completed explicitly in differing orders, avoiding
wall-clock timing assumptions.

Coverage includes a 2,000-entry feed, malformed metadata, failed icons and image
ordering.

`feed_test_support.cpp` supplies the same QJsonDocument conversion as Json::parse
without linking schema-validation libraries. Fixtures supply nominal_size, so
hardware-dependent media polling is not linked. These tests do not cover schema
validation, board hardware, real HTTP transport or device memory usage.

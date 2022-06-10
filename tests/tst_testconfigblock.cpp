#include <QtTest>
#include <QCoreApplication>
#include "configblock.h"

// add necessary includes here

class TestConfigBlock : public QObject
{
    Q_OBJECT

public:
    TestConfigBlock();
    ~TestConfigBlock();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testConfigBlockParsing();
    void testConfigBlockParsingExtendedRevisions();
    void testConfigBlockPrototypeProdid();
    void testIsProductSupportedPid4();
    void testIsProductSupportedPid8();
    void testIsProductSupportedPid8Range();
};

TestConfigBlock::TestConfigBlock()
{

}

TestConfigBlock::~TestConfigBlock()
{

}

void TestConfigBlock::initTestCase()
{

}

void TestConfigBlock::cleanupTestCase()
{

}


void TestConfigBlock::testConfigBlockPrototypeProdid()
{
    QVERIFY(ConfigBlock::isTdxPrototypeProdid(2600));
    QVERIFY(!ConfigBlock::isTdxPrototypeProdid(39));
}

void TestConfigBlock::testConfigBlockParsing()
{
    /*
     * A real-world binary blob can be obtained from a board using
     * hexdump -C /dev/mmcblk0boot0 | tail
     */
    QByteArray cfgblockBinary = QByteArray::fromHex(
                "00 40 01 cf 02 40 08 00  01 00 01 00 00 00 27 00"
                "02 40 00 00 00 14 2d 2f  7e 29 00 00 00 00 00 00"
                );
    ConfigBlock cfg(cfgblockBinary);

    QVERIFY(cfg.getProductNumber() == "0039");
    QVERIFY(cfg.getSerialNumber() == "03112489");
    QVERIFY(cfg.getBoardRev() == "V1.1A");
    QVERIFY(cfg.getPID8() == "00391100");
}

void TestConfigBlock::testConfigBlockParsingExtendedRevisions()
{
    /*
     * A real-world binary blob can be obtained from a board using
     * hexdump -C /dev/mmcblk0boot0 | tail
     */
    QByteArray cfgblockBinary = QByteArray::fromHex(
                "00 40 01 cf 02 40 08 00  09 00 09 00 1a 00 27 00"
                "02 40 00 00 00 14 2d 2f  7e 29 00 00 00 00 00 00"
                );
    ConfigBlock cfg(cfgblockBinary);

    QVERIFY(cfg.getProductNumber() == "0039");
    QVERIFY(cfg.getSerialNumber() == "03112489");
    QVERIFY(cfg.getBoardRev() == "V9.9#26");
    QVERIFY(cfg.getPID8() == "00399926");
}

void TestConfigBlock::testIsProductSupportedPid4()
{
    // Legacy image, should work as usual
    QStringList supportedProducts = { "0039", "0040" };

    QVERIFY(ConfigBlock::isProductSupported("00391100", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("00411100", supportedProducts));
}

void TestConfigBlock::testIsProductSupportedPid8()
{
    // Backward compatible image: Newer Tezi should only parse Pid8
    QStringList supportedProducts = { "00391100", "0039" };

    QVERIFY(ConfigBlock::isProductSupported("00391100", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("00391101", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("00410000", supportedProducts));
}

void TestConfigBlock::testIsProductSupportedPid8Range()
{
    QStringList supportedProducts = { "00391100-00391102", "00391200-", "0039" };

    QVERIFY(ConfigBlock::isProductSupported("00391102", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("00391103", supportedProducts));
    QVERIFY(ConfigBlock::isProductSupported("00391200", supportedProducts));
    QVERIFY(ConfigBlock::isProductSupported("00391201", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("00401000", supportedProducts));
}

QTEST_MAIN(TestConfigBlock)

#include "tst_testconfigblock.moc"

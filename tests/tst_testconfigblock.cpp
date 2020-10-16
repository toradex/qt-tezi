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
    void testConfigBlockPrototypeProdid();
    void testIsProductSupported();
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

void TestConfigBlock::testIsProductSupported()
{
    QStringList supportedProducts = { "0039", "0040" };

    QVERIFY(ConfigBlock::isProductSupported("0039", supportedProducts));
    QVERIFY(!ConfigBlock::isProductSupported("0041", supportedProducts));
}

QTEST_MAIN(TestConfigBlock)

#include "tst_testconfigblock.moc"

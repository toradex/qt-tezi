#include "json.h"
#include <QDebug>
#include <QStringBuilder>
#include <QFile>
#include <QJsonDocument>


#include <fstream>
#include <iostream>
#include <stdexcept>

#include <rapidjson/document.h>

#include <valijson/adapters/rapidjson_adapter.hpp>
#include <valijson/utils/rapidjson_utils.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

using std::cerr;
using std::endl;

using valijson::Schema;
using valijson::SchemaParser;
using valijson::Validator;
using valijson::ValidationResults;
using valijson::adapters::RapidJsonAdapter;

/* Json helper class
 *
 * Outsources the hard work to QJson
 * Schema validation uses RapidJson and ValiJson,
 * both are header only libraries
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

QVariant Json::parse(const QByteArray &json)
{
    QJsonDocument doc(QJsonDocument::fromJson(json));
    if (doc.isNull())
    {
        qDebug() << "Error parsing json";
        qDebug() << "Json input:" << json;
    }

    return doc.toVariant();
#if 0
    QJson::Parser parser;
    bool ok;
    QVariant result = parser.parse (json, &ok);

    if (!ok)
    {
        qDebug() << "Error parsing json";
        qDebug() << "Json input:" << json;
    }

    return result;
#endif
    return QVariant();
}

QVariant Json::loadFromFile(const QString &filename)
{
    QFile f(filename);

    if (!f.open(f.ReadOnly))
    {
        qDebug() << "Error opening file:" << filename;
        return QVariant();
    }

    QByteArray json = f.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(json));
    f.close();

    if (doc.isNull())
    {
        qDebug() << "Error parsing json file:" << filename;
    }

    return doc.toVariant();
#if 0
    QFile f(filename);
    QJson::Parser parser;
    bool ok;

    if (!f.open(f.ReadOnly))
    {
        qDebug() << "Error opening file:" << filename;
        return QVariant();
    }


    QVariant result = parser.parse (&f, &ok);
    f.close();

    if (!ok)
    {
        qDebug() << "Error parsing json file:" << filename;
    }

    return result;
#endif
    return QVariant();
}

/*
 * This currently uses RapidJson to parse the Json file (again!). This
 * is far from a ideal solution, but it works today. Unfortunately there
 * is no Json Schema parser which works with QJson out of the box.
 * The ValiJson Schema validator would allow to write an adapter to any
 * Json parsers document type, hence an adapter to QJson (or Json files
 * as QVariant) should be doable.
 *
 * However, ValiJson has an adapter for the Qt5 native JSON support... So
 * rather to invest time in writing an adapter for the Qt4 library, it
 * would be better to move forward and start porting Tezi to Qt5...
 */
bool Json::validate(const QByteArray &schemastr, const QByteArray &filestr, QString &errortext)
{
    // Load the document containing the schema
    rapidjson::Document schemaDocument;
    if (!valijson::utils::loadDocumentFromString(schemastr.constData(), schemaDocument)) {
        errortext = "Failed to load schema document.";
        return false;
    }

    // Load the document that is to be validated
    rapidjson::Document targetDocument;
    if (!valijson::utils::loadDocumentFromString(filestr, targetDocument)) {
        errortext = "Failed to load target document.";
        return false;
    }

    // Parse the json schema into an internal schema format
    Schema schema;
    SchemaParser parser;
    RapidJsonAdapter schemaDocumentAdapter(schemaDocument);
    try {
        parser.populateSchema(schemaDocumentAdapter, schema);
    } catch (std::exception &e) {
        errortext = "Failed to parse schema: " + QString::fromStdString(e.what()) + "\n";
        return false;
    }

    // Perform validation
    Validator validator(Validator::kWeakTypes);
    ValidationResults results;
    RapidJsonAdapter targetDocumentAdapter(targetDocument);
    if (!validator.validate(schema, targetDocumentAdapter, &results)) {
        //std::cerr << "Validation failed." << endl;
        ValidationResults::Error error;
        unsigned int errorNum = 1;
        while (results.popError(error)) {

            std::string context;
            std::vector<std::string>::iterator itr = error.context.begin();
            for (; itr != error.context.end(); itr++) {
                context += *itr;
            }

            errortext += "Error #" + QString::number(errorNum)
                 + " at " + QString::fromStdString(context) + "\n"
                 + "Description: " + QString::fromStdString(error.description) + "\n";
            ++errorNum;
        }
        return false;
    }

    return true;
}

QByteArray Json::serialize(const QVariant &json)
{

    QJsonDocument doc(QJsonDocument::fromVariant(json));
    return doc.toJson();
#if 0
    QJson::Serializer serializer;
    bool ok;

    serializer.setIndentMode(QJson::IndentFull);
    QByteArray result = serializer.serialize(json, &ok);

    if (!ok)
    {
        qDebug() << "Error serializing json";
    }

    return result;
#endif
    return NULL;
}

void Json::saveToFile(const QString &filename, const QVariant &json)
{
    QFile f(filename);
    QJsonDocument doc(QJsonDocument::fromVariant(json));

    if (!f.open(f.WriteOnly))
    {
        qDebug() << "Error opening file for writing: " << filename;
        return;
    }

    QByteArray data = doc.toBinaryData();
    if (f.write(data) != data.count())
    {
        qDebug() << "Error writing data to file: " << filename;
        return;
    }

#if 0
    QFile f(filename);
    QJson::Serializer serializer;
    bool ok;

    if (!f.open(f.WriteOnly))
    {
        qDebug() << "Error opening file for writing: " << filename;
        return;
    }
    serializer.setIndentMode(QJson::IndentFull);
    serializer.serialize(json, &f, &ok);
    f.close();

    if (!ok)
    {
        qDebug() << "Error serializing json to file:" << filename;
    }
#endif
}

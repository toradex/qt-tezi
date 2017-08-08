#include "json.h"
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <QDebug>
#include <QStringBuilder>
#include <QFile>


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
    QJson::Parser parser;
    bool ok;
    QVariant result = parser.parse (json, &ok);

    if (!ok)
    {
        qDebug() << "Error parsing json";
        qDebug() << "Json input:" << json;
    }

    return result;
}

QVariant Json::loadFromFile(const QString &filename)
{
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
}

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
    QJson::Serializer serializer;
    bool ok;

    serializer.setIndentMode(QJson::IndentFull);
    QByteArray result = serializer.serialize(json, &ok);

    if (!ok)
    {
        qDebug() << "Error serializing json";
    }

    return result;
}

void Json::saveToFile(const QString &filename, const QVariant &json)
{
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
}

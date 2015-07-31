/*=====================================================================

 QGroundControl Open Source Ground Control Station

 (c) 2009 - 2014 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>

 This file is part of the QGROUNDCONTROL project

 QGROUNDCONTROL is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 QGROUNDCONTROL is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with QGROUNDCONTROL. If not, see <http://www.gnu.org/licenses/>.

 ======================================================================*/

/// @file
///     @author Don Gagne <don@thegagnes.com>

#include "PX4AirframeLoader.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "AirframeComponentAirframes.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QGC_LOGGING_CATEGORY(PX4AirframeLoaderLog, "PX4AirframeLoaderLog")

bool PX4AirframeLoader::_airframeMetaDataLoaded = false;

PX4AirframeLoader::PX4AirframeLoader(AutoPilotPlugin* autopilot, UASInterface* uas, QObject* parent)
{
    Q_UNUSED(autopilot);
    Q_UNUSED(uas);
    Q_UNUSED(parent);
    Q_ASSERT(uas);
}

/// Load Airframe Fact meta data
///
/// The meta data comes from firmware airframes.xml file.
void PX4AirframeLoader::loadAirframeFactMetaData(void)
{
    if (_airframeMetaDataLoaded) {
        return;
    }

    qCDebug(PX4AirframeLoaderLog) << "Loading PX4 airframe fact meta data";
    qDebug() << "LOADING META DATA";

    Q_ASSERT(AirframeComponentAirframes::get().count() == 0);

    QString airframeFilename;

    // We want unit test builds to always use the resource based meta data to provide repeatable results
    if (!qgcApp()->runningUnitTests()) {
        // First look for meta data that comes from a firmware download. Fall back to resource if not there.
        QSettings settings;
        QDir parameterDir = QFileInfo(settings.fileName()).dir();
        airframeFilename = parameterDir.filePath("PX4AirframeFactMetaData.xml");
    }
    if (airframeFilename.isEmpty() || !QFile(airframeFilename).exists()) {
        airframeFilename = ":/AutopilotPlugins/PX4/AirframeFactMetaData.xml";
    }

    qCDebug(PX4AirframeLoaderLog) << "Loading meta data file:" << airframeFilename;

    QFile xmlFile(airframeFilename);
    Q_ASSERT(xmlFile.exists());

    bool success = xmlFile.open(QIODevice::ReadOnly);
    Q_UNUSED(success);
    Q_ASSERT(success);

    if (!success) {
        qWarning() << "Failed opening airframe XML";
        return;
    }

    QXmlStreamReader xml(xmlFile.readAll());
    xmlFile.close();
    if (xml.hasError()) {
        qWarning() << "Badly formed XML" << xml.errorString();
        return;
    }

    QString         airframeGroup;
    QString         image;
    QString         errorString;
    int             xmlState = XmlStateNone;
    bool            badMetaData = true;

    while (!xml.atEnd()) {
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "airframes") {
                if (xmlState != XmlStateNone) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundAirframes;

            } else if (elementName == "version") {
                if (xmlState != XmlStateFoundAirframes) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundVersion;

                bool convertOk;
                QString strVersion = xml.readElementText();
                int intVersion = strVersion.toInt(&convertOk);
                if (!convertOk) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                if (intVersion < 1) {
                    // We can't read these old files
                    qDebug() << "Airframe version stamp too old, skipping load. Found:" << intVersion << "Want: 3 File:" << airframeFilename;
                    return;
                }


            } else if (elementName == "airframe_group") {
                if (xmlState != XmlStateFoundVersion) {
                    // We didn't get a version stamp, assume older version we can't read
                    qDebug() << "Parameter version stamp not found, skipping load" << airframeFilename;
                    return;
                }
                xmlState = XmlStateFoundGroup;

                if (!xml.attributes().hasAttribute("name") || !xml.attributes().hasAttribute("image")) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                airframeGroup = xml.attributes().value("name").toString();
                image = xml.attributes().value("image").toString();
                qCDebug(PX4AirframeLoaderLog) << "Found group: " << airframeGroup;

            } else if (elementName == "airframe") {
                if (xmlState != XmlStateFoundGroup) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundAirframe;

                if (!xml.attributes().hasAttribute("name") || !xml.attributes().hasAttribute("id")) {
                    qWarning() << "Badly formed XML";
                    return;
                }

                QString name = xml.attributes().value("name").toString();
                QString id = xml.attributes().value("id").toString();

                qCDebug(PX4AirframeLoaderLog) << "Found airframe name:" << name << " type:" << airframeGroup << " id:" << id;

                // Now that we know type we can airframe meta data object and add it to the system
                AirframeComponentAirframes::insert(airframeGroup, image, name, id.toInt());

            } else {
                // We should be getting meta data now
                if (xmlState != XmlStateFoundAirframe) {
                    qWarning() << "Badly formed XML";
                    return;
                }

                if (!badMetaData) {

                    // We eventually want this, just not yet now
//                    if (elementName == "short_desc") {
//                        Q_ASSERT(metaData);
//                        QString text = xml.readElementText();
//                        text = text.replace("\n", " ");
//                        qCDebug(PX4AirframeLoaderLog) << "Short description:" << text;
//                        metaData->setShortDescription(text);

//                    } else if (elementName == "long_desc") {
//                        Q_ASSERT(metaData);
//                        QString text = xml.readElementText();
//                        text = text.replace("\n", " ");
//                        qCDebug(PX4AirframeLoaderLog) << "Long description:" << text;
//                        metaData->setLongDescription(text);

//                    } else if (elementName == "min") {
//                        Q_ASSERT(metaData);
//                        QString text = xml.readElementText();
//                        qCDebug(PX4AirframeLoaderLog) << "Min:" << text;

//                        QVariant varMin;
//                        if (metaData->convertAndValidate(text, true /* convertOnly */, varMin, errorString)) {
//                            metaData->setMin(varMin);
//                        } else {
//                            qCWarning(PX4AirframeLoaderLog) << "Invalid min value, name:" << metaData->name() << " type:" << metaData->type() << " min:" << text << " error:" << errorString;
//                        }

//                    } else if (elementName == "max") {
//                        Q_ASSERT(metaData);
//                        QString text = xml.readElementText();
//                        qCDebug(PX4AirframeLoaderLog) << "Max:" << text;

//                        QVariant varMax;
//                        if (metaData->convertAndValidate(text, true /* convertOnly */, varMax, errorString)) {
//                            metaData->setMax(varMax);
//                        } else {
//                            qCWarning(PX4AirframeLoaderLog) << "Invalid max value, name:" << metaData->name() << " type:" << metaData->type() << " max:" << text << " error:" << errorString;
//                        }

//                    } else if (elementName == "unit") {
//                        Q_ASSERT(metaData);
//                        QString text = xml.readElementText();
//                        qCDebug(PX4AirframeLoaderLog) << "Unit:" << text;
//                        metaData->setUnits(text);

//                    } else {
                        qDebug() << "Unknown element in XML: " << elementName;
//                    }
                }
            }
        } else if (xml.isEndElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "airframe") {
                // Done loading this airframe, validate

                // Reset for next airframe
                badMetaData = false;
                xmlState = XmlStateFoundGroup;
            } else if (elementName == "airframe_group") {
                xmlState = XmlStateFoundVersion;
            } else if (elementName == "airframes") {
                xmlState = XmlStateFoundAirframes;
            }
        }
        xml.readNext();
    }

    _airframeMetaDataLoaded = true;
}

void PX4AirframeLoader::clearStaticData(void)
{
    AirframeComponentAirframes::clear();
    _airframeMetaDataLoaded = false;
}
